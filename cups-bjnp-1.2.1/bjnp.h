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
#define _CUPS_BJNP_H_

#include "config.h"
#include <cups/cups.h>
#include <cups/backend.h>
#include <cups/http.h>

#define BJNP_CUPS_VERSION  (100 * CUPS_VERSION_MAJOR + CUPS_VERSION_MINOR )

#if BJNP_CUPS_VERSION  >= 103
#include <cups/sidechannel.h>
#endif

/* 
 *  BJNP definitions 
 */

#define BJNP_PRINTBUF_MAX 4096	/* size of printbuffer */
#define BJNP_CMD_MAX 2048	/* size of BJNP response buffer */
#define BJNP_RESP_MAX 2048	/* size of BJNP response buffer */
#define BJNP_STATUS_MAX 256     /* max size for status string */
#define BJNP_IEEE1284_MAX 1024  /* max. allowed size of IEEE1284 id */
#define BJNP_METHOD_MAX 16      /* max length of method */
#define BJNP_HOST_MAX 128       /* max length of hostname or address */
#define BJNP_PORT_MAX 64        /* max length of port string */
#define BJNP_ARGS_MAX 128       /* max size of argument string */
#define BJNP_MODEL_MAX 64	/* max allowed size for make&model */
#define BJNP_IEEE1284_MAX 1024	/* max. allowed size of IEEE1284 id */
#define BJNP_SERIAL_MAX 16	/* siuze of serial (mac-address) string */
#define BJNP_SOCK_MAX 256	/* maximum number of open sockets */
#define BJNP_PRINTERS_MAX 64	/* nax. number of printers in discovery */
#define KEEP_ALIVE_SECONDS 3	/* max interval/2 seconds before we */
				/* send an empty data packet to the */
				/* printer */
#define BJNP_MAX_BROADCAST_ATTEMPTS 2   /* number of broadcast packets to be sent */
#define BJNP_BROADCAST_INTERVAL 10      /* ms between broadcasts */

#define USLEEP_MS 1000                  /* sleep for 1 msec */
#define BJNP_BC_RESPONSE_TIMEOUT 500    /* waiting time for broadc. responses */
#define BJNP_PORT_PRINT 8611
typedef enum
{
  BJNP_ADDRESS_IS_LINK_LOCAL = 0,
  BJNP_ADDRESS_IS_GLOBAL = 1,
  BJNP_ADDRESS_HAS_FQDN = 2
} bjnp_address_type_t;

typedef enum bjnp_paper_status_e
{
  BJNP_PAPER_UNKNOWN = -1,
  BJNP_PAPER_OK = 0,
  BJNP_PAPER_OUT = 1
} bjnp_paper_status_t;

typedef enum
{
  BJNP_STATUS_GOOD,
  BJNP_STATUS_INVAL,
  BJNP_STATUS_ALREADY_ALLOCATED
} BJNP_Status;

/*
 * structure that stores information on found printers 
 */

struct printer_list
{
  http_addr_t *addr;			/* adress of printer */
  char hostname[BJNP_HOST_MAX];		/* hostame, if found, else ip-address */
  int host_type;			/* indicates how desirable it is to use */
                                        /* this address: */
                                        /* 0 = link local address */
                                        /* 1 = global address without a FQDN */
                                        /* 2 = globall address with FQDN */
  int port;				/* port number */
  char IEEE1284_id[BJNP_IEEE1284_MAX];  /* IEEE1284 printer id */
  char model[BJNP_MODEL_MAX];	        /* printer make and model */
  char mac_address[BJNP_SERIAL_MAX];    /* unique serial number (mac_address) */
};

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

/* opaque types, defined in bjnp-protocol.h */
union bjnp_command_u;
union bjnp_response_u;
typedef union bjnp_command_u bjnp_command_t;
typedef union bjnp_response_u bjnp_response_t;

/*
 * bjnp-runloop.c
 */
extern ssize_t bjnp_backendRunLoop (int print_fd, int device_fd,
				    http_addr_t * addr);

/* 
 * bjnp-io.c
 */

int bjnp_addr_connect( http_addr_t *addr);
http_addr_t * bjnp_start_job (http_addrlist_t * list, 
               char *user, char *title);
void bjnp_finish_job (http_addr_t * addr);
int bjnp_backchannel (int fd, ssize_t * written);
ssize_t bjnp_write (int fd, const void *buf, size_t count);
extern int bjnp_backendGetDeviceID (char *device_id,
				    int device_id_size, char *make_model,
				    int make_model_size);

/*
 * bjnp-discover.c
 */
int bjnp_discover_printers (struct printer_list *list);

/*
 * bjnp-commands.c
 */
void clear_cmd( bjnp_command_t *cmd);
int bjnp_set_command_header (bjnp_command_t *cmd, char cmd_code, 
                           int my_session_id, int payload_len);
int get_printer_id (http_addr_t * addr, char *model, char *IEEE1284_id);
bjnp_paper_status_t bjnp_get_paper_status (http_addr_t * addr);
int bjnp_send_job_details (http_addr_t *addr, char *user, char *title );
int bjnp_send_close( http_addr_t *addr );

/*
 * bjnp-utils.c
 */
int sa_is_equal( const http_addr_t * sa1, const http_addr_t * sa2);
int sa_size( const http_addr_t *sa);
int get_protocol_family( const http_addr_t *sa);
void get_address_info ( const http_addr_t *addr, char * addr_string, int *port);
int parse_IEEE1284_to_model (char *printer_id, char *model);
int parse_status_to_paperout (int len, char *status_str);
int charTo2byte (char d[], char s[], int len);
int find_bin_string (const void *in, int len, char *lookfor, int size);
bjnp_address_type_t get_printer_host (const http_addr_t *printer_addr, 
     char *name, int *port);
void u8tohex_string( uint8_t * input, char * str, int size);
char * bjnp_map_status(cups_sc_status_t status);

/*
 * bjnp-debug.c
 */
void bjnp_set_debug_level (const char *level);
void bjnp_debug (bjnp_loglevel_t, const char *, ...);
void bjnp_hexdump (bjnp_loglevel_t level, char *header, const void *d_,
		   unsigned len);

/*
 * return values for bjnp_backchannel
 */
#define BJNP_OK 0
#define BJNP_IO_ERROR -1
#define BJNP_NOT_AN_ACK 1

/* definitions for functions available in cups 1.3 and later source tree only*/

#define _cupsLangPrintf	fprintf
#define _cupsLangPuts(a,b)  fputs(b,a)
#define _(x) (x)

#ifndef CUPS_LLCAST
#  define CUPS_LLCAST	(long)
#endif

/* static data */

extern uint16_t session_id;

#endif /* ! CUPS_BJNP_H_ */
