/*
 *
 *   bjnp specifc run loop APIs for the Common UNIX Printing System (CUPS).
 *   Copyright 2008 by  Louis Lagendijk
 *
 *   based on: 
 *
 *   Common run loop APIs for the Common UNIX Printing System (CUPS).
 *
 *   Copyright 2007-2008 by Apple Inc.
 *   Copyright 2006-2007 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Apple Inc. and are protected by Federal copyright
 *   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
 *   "LICENSE" which should have been included with this file.  If this
 *   file is missing or damaged, see the license at "http://www.cups.org/".
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   backendDrainOutput() - Removed, integrated in main loop         
 *   backendRunLoop()     - Read and write print and back-channel data.
 */

/*
 * Include necessary headers.
 */

#include "bjnp.h"
#ifdef __hpux
#  include <sys/time.h>
#else
#  include <sys/select.h>
#endif /* __hpux */

/*
 * 'backendRunLoop()' - Read and write print and back-channel data.
 */

ssize_t				/* O - Total bytes on success, -1 on error */
bjnp_backendRunLoop (int print_fd,	/* I - Print file descriptor */
		     int device_fd,	/* I - Device file descriptor */
		     http_addrlist_t * addrlist)
					/* I - addresslist for printer */
{
  int send_keep_alive;		/* flag that an empty data packet should be sent to printer */
  int nfds;			/* Maximum file descriptor value + 1 */
  fd_set input,			/* Input set for reading */
    output;			/* Output set for writing */
  ssize_t print_bytes,		/* Print bytes read */
    total_bytes,		/* Total bytes written */
    bytes;			/* Bytes written */
  int result;			/* result code from select */
  int paperout,			/* "Paper out" status */
    ack_pending;		/* io slot status */
  int offline;			/* "Off-line" status */
  int draining;			/* Drain command recieved? */
  char print_buffer[BJNP_PRINTBUF_MAX],
    /* Print data buffer */
   *print_ptr;			/* Pointer into print data buffer */
  struct timeval timeout;
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Actions for POSIX signals */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR >= 3)
  cups_sc_command_t command;	/* Request command */
  cups_sc_status_t status;	/* Request/response status */
  char data[2048];		/* Request/response data */
  int datalen;			/* Request/response data size */
  char model[256];		/* printer make & model */
  char dev_id[1024];		/* IEEE1284 device id */
#endif /* cups >= 1.3 */

  fprintf (stderr,
	   "DEBUG: bjnp_backendRunLoop(print_fd=%d, device_fd=%d\n",
	   print_fd, device_fd);

  /*
   * If we are printing data from a print driver on stdin, ignore SIGTERM
   * so that the driver can finish out any page data, e.g. to eject the
   * current page.  We only do this for stdin printing as otherwise there
   * is no way to cancel a raw print job...
   */

  if (!print_fd)
    {
#ifdef HAVE_SIGSET		/* Use System V signals over POSIX to avoid bugs */
      sigset (SIGTERM, SIG_IGN);
#elif defined(HAVE_SIGACTION)
      memset (&action, 0, sizeof (action));

      sigemptyset (&action.sa_mask);
      action.sa_handler = SIG_IGN;
      sigaction (SIGTERM, &action, NULL);
#else
      signal (SIGTERM, SIG_IGN);
#endif /* HAVE_SIGSET */
    }

  /*
   * Figure out the maximum file descriptor value to use with select()...
   */

  nfds = (print_fd > device_fd ? print_fd : device_fd) + 1;

  /*
   * Now loop until we are out of data from print_fd...
   */

  for (print_bytes = 0, print_ptr = print_buffer, offline = -1,
       paperout = -1, total_bytes = 0, ack_pending = 0, draining =
       0, send_keep_alive = 0;;)
    {
      /*
       * Use select() to determine whether we have data to copy around...
       */

      FD_ZERO (&input);
      FD_ZERO (&output);

      /*
       * Accept new printdata only when no data is left 
       */

      if (!print_bytes)
	FD_SET (print_fd, &input);

      /*
       * accept backchannel data from printer, used for acks 
       */

      FD_SET (device_fd, &input);


      /*
       * Accept side channel data, unless there is print data pending (cups >= 1.3)
       */

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR >= 3)
      if (!print_bytes && !draining)
	FD_SET (CUPS_SC_FD, &input);
