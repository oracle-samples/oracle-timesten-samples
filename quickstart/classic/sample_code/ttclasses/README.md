Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

# How to Compile and Run the TTClasses Sample Programs

# IMPORTANT PRE-REQUISITES

1. Manually Configure the Sample DSN for the Sample Programs; refer to _quickstart/classic/html/developer/sample\_dsn\_setup.html_

2. Set up sample database and user accounts

    The following build_sampledb script should be run once to set up the sample database and user accounts. First set up the Instance Environment Variables e.g. if your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

    `source /home/timesten/instance/tt181/bin/ttenv.sh`

    Run the quickstart/classic/sample_scripts/createdb/build\_sampledb script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

    Unix/Linux:
    
    `cd quickstart/classic/sample\_scripts/createdb`
    
    `./build\_sampledb.sh`

3. Set up environment to compile and run the sample application

    The following scripts must be run in every terminal session.

    Set up the Instance environment variables. e.g. if your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

    `source /home/timesten/instance/tt181/bin/ttenv.sh`

    Set up quickstart environment variables:
    
    Unix/Linux: 	  	
    
    `. quickstart/classic/ttquickstartenv.sh`
    
    or
    
    `source quickstart/classic/ttquickstartenv.csh`

# How to compile the sample TTClasses (C++) programs

To compile the sample programs in the sample_code/oci directory, you simply run the makefile supplied in the same directory. Note that the appropriate makefile is made available based on the platform of your TimesTen installation.

Firstly, select the platform specific Makefile and copy from quickstart/classic/sample\_code/ttclasses/makefiles to the upper level directory.

For example, if your platform is Linux x86-64bit, execute the following commands:

`cd quickstart/classic/sample\_code/ttclasses/makefiles`

`cp Makefile.linux8664.proto ../Makefile`

`cd ..`

To build a sample program in the sample_code/ttclasses directory, use the following command:

Linux/Unix:

`make <program-name>`

where \<program-name\> is the program you want to compile.

For example, to compile the typetest program, you do:

Linux/Unix:

`make typetest`

## How to run the sample TTClasses (C++) programs

**basics**

This sample program is a good starting template for projects that use TTClasses. It shows how to connect to and disconnect from a TimesTen database and how to prepare, bind, execute, fetch and close SQL statements. Examples of TTClasses exception handling and timing SQL operations for both serializable and committed read transactions are shown.

Examples:

Run as appuser: DSN=sampledb;UID=appuser\

`basics`

Run as instance administrator

`basics sampledb`

Run as user adm in Client/Server mode

`basicsCS -connstr "DSN=sampledbCS;uid=adm"`

For the full syntax of the program, use "basics -help".


**bulkcp**

This sample program uses dynamic SQL to run a SQL query and writes the output to the console or a file. The user enters the SQL query on the command line. The user can optionally specify the characters to display for NULL values and column separator.

Examples:

Display user query to STDOUT with default separators

`bulkcp -query "SELECT * from emp"`

Display user query to a file with default separators on Linux/Unix

`bulkcp -query "SELECT * from dept" -output /tmp/myQuery.txt`
 
For the full syntax of the program, use "bulkcp -help".


**bulktest**

This sample code executes a series of insert, update and delete operations first in 'non-bulk' and then in 'bulk' mode with variable batch sizes. When the "batchsize" is set to 256 (the optimal value), significant performance benefits can be achieved for insert operations and modest improvements for update and delete operations.

Examples:

Without any parameters, the program runs with the default settings: DSN=sampledb;UID=appuser; batch size= 256

`bulktest`

Run the program with batch size = 512, using default connect string \[dsn=sampledb and uid=appuser\]

`bulktest -batchsize 512`

Run the program against a different dsn and user id of your choice

`bulktest -connstr "DSN=sampledb;uid=adm"`

For the full syntax of the program, use "bulktest -h".


**catalog**

This sample program prints out the database schema information including all tables, views, and indexes.

Examples:

Without any parameters, the program runs with the default settings: DSN=sampledb;UID=appuser

`catalog`

Run the program against a different dsn and user id of your choice

`catalog -connstr "DSN=sampledb;uid=adm"`

For the full syntax of the program, use "catalog -h".


**plsqlTTCLASSES**

This sample program uses TTClasses to access common PLSQL packages (emp\_pkg and sample\_pkg) in four different ways:

- Calls a stored procedure with IN and OUT parameters

- Calls a stored function with IN and OUT parameters

- Calls an anonymous block and passes host variables into and out of the block

- Calls a store procedure to open a ref cursor and uses TTClasses to process the result-set of the ref cursor

The EMP table and the emp\_pkg and sample\_pkg PLSQL packages need to exist for this program to work.

Example usage:

Run the program using the default DSN and default user, it will prompt for the password

`plsqlTTCLASSES`

Run the program specifying the DSN and username, it will prompt for the password

`plsqlTTCLASSES -connstr "DSN=sampledb;UID=appuser"`

Run the program specifying the DSN, username and password

`plsqlTTCLASSES -connstr "DSN=sampledb;UID=appuser;PWD=mypassword"`

