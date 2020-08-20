#
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#
# Makefile for GridSample Java/JDBC sample program
#

TTHOME     = $(TIMESTEN_HOME)
TTJDBCCP   = $(TTHOME)/install/lib/ttjdbc8.jar

JAVACOPTS  = -Xlint:unchecked

SRCFILES   =  GridSample.java GSConstants.java GSGridRetryException.java \
              GSConnectionFailoverException.java GSDbConnection.java \
              GSDbStatement.java GSDbResultSet.java GSAppTransaction.java \
              GSTxnGetCounts.java GSTxnClearHistory.java \
              GSTxnAuthorize.java GSTxnQuery.java GSTxnPurge.java \
              GSTxnCharge.java GSTxnTopup.java \
              GSPasswordReader.java GSUtil.java GSErrors.java GSGlobal.java

CLASSFILES =  GridSample.class GSConstants.class GSGridRetryException.class \
              GSConnectionFailoverException.class GSDbConnection.class \
              GSDbStatement.class GSDbResultSet.class GSAppTransaction.class \
              GSTxnGetCounts.class GSTxnClearHistory.class \
              GSTxnAuthorize.class GSTxnQuery.class GSTxnPurge.class \
              GSTxnCharge.class GSTxnTopup.class \
              GSPasswordReader.class GSUtil.class GSErrors.class GSGlobal.class

JARS	   =  GridSample.jar

.SUFFIXES:
.SUFFIXES: .java .class

.java.class:
	javac $(JAVACOPTS) -cp .:$(TTJDBCCP) $<

all:	jars 

jars:	$(JARS)

pclean:
	-rm -f *.class

clean:	pclean
	-rm -f *.jar

GridSample.jar:	$(CLASSFILES)
	rm -f GridSample.jar
	jar cf GridSample.jar *.class

