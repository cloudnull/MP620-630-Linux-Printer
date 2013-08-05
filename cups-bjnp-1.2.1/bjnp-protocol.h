/*
 *   BJNP protocol definitions for the
 *   bjnp backend for the Common UNIX Printing System (CUPS).
 *   Copyright 2008 - 2012 by Louis Lagendijk
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
#ifndef _CUPS_BJNP_PROTOCOL_H_
#define _CUPS_BJNP_PROTOCOL_H_

/*
 * BJNP protocol related definitions
 */

#define BJNP_STRING "BJNP"
#define BJNP_METHOD "bjnp"

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

/* the bjnp header, used for commands and responses */

struct  __attribute__ ((__packed__)) bjnp_header
{
  char BJNP_id[4];		/* string: BJNP */
  uint8_t dev_type;		/* 1 = printer, 2 = scanner */
  uint8_t cmd_code;		/* command code/response code */
  uint16_t unknown1;		/* unknown, always 0? */
  uint16_t seq_no;		/* sequence number */
  uint16_t session_id;		/* session id for printing */
  uint32_t payload_len;		/* length of command buffer */
};

#define bjnp_header_size sizeof(struct bjnp_header)

/* command definition */

typedef union __attribute__ ((__packed__)) bjnp_command_u
  {
    struct bjnp_header header;
    struct  __attribute__ ((__packed__))
      {
        struct bjnp_header header;
      } udp_discover;
     struct  __attribute__ ((__packed__))
      {
        struct bjnp_header header;
        char unknown[8];        /* don't know what these are for */
        char hostname[64];      /* hostname of sender */
        char username[64];      /* username */
        char jobtitle[256];     /* job title */
      } udp_job_details;
     struct  __attribute__ ((__packed__))
      {
        struct bjnp_header header;
      } udp_close; 
     struct  __attribute__ ((__packed__))
      {
        struct bjnp_header header;
        char data [BJNP_PRINTBUF_MAX];
                                /* data to be printer */
      } tcp_print;
     struct  __attribute__ ((__packed__))
      {
        struct bjnp_header header;
      } udp_get_status; 
     struct  __attribute__ ((__packed__))
      {
        struct bjnp_header header;
      } udp_get_id;
  } bjnp_command_u;

typedef union __attribute__ ((__packed__)) bjnp_response_u
{
  struct bjnp_header header;
  struct  __attribute__ ((__packed__))
    {
      struct bjnp_header header;
      char unknown1[4];         /* 00 01 08 00 */
      char mac_len;             /* length of mac address */
      char addr_len;            /* length od address field */
      unsigned char mac_addr[6];/* printers mac address */
      union __attribute__ ((__packed__) )
        {
          struct __attribute__ ((__packed__)) 
            {
              unsigned char ipv4_addr[4];
                                /* a single IPv4 address */ 
            } ipv4;
            struct  __attribute__ ((__packed__)) 
              {
                unsigned char ipv6_addr[8][16];
                                /* array of IPv6 address */ 
              } ipv6;
        } addresses;
    } udp_discover_response;
  
  struct  __attribute__ ((__packed__))
    {
      struct bjnp_header header;
    } udp_print_job_details_response; 
  
  struct  __attribute__ ((__packed__))
    {
      struct bjnp_header header;
    } udp_close_response;
  
  struct  __attribute__ ((__packed__))
    {
      struct bjnp_header header;
      uint32_t accepted;        /* nr of bytes accepted by printer */       
    } tcp_print_response; 
  
  struct  __attribute__ ((__packed__))
    {
      struct bjnp_header header;
      uint16_t status_len;	/* length of status field */
      char status[2048];	
    } udp_status_response;
  
  struct  __attribute__ ((__packed__))
    {
      struct bjnp_header header;
      uint16_t id_len;		/* length of identity field */
      char id[2048];		/* identity */
    } udp_identity_response;
  char fillers[4096]; 
} bjnp_response_u;

#define BST_PRINTING 0x80
#define BST_BUSY     0x20
#define BST_OPCALL   0x08
#define STR_BST      "BST:"

#endif /* _CUPS_BJNP_PROTOCOL_H_ */
