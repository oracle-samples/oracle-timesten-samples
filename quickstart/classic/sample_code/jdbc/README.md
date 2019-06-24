Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

# Compile and Run JDBC Sample Programs

## IMPORTANT PRE-REQUISITES

1. Manually Configure the Sample DSN for the Sample Programs. Refer to _quickstart/classic/html/developer/sample\_dsn\_setup.html_.

2. Set up the sample database and user accounts. The following build_sampledb script should be run once to set up the sample database and user accounts.
  
     Set up the Instance Environment Variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

     `source /home/timesten/instance/tt181/bin/ttenv.sh`

     Run the _quickstart/classic/sample\_scripts/createdb/build\_sampledb_ script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

      Unix/Linux:
      
      `cd  quickstart/classic/sample\_scripts/createdb`
      
      `./build\_sampledb.sh`

3. Set up environment to compile and run the sample application

     The following scripts must be run for each of your terminal sessions...

    Set up the Instance environment variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

    `source /home/timesten/instance/tt181/bin/ttenv.sh`

    Set up quickstart environment variables:
    
    Unix/Linux: 	  	
    
    `. quickstart/classic/ttquickstartenv.sh`
    
    or
    
    `source quickstart/classic/ttquickstartenv.csh`

## How to compile the sample JDBC programs

To compile the sample programs in the sample\_code/jdbc directory, use the relevant TimesTen supported Java compiler for your platform (eg Sun, HP, JRocket or IBM JDK) to compile each sample program. Refer to the [OracleTimesTen In-Memory Database Installation Guide](https://docs.oracle.com/cd/E21901_01/timesten.1122/e21632/toc.htm) for the list of supported JDKs for your preferred platform.

To compile specific program:

`javac <progname>.java`

To compile the sample programs all at once:

`javac *.java`

**NOTE:** Since XLA does not support applications linked with a driver manager library or the client/server library, the asyncJMS and syncJMS demos cannot be compiled or run in client-only installations.

## How to run the sample JDBC programs

**NOTE:** On platforms where both the 32-bit and 64-bit JDKs are installed in
the same directory (e.g. Solaris), java must be invoked with the -d64
option in order to run the 64-bit JVM.

**NOTE:** On some platforms, such as macOS, you may need to explicitly pass a setting for java.library path tp the JVM in order to run the samples. For example:

    java -Djava.library.path=${TIMESTEN_HOME}/install/lib <progname>

**asyncJMS**

This program uses the TimesTen JMS/XLA implementation to process messages:

a) Connect to the database as an XLA user

b) Listen to committed changes to the CUSTOMER table

c) Use a JMS Subscriber

d) ASYNCHRONOUSLY receive messages via onMessage() and a JMS MessageListener

e) Display the change records to the console

f) Disconnect from the database when the used enters CTRL-C

Either the level1 - level4 JDBC programs or ttIsql can be used to apply committed changes to the CUSTOMER table.

Examples:

  Connect using the default DSN sampledb, uid=xlauser, prompted password, listen to APPUSER.CUSTOMER
  
  `java asyncJMS`

  Connect using the default DSN sampledb, uid=xlauser, listen to APPUSER.CUSTOMER
 
  `java asyncJMS -xlauser <xlausername>`

  Connect using the default DSN sampledb, uid=xlauser and listen to the MYUSER.CUSTOMER table
  
  `java asyncJMS -xlauser <xlausername> -schema myUser`

  For the full syntax of the program, use "java asyncJMS -h".

**level1**

This program uses the TimesTen JDBC DriverManager interface to show basic operations on a TimesTen database: 

a) Connect using the com.timesten.jdbc.timesTenDriver interface

b) Insert into the CUSTOMER table all the data read from input file input1.dat file

c) Call ttOptUpdateStats to update statistics

d) SELECT from the CUSTOMER table, fetch, and print the result set to stdout

e) Disconnect from the database

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java level1`

  Connect using default dsn and uid, and client/server mode
  
  `java level1 -c`

  Connect using the dsn **my_dsn**
  
  `java level1 my_dsn`

  For the full syntax of the program, use "java level1 -h".

**level2**

This program uses the TimesTen DataSource interface to show basic operations on a TimesTen database:

a) Connect using the TimesTenDataSource interface

b) Insert all the data read from the input2.dat file located in the datfiles subdirectory

c) Call ttOptUpdateStats to update statistics

d) DELETE duplicate product data from the PRODUCT table

e) Update the PRODUCT table by increasing the product price by 10%

f) SELECT the data from the PRODUCT table, fetch and print the result set to stdout

g) Disconnect from the database.

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java level2`

  Connect using default dsn and uid, and client/server mode
  
  `java level2 -c`

  Connect using the dsn **my_dsn**
  
  `java level2 my_dsn`

  For the full syntax of the program, use "java level2 -h".

**level3**

This program uses the TimesTen JDBC driver to perform order processing operations:

