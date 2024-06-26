Copyright (c) 2010, 2024, Oracle and/or its affiliates. All rights reserved.

# Compile and Run ODBC Sample Programs

## IMPORTANT PRE-REQUISITES

1. Manually Configure the Sample DSN for the Sample Programs; refer to _quickstart/classic/html/developer/sample\_dsn\_setup.html_

 
2. Set up sample database and user accounts

    The following build_sampledb script should be run once to set up the sample database and user accounts. First set up the Instance Environment Variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt221 directory, execute the command

    `source /home/timesten/instance/tt221/bin/ttenv.sh`

    Run the quickstart/classic/sample_scripts/createdb/build_sampledb script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

    Unix/Linux:
    
    `cd quickstart/classic/sample_scripts/createdb`
    
    `./build_sampledb.sh`

3. Set up environment to compile and run the sample application

    The following script must be run for each of the terminal session:

    Set up the Instance environment variables:
    
    e.g. If your TimesTen instance location is under /home/timesten/instance/tt221 directory, execute the command

    `source /home/timesten/instance/tt221/bin/ttenv.sh`

    Set up quickstart environment variables:
    
    Unix/Linux: 	  	
    
    `. quickstart/classic/ttquickstartenv.sh`
    
    or
    
    `source quickstart/classic/ttquickstartenv.csh`

NOTE: On Solaris and AIX platforms, use bash shell to run against ttquickstartenv.sh and tcsh shell to run against ttquickstartenv.csh for best results.

## How to compile the sample ODBC programs

To compile the sample programs in the sample_code/odbc directory you use a provided makefile.

**NOTE:** By default the makefiles for platforms where TTDM is supported build versions of the samples that link directly with the TimesTen driver libraries (direct and client-server) and also with the TimesTen Driver Manager (TTDM). The TTDM binaries can only be built (and executed) against **TimesTen 18.1.4.9.0** or later, as this is the first release that includes TTDM. To build without the TTDM versions (for example if you are using an older version of TimesTen 18.1), use the make target 'nodm'. For more information on TTDM, see the [TimesTen Driver Manager User Guide](./TTDMUserGuide.md).

Firstly, select the platform specific Makefile and copy from quickstart/classic/sample\_code/odbc/makefiles to the upper level directory.

For example, if your platform is Linux x86-64bit, execute the following commands:

`cd quickstart/classic/sample_code/odbc/makefiles`

`cp Makefile.linux8664.proto ../Makefile`

`cd ..`

To build the sample programs in the sample_code/odbc directory, use the following command:

Linux/Unix
	
`make`

Windows

`nmake`
	
There are three versions of each program; *progname* for direct mode, *progname****CS*** for client-server mode and *progname****DM*** for the driver manager linked binary..

**NOTE:**  The XLA sample programs cannot be compiled or run using a client-only instance.

## How to run the sample ODBC programs

**bulkinsert**

This program measures the time needed for insertion of many rows into a table. Parameters allow you to specify whether there are indexes on the table and whether statistics should be computed after doing the inserts. You can also specify insertion of the rows in batches to achieve better performance.

  Examples:

  Default connection string, 10,000 rows, batchsize 256, no indexes
  
  `bulkinsert`

  Default connection string, 100,000 rows, batchsize 256, no indexes, driver manager version
  
  `bulkinsertDM -r 100000`

  Default connection string, 1,000,000 rows, batchsize 512, 3 indexes, client-server version
  
  `bulkinsertCS -r 1000000 -s 512 -i 3`

  For the full syntax of the program, use "bulkinsert -help".


**plsqlODBC**

This sample program uses ODBC to access common PLSQL packages (emp_pkg and sample_pkg) in four different ways:

- Calls a stored procedure with IN and OUT parameters

- Calls a stored function with IN and OUT parameters

- Calls an anonymous block and passes host variables into and out of the block

- Calls a store procedure to open a ref cursor and uses ODBC to process the result-set of the ref cursor

The EMP table and the emp_pkg and sample_pkg PLSQL packages need to exist for this program to work.

Examples:

  Default connection string
  
  `plsqlODBC`

  Custom connection string
  
  `plsqlODBC -connstr "DSN=sampledb;UID=appuser"`

