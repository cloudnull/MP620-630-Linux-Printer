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
 * Process bjnp commands
 */

#include "bjnp.h"
#include "bjnp-protocol.h"
#include <errno.h>
#include <stdio.h>

static int serial = 0;
uint16_t  session_id = 0;

int
parse_status_to_paperout (int len, char *status_str)
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
  s[len] = '\0';

  tok = strtok (s, ";");
  while (tok != NULL)
    {
      /* BST contains status */

      if (strncmp (tok, STR_BST, strlen (STR_BST)) == 0)
        {
          if (sscanf (tok + 4, "%2x", &status) != 1)
            {
              bjnp_debug (LOG_WARN, "Could not parse paper status tag: %s!\n",
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

void
clear_cmd(bjnp_command_t * cmd)
{
  memset(cmd, 0, sizeof(*cmd) );
}
                                    
int
bjnp_set_command_header (bjnp_command_t *cmd, char cmd_code, int my_session_id,
	 int command_len)
{
  /*
   * Set command buffer with command code, session_id and lenght of payload
   * Returns: sequence number of command
   */
  strncpy (cmd->header.BJNP_id, BJNP_STRING, sizeof (cmd->header.BJNP_id));
  cmd->header.dev_type = BJNP_CMD_PRINT;
  cmd->header.cmd_code = cmd_code;
  cmd->header.unknown1 = htons(0);
  cmd->header.seq_no = htons (++serial);
  cmd->header.session_id = htons (my_session_id);

  cmd->header.payload_len = htonl (command_len - bjnp_header_size);

  return serial;
}



static int
bjnp_process_udp_command (http_addr_t * addr, bjnp_command_t *command, int cmd_len, bjnp_response_t *response)
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
  char ipaddress[128];
  int port;
  int size;

  get_address_info( addr, ipaddress, &port);
  bjnp_debug (LOG_DEBUG, "Sending UDP command to %s port %d\n",
	      ipaddress, port );

  if ((sockfd = socket (get_protocol_family( addr), SOCK_DGRAM, 0)) == -1)
    {
      bjnp_debug (LOG_CRIT, "bjnp_process_udp_command: sockfd - %s\n", strerror (errno));
      return -1;
    }

  size = sa_size(addr);
  if (connect (sockfd, &(addr->addr), (socklen_t) size )
      != 0)
    {
      bjnp_debug (LOG_CRIT, "bjnp_process_udp_command: connect - %s\n", strerror (errno));
      close( sockfd);
      return -1;
    }

  for (try = 0; try < 3; try++)
    {
      if ((numbytes = send (sockfd, command, cmd_len, 0)) != cmd_len)
	{
	  bjnp_debug (LOG_CRIT, "bjnp_process_udp_command: Sent only %d bytes of packet",
		      numbytes);
	}


      FD_ZERO (&fdset);
      FD_SET (sockfd, &fdset);
      timeout.tv_sec = 3;
      timeout.tv_usec = 0;

      if (select (sockfd + 1, &fdset, NULL, NULL, &timeout) <= 0)
	{
          /* no data recieved OR error, in either case retry */
	  continue;
	}

      if ((numbytes = recv (sockfd, response, sizeof(*response), MSG_WAITALL)) == -1)
	{
	  bjnp_debug (LOG_CRIT, "bjnp_process_udp_command: no data received (recv)");
	  continue;
	}
      close (sockfd);
      return numbytes;
    }
  /* max tries reached, return failure */
  close (sockfd);
  return -1;
}

int
get_printer_id (http_addr_t * addr, char *model, char *IEEE1284_id)
{
  /*
   * get printer identity
   * Sets model (make and model) and IEEE1284_id
   */

  bjnp_command_t cmd;
  bjnp_response_t id;
  char printer_id[BJNP_IEEE1284_MAX];
  int resp_len;
  int id_len;
  int min_size;

  /* set defaults */

  strcpy (model, "Unidentified printer");
  strcpy (IEEE1284_id, "");

  clear_cmd(&cmd);
  bjnp_set_command_header (&cmd, CMD_UDP_GET_ID, 0, sizeof(cmd.udp_get_id) );

  bjnp_hexdump (LOG_DEBUG2, "Get printer identity", (char *) &cmd,
		sizeof (cmd.udp_get_id));

  resp_len =
    bjnp_process_udp_command (addr, &cmd, sizeof (cmd.udp_get_id),
		 &id);

  min_size = bjnp_header_size + sizeof( id.udp_identity_response.id_len);
  if ( resp_len <= min_size )
    {
      return -1;
    }

  bjnp_hexdump (LOG_DEBUG2, "Printer identity:", &id, resp_len);

  id_len = ntohs (id.udp_identity_response.id_len) - sizeof (id.udp_identity_response.id_len);

  if ( (id_len < 0) || (id_len > (resp_len - bjnp_header_size) ) || ( id_len > BJNP_IEEE1284_MAX) )
    {
      bjnp_debug( LOG_DEBUG, "Id - length recieved is invalid: %d (total response length = %d\n",
                  id_len, resp_len);
      return -1;
    } 

  /* set IEEE1284_id */

  strncpy (printer_id, id.udp_identity_response.id, id_len);
  printer_id[id_len] = '\0';

  bjnp_debug (LOG_INFO, "Identity = %s\n", printer_id);

  if (IEEE1284_id != NULL)
    strcpy (IEEE1284_id, printer_id);

  /* get make&model from IEEE1284 id  */

  if (model != NULL)
    {
      parse_IEEE1284_to_model (printer_id, model);
      bjnp_debug (LOG_DEBUG, "Printer model = %s\n", model);
    }
  return 0;
}

bjnp_paper_status_t
bjnp_get_paper_status (http_addr_t * addr)
{
  /*
   * get printer paper status
   */

  bjnp_command_t cmd;
  bjnp_response_t response;
  int resp_len;

  /* set defaults */
  
  clear_cmd(&cmd);
  bjnp_set_command_header (&cmd, CMD_UDP_GET_STATUS, 0, sizeof(cmd.udp_get_status) );

  bjnp_hexdump (LOG_DEBUG2, "Get printer status", (char *) &cmd,
		sizeof (cmd.udp_get_status));

  resp_len =
    bjnp_process_udp_command (addr, &cmd, sizeof (cmd.udp_get_status),
		 &response);

  if (resp_len <= 0)
    return BJNP_PAPER_UNKNOWN;

  bjnp_hexdump (LOG_DEBUG2, "Printer status:", &response, resp_len);

  return parse_status_to_paperout ( ntohs(response.udp_status_response.status_len), response.udp_status_response.status);

}

int bjnp_send_job_details (http_addr_t *addr, char *user, char *title )
{
/* 
 * send details of printjob to printer
 * Returns: 0 = ok, -1 = error
 */

  char hostname[BJNP_HOST_MAX];
  int resp_len;
  bjnp_command_t cmd;
  bjnp_response_t resp;

  /* send job details command */

  clear_cmd(&cmd);
  bjnp_set_command_header (&cmd, CMD_UDP_PRINT_JOB_DET, 0,
                           sizeof (cmd.udp_job_details) );

  /* create payload */

  gethostname (hostname, BJNP_HOST_MAX - 1);

  charTo2byte (cmd.udp_job_details.unknown, "", sizeof (cmd.udp_job_details.unknown));
  charTo2byte (cmd.udp_job_details.hostname, hostname, sizeof (cmd.udp_job_details.hostname));
  charTo2byte (cmd.udp_job_details.username, user, sizeof (cmd.udp_job_details.username));
  charTo2byte (cmd.udp_job_details.jobtitle, title, sizeof (cmd.udp_job_details));

  bjnp_hexdump (LOG_DEBUG2, "Job details", &cmd,
                sizeof(cmd.udp_job_details));
  resp_len = bjnp_process_udp_command (addr, &cmd, sizeof(cmd.udp_job_details), &resp);

  if (resp_len > 0)
    {
      bjnp_hexdump (LOG_DEBUG2, "Job details response:", &resp,
                        resp_len);
      session_id = ntohs (resp.udp_print_job_details_response.header.session_id);
      return 0;
    } 
  return -1;
}

int bjnp_send_close( http_addr_t *addr)
{
/* 
 * Signal end of printjob to printer
 */

  int resp_len;
  bjnp_command_t cmd;
  bjnp_response_t resp;

  clear_cmd(&cmd);
  bjnp_set_command_header (&cmd, CMD_UDP_CLOSE, session_id, sizeof(cmd.udp_close) );

  bjnp_hexdump (LOG_DEBUG2, "bjnp_send_close", (char *) &cmd,
                sizeof( cmd.udp_close));
  resp_len =
    bjnp_process_udp_command (addr, &cmd, sizeof (cmd.udp_close),
                 &resp);

  if (resp_len != sizeof (resp.udp_close_response))
    {
      bjnp_debug (LOG_CRIT,
                  "Received %d characters in close response, expected %d\n",
                  resp_len, sizeof (resp.udp_close_response));
      return -1;
    }
  bjnp_hexdump (LOG_DEBUG2, "Finish printjob response", &resp, resp_len);
  return 0;
}