#endif

      /*
       * Check if printer is ready to receive data when we have something to send 
       * (printdata is left or keep-alive is to be sent) but no ack is pending
       */

      if ((send_keep_alive || print_bytes) && !ack_pending)
	FD_SET (device_fd, &output);

      timeout.tv_sec = KEEP_ALIVE_SECONDS;
      timeout.tv_usec = 0;

      result = select (nfds, &input, &output, NULL, &timeout);

      if (result < 0)
	{
	  /*
	   * Pause printing to clear any pending errors...
	   */

	  if (errno == ENXIO && offline != 1)
	    {
	      fputs ("STATE: +offline-error\n", stderr);
	      _cupsLangPuts (stderr,
			     _("INFO: Printer is currently off-line.\n"));
	      offline = 1;
	    }
	  else if (errno == EINTR && total_bytes == 0)
	    {
	      fputs ("DEBUG: Received an interrupt before any bytes were "
		     "written, aborting!\n", stderr);
	      return (0);
	    }

	  sleep (1);
	  continue;
	}
      if (result == 0)
	{
	  /*
	   * timeout - no data for printer; make sure that next time we
	   * send a keep-alive packet to avoid that  connection to printer  
	   * times out
	   */
	  if (!ack_pending)
	    send_keep_alive = 1;

	  bjnp_debug (LOG_DEBUG,
		      "bjnp_runloop: select timeout send_keep_alive=%d print_fd=%d "
		      "device_fd=%d print_bytes=%d ack_pending=%d\n",
		      send_keep_alive, print_fd, device_fd, print_bytes,
		      ack_pending);
	  continue;
	}

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR >= 3)

      /*
       * Check if we have a side-channel request ready (cups >= 1.3)...
       */

      if (FD_ISSET (CUPS_SC_FD, &input))
	{
	  /*
	   * Do the side-channel request
	   */

	  datalen = sizeof (data);

	  if (cupsSideChannelRead (&command, &status, data, &datalen, 1.0))
	    {
	      _cupsLangPuts (stderr,
			     _
			     ("WARNING: Failed to read side-channel request!\n"));
	      bjnp_debug (LOG_DEBUG, "Failed to read side-channel request! Status is %d\n",  status);
	    }
	  else
	    {
	      bjnp_debug (LOG_DEBUG, "Received side-channel request, command is %d\n",  command);
	      switch (command)
		{
		case CUPS_SC_CMD_NONE:
		   /* Nothing to do.... */
		   break;

		case CUPS_SC_CMD_DRAIN_OUTPUT:
		  /*
		   * Our sockets disable the Nagle algorithm and data is sent immediately.
		   * 
		   */

		  draining = 1;
		  break;

		case CUPS_SC_CMD_GET_BIDI:
		  data[0] = 0;
		  datalen = 1;
		  cupsSideChannelWrite (command, status, data, datalen, 1.0);
		  break;

		case CUPS_SC_CMD_GET_DEVICE_ID:
		  if (bjnp_backendGetDeviceID
		      (dev_id, sizeof (dev_id), model, sizeof (model)) == 0)
		    {
		      strncpy (data, dev_id, sizeof (data));
		      datalen = (int) strlen (data);
		      cupsSideChannelWrite (command, status, data, datalen,
					    1.0);
		      break;
		    }

		default:
		  status = CUPS_SC_STATUS_NOT_IMPLEMENTED;
		  datalen = 0;
		  cupsSideChannelWrite (command, status, data, datalen, 1.0);
		  break;
		}

	    }
	}