For the full syntax of the program, use "plsqlODBC -help".


**tpcb**

This program implements a TPCB-like workload benchmark. The code populates the ACCOUNTS, BRANCHES, and TELLERS tables. It then executes a modified version of the TPCB benchmark, performing 25,000 transactions.

Each transaction updates the balance in the ACCOUNTS, BRANCHES and TELLERS tables, fetches the new balance from the ACCOUNTS table, and appends a record to the HISTORY table. At termination, it prints the measured performance.

Examples:

  Default connection string, 25,000 transactions, scale 1
  
  `tpcb`

  Default connection string, 250,000 transactions, scale 2)
  
  `tpcb -xact 250000 -scale 2`

  For the full syntax of the program, use "tpcb -help".


**tptbm**

This program implements a multi-user throughput benchmark. By default, the transaction mix consists of 80% SELECT (read) transactions and 20% UPDATE (write) transactions. In addition to SELECTs and UPDATEs, INSERTs can also be included in the transaction mix. The ratio of SELECTs, UPDATEs and INSERTs is specified at the command line. Each transaction consists of one or more SQL operations.

The benchmark initially populates the data store, and then executes the transaction mix on it. The number of rows inserted as part of the transaction mix cannot exceed the number of rows with which the database is populated initially.

The measurement error for the benchmark is at most 2 seconds. This will be negligible at loads with a duration in excess of 200 seconds. A suggested load for the benchmark is one which lasts at least 600 seconds.

The schema for this test is described in the program source file tptbm.c.

Examples:

  Default connection string, 80% reads, 20% updates, 1 process, 100,000 transactions
  
  `tptbm`

  Default connection string, 80% reads, 20% updates, 2 process, 10,000,000 transactions, random number generator uses 3 as the seed
  
  `tptbm -proc 2 -xact 10000000 -seed 3`

  Default connection string, 85% reads, 10% inserts, 5% updates, 1 process, driver manager version
  
  `tptbmDM -read 85 -insert 10`

  For the full syntax of the program, use "tptbm -help".


**wiscbm**

This program implements and measures a subset of queries from the Wisconsin benchmark. It will run about 30 queries, the first batch without indexes and the second batch with indexes. When finished, a report of the response time per query is generated.

Example usage:

  Default connection string, scale 1, all queries, read-committed isolation
  
  `wiscbm`

  Default connection string, scale 5, all queries, read-committed isolation
  
  `wiscbm -scale 2`

  Default connection string, scale 2, only queries [1, 3, 5-7, 10-14, 25], read-committed isolation
  
  `wiscbm -scale 2 -q 1,3,5-7,10-14,25`

For the full syntax, use "wiscbm -help".

**updatelob**

This program queries against a Large Binary Object(LOB) field and then update the selected LOB field with new value of the same row.  PRINT_MEDIA table needs to be provisioned before executing this program.

Example usage:

  Default connection string

  `updatelob`

For the full syntax, use "updatelob -help".


**xlaSimple**

This sample program is available in the _quickstart/classic/sample\_code/odbc/xla_ directory. Two versionbs of the binary will be built; **xlaSimple** linked directly against the **direct mode** library and **xlaSimpleDM** linked against **TTDM**.

This program demonstrates the usage of the TimesTen Transaction Log API (XLA) for C developers using ODBC. This program requires a user with XLA privilege to subscribe to changes in the database.

This program subscribes to the table APPUSER.MYDATA in the sampledb database. If running the program without any parameters, the program uses the default xlauser user (created in the build_sampledb script).

To generate changes to the APPUSER.MYDATA, use ttIsql to connect to sampledb under appuser. Then execute database transactions on the MYDATA table.

Example usage:

  Default connection string
  
  `xlaSimple`

To exit the program, press CTRL-C. For the full syntax, use "xlaSimple -help"

For more information on how to use ODBC to develop database programs for
the TimesTen database, see the [Oracle TimesTen In-Memory Database C Developer's Guide](https://docs.oracle.com/en/database/other-databases/timesten/22.1/c-developer/working-timesten-databases-odbc1.html#GUID-FFD312BF-77F6-4BE2-BF22-FCAB43D7BEC8).
