#
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#
# Makefile for GridSample C/ODBC sample program
#

TTDIR		= $(TIMESTEN_HOME)
OBJDIR		= obj
BINDIR		= bin
INSTDIR         = $(TTDIR)/install

CC		= gcc

PLATCFLAGS	= -m64 -O2
LDFLAGS		= -O2 -Wl,-rpath -Wl,$(INSTDIR)/lib

INCS		= -I. -I$(INSTDIR)/include
#
# uncomment the OPTDEFS directive to enable debug mode within the app
#
#OPTDEFS		= -DDEBUG
CSDEFS		= -DTTCLIENTSERVER
CFLAGS		= $(PLATCFLAGS) $(OPTDEFS) $(INCS)

TTLINK		= -L$(INSTDIR)/lib
DCLIBS		= $(TTLINK) -ltten
CSLIBS		= $(TTLINK) -lttclient

GSOBJS		= $(OBJDIR)/GridSample.o $(OBJDIR)/GSOdbc.o $(OBJDIR)/GSArgs.o \
		  $(OBJDIR)/GSUtil.o $(OBJDIR)/GSUsage.o $(OBJDIR)/GSErrors.o \
                  $(OBJDIR)/GSTxnUpdateGridInfo.o \
		  $(OBJDIR)/GSTxnGetCounts.o \
		  $(OBJDIR)/GSTxnClearHistory.o \
		  $(OBJDIR)/GSTxnPurge.o \
		  $(OBJDIR)/GSTxnQuery.o \
		  $(OBJDIR)/GSTxnAuthorize.o \
		  $(OBJDIR)/GSTxnCharge.o \
		  $(OBJDIR)/GSTxnTopup.o
GSOBJSCS	= $(OBJDIR)/GridSampleCS.o $(OBJDIR)/GSOdbcCS.o \
		  $(OBJDIR)/GSArgsCS.o $(OBJDIR)/GSUtilCS.o \
		  $(OBJDIR)/GSUsageCS.o $(OBJDIR)/GSErrorsCS.o \
                  $(OBJDIR)/GSTxnUpdateGridInfoCS.o \
		  $(OBJDIR)/GSTxnGetCountsCS.o \
		  $(OBJDIR)/GSTxnClearHistoryCS.o \
		  $(OBJDIR)/GSTxnPurgeCS.o \
		  $(OBJDIR)/GSTxnQueryCS.o \
		  $(OBJDIR)/GSTxnAuthorizeCS.o \
		  $(OBJDIR)/GSTxnChargeCS.o \
		  $(OBJDIR)/GSTxnTopupCS.o
GRIDSAMPLE	= $(BINDIR)/GridSample
GRIDSAMPLECS	= $(BINDIR)/GridSampleCS

all:		csonly direct

direct:		$(GRIDSAMPLE)

csonly:		$(GRIDSAMPLECS)

clean:
	rm -f $(GRIDSAMPLECS) $(GRIDSAMPLE) $(OBJDIR)/*.o

# Targets for binaries

$(GRIDSAMPLE):	$(GSOBJS)
	$(CC) -o $@ $(LDFLAGS) $(GSOBJS) $(DCLIBS)

$(GRIDSAMPLECS):	$(GSOBJSCS)
	$(CC) -o $@ $(LDFLAGS) $(GSOBJSCS) $(CSLIBS)

# Targets for object files

$(OBJDIR)/GridSample.o:	GridSample.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GridSample.c

$(OBJDIR)/GridSampleCS.o:	GridSample.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GridSample.c

$(OBJDIR)/GSUtil.o:	GSUtil.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSUtil.c

$(OBJDIR)/GSUtilCS.o:	GSUtil.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSUtil.c

$(OBJDIR)/GSUsage.o:	GSUsage.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSUsage.c

$(OBJDIR)/GSUsageCS.o:	GSUsage.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSUsage.c

$(OBJDIR)/GSArgs.o:	GSArgs.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSArgs.c

$(OBJDIR)/GSArgsCS.o:	GSArgs.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSArgs.c

$(OBJDIR)/GSOdbc.o:	GSOdbc.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSOdbc.c

$(OBJDIR)/GSOdbcCS.o:	GSOdbc.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSOdbc.c

$(OBJDIR)/GSErrors.o:	GSErrors.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSErrors.c

$(OBJDIR)/GSErrorsCS.o:	GSErrors.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSErrors.c

$(OBJDIR)/GSTxnUpdateGridInfoCS.o:	GSTxnUpdateGridInfo.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSTxnUpdateGridInfo.c

$(OBJDIR)/GSTxnUpdateGridInfo.o:	GSTxnUpdateGridInfo.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSTxnUpdateGridInfo.c

$(OBJDIR)/GSTxnGetCountsCS.o:	GSTxnGetCounts.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSTxnGetCounts.c

$(OBJDIR)/GSTxnGetCounts.o:	GSTxnGetCounts.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSTxnGetCounts.c

$(OBJDIR)/GSTxnClearHistoryCS.o:	GSTxnClearHistory.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSTxnClearHistory.c

$(OBJDIR)/GSTxnClearHistory.o:	GSTxnClearHistory.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSTxnClearHistory.c

$(OBJDIR)/GSTxnPurgeCS.o:	GSTxnPurge.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSTxnPurge.c

$(OBJDIR)/GSTxnPurge.o:	GSTxnPurge.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSTxnPurge.c

$(OBJDIR)/GSTxnQueryCS.o:	GSTxnQuery.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSTxnQuery.c

$(OBJDIR)/GSTxnQuery.o:	GSTxnQuery.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSTxnQuery.c

$(OBJDIR)/GSTxnAuthorizeCS.o:	GSTxnAuthorize.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSTxnAuthorize.c

$(OBJDIR)/GSTxnAuthorize.o:	GSTxnAuthorize.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSTxnAuthorize.c

$(OBJDIR)/GSTxnChargeCS.o:	GSTxnCharge.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSTxnCharge.c

$(OBJDIR)/GSTxnCharge.o:	GSTxnCharge.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSTxnCharge.c

$(OBJDIR)/GSTxnTopupCS.o:	GSTxnTopup.c GridSample.h
	$(CC) -o $@ -c $(CSDEFS) $(CFLAGS) GSTxnTopup.c

$(OBJDIR)/GSTxnTopup.o:	GSTxnTopup.c GridSample.h
	$(CC) -o $@ -c $(CFLAGS) GSTxnTopup.c

