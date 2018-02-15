Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

# Compile and Run Pro*C Sample Programs

## IMPORTANT PRE-REQUISITES

1. Manually Configure the Sample DSN for the Sample Programs; refer to _quickstart/classic/html/developer/sample\_dsn\_setup.html_

2. Set up sample database and user accounts

    The following build\_sampledb script should be run once to set up the sample database and user accounts. First set up the Instance Environment Variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

    $ source /home/timesten/instance/tt181/bin/ttenv.sh

    Run the quickstart/classic/sample_scripts/createdb/build\_sampledb script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

    Unix/Linux:
    
    $ cd quickstart/classic/sample\_scripts/createdb
    
    $ ./build\_sampledb.sh

3. Set up environment to compile and run the sample application

    The following script must be run in every terminal session:

    Set up the Instance environment variables: e.g. If your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

    $ source /home/timesten/instance/tt181/bin/ttenv.sh

    Set up quickstart environment variables:
    
    Unix/Linux:
    
    $ . quickstart/classic/ttquickstartenv.sh OR
    
    $ source quickstart/classic/ttquickstartenv.csh

## How to compile the sample Pro*C programs

To compile the sample programs in the sample\_code/proc directory, you simply run the makefile supplied in the same directory. Note that the appropriate makefile is made available based on the platform of your TimesTen installation.

Firstly, select the platform specific Makefile and copy from _quickstart/classic/sample\_code/proc/makefiles_ to the upper level directory.

For example, if your platform is Linux x86-64bit, execute the following commands:

cd quickstart/classic/sample_code/proc/makefiles
cp Makefile.linux8664.proto ../Makefile
cd ..

To build a sample program in the sample_code/proc directory, use the following command:

Linux/Unix:

$ make \<program-name\>

where \<program-name\> is the program you want to compile.

For example, to compile the ansidyn1 program, you do:

Linux/Unix:

$ make ansidyn1

## How to run the sample Pro*C programs

**addempPROC**

This Pro*C sample program prompts the user for information and inserts the corresponding data as a new employee into the EMP table.

This program is based on the Oracle Pro*C sample.pc program and minimal error checking is performed on the data to be inserted.

The EMP table needs to exist for this program to work.

Examples:

  (run the program using the default user appuser and the default TNS Service Name sampledb, will prompt for the password)

  addempPROC

  (run the program specifying the username, password and service name)

  addempPROC -user myuser -password mypassword -service myservice

For full syntax of the program, try "addempPROC -help".

**ansidyn1**

ansidyn1.pc is an Oracle Pro*C program that implements a dynamic SQL interpreter. Using the ANSI embedded SQL syntax, ansidyn1.pc accepts dynamic SQL at the input command prompt (SQL>)

You are prompted for the username and password at runtime. The servicename can be either an entry in the TNSnames.ora file or an easy connect string.

No existing tables need to exist for this program to work.

Example:

  (Run the program using either a TNS Service Name or Easy Connect String. For a TNS Service Name enter sampledb. For an Easy Connect string, enter localhost/sampledb:timesten_direct)

  ansidyn1

**batchfetchPROC **

This Pro*C sample program connects to the database and then declares and opens a cursor, fetches in batches using arrays, and prints the results using the function print_rows().

The EMP table needs to exist for this program to work.

Examples:

  (run the program using the default user appuser and the default TNS Service Name sampledb, will prompt for the password)

  batchfetchPROC

  (run the program specifying the username, password and service name)

  batchfetchPROC -user myuser -password mypassword -service myservice

For full syntax of the program, try "batchfetchPROC -help".

**cursorPROC**

This Pro*C sample program connects to the database and then declares and opens a cursor.  The cursor fetches the names, salaries, and commissions of all salespeople, displays the results, then closes the cursor.

The EMP table needs to exist for this program to work.

Examples:

  (run the program using the default user appuser and the default TNS Service Name sampledb, will prompt for the password)

  cursorPROC

  (run the program specifying the username, password and service name)

  cursorPROC -user myuser -password mypassword -service myservice

For full syntax of the program, try "cursorPROC -help".

**getempPROC**

This Pro*C sample program prompts the user for an employee number and then:

- Queries the emp table for the employee's name, salary and commission. 
- Uses indicator variables (in an indicator struct) to determine if the commission is NULL.

The employee numbers can be found by running the Pro*C batchfetchPROC sample program or via the EMP table.

The EMP table needs to exist for this program to work.

Examples:

  (run the program using the default user appuser and the default TNS Service Name sampledb, will prompt for the password)

  getempPROC

  (run the program specifying the username, password and service name)

  getempPROC -user myuser -password mypassword -service myservice

For full syntax of the program, try "getempPROC -help".

**plsqlPROC**

This sample program uses Pro*C to access common PLSQL packages (emp\_pkg and sample\_pkg) in four different ways:

- Calls a stored procedure with IN and OUT parameters

- Calls a stored function with IN and OUT parameters

- Calls an anonymous block and passes host variables into and out of the block

- Calls a store procedure to open a ref cursor and uses Pro*C to process the result-set of the ref cursor

The EMP table and the emp_pkg and sample_pkg PLSQL packages need to exist 
for this program to work.

Build:

When plsqlPROC is being built via make, the Oracle Pro*C pre-compiler will prompt for the password of the user 'appuser'. To avoid being prompted for the password at build time, you can hard-code the password in the Makefile, but this is a security risk so is not recommended.

e.g. USERID=appuser/your\_password\_for\_appuser

Examples:

  (run the program using the default user appuser and the default TNS Service Name sampledb, will prompt for the password)

  plsqlPROC

  (run the program specifying the username, password and service name)

  plsqlPROC -user myuser -password mypassword -service myservice

For full syntax of the program, try "plsqlPROC -help".

For more information on Pro\*C/C++ support in TimesTen, refer to the Pro\*C/C++ chapter in the [Oracle TimesTen In-Memory Database C Developer's Guide](https://docs.oracle.com/cd/E21901_01/timesten.1122/e21637/toc.htm).
