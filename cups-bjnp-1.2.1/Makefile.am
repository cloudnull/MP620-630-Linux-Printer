## Process this file with automake to produce Makefile.in
## AUTOMAKE_OPTIONS = foreign

cupsbackend_PROGRAMS = bjnp
bjnp_SOURCES = \
	bjnp.c \
	bjnp-runloop.c \
	bjnp-io.c \
	bjnp-debug.c \
	bjnp-utils.c \
	bjnp-discover.c \
	bjnp-commands.c \
	cups-bjnp.spec \
	bjnp.h \
	bjnp-protocol.h \
	TODO \
	conf/rpmbuild \
	conf/norpm
bjnp_CFLAGS = -Wall $(EXTRA_CFLAGS) 

@rpmtarget@