#endif

      /*
       * Check if we have back-channel data (ack) ready...
       */

      if (FD_ISSET (device_fd, &input))
	{
	  result = bjnp_backchannel (device_fd, &bytes);
	  switch (result)
	    {
	    case BJNP_IO_ERROR:
	      perror ("ERROR: failed to read backchannel data");
	      return (-1);
	      break;
	    case BJNP_OK:
	      print_bytes -= bytes;
	      print_ptr += bytes;
	      total_bytes += bytes;
	      ack_pending = 0;

	      /*
	       * Success, reset paper out error conditions
	       */

	      if (paperout)
		{
		  fputs ("STATE: -media-empty-error\n", stderr);
		  paperout = 0;
		}

	      fprintf (stderr, "DEBUG: Wrote %d bytes of print data...\n",
		       (int) bytes);
	      break;
	    case BJNP_THROTTLE:
	      /*
	       * Data not accepted by printer, check paper out condition
	       */

	      if ((paperout != 1)
		  && (bjnp_get_paper_status (addrlist) == BJNP_PAPER_OUT))
		{
		  fputs ("STATE: +media-empty-error\n", stderr);
		  _cupsLangPuts (stderr, _("ERROR: Out of paper!\n"));
		  paperout = 1;
		}
	      ack_pending = 0;
	      break;

	    case BJNP_NOT_AN_ACK:
	      /* what we received was not an ack, no action */
	      break;

	    default:
	      /* no action */
	      break;
	    }
	}

      /*
       * Check if we have print data ready...
       */

      if (FD_ISSET (print_fd, &input))
	{
	  if ((print_bytes = read (print_fd, print_buffer,
				   sizeof (print_buffer))) < 0)
	    {
	      /*
	       * Read error - bail if we don't see EAGAIN or EINTR...
	       */

	      if (errno != EAGAIN || errno != EINTR)
		{
		  perror ("ERROR: Unable to read print data");
		  return (-1);
		}

	      print_bytes = 0;
	    }
	  else if (print_bytes == 0)
	    {
	      /*
	       * End of input file, break out of the loop
	       */

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR >= 3)
	      if (draining)
		{
		  command = CUPS_SC_CMD_DRAIN_OUTPUT;
		  status = CUPS_SC_STATUS_OK;
		  datalen = 0;
		  cupsSideChannelWrite (command, status, data, datalen, 1.0);
		  draining = 0;
		}
#endif

	      break;
	    }
	  else
	    {
	      print_ptr = print_buffer;

	      fprintf (stderr, "DEBUG: Read %d bytes of print data...\n",
		       (int) print_bytes);
	    }
	}

      /*
       * Check if the device is ready to receive data and we have data to
       * send...
       */

      if ((send_keep_alive || print_bytes) && FD_ISSET (device_fd, &output))
	{
	  bytes = bjnp_write (device_fd, print_ptr, print_bytes);
	  send_keep_alive = 0;
	  if (bytes < 0)
	    {
	      /*
	       * Write error - bail if we don't see an error we can retry...
	       */

	      if (errno == ENOSPC)
		{
		  if (paperout != 1)
		    {
		      fputs ("STATE: +media-empty-error\n", stderr);
		      _cupsLangPuts (stderr, _("ERROR: Out of paper!\n"));
		      paperout = 1;
		    }
		}
	      else if (errno == ENXIO)
		{
		  if (offline != 1)
		    {
		      fputs ("STATE: +offline-error\n", stderr);
		      _cupsLangPuts (stderr,
				     _
				     ("INFO: Printer is currently off-line.\n"));
		      offline = 1;
		    }
		}
	      else if (errno != !EAGAIN && errno != EINTR && errno != ENOTTY)
		{
		  fprintf (stderr,
			   _("ERROR: Unable to write print data: %s\n"),
			   strerror (errno));
		  return (-1);
		}
	    }
	  else
	    {
	      if (offline)
		{
		  fputs ("STATE: -offline-error\n", stderr);
		  _cupsLangPuts (stderr,
				 _("INFO: Printer is now on-line.\n"));
		  offline = 0;
		}

	      /*
	       * we sent data, wait for the ack before sending more data
	       */

	      ack_pending = 1;
	    }
	}
    }

  /*
   * Return with success...
   */

  return (total_bytes);
}
