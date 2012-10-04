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

#include "bjnp.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>   /* inet_ functions / structs */
#endif
#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h> /* DNS HEADER struct */
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif

#include <resolv.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <netdb.h>
#include <cups/http.h>
#include <net/if.h>

#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

/* local definitions */

#define BST_PRINTING 0x80
#define BST_BUSY     0x20
#define BST_OPCALL   0x08
#define STR_BST      "BST:"


/* static data */

static uint16_t serial = 0;
static uint16_t session_id;
static char cur_printer_model[BJNP_MODEL_MAX];
static char cur_printer_IEEE1284_id[BJNP_IEEE1284_MAX];
static struct
{
  uint16_t seq_no;
  ssize_t count;
  char print_buf[BJNP_PRINTBUF_MAX + sizeof (struct BJNP_command)];
  char free;
}
io_slot;

int
parse_IEEE1284_to_model (char *printer_id, char *model)
{
/*
 * parses the  IEEE1284  ID of the printer to retrieve make and model
 * of the printer
 * Returns: 0 = not found
 *          1 = found, model is set
 */

  char s[BJNP_IEEE1284_MAX];
  char *tok;

  strcpy (s, printer_id);
  model[0] = '\0';

  tok = strtok (s, ";");
  while (tok != NULL)
    {
      /* DES contains make and model */

      if (strncmp (tok, "DES:", 4) == 0)
	{
	  strcpy (model, tok + 4);
	  return 1;
	}
      tok = strtok (NULL, ";");
    }
  return 0;
}

int
parse_status_to_paperout (char *status_str)
{
/*
 * parses the  status string of the printer to retrieve paper status
 * of the printer
 * Returns: BJNP_PAPER_OK = paper ok
 *          BJNP_PAPER_OUT = out of paper
 *          BJNP_PAPER_UNKNOWN = paper status not found
 */

  char s[BJNP_IEEE1284_MAX];
  char *tok;
  unsigned int status;

  strcpy (s, status_str);

  tok = strtok (s, ";");
  while (tok != NULL)
    {
      /* BST contains status */

      if (strncmp (tok, STR_BST, strlen (STR_BST)) == 0)
	{
	  if (sscanf (tok + 4, "%2x", &status) != 1)
	    {
	      bjnp_debug (LOG_WARN, "Could not find paper status tag: %s!\n",
			  STR_BST);
	      return BJNP_PAPER_UNKNOWN;
	    }
	  else
	    {
	      bjnp_debug (LOG_DEBUG,
			  "Read printer status: %u\n  Printing = %d\n  Busy = %d\n  PaperOut = %d\n",
			  status, ((status & BST_PRINTING) != 0),
			  ((status & BST_BUSY) != 0),
			  ((status & BST_OPCALL) != 0));
	      if (status & BST_OPCALL)
		{
		  bjnp_debug (LOG_INFO, "Paper out!\n");
		  return BJNP_PAPER_OUT;
		}
	      else
		{
		  bjnp_debug (LOG_INFO, "Paper ok!\n");
		  return BJNP_PAPER_OK;
		}
	    }
	}
      tok = strtok (NULL, ";");
    }
  return BJNP_PAPER_UNKNOWN;
}


int
charTo2byte (char d[], char s[], int len)
{
  /*
   * copy ASCII string to 2 byte unicode string
   * Returns: number of characters copied
   */

  int done = 0;
  int copied = 0;
  int i;

  for (i = 0; i < len; i++)
    {
      d[2 * i] = '\0';
      if (s[i] == '\0')
	{
	  done = 1;
	}
      if (done == 0)
	{
	  d[2 * i + 1] = s[i];
	  copied++;
	}
      else
	d[2 * i + 1] = '\0';
    }
  return copied;
}


int
find_bin_string (const void *in, int len, char *lookfor, int size)
{
  /* 
   * looks for a new print command in the input stream 
   * Returns: offset where lookfor is found or -1 when not found
   */

  int i;
  const char *buf = in;

  /* start at offset 1 to avoid match at start of input */
  for (i = 1; i < (len - size); i++)
    {
      if ((buf[i] == lookfor[0]) && (memcmp (buf + i, lookfor, size) == 0))
	{
	  return i;
	}
    }
  return -1;
}




