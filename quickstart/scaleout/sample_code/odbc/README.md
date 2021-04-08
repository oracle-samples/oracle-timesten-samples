Copyright (c) 1998, 2019, Oracle and/or its affiliates. All rights reserved.

# C/ODBC sample programs for Scaleout

This directory contains the source code for some C/ODBC programs that show how to connect to and execute transactions against a TimesTen Scaleout database.

**NOTE:**  For platforms where only a TimesTen client is supported, such as Windows and macOS, these samples must connect to a remote TimesTen Scaleout database.

**TptBm**

This program implements a multi-user throughput benchmark. By default, the transaction mix consists of 80% SELECT (read) transactions and 20% UPDATE (write) transactions. In addition to SELECTs and UPDATEs, INSERTs can also be included in the transaction mix. The ratio of SELECTs, UPDATEs and INSERTs is specified at the command line. Each transaction consists of one or more SQL operations (controlled by the '-ops' command line parameter).

The benchmark initially populates the data store, and then executes the transaction mix on it. The number of rows inserted as part of the transaction mix cannot exceed the number of rows with which the database is populated initially.

The measurement error for the benchmark is at most 2 seconds. This will be negligible at loads with a duration in excess of 200 seconds. A suggested load for the benchmark is one which lasts at least 600 seconds.

The schema for the bnchmark table is described in the program source file tptbm.c. There are two versions of the program; tptbm for direct mode and tptbmCS for client-server mode.

For the full syntax of the program, use "tptbm[CS] -help".


The program can run in 4 different modes:

_Classic mode (the default): direct and client-server_

The program treats the target database as a TimesTen Classic database. In this mode it behaves identically to the 'tptbm' sample program in the Classic QuickStart (they are in fact exactly the same programs just built with different options enabled).

Examples:

  Default connection string, 80% reads, 20% updates, 1 process, 100,000 transactions
  
`  tptbm`
  
  Default connection string, 80% reads, 20% updates, 4 processes, run for 600 seconds with a 30 second ramp time, random number generator uses 3 as the seed
  
  `tptbm -proc 4 -sec 600 -ramp 30 -seed 3`

  Custom connection string, 85% reads, 10% inserts, 5% updates, 1 process, run for 300 seconds with a 10 second ramp time
  
  `tptbm -read 85 -insert 10 -sec 300 -connstr "DSN=mytestdb;UID=someuser"`

_Scaleout mode - use '-scaleout' on the command line: direct and client-server_

The program treats the target database as a TimesTen Scaleout database. In this mode it creates the benchmark table with a hash distribution clause and makes some minor adaptions to its behavior to better suit a Scaleout database.

Examples:

  Default connection string, 80% reads, 20% updates, 1 process, 100,000 transactions
  
`  tptbm -scaleout
`
  
  Default connection string, 80% reads, 20% updates, 8 processes, run for 600 seconds with a 30 second ramp time
  
  `tptbm -scaleout -proc 8 -sec 600 -ramp 30`

  Custom connection string, 85% reads, 10% inserts, 5% updates, 1 process, run for 300 seconds with a 10 second ramp time
  
  `tptbm -scaleout -read 85 -insert 10 -sec 300 -connstr "DSN=mytestdb;UID=someuser"`

_Scaleout Local mode - use '-scaleout local' on the command line: direct and client-server_

Behaves like the regular scaleout mode but in addition each benchmark process uses the TimesTen Scaleout Routing API to enable it to generate and use key values that ensure that it always manipulates rows that reside in the database element to which it is connected thus minimising network round trips within the Scaleout grid. This is a form of data locality based optimization.

Examples:

  Default connection string, 80% reads, 20% updates, 1 process, 100,000 transactions, throttle each process to no more than 1000 TPS
  
`  tptbm -scaleout local -throttle 1000
`
  
  Default connection string, 80% reads, 20% updates, 8 processes, run for 600 seconds with a 30 second ramp time
  
  `tptbm -scaleout local -proc 8 -sec 600 -ramp 30`

  Custom connection string, 85% reads, 10% inserts, 5% updates, 1 process, run for 300 seconds with a 10 second ramp time
  
  `tptbm -scaleout local -read 85 -insert 10 -sec 300 -connstr "DSN=mytestdb;UID=someuser"`

_Scaleout Routing mode - use '-scaleout routing' on the command line: client-server only_

This mode is only available in the client-server version of the program, tptbmCS. Behaves like the regular scaleout mode but in addition each benchmark process opens connectiosn to every database element. During the benchmark it uses the TimesTen Scaleout Routing API to send operations directly to a database element containing the target row thus minimising network round trips within the Scaleout grid. This is another form of data locality based optimization.

