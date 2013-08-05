/*
 *   TCP/IP IO communication implementation for
 *   bjnp backend for the Common UNIX Printing System (CUPS).
 *   Copyright 2008 by Louis Lagendijk
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Louis Lagendijk and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 * <to be added>
 */

#include <fcntl.h>
#include <errno.h>

#include "bjnp.h"
#include "bjnp-protocol.h"

typedef struct
{
  uint16_t seq_no;
  ssize_t count;
  bjnp_command_t print_buf;
  char free;
}
io_slot_t;

char cur_printer_IEEE1284_id[BJNP_IEEE1284_MAX];
char cur_printer_model[BJNP_MODEL_MAX];
io_slot_t io_slot;

/*
 * 'bjnp_addr_connect - Setup a TCP connection to the addresses .
 *
 */

int bjnp_addr_connect( http_addr_t *addr)
{
  int val;		
  int tcp_socket = -1;
#ifdef __APPLE__
  struct timeval timeout;
#endif /* __APPLE__ */
  char host[BJNP_HOST_MAX];
  int port;

  /*
   * Create the socket...
   */

  if ((tcp_socket = (int)socket(get_protocol_family(addr), SOCK_STREAM,
                             0)) < 0)
    {
      return tcp_socket;
    }

  /*
   * Set options...
   */
  val = 1;
  setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

#if 0
  val = 1;
  setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));

  val = 1;
  setsockopt(tcp_socket, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val));
#endif

#ifdef __APPLE__
  /*
   * Use a 30-second read timeout when connecting to limit the amount of time
   * we block...
   */

  timeout.tv_sec  = 30;
  timeout.tv_usec = 0;
  setsockopt(tcp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif /* __APPLE__ */

  /*
   * Using TCP_NODELAY improves responsiveness, especially on systems
   * with a slow loopback interface...
   */

  val = 1;
  setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));

  /*
   * Close this socket when starting another process...
   */

  fcntl(tcp_socket, F_SETFD, FD_CLOEXEC);

  /*
   * Then connect...
   */

  get_address_info(addr, host, &port);

  if ( connect( tcp_socket, &(addr->addr), sa_size(addr) ) != 0)
    {
      bjnp_debug(LOG_CRIT, "Failed to establish TCP connection to printer %s port %d\n",
                 host, port);
      close( tcp_socket);
      return -1;
    }
  bjnp_debug( LOG_DEBUG, "Established TP connection to printer %s port %d\n",
                 host, port);
  return tcp_socket;
}

http_addr_t *
bjnp_start_job (http_addrlist_t * list, char *user, char *title)
{
/* 
 * send details of printjob to printer
 * Returns: addrlist set to address details of used printer
 */
  http_addr_t * addr;
  http_addr_t * ret_addr;
  char host[BJNP_HOST_MAX];
  int port;

  while (list != NULL)
    {
      addr = (http_addr_t *) &(list->addr);
      get_address_info( addr, host, &port);
      bjnp_debug (LOG_DEBUG, "Connecting to %s port %d\n",
                  host, port);

      if (bjnp_send_job_details( addr, user, title ) == 0 )
	{
	  io_slot.free = 1;

	  /* set printer information in case it is needed later */

	  get_printer_id (&(list->addr), cur_printer_model,
			  cur_printer_IEEE1284_id);
          ret_addr = malloc(sizeof(http_addr_t) );
          memset(ret_addr, 0, sizeof(http_addr_t) );
          memcpy(ret_addr, addr, sa_size(addr) );
	  return ret_addr;
	}
      list = list->next;
    }
  return NULL;			/* NULL */
}

void
bjnp_finish_job (http_addr_t * addr)
{
/* 
 * Signal end of printjob to printer
 */

  bjnp_debug (LOG_DEBUG2, "Finish printjob\n");
  bjnp_send_close( addr );
}

ssize_t
bjnp_write2 (int fd, const void *buf, size_t count)
{
/*
 * This function writes printdata to the printer.  This function mimicks the std. 
 * lib. write function as much as possible.
 * Returns: number of bytes written to the printer
 */
  int sent_bytes;
  int terrno;
  int command_len;

  if (!io_slot.free)
    {
      errno = EAGAIN;
      return -1;
    }

  /* set BJNP command header */

  command_len =  bjnp_header_size + count;
  io_slot.seq_no = bjnp_set_command_header (&io_slot.print_buf, CMD_TCP_PRINT,
	                           session_id, command_len);
  io_slot.count = count;
  memcpy (&(io_slot.print_buf.tcp_print.data), buf, count);


  bjnp_debug (LOG_DEBUG, "bjnp_write2: printing %d bytes\n", count);
  bjnp_hexdump (LOG_DEBUG2, "Print data:", (char *) &io_slot.print_buf,
		command_len);

  if ((sent_bytes =
       write (fd, &io_slot.print_buf, command_len)) < 0)
    {
      /* return result from write */
      terrno = errno;
      bjnp_debug (LOG_CRIT, "bjnp_write2: Could not send data!\n");
      errno = terrno;
      return sent_bytes;
    }
  else if (sent_bytes != command_len)
    {
      errno = EIO;
      return -1;
    }
  io_slot.free = 0;

  /* correct nr of bytes sent for length of command */
  /* sent_byte < expected is an unrecoverable error */

  return sent_bytes - bjnp_header_size;
}



