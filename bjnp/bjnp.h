/*
 *   Data structures and definitions for
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
#ifndef _CUPS_BJNP_H_
#  define _CUPS_BJNP_H_

#   include "config.h"

#  include <cups/backend.h>
#  include <cups/cups.h>
#  include <cups/http.h>


#if CUPS_VERSION_MAJOR > 1 || CUPS_VERSION_MINOR >= 3
#  include <cups/sidechannel.h>
#endif


/*
 * Include standard headers 
 */

#  include <stdlib.h>
#  include <errno.h>
#  include <signal.h>
#  include <stdint.h>
#  include <string.h>
#  include <wchar.h>
#  include <unistd.h>

/*
 * BJNP protocol related definitions
 */

/* port numbers */
typedef enum bjnp_port_e
{
  BJNP_PORT_PRINT = 8611,
  BJNP_PORT_SCAN = 8612,
  BJNP_PORT_3 = 8613,
  BJNP_PORT_4 = 8614
} bjnp_port_t;

#define BJNP_STRING "BJNP"

/* commands */
typedef enum bjnp_cmd_e
{
  CMD_UDP_DISCOVER = 0x01,	/* discover if service type is listening at 
                                   this port */
  CMD_UDP_PRINT_JOB_DET = 0x10,	/* send print job owner details */
  CMD_UDP_CLOSE = 0x11,		/* request connection closure */
  CMD_TCP_PRINT = 0x21,		/* print */
  CMD_UDP_GET_STATUS = 0x20,	/* get printer status  */
  CMD_UDP_GET_ID = 0x30,	/* get printer identity */
  CMD_UDP_SCAN_JOB = 0x32	/* send scan job owner details */
} bjnp_cmd_t;

/* command type */

typedef enum uint8_t
{
  BJNP_CMD_PRINT = 0x1,		/* printer command */
  BJNP_CMD_SCAN = 0x2,		/* scanner command */
  BJNP_RES_PRINT = 0x81,	/* printer response */
  BJNP_RES_SCAN = 0x82		/* scanner response */
} bjnp_cmd_type_t;


struct BJNP_command
{
  char BJNP_id[4];		/* string: BJNP */
  uint8_t dev_type;		/* 1 = printer, 2 = scanner */
  uint8_t cmd_code;		/* command code/response code */
  uint16_t unknown1;		/* unknown, always 0? */
  uint16_t seq_no;		/* sequence number */
  uint16_t session_id;		/* session id for printing */
  uint32_t payload_len;		/* length of command buffer */
} __attribute__ ((__packed__));

/* Layout of the init response buffer */

struct INIT_RESPONSE
{
  struct BJNP_command response;	/* reponse header */
  char unknown1[6];		/* 00 01 08 00 06 04 */
  char mac_addr[6];		/* printers mac address */
  unsigned char ip_addr[4];	/* printers IP-address */
} __attribute__ ((__packed__));

/* layout of payload for the JOB_DETAILS command */

struct JOB_DETAILS
{
  struct BJNP_command cmd;	/* command header */
  char unknown[8];		/* don't know what these are for */
  char hostname[64];		/* hostname of sender */
  char username[64];		/* username */
  char jobtitle[256];		/* job title */
} __attribute__ ((__packed__));

/* Layout of ID and status responses */

struct IDENTITY
{
  struct BJNP_command cmd;
  uint16_t id_len;		/* length of identity */
  char id[2048];		/* identity */
} __attribute__ ((__packed__));


/* response to TCP print command */

struct PRINT_RESP
{
  struct BJNP_command cmd;
  uint32_t num_printed;		/* number of print bytes received */
} __attribute__ ((__packed__));

typedef enum bjnp_paper_status_e
{
  BJNP_PAPER_UNKNOWN = -1,
  BJNP_PAPER_OK = 0,
  BJNP_PAPER_OUT = 1
} bjnp_paper_status_t;


/* 
 *  BJNP definitions 
 */

#define BJNP_PRINTBUF_MAX 4096	/* size of printbuffer */
#define BJNP_CMD_MAX 2048	/* size of BJNP response buffer */
#define BJNP_RESP_MAX 2048	/* size of BJNP response buffer */
#define BJNP_SOCK_MAX 256	/* maximum number of open sockets */
#define BJNP_MODEL_MAX 64	/* max allowed size for make&model */
#define BJNP_IEEE1284_MAX 1024	/* max. allowed size of IEEE1284 id */
#define KEEP_ALIVE_SECONDS 3	/* max interval/2 seconds before we */
				/* send an empty data packet to the */
				/* printer */

/*
 * structure that stores information on found printers 
 */

struct printer_list
{
  char ip_address[16];
  char hostname[256];		/* hostame, if found, else ip-address */
  char IEEE1284_id[BJNP_IEEE1284_MAX];
  /* IEEE1284 printer id */
  int port;			/* udp/tcp port */
  char model[BJNP_MODEL_MAX];	/* printer make and model */
};



/* 
 * bjnp printing related functions 
 */

int bjnp_discover_printers (struct printer_list *list);
http_addrlist_t *bjnp_send_job_details (http_addrlist_t * list, char *user,
					char *title);
void bjnp_finish_job (http_addrlist_t * list);
ssize_t bjnp_write (int fd, const void *buf, size_t count);
int bjnp_backchannel (int fd, ssize_t * written);
bjnp_paper_status_t bjnp_get_paper_status (http_addrlist_t * addr);

/*
 * return values for bjnp_backchannel
 */
#define BJNP_OK 0
#define BJNP_IO_ERROR -1
#define BJNP_NOT_AN_ACK 1
#define BJNP_THROTTLE 2

typedef enum bjnp_loglevel_e
{
  LOG_NONE,
  LOG_EMERG,
  LOG_ALERT,
  LOG_CRIT,
  LOG_ERROR,
  LOG_WARN,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG,
  LOG_DEBUG2,
  LOG_END		/* not a real loglevel, but indicates end of list */
} bjnp_loglevel_t;

#ifndef CUPS_LOGDIR
#define CUPS_LOGDIR "/var/log/cups"
#endif /* CUPS_LOGDIR */

#define LOGFILE "bjnp_log"
/* 
 * debug related functions 
 */

void bjnp_set_debug_level (const char *level);
void bjnp_debug (bjnp_loglevel_t, const char *, ...);
void bjnp_hexdump (bjnp_loglevel_t level, char *header, const void *d_,
		   unsigned len);

/* 
 * backend related functions 
 */

extern int bjnp_backendGetDeviceID (char *device_id,
				    int device_id_size, char *make_model,
				    int make_model_size);
extern int bjnp_backendGetMakeModel (const char *device_id, char *make_model,
				     int make_model_size);
extern int bjnp_backendDrainOutput (int print_fd, int device_fd);
extern ssize_t bjnp_backendRunLoop (int print_fd, int device_fd,
				    http_addrlist_t * addr);

/* definitions for functions available in cups 1.3 and later source tree only*/

// #if ((CUPS_VERSION_MAJOR == 1) && (CUPS_VERSION_MINOR < 3)) || (STANDALONE == 1)
#define _cupsLangPrintf	fprintf
#define _cupsLangPuts(a,b)  fputs(b,a)
#define _(x) (x)
// #endif /* cups < 1.3 */

#ifndef CUPS_LLCAST
#  define CUPS_LLCAST	(long)
#endif
#endif /* ! CUPS_BJNP_H_ */