Examples:

  Default connection string, 80% reads, 20% updates, 1 process, 100,000 transactions, throtle each process to no more than 1000 TPS
  
`  tptbmCS -scaleout routing -throttle 1000
`
  
  Default connection string, 80% reads, 20% updates, 8 processes, run for 600 seconds with a 30 second ramp time
  
  `tptbmCS -scaleout routing -proc 8 -sec 600 -ramp 30`

  Custom connection string, 85% reads, 10% inserts, 5% updates, 1 process, run for 300 seconds with a 10 second ramp time
  
  `tptbmCS -scaleout rouing -read 85 -insert 10 -sec 300 -connstr "DSN=mytestdbCS;UID=someuser"`

**GridSample**

The program is written to illustrate best practice and demonstrates how to make a program resilient by providing a fully functional code example showing how to properly handle events such as client connection failovers and transient errors. 

The program can also be run against a Classic database; the program determines the database mode at run-time and adapts accordingly. There are two versions of the program; GridSample for direct mode and GridSampleCS for client-server mode.

GridSample uses the Mobile Payments sample database which must be created and populated before running the program.  See [sample_config](../../sample_config) and [sample_scripts](../../sample_scripts) for instructions.

The GridSample program implements a configurable workload consisting of 5 different business transactions:

**Authorize**   -    Check that an account exists and is permitted to make calls.

**Charge**      -    Deplete an account's balance by a randomly chosen
                 amount.

**TopUp**       -    Add $10.00 to an account's balance.

**Query**       -    Retrieve details for a customer and their accounts.

**Purge**       -    Delete 'old' entries from the transaction history table.

Here is the online help for the program (this help text is from the client/server version):

````
Usage:

    GridSampleCS [-help] [-dsn dsname] [-uid username] [-pwd password]
        [-txnmix A,C,T,Q,P] [-nocleanup] [{-numtxn t | -duration s}]
        [{-silent | -verbose [n]}] [-log logpath]

Parameters:

  -help      - Displays this usage information and then exits. Any
               other options specified are ignored.

  -dsn       - Connect to 'dsname' instead of the default DSN (sampledbCS).
               The DSN must be of the correct type (direct or client/server).

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

Here are some examples of running the program.

Connect via client/server using all defaults. You will be prompted for the 
password for the user 'appuser':

  GridSampleCS

Connect via direct mode to the DSN 'demodb' using the user 'griddemo' with 
password 'griddemo'. Use a transaction mix of 50% Authorize, 25% Charge,
5% TopUp, 10% Query and 10% Purge and run for 120 seconds with status reports
every 10 seconds:

  GridSample -dsn demodb -uid griddemo -pwd griddemo -txnmix 50,25,5,10,10 \
             -duration 120 -verbose 10
````

You can observe the program's resilience to failures by using ttGridAdmin to stop and start grid data instances or server processes while the program is running.

**Building and running the sample programs**

**NOTE:** By default the makefiles for platforms where TTDM is supported build versions of the samples that link directly with the TimesTen driver libraries (direct and client-server) and also with the TimesTen Driver Manager (TTDM). The TTDM binaries can only be built (and executed) against **TimesTen 18.1.4.9.0** or later, as this is the first release that includes TTDM. To build without the TTDM versions (for example if you are using an older versionb of TimesTen 18.1), use the make target 'nodm'. For more information on TTDM, see the [TimesTen Driver Manager User Guide](TTDMUserGuide.md).

To build and run the sample programs:

1.    Make sure that you have a suitable C development environment installed (compiler, linker etc.) and a suitable TimesTen instance (a client instance is sufficient).

2.    Select the platform specific Makefile and copy from quickstart/classic/sample\_code/odbc/makefiles to the upper level directory.

	For example, if your platform is Linux x86-64bit, execute the following commands:

	`cd quickstart/scaleout/sample_code/odbc/makefiles`

	`cp Makefile.linux8664.proto ../Makefile`

	`cd ..`

3.    Make sure that you have deployed a TimesTen Scaleout grid, created a database, created the Mobile Payments user and schema and populated it with the Mobile Payments example data set. See the 'database' directory for more information. For TptBm only the user need be created.

4.    Set your environment for the TimesTen instance that you will use to connect to the database (this instance must be configured with a suitable connectable):

      `source <instance_home>/bin/ttenv.[c]sh`
      
      `source <quickstart_install_dir>/scaleout/ttquickstartenv.[c]sh`


5.    Build the binaries using the provided Makefile. If you have a full instance then you can build both the direct mode and client/server binaries:

      `make clean`
  
      `make`

6.   If you have a client instance then instead build just the client/server binaries:

      `make clean`

      `make csonly`

7.    Run the sample program(s). Note that the executables are located in the **bin** directory.
