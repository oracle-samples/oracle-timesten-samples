Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

# Compile and Run OCI Sample Programs

## IMPORTANT PRE-REQUISITES

1. Manually Configure the Sample DSN for the Sample Programs; refer to _quickstart/classic/html/developer/sample\_dsn\_setup.html_.

2. Set up sample database and user accounts. The following build_sampledb script should be run once to set up the sample database and user accounts.

     Set up the Instance Environment Variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

     `source /home/timesten/instance/tt181/bin/ttenv.sh`

     Run the quickstart/classic/sample\_scripts/createdb/build\_sampledb script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

     Unix/Linux:

     `cd quickstart/classic/sample\_scripts/createdb`

     `./build\_sampledb.sh`

3. Set up environment to compile and run the sample application

     The following scripts must be run in each of your terminal sessions...

     Set up the Instance environment variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

     `source /home/timesten/instance/tt181/bin/ttenv.sh`

     Set up quickstart environment variables:

     Unix/Linux:

     `. quickstart/classic/ttquickstartenv.sh` 
     
     or
     
     `source quickstart/classic/ttquickstartenv.csh`

## How to compile the sample OCI programs

To compile the sample programs in the sample_code/oci directory, you simply run the makefile supplied in the same directory. Note that the appropriate makefile is made available based on the platform of your TimesTen installation.

Firstly, select the platform specific Makefile and copy from quickstart/classic/sample\_code/oci/makefiles to the upper level directory.

For example, if your platform is Linux x86-64bit, execute the following commands:

`cd quickstart/classic/sample\_code/oci/makefiles`

`cp Makefile.linux8664.proto ../Makefile`

`cd ..`

To build a sample program in the sample\_code/oci directory, use the following command:

Linux/Unix

`make <program-name>`

where \<program-name\> is the program you want to compile.

For example, to compile the addemp program, you do:

Linux/Unix

`make addemp`

## How to run the sample OCI programs

**addemp**

This is an OCI program that prompts the users for information and inserts the corresponding data into a new employee into the EMP table.

You are prompted for the username and password at runtime. The servicename can be either an entry in the TNSnames.ora file or an easy connect string.

The EMP table needs to exist for this program to work.

A sample tnsnames.ora file is available at \<instance_dir\>/network/admin/samples/tnsnames.ora.

Examples:

Run the program using default DSN sampledb and default user appuser. For TNS Service Name, enter sampledb; alternatively, using Easy Connect string, enter localhost/sampledb:timesten_direct

`addemp`

**plsqlOCI**

This sample program uses OCI to access common PLSQL packages (emp\_pkg and sample\_pkg) in four different ways:

- Calls a stored procedure with IN and OUT parameters

- Calls a stored function with IN and OUT parameters

- Calls an anonymous block and passes host variables into and out of the block

- Calls a store procedure to open a ref cursor and uses OCI to process the result-set of the ref cursor

The EMP table and the emp\_pkg and sample\_pkg PLSQL packages need to exist for this program to work.

Examples:

Run the program using the default user appuser and the default TNS Service Name sampledb, will prompt for the password

`plsqlOCI`

Run the program specifying the username, password and service name

`plsqlOCI -user myuser -password mypassword -service myservice`

For the full syntax of the program, use "plsqlOCI -help".


**tptbmOCI**

This program implements a multi-user throughput benchmark. By default, the transaction mix consists of 80% SELECT (read) transactions and 20% UPDATE (write) transactions. In addition to SELECTs and UPDATEs, INSERTs and DELETES can also be included in the transaction mix. The ratio of SELECTs, UPDATEs and INSERTs and DELETES is specified at the command line. Each transaction consists of one or more SQL operations.

The benchmark initially populates the data store, and then executes the transaction mix on it. The number of rows inserted as part of the transaction mix cannot exceed the number of rows with which the database is populated initially.

The measurement error for the benchmark is at most 2 seconds. This will be negligible at loads with a duration in excess of 200 seconds. A suggested load for the benchmark is one which lasts at least 600 seconds.

The schema for this test is described in the program source file tptbmOCI.c.  

Examples:

80% reads, 20% updates for appuser@sampledb with 10,000 transactions  in a database with 10,000 rows.

`tptbmOCI`

60% reads, 10% updates, 5% insert and 25% deletes with 1 SQL operation per transation for appuser@sampledb_112

`tptbmOCI -read 60 -update 10 -insert 5 -delete 25`

Use a workload with 3 selects, 1 insert and 1 update per transaction for appuser@sampledb_112

`tptbmOCI -multiop`

Use a client/server workload with 10 * (3 reads, 1 insert and 1 update)  per transaction for appuser@sampledbCS_112

`tptbmOCI -multiop -min 10 -max 10 -service sampledbCS`

80% reads, 20% updates for <myuser>@<myservice> with 1,000 transactions in the database with 1,089 rows.

 `tptbmOCI -user <myuser> -service <myservice> -xact 1000 -key 33 `

For the full syntax of the program, use "tptbmOCI -help".

For more information on OCI, refer to the OCI chapter in the [Oracle TimesTen In-Memory Database C Developer's Guide](https://docs.oracle.com/database/timesten-18.1/TTCDV/toc.htm).