int
bjnp_backchannel (int fd, ssize_t * written)
{
/*
 * This function receives the responses to the write commands.
 * written wil be set to the number of bytes confirmed by the printer
 * Returns: 
 * BJNP_OK when valid ack is received, written is set to number of bytes 
 *         sent to and accepted by printer (could be 0 for keep-alive)
 * BJNP_IO_ERROR when any io-error occurred
 * BJNP_NOT_AN_ACK when the packet received was not an ack, must be ignored
 * BJNP_THROTTLE when printer indicated it could not handle the input data
 */
  bjnp_response_t response;
  unsigned int recv_bytes;
  unsigned int resp_seqno;
  int terrno;
  uint32_t payload_len;
  fd_set input;
  struct timeval timeout;

  bjnp_debug (LOG_DEBUG, "bjnp_backchannel: receiving response\n");

  /* get response header, we can unfortunately not */
  /* rely on getting the payload in the first read */

  if ((recv_bytes =
       read (fd, &response,
	     bjnp_header_size ) ) < bjnp_header_size )
    {
      terrno = errno;
      bjnp_debug (LOG_CRIT,
		  "bjnp_backchannel: (recv) could not read response header, recieved %d bytes!\n",
		  recv_bytes);
      bjnp_debug (LOG_CRIT, "bjnp_backchannel: (recv) error: %s!\n",
		  strerror (terrno));
      errno = terrno;
      return BJNP_IO_ERROR;
    }

  /* got response header back, get payload length */

  payload_len = ntohl(response.tcp_print_response.header.payload_len);

  /* it should be the same size as the accepted field */

  if (payload_len >= sizeof(response.tcp_print_response.accepted))
    {
      /* read nr of bytes accepted by printer */

      FD_ZERO (&input);
      FD_SET (fd, &input);

      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      if (select (fd + 1, &input, NULL, NULL, &timeout) <= 0)
	{
	  terrno = errno;
	  bjnp_debug (LOG_CRIT,
		      "bjnp_backchannel: could not read response payload (select)!\n");
	  errno = terrno;
	  return BJNP_IO_ERROR;
	}

      if ((recv_bytes =
	   read (fd, &response.tcp_print_response.accepted,
		 payload_len)) < payload_len)
	{
	  terrno = errno;
	  bjnp_debug (LOG_CRIT,
		      "bjnp_backchannel: could not read response payload (recv)!\n");
	  errno = terrno;
	  return BJNP_IO_ERROR;
	}
      *written = ntohl (response.tcp_print_response.accepted);
    }
  else
    {
      /* there is no payload, assume 0 bytes received */
      *written = 0;
    }


  bjnp_hexdump (LOG_DEBUG2, "TCP response:", &response,
		bjnp_header_size + recv_bytes);

  if (response.tcp_print_response.header.cmd_code != CMD_TCP_PRINT)
    {
      /* not a print response, discard */

      bjnp_debug (LOG_DEBUG, "Not a printing response packet, discarding!");
      return BJNP_NOT_AN_ACK;
    }

  resp_seqno = ntohs (response.header.seq_no);

  /* do sanity check on sequence number of response */
  if (resp_seqno != io_slot.seq_no)
    {
      bjnp_debug (LOG_CRIT,
		  "bjnp_backchannel: printer reported sequence number %d, expected %d\n",
		  resp_seqno, io_slot.seq_no);

      errno = EIO;
      return BJNP_IO_ERROR;
    }

  bjnp_debug (LOG_DEBUG,
	      "bjnp_backchannel: response: written = %lx, seqno = %lx\n",
	      *written, resp_seqno);

  io_slot.free = 1;

  /* check length reported by printer */

  if (io_slot.count == *written)
    {
      /* printer reported expected number of bytes */
      return BJNP_OK;
    }
  else if (*written == 0)
    {
      /* data was sent to printer, but printer reports that it is busy */
      /* add a delay before we try again */

      bjnp_debug (LOG_INFO, "Printer does not accept data, throttling....\n");
      usleep (40000);
      return BJNP_THROTTLE;
    }

  /* printer reports unexpected number of bytes */
  bjnp_debug (LOG_CRIT,
	      "bjnp_backchannel: printer reported %d bytes received, expected %d\n",
	      written, io_slot.count);
  errno = EIO;
  return BJNP_IO_ERROR;
}

ssize_t
bjnp_write (int fd, const void *buf, size_t count)
{
  /* This is a wrapper around bjnp_write2. It parses the input stream for BJL commands 
   * and outputs these in a new/separate tcp packet. Each call prints at most buffer upto
   * next command
   * It is an ugly hack, I know....
   * This function can also be used to send keep-alive packets when count = 0 
   */

  char start_cmd[] = { 0x1b, 0x5b, 0x4b, 0x2, 0x0, 0x0 };
  int print_count;
  int result;
  int terrno;

  /* TODO: allow scanning over buffer borders */

  bjnp_debug (LOG_DEBUG, "bjnp_write: starting printing of %d characters\n",
	      count);

  if ((print_count =
       find_bin_string (buf, count, start_cmd, sizeof (start_cmd))) == -1)
    {
      /* no command found, print whole buffer */

      print_count = count;
    }
  /* print content of buf upto command */

  result = bjnp_write2 (fd, buf, print_count);
  terrno = errno;
  bjnp_debug (LOG_DEBUG,
	      "bjnp_write: Printed %d bytes, last command sent: %d\n",
	      result, io_slot.seq_no);
  errno = terrno;

  return result;
}

int
bjnp_backendGetDeviceID (char *device_id, int device_id_size,
			 char *make_model, int make_model_size)
{
/*
 * Returns the printer information for the active printer
 * Returns: 0 if ok
 *          -1 if not found
 */
  strncpy (device_id, cur_printer_IEEE1284_id, device_id_size);
  device_id[device_id_size] = '\0';

  strncpy (make_model, cur_printer_model, make_model_size);
  make_model[make_model_size] = '\0';
  if ((strlen (make_model) == 0) && (strlen (device_id) == 0))
    return -1;
  return 0;
}
