Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.

# Compile and Run ODPI-C Sample Programs

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

4. Install/Compile/Build [ODPI-C package from github](https://github.com/oracle/odpi). Minimum version to use is ODPI-C 3.2.1.

  macOS 64-bit:
    The TimesTen client release on macOS 64-bit is based on x86-64 architecure at this point. Some architecure on macOS is default to Arm base. Ensure to switch to "x86_64" architecure when building odpi-c to match the same architecure as the TimesTen release. You can use the following command to switch to "x86_64" architecure:

    `$env /usr/bin/arch -x86_64 /bin/zsh`

  Note:
   After installation, make sure to include the path to the built ODPI library as part of the environment variable LD_LIBRARY_PATH for Linux and macOS and LIB and PATH on Windows.  For example, 

   `export LD_LIBRARY_PATH=<location of odpi-c build dir>/lib:$LD_LIBRARY_PATH`

## How to compile the sample ODPI-C programs

To compile the sample programs in the sample_code/odpi-c directory you use a provided makefile.
 
Firstly, select the platform specific Makefile and copy from quickstart/classic/sample\_code/odpi-c/makefiles to the upper level directory.

For example, if your platform is Linux x86-64bit, execute the following commands:

`cd quickstart/classic/sample_code/odpi-c/makefiles`

`cp Makefile.linux8664.proto ../Makefile`

`cd ..`

To build the sample programs in the sample_code/odpi-c directory, use the following command:

Linux/macOS
	
`make ODPIDIR=<location of odpi-c build dir>`

Windows

`nmake ODPIDIR=<location of odpi-c build dir>`
	
When compiling on Windows using Visual Studio, ensure to use the x64 version of nmake.exe and cl.exe to match the TimesTen 64bit client release for Windows.

## How to run the sample ODPI-C programs


**TptbmODPI**

This program implements a multi-user throughput benchmark. By default, the transaction mix consists of 80% SELECT (read) transactions and 20% UPDATE (write) transactions. In addition to SELECTs and UPDATEs, INSERTs can also be included in the transaction mix. The ratio of SELECTs, UPDATEs and INSERTs is specified at the command line. Each transaction consists of one or more SQL operations.

The benchmark initially populates the data store, and then executes the transaction mix on it. The number of rows inserted as part of the transaction mix cannot exceed the number of rows with which the database is populated initially.

The measurement error for the benchmark is at most 2 seconds. This will be negligible at loads with a duration in excess of 200 seconds. A suggested load for the benchmark is one which lasts at least 600 seconds.

The schema for this sample program is described in the program source file tptbmODPI.c.  The connection to the database is through OCI connection, so an easy-connect string or reference to TNS service name in the tnsname.ora file is needed.  A sample tnsnames.ora file is available at <TT_instance_dir>/network/admin/samples/tnsnames.ora.

Examples:

 Using direct connection to sampledb using easy-connect method to localhost, 80% reads, 20% updates, 1 process, 100,000 transactions
  
  `tptbmODPI -connstr localhost/sampledb:timesten_direct`

  Default connection string, 80% reads, 20% updates, 2 process, 10,000,000 transactions, random number generator uses 3 as the seed
  
  `tptbmODPI -proc 2 -xact 10000000 -seed 3`

  For the full syntax of the program, use "tptbmODPI -help".


For more information on how to use ODPI-C to develop database programs for
the TimesTen database, see the [Oracle TimesTen In-Memory Database Open Source Languages Support Guide](https://docs.oracle.com/en/database/other-databases/timesten/22.1/open-source-languages/additional-information-python.html#GUID-D96A7031-A40E-4A82-92BE-9D258B05022B).