For the full syntax of the program, use "plsqlTTCLASSES -h".


**pooltest**

This sample program demonstrates multi-threaded connection pool usage and error handling. This program shows how to use the TTConnectionPool class in TTClasses in a multi-threaded application. The user can specify the number of threads to use for the insert operations and the number of connections to keep in the connection pool.

Examples:

Without any parameters, the program runs use the default of 1 connection, 2 insert threads, 2 fetch threads, and DSN=sampledb;UID=appuser

`pooltest`

Run the program specifying 2 connections, 4 insert threads

`pooltest -insert 4 -conn 2`

Run the program against a different dsn and user id of your choice

`pooltest -connstr "DSN=sampledb;uid=adm"`

For the full syntax of the program, use "pooltest -h".


**ttSizeAll**

This sample program demonstrates calling TimesTen utility ttSize for multiple tables. Only the tables which the user has read permissions on will be shown.

Examples:

Without any parameters, the program uses DSN=sampledb;UID=appuser

`ttSizeAll`

Show both user and system tables in the output

`ttSizeAll -listsys`

Run the program against a different dsn and user id of your choice

`ttSizeAll -connstr "dsn=sampledb;uid=xlauser"`

For the full syntax of the program, use "ttSizeAll -h".


**typetest**

This sample program demonstrates the use of different datatypes supported by Oracle TimesTen by creating a table, typetest, which contains columns of all supported datatypes. The program then inserts data into the table and queries the data from the table.

Examples:

Without any parameters, the program runs with the default settings: DSN=sampledb;UID=appuser

`typetest`

Run the program against a different dsn and user id of your choice

`typetest -connstr "dsn=sampledb;uid=appuser"`

For the full syntax of the program, use "typetest -h".


**ttXlaAdmin**

This particular TTClasses sample program is available in the _quickstart/classic/sample\_code/ttclasses/xla_ directory.

This program shows how to list or remove XLA bookmarks in the database. This program requires a user with ADMIN privilege to run because only the administrator is allowed to see all bookmarks in the system. An user with the XLA privilege is allowed to list or remove his/her own bookmarks.

Unwanted bookmarks that are preventing transaction log files from being purged can create operational issues on the system and should be removed.

The "list" option is the default option for running the program without any input. For each XLA bookmark found in the system, the program lists the bookmark name, its LSN and whether the bookmark is holding transaction logs.

The "remove" option allows the user to delete the XLA bookmark via its bookmark name. The "removeall" option allows all of the bookmarks to be deleted. Bookmarks can only be deleted when not in use.

Examples:

Without any parameters, the program executes the "list" command and connects to DSN=sampledb;UID=adm

`ttXlaAdmin`

Remove all of the XLA bookmark for any user in the DB. The user needs to have ADMIN privilege to do this.
 
`ttXlaAdmin -removeall`

Remove a specific bookmark. The user needs to have ADMIN privilege to do this.
 
`ttXlaAdmin -remove <bookmark>`

For the full syntax of the program, use "ttxlaAdmin -h".


**xlasubscriber1**

This particular TTClasses sample program is available in the _quickstart/classic/sample\_code/ttclasses/xla_ directory.

This program shows how to use the Transaction Log API (XLA) facility to subscribe to change notifications for a table, chosen by the user at run time.

This program shows how to create persistent XLA bookmark and monitor and process changes on the specified table. The program displays the change records processed and the operations that were performed on the table (insert, update, or delete) to cause the change.

To create workload on the table being tracked, use a ttIsql session or a TimesTen user application to inject changes to the the table being tracked. This program requires a user with XLA privilege to run.

Examples:

Without any parameters, the program connects to DSN=sampledb;UID=xlauser, and prompt for the table to track

`xlasubscriber1`

Run the program against a different dsn and user id of your choice; the user needs to have XLA privilege

`xlasubscriber1 -connstr "dsn=<mydsn name>;uid=<myusername>"`

For the full syntax of the program, use "xlaSubscriber1 -h".


**xlasubscriber2**

This particular TTClasses sample program is available in the  _quickstart/classic/sample\_code/ttclasses/xla_ directory.

This program shows how to subscribe to XLA events on multiple tables of interest (in this case three). This program monitors changes to three tables: MYTABLE, ICF and ABC.

The column names and their values are shown for every row affected by the transaction's inserts, updates and deletes. To create the transactional workload, a ttIsql session or a TimesTen user application can perform the transactions to the MYTABLE, ICF and/or ABC tables.

This program is an example of an XLA subscriber which only has the permission to see the changes to the table of interest, but not to access the table of interest itself. The program requires a user with XLA privilege to run.

Examples:

Without any parameters, the program connects to DSN=sampledb;UID=xlauser

`xlasubscriber2`

Run the program against a different dsn and user id of your choice; the user needs to have XLA privilege

`xlasubscriber2 -connstr "dsn=<mydsn name>;uid=<myusername>"`

For the full syntax of the program, use "xlasubscriber2 -h".


For more information on TTClasses, refer to the [Oracle TimesTen In-Memory Database TTClasses Guide](https://docs.oracle.com/database/timesten-18.1/TTCLS/toc.htm).
