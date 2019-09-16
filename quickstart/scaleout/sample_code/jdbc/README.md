Copyright (c) 1998, 2017, Oracle and/or its affiliates. All rights reserved.

# Java/JDBC sample program for Scaleout

This directory contains the source code for a relatively simple Java/JDBC program (GridSample) that shows how to connect to and execute transactions against a TimesTen Scaleout database. The program is written to illustrate best practice and demonstrates how to make a program resilient by providing a fully functional code example showing how to properly handle events such as client connection failovers and transient errors.

(If you want some more basic samples to get you started, the Classic quickstart JDBC sample programs will run without change against Scaleout.) 

The program can also be run against a Classic database; the program determines the database mode at run-time and adapts accordingly.

GridSample uses the Mobile Payments sample database which must be created and populated before running the program.

The program implements a configurable workload consisting of 5 different business transactions:

**Authorize**   -    Check that an account exists and is permitted to make calls.

**Charge**      -    Deplete an account's balance by a randomly chosen amount.

**TopUp**       -    Add $10.00 to an account's balance.

**Query**       -    Retrieve details for a customer and their accounts.

**Purge**       -    Delete 'old' entries from the transaction history table.

To build and run the sample program:

1.    Make sure that you have a supported JDK installed and a suitable TimesTen instance (a client instance is sufficient).

2.    Make sure that you have deployed a TimesTen Scaleout grid, created the sample database, created the Mobile Payments user and schema and populated it with the Mobile Payments example data set.

3.    Set your environment for the TimesTen instance that you will use to connect to the database (this instance must be configured with a suitable connectable):

      source \<instance\_home\>/bin/ttenv.[c]sh
      
      source \<quickstart\_install\_dir\>/scaleout/ttquickstartenv.[c]sh

4.    Build GridSample using the provided Makefile.

      make clean
      
      make

5.    Run the sample program. You can use the 'gridsample.sh' wrapper script as a convenience if so desired.

Here is the online help for the program:

````
Usage:

    java GridSample [-help] [-cs] [-dsn dsname] [-uid username] [-pwd password]
        [-txnmix A,C,T,Q,P] [-nocleanup] [{-numtxn t | -duration s}]
        [{-silent | -verbose [n]}] [-log logpath]

Parameters:

  -help      - Displays this usage information and then exits. Any
               other options specified are ignored.

  -dsn       - Connect to 'dsname' instead of the default DSN (sampledb or
               sampledbCS). The type of the DSN must match the requested
               connection type (direct or client/server).

  -uid       - Connect as user 'username' instead of the default user
               (appuser).

  -pwd       - Connect using 'password'. If omitted then the user will
               be prompted for the password.

  -txnmix    - Sets the transaction mix for the workload; A = percentage
               of Authorize, C = percentage of Charge, T = percentage of
               TopUp, Q = percentage of Query and P = percentage of Purge.
               All values are integers >= 0 and <= 100 and the sum of the
               values must equal 100. If not specified, the default mix
               is A=70, C=15, T=5, Q=5 and P=5.

  -nocleanup - The transaction history table will not be truncated prior
               to starting the workload.

  -numtxn    - The workload will run until it has completed 't' transactions
               (t > 0) and then the program will terminate.

  -duration  - The workload will run for approximately 's' seconds (s > 0)
               and then the program will terminate.

  -silent    - The program will not produce any output other than to report
               errors. Normally the program will report significant events
               (connect, failover, ...) as it runs plus a workload summary
               when it terminates.

  -verbose   - Execution statistics will be reported every 'n' seconds
               (n > 0, default is 30) as the program runs, in addition to
               the normal reporting.

  -log       - Write an execution log to 'logpath'.

  The '-numtxn' and '-duration' options are mutually exclusive. If neither
  is specified then the program will run until it is manually terminated
  using Ctrl-C, SIGTERM etc.

  The '-silent' and '-verbose' options are mutually exclusive.

Exit status:

  The exit code reflects the outcome of execution as follows:
    0  - Success
    1  - Parameter error
    2  - Help requested
   >2  - Fatal error encountered

A convenience wrapper script, gridsample.sh, is provided which sets some
Java related necessities.  Here are some examples of running the program
using that script...

Connect via client/server using all defaults. You will be prompted for the 
password for the user 'appuser':

gridsample.sh -cs

Connect via direct mode to the DSN 'demodb' using the user 'griddemo' with 
password 'griddemo'. Use a transaction mix of 50% Authorize, 25% Charge,
5% TopUp, 10% Query and 10% Purge and run for 120 seconds with status reports
every 10 seconds:

gridsample.sh -dsn demodb -uid griddemo -pwd griddemo -txnmix 50,25,5,10,10 \
           -duration 120 -verbose 10
````

You can observe the program's resilience to failures by using ttGridAdmin to stop and start grid data instances or server processes while the program is running.