int
set_cmd (struct BJNP_command *cmd, char cmd_code, int my_session_id,
	 int payload_len)
{
  /*
   * Set command buffer with command code, session_id and lenght of payload
   * Returns: sequence number of command
   */
  strncpy (cmd->BJNP_id, BJNP_STRING, sizeof (cmd->BJNP_id));
  cmd->dev_type = BJNP_CMD_PRINT;
  cmd->cmd_code = cmd_code;
  cmd->unknown1 = htons(0);
  cmd->seq_no = htons (++serial);
  cmd->session_id = htons (my_session_id);

  cmd->payload_len = htonl (payload_len);

  return serial;
}



int
udp_command (http_addr_t * addr, char *command, int cmd_len, char *response,
	     int resp_len)
{
  /*
   * Send UDP command and retrieve response
   * Returns: length of response or -1 in case of error
   */

  int sockfd;
  int numbytes;
  fd_set fdset;
  struct timeval timeout;
  int try;

  bjnp_debug (LOG_DEBUG, "Sending UDP command to %s:%d\n",
	      inet_ntoa (addr->ipv4.sin_addr), ntohs (addr->ipv4.sin_port));

  if ((sockfd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
      bjnp_debug (LOG_CRIT, "udp_command: sockfd - %s\n", strerror (errno));
      return -1;
    }

  if (connect (sockfd, &(addr->addr), (socklen_t) sizeof (struct sockaddr_in))
      != 0)
    {
      bjnp_debug (LOG_CRIT, "udp_command: connect - %s\n", strerror (errno));
      return -1;
    }

  for (try = 0; try < 3; try++)
    {
      if ((numbytes = send (sockfd, command, cmd_len, 0)) != cmd_len)
	{
	  bjnp_debug (LOG_CRIT, "udp_command: Sent only %d bytes of packet",
		      numbytes);
	}


      FD_ZERO (&fdset);
      FD_SET (sockfd, &fdset);
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      if (select (sockfd + 1, &fdset, NULL, NULL, &timeout) <= 0)
	{
          /* no data recieved OR error, in either case retry */
	  continue;
	}

      if ((numbytes = recv (sockfd, response, resp_len, MSG_WAITALL)) == -1)
	{
	  bjnp_debug (LOG_CRIT, "udp_command: no data received (recv)");
	  continue;
	}
      close (sockfd);
      return numbytes;
    }
  /* max tries reached, return failure */
  close (sockfd);
  return -1;
}

void
get_printer_id (http_addr_t * addrlist, char *model, char *IEEE1284_id)
{
  /*
   * get printer identity
   * Sets model (make and model) and IEEE1284_id
   */

  struct BJNP_command cmd;
  struct IDENTITY *id;
  char printer_id[BJNP_IEEE1284_MAX];
  int resp_len;
  int id_len;
  char resp_buf[BJNP_RESP_MAX];

  /* set defaults */

  strcpy (model, "Unidentified printer");
  strcpy (IEEE1284_id, "");

  set_cmd (&cmd, CMD_UDP_GET_ID, 0, 0);

  bjnp_hexdump (LOG_DEBUG2, "Get printer identity", (char *) &cmd,
		sizeof (struct BJNP_command));

  resp_len =
    udp_command (addrlist, (char *) &cmd, sizeof (struct BJNP_command),
		 resp_buf, BJNP_RESP_MAX);

  if (resp_len <= 0)
    return;

  bjnp_hexdump (LOG_DEBUG2, "Printer identity:", resp_buf, resp_len);

  id = (struct IDENTITY *) resp_buf;

  id_len = ntohs (id->id_len) - sizeof (id->id_len);

  /* set IEEE1284_id */

  strncpy (printer_id, id->id, id_len);
  printer_id[id_len] = '\0';

  bjnp_debug (LOG_INFO, "Identity = %s\n", printer_id);

  if (IEEE1284_id != NULL)
    strcpy (IEEE1284_id, printer_id);

  /* get make&model from IEEE1284 id  */

  if (model != NULL)
    {
      parse_IEEE1284_to_model (printer_id, model);
      bjnp_debug (LOG_INFO, "Printer model = %s\n", model);
    }
}

bjnp_paper_status_t
bjnp_get_paper_status (http_addrlist_t * addrlist)
{
  /*
   * get printer paper status
   */

  struct BJNP_command cmd;
  struct IDENTITY *id;
  int resp_len;
  int id_len;
  char resp_buf[BJNP_RESP_MAX];

  /* set defaults */

  set_cmd (&cmd, CMD_UDP_GET_STATUS, 0, 0);

  bjnp_hexdump (LOG_DEBUG2, "Get printer status", (char *) &cmd,
		sizeof (struct BJNP_command));

  resp_len =
    udp_command (&addrlist->addr, (char *) &cmd, sizeof (struct BJNP_command),
		 resp_buf, BJNP_RESP_MAX);

  if (resp_len <= 0)
    return BJNP_PAPER_UNKNOWN;

  bjnp_hexdump (LOG_DEBUG2, "Printer status:", resp_buf, resp_len);

  id = (struct IDENTITY *) resp_buf;

  id_len = ntohs (id->id_len) - sizeof (id->id_len);

  return parse_status_to_paperout (id->id);

}


void
get_printer_address (char *resp_buf, char *address, char *name)
{
  /*
   * Parse identify responses to ip-address
   * and lookup hostname
   */

  struct in_addr ip_addr;
  struct hostent *myhost;

  struct INIT_RESPONSE *init_resp;

  init_resp = (struct INIT_RESPONSE *) resp_buf;
  sprintf (address, "%u.%u.%u.%u",
	   init_resp->ip_addr[0],
	   init_resp->ip_addr[1],
	   init_resp->ip_addr[2], init_resp->ip_addr[3]);

  bjnp_debug (LOG_INFO, "Found printer at ip address: %s\n", address);

  /* do reverse name lookup, if hostname can not be fouund return ip-address */

  inet_aton (address, &ip_addr);
  myhost = gethostbyaddr (&ip_addr, sizeof (ip_addr), AF_INET);

  /* some buggy routers return noname if reverse lookup fails */

  if ((myhost == NULL) || (myhost->h_name == NULL) ||
      (strncmp (myhost->h_name, "noname", 6) == 0))

    /* no valid name found, so we will use the ip-address */

    strcpy (name, address);
  else
    /* we received a name, so we will use it */

    strcpy (name, myhost->h_name);
}




int
bjnp_send_broadcast (struct in_addr local_addr, struct in_addr broadcast_addr, 
			struct BJNP_command cmd, int size)
{
  /*
   * send command to interface and return open socket
   */

  struct sockaddr_in locaddr;
  struct sockaddr_in sendaddr;
  int sockfd;
  int broadcast = 1;
  int numbytes;


  if ((sockfd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
      bjnp_debug (LOG_CRIT, "discover_printer: sockfd - %s",
		  strerror (errno));
      return -1;
    }

  /* Set broadcast flag on socket */

  if (setsockopt
      (sockfd, SOL_SOCKET, SO_BROADCAST, (const char *) &broadcast,
       sizeof (broadcast)) != 0)
    {
      bjnp_debug (LOG_CRIT, "discover_printer: setsockopts - %s",
		  strerror (errno));
      close (sockfd);
      return -1;
    };

  /* Bind to local address of interface, use BJNP printer port */

  memset (&locaddr, '\0', sizeof locaddr); 
  locaddr.sin_family = AF_INET;
  locaddr.sin_port = htons (BJNP_PORT_PRINT);
  locaddr.sin_addr = local_addr;

  if (bind
      (sockfd, (struct sockaddr *) &locaddr,
       (socklen_t) sizeof (locaddr)) != 0)
    {
      bjnp_debug (LOG_CRIT, "discover_printer: bind - %s\n",
		  strerror (errno));
      close (sockfd);
      return -1;
    }

  /* set address to send packet to */
  /* usebroadcast address of interface */

  memset (&sendaddr, '\0', sizeof sendaddr);
  sendaddr.sin_family = AF_INET;
  sendaddr.sin_port = htons (BJNP_PORT_PRINT);
  sendaddr.sin_addr = broadcast_addr; 


  if ((numbytes = sendto (sockfd, &cmd, sizeof (struct BJNP_command), 0,
			  (struct sockaddr *) &sendaddr, size)) != size)
    {
      bjnp_debug (LOG_DEBUG,
		  "discover_printers: Sent only %d bytes of packet, error = %s\n",
		  numbytes, strerror (errno));
      /* not allowed, skip this interface */

      close (sockfd);
      return -1;
    }
  return sockfd;
}

int
bjnp_discover_printers (struct printer_list *list)
{
  struct sockaddr_in sendaddr;
  int numbytes = 0;
  struct BJNP_command cmd;
  int num_printers = 0;
  char resp_buf[2048];
  http_addr_t http_addr;
#ifdef HAVE_GETIFADDRS
  struct ifaddrs *interfaces;
  struct ifaddrs *interface;
  char addr[16];
  char broadcast[16];
#else
  struct in_addr broadcast;
  struct in_addr local;
#endif
  int socket_fd[BJNP_SOCK_MAX];
  int no_sockets;
  int i;
  int last_socketfd = 0;
  fd_set fdset;
  fd_set active_fdset;
  struct timeval timeout;

  FD_ZERO (&fdset);

  set_cmd (&cmd, CMD_UDP_DISCOVER, 0, 0);

#ifdef HAVE_GETIFADDRS

  /*
   * Send UDP broadcast to discover printers and return the list of printers found
   * Returns: number of printers found
   */

  getifaddrs (&interfaces);
  interface = interfaces;

  for (no_sockets = 0; (no_sockets < BJNP_SOCK_MAX) && (interface != NULL);)
    {
      /* send broadcast packet to each suitable  interface */

      if ((interface->ifa_addr == NULL) || (interface->ifa_broadaddr == NULL) ||
          (interface->ifa_addr->sa_family != AF_INET) ||
          (((struct sockaddr_in *) interface->ifa_addr)->sin_addr.s_addr ==
           htonl(INADDR_LOOPBACK)))
        {
          /* not an IPv4 capable interface */

         bjnp_debug(LOG_DEBUG, "%s is not a valid IPv4 interface, skipping...\n",
                 interface->ifa_name);
        }
    else    
      {
        strcpy(addr, inet_ntoa (((struct sockaddr_in *) interface->ifa_addr)->sin_addr));
        strcpy(broadcast, inet_ntoa (((struct sockaddr_in *) interface->ifa_broadaddr)->sin_addr));
        bjnp_debug(LOG_DEBUG, "%s is IPv4 capable, sending broadcast from %s to %s.\n",
                 interface->ifa_name, addr, broadcast);
          
      if ((socket_fd[no_sockets] =
	   bjnp_send_broadcast (((struct sockaddr_in *) interface->
                                     ifa_addr)->sin_addr, 
				((struct sockaddr_in *) interface->
                                     ifa_broadaddr)->sin_addr, 
				     cmd, sizeof (cmd))) != -1)

	{
	  if (socket_fd[no_sockets] > last_socketfd)
	    {
	      /* track highest used socket for use in select */

	      last_socketfd = socket_fd[no_sockets];
	    }
	  FD_SET (socket_fd[no_sockets], &fdset);
	  no_sockets++;
	}
       }
      interface = interface->ifa_next;
    }
  freeifaddrs (interfaces);
#else
 /* 
  * we do not have getifaddrs(), so there is no easy way to find all interfaces
  * with teir broadcast addresses. We use a single global broadcast instead
  */
  no_sockets = 0;
  broadcast.s_addr = htonl(INADDR_BROADCAST);
  local.s_addr = htonl(INADDR_ANY);

  if ((socket_fd[no_sockets] =
       bjnp_send_broadcast (local, broadcast, cmd, sizeof (cmd))) != -1)
    {
      if (socket_fd[no_sockets] > last_socketfd)
        {
          /* track highest used socket for use in select */

          last_socketfd = socket_fd[no_sockets];
        }
      FD_SET (socket_fd[no_sockets], &fdset);
      no_sockets++;
    }
#endif

  /* wait for up to 1 second for a UDP response */

  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  active_fdset = fdset;

  while (select (last_socketfd + 1, &active_fdset, NULL, NULL, &timeout) > 0)
    {
      bjnp_debug (LOG_DEBUG, "Select returned, time left %d.%d....\n",
		  timeout.tv_sec, timeout.tv_usec);

      for (i = 0; i < no_sockets; i++)
	{
	  if (FD_ISSET (socket_fd[i], &active_fdset))
	    {
	      if ((numbytes =
		   recv (socket_fd[i], resp_buf, sizeof (resp_buf),
			 MSG_WAITALL)) == -1)
		{
		  bjnp_debug (LOG_CRIT,
			      "discover_printers: no data received");
		  break;
		}
	      else
		{

		  bjnp_hexdump (LOG_DEBUG2, "Discover response:", &resp_buf,
				numbytes);


		  /* check if ip-address of printer is returned */

		  if ((numbytes != sizeof (struct INIT_RESPONSE))
		      || (strncmp ("BJNP", resp_buf, 4) != 0))
		    {
		      /* printer not found */
		      break;
		    }
		};


	      /* printer found, get IP-address and hostname */
	      get_printer_address (resp_buf,
				   list[num_printers].ip_address,
				   list[num_printers].hostname);
	      list[num_printers].port = BJNP_PORT_PRINT;
	      http_addr.ipv4.sin_family = AF_INET;
	      http_addr.ipv4.sin_port = htons (BJNP_PORT_PRINT);
	      http_addr.ipv4.sin_addr.s_addr =
		inet_addr (list[num_printers].ip_address);
	      memset (http_addr.ipv4.sin_zero, '\0',
		      sizeof sendaddr.sin_zero);


	      /* set printer make and model as well as IEEE1284 identity */

	      get_printer_id (&http_addr, list[num_printers].model,
			      list[num_printers].IEEE1284_id);

	      num_printers++;
	    }
	}
      active_fdset = fdset;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
    }
  bjnp_debug (LOG_DEBUG, "printer discovery finished...\n");

  for (i = 0; i < no_sockets; i++)
    close (socket_fd[i]);

  return num_printers;
}


http_addrlist_t *
bjnp_send_job_details (http_addrlist_t * list, char *user, char *title)
{
/* 
 * send details of printjob to printer
 * Returns: addrlist set to address details of used printer
 */

  char cmd_buf[BJNP_CMD_MAX];
  char resp_buf[BJNP_RESP_MAX];
  char hostname[256];
  int resp_len;
  struct JOB_DETAILS *job;
  struct BJNP_command *resp;

  /* send job details command */

  while (list != NULL)
    {
      /* we only support IPv4 for now */

      if (list->addr.addr.sa_family != AF_INET) {
	list = list->next;
	continue;
      }
      set_cmd ((struct BJNP_command *) cmd_buf, CMD_UDP_PRINT_JOB_DET, 0,
	       sizeof (*job));

      /* create payload */

      gethostname (hostname, 255);

      job = (struct JOB_DETAILS *) (cmd_buf);
      charTo2byte (job->unknown, "", sizeof (job->unknown));
      charTo2byte (job->hostname, hostname, sizeof (job->hostname));
      charTo2byte (job->username, user, sizeof (job->username));
      charTo2byte (job->jobtitle, title, sizeof (job->jobtitle));

      bjnp_hexdump (LOG_DEBUG2, "Job details", cmd_buf,
		    (sizeof (struct BJNP_command) + sizeof (*job)));

      bjnp_debug (LOG_DEBUG, "Connecting to %s:%d\n",
		  inet_ntoa (list->addr.ipv4.sin_addr),
		  ntohs (list->addr.ipv4.sin_port));
      resp_len =
	udp_command (&list->addr, cmd_buf,
		     sizeof (struct BJNP_command) +
		     sizeof (struct JOB_DETAILS), resp_buf, BJNP_RESP_MAX);

      if (resp_len > 0)
	{
	  bjnp_hexdump (LOG_DEBUG2, "Job details response:", resp_buf,
			resp_len);
	  resp = (struct BJNP_command *) resp_buf;
	  session_id = ntohs (resp->session_id);
	  io_slot.free = 1;

	  /* set printer information in case it is needed later */

	  get_printer_id (&(list->addr), cur_printer_model,
			  cur_printer_IEEE1284_id);
	  return list;
	}
      list = list->next;
    }
  return list;			/* NULL */
}

void
bjnp_finish_job (http_addrlist_t * list)
{
/* 
 * Signal end of printjob to printer
 */

  char resp_buf[BJNP_RESP_MAX];
  int resp_len;
  struct BJNP_command cmd;

  set_cmd (&cmd, CMD_UDP_CLOSE, session_id, 0 );

  bjnp_hexdump (LOG_DEBUG2, "Finish printjob", (char *) &cmd,
		sizeof (struct BJNP_command));
  resp_len =
    udp_command (&list->addr, (char *) &cmd, sizeof (struct BJNP_command),
		 resp_buf, BJNP_RESP_MAX);

  if (resp_len != sizeof (struct BJNP_command))
    {
      bjnp_debug (LOG_CRIT,
		  "Received %d characters on close command, expected %d\n",
		  resp_len, sizeof (struct BJNP_command));
    }
  bjnp_hexdump (LOG_DEBUG2, "Finish printjob response", resp_buf, resp_len);

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

  if (!io_slot.free)
    {
      errno = EAGAIN;
      return -1;
    }

  /* set BJNP command header */

  io_slot.seq_no =
    set_cmd ((struct BJNP_command *) io_slot.print_buf, CMD_TCP_PRINT,
	     session_id, count);
  io_slot.count = count;
  memcpy (io_slot.print_buf + sizeof (struct BJNP_command), buf, count);

  bjnp_debug (LOG_DEBUG, "bjnp_write2: printing %d bytes\n", count);
  bjnp_hexdump (LOG_DEBUG2, "Print data:", (char *) io_slot.print_buf,
		sizeof (struct BJNP_command) + count);

  if ((sent_bytes =
       write (fd, io_slot.print_buf,
	      sizeof (struct BJNP_command) + count)) < 0)
    {
      /* return result from write */
      terrno = errno;
      bjnp_debug (LOG_CRIT, "bjnp_write2: Could not send data!\n");
      errno = terrno;
      return sent_bytes;
    }
  /* correct nr of bytes sent for length of command */
  /* sent_byte < expected is an unrecoverable error */

  else if (sent_bytes != ((int) (sizeof (struct BJNP_command) + count)))
    {
      errno = EIO;
      return -1;
    }
  io_slot.free = 0;
  return sent_bytes - sizeof (struct BJNP_command);
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
  char resp_buf[BJNP_RESP_MAX];
  struct PRINT_RESP *resp;
  fd_set input;
  struct timeval timeout;
  unsigned int recv_bytes;
  unsigned int resp_seqno;
  int terrno;
  int payload;

  bjnp_debug (LOG_DEBUG, "bjnp_backchannel: receiving response\n");

  /* get response header */

  if ((recv_bytes =
       read (fd, resp_buf,
	     sizeof (struct BJNP_command))) != sizeof (struct BJNP_command))
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

  resp = (struct PRINT_RESP *) resp_buf;

  payload = ntohl (resp->cmd.payload_len);

  if (payload > 0)
    {
      /* read payload (nr of bytes received by printer */

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
	   read (fd, resp_buf + sizeof (struct BJNP_command),
		 ntohl (resp->cmd.payload_len))) !=
	  ntohl (resp->cmd.payload_len))
	{
	  terrno = errno;
	  bjnp_debug (LOG_CRIT,
		      "bjnp_backchannel: could not read response payload (recv)!\n");
	  errno = terrno;
	  return BJNP_IO_ERROR;
	}
      *written = ntohl (resp->num_printed);
    }
  else
    {
      /* there is no payload, assume 0 bytes received */
      *written = 0;
    }

  bjnp_hexdump (LOG_DEBUG2, "TCP response:", resp_buf,
		sizeof (struct BJNP_command) + ntohl (resp->cmd.payload_len));

  if (resp->cmd.cmd_code != CMD_TCP_PRINT)
    {
      /* not a print response, discard */

      bjnp_debug (LOG_DEBUG, "Not a printing response packet, discarding!");
      return BJNP_NOT_AN_ACK;
    }

  resp_seqno = ntohs (resp->cmd.seq_no);

  /* do sanity check on sequence number of response */
  if (resp_seqno != io_slot.seq_no)
    {
      bjnp_debug (LOG_CRIT,
		  "bjnp_backchannel: printer reported sequence number %d, expected %d\n",
		  resp_seqno, io_slot.seq_no);

      errno = EIO;
      return BJNP_IO_ERROR;
    }

  /* valid response */

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
   * This function van also be used to send keep-alive packets when count = 0 
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