a) Connect to the database using the TimesTenDataSource interface

b) Process all orders in the input3.dat file by inserting into the ORDERS and ORDER_ITEM tables, select from the Inventory table to check available quantity, and update the INVENTORY table to debit the quantity from the order. If there is not enough quantity in the inventory, the program rolls back the transaction and issues the message.

c) Disconnect from the database.

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java level3`

  Connect using default dsn and uid, and client/server mode
  
  `java level3 -c`

  Connect using the dsn **my_dsn**
  
  `java level3 my_dsn`

  For the full syntax of the program, use "java level3 -h".

**level4**

This program is the multi-threaded version of the level3.java program. This program uses multiple threads to increase the application throughput.

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java level4`

  Connect using default dsn and uid, and client/server mode
  
  `java level4 -c`

  Connect using the dsn **my_dsn**
  
  `java level4 my_dsn`

  For the full syntax of the program, use "java level4 -h".

**plsqlJDBC**

This sample program uses JDBC to access common PLSQL packages (emp\_pkg and sample\_pkg) in four different ways:

- Calls a stored procedure with IN and OUT parameters

- Calls a stored function with IN and OUT parameters

- Calls an anonymous block and passes host variables into and out of the block

- Calls a store procedure to open a ref cursor and uses JDBC to process 
the result-set of the ref cursor

The EMP table and the emp\_pkg and sample\_pkg PLSQL packages need to exist for this program to work.

Examples:

  Run the program using the default DSN sampledb, it will prompt for the username and password
  
  `java plsqlJDBC`

For the full syntax of the program, use "java plsqlJDBC -help".

**syncJMS**

This program uses the TimesTen JMS/XLA implementation to process messages:

a) Connects to the database as an XLA user

b) Listen to committed changes for ANY table (defaults to the appuser.customer table)

c) Use a JMS Subscriber

d) SYNCHRONOUSLY receive messages via the receive method in the get routine

e) Display the change records to the console

f) Disconnect from the database when the used enters CTRL-C

Either the level1 - level4 JDBC programs or ttIsql can be used to apply committed changes to the CUSTOMER, PRODUCT, ORDERS, ORDER_ITEM and INVENTORY tables

Examples:

  Connect using the default DSN sampledb, uid=xlauser, prompted password, listen to APPUSER.CUSTOMER
  
  `java syncJMS`

  Connect using the default DSN sampledb, uid=xlauser, listen to APPUSER.CUSTOMER
  
  `java syncJMS -xlauser <xlausername>`

  Connect using the default DSN sampledb, uid=xlauser and listen to the MYUSER.PRODUCT table
  
  `java syncJMS -xlauser <xlausername> -schema myUser -table product `

For the full syntax of the program, use "java syncJMS -h".

**Tptbm**

This program implements a multi-user throughput benchmark. By default, the transaction mix consists of 80% SELECT (read) transactions and 20% UPDATE (write) transactions. In addition to SELECTs and UPDATEs, INSERTs can also be included in the transaction mix. The ratio of SELECTs, UPDATEs and INSERTs is specified at the command line. Each transaction consists of one or more SQL operations.

The benchmark initially populates the data store, and then executes the transaction mix on it. The number of tuples inserted as part of the transaction mix cannot exceed the number of tuples with which the database is populated initially.

The measurement error for the benchmark is at most 2 seconds. This will
be negligible at loads with a duration in excess of 200 seconds. A suggested load for the benchmark is one that lasts at least 600 seconds.

The schema for this test is described in the program source file tptbm.java.

Examples:

  
Run the program using default workload mix of 80% reads, 20% updates, dsn=sampledb, uid=appuser
  
  `java Tptbm`

80% reads, 20% updates, 4 threads, populate the table with 400,000 rows, and run 100,000 transactions
  
  `java Tptbm -threads 4 -key 200 -xacts 100000`

85% reads, 10% inserts, 5% updates, 4 threads, 10,000 transactions
  
  `java Tptbm -threads 4 -reads 85 -inserts 10 -xacts 10000`

For the full syntax of the program, use "java Tptbm -h".


**TTJdbcExamples**

This program demonstrates the following operations using the TimesTen JDBC DriverManager interface to:

a) Connect and disconnect

b) Perform some DDL and DML operations: create table, create index, prepare statements, insert into and select from a table

c) Make changes to influence the query plan

d) Execute batch updates

Examples:

  Run all examples, connect using the default DSN (sampledb) as user appuser
  
  `java TTJdbcExamples`

  To run only example 2
  
  `java TTJdbcExamples -run 2`

  To run the 64-bit JDK for example 2
  
  `java -d64 TTJdbcExamples -run 2`

  For the full syntax of the program, use "java TTJdbcExamples -h".


For more information on Java programming with Oracle TimesTen, refer to the [Oracle TimesTen In-Memory Database Java Developer's Guide](https://docs.oracle.com/database/timesten-18.1/TTJDV/toc.htm).
