#
# File:
#    Makefile
#
# Description:
#    Makefile for the CODA 3.0 primary and secondary readout lists
#    running on an Intel-based controller running Linux
#
#
# Uncomment DEBUG line for debugging info ( -g and -Wall )
DEBUG   ?= 1
QUIET	?= 1
#
ARCH=armv7l

# Plug in your primary readout lists here..
VMEROL		= vtp_list.so
# Add shared library dependencies here.  (jvme already included)
ROLLIBS		= -lvtp -li2c

COMPILE_TIME	= \""$(shell date)"\"

LINUXVME_LIB	?= $(CODA)/Linux-$(ARCH)/lib
LINUXVME_INC	?= $(CODA)/linuxvme/include
LINUXVME_BIN	?= $(CODA)/Linux-$(ARCH)/bin

# DEFs for compiling primary readout lists
CC			= gcc
AR                      = ar
RANLIB                  = ranlib
ifdef DEBUG
CFLAGS			= -Wall -g
else
CFLAGS			= -O3
endif
CFLAGS			+= -DJLAB -DLINUX -DDAYTIME=$(COMPILE_TIME)

INCS			= -I. -I$(HOME)/Linux-$(ARCH)/include \
				-isystem${LINUXVME_INC} \
				-isystem${CODA}/common/include
LIBS			= -L. -L$(HOME)/Linux-$(ARCH)/lib \
				-L${LINUXVME_LIB} \
				-L${CODA}/${MACHINE}/lib \
				-lrt -lpthread -ljvme $(ROLLIBS)

# DEFs for compiling CODA readout lists
CCRL			= ${CODA_BIN}/ccrl
CODA_INCS		= -I.  -I${LINUXVME_INC} -I${CODA}/common/include
CODA_LIBDIRS            = -L. -L${LINUXVME_LIB} -L${CODA}/${MACHINE}/lib
CODA_LIBS		= -ljvme
CODA_DEFS		= -DLINUX -DDAYTIME=$(COMPILE_TIME)
CODA_CFLAGS		= -O -w -fpic -shared ${CODA_INCS} ${CODA_LIBDIRS} \
			  ${CODA_LIBS} ${CODA_DEFS}
ifdef DEBUG
CODA_CFLAGS		+= -Wall -g
endif
CRLFILES		= $(wildcard *.crl)
CFILES			= $(CRLFILES:.crl=.c)
SOBJS			= $(CRLFILES:.crl=.so)
DEPS			= $(VMEROL:%.so=%.d)
DEPS			+= $(CFILES:%.c=%.d)


ifeq ($(QUIET),1)
	Q = @
else
	Q =
endif


all: $(VMEROL) $(SOBJS)

crl: $(SOBJS)

%.c: %.crl
	@echo " CCRL   $@"
	@${CCRL} $<

%.so: %.c
	@echo " CC     $@"
	$(Q)$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

clean distclean:
	$(Q)rm -f  $(VMEROL) $(SOBJS) $(CFILES) *~ $(DEPS)

%.d: %.c
	@echo " DEP    $@"
	$(Q)set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.so $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(DEPS)

.PHONY: clean distclean
