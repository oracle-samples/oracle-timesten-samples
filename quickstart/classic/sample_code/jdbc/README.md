Copyright (c) 2010, 2024, Oracle and/or its affiliates. All rights reserved.

# Compile and Run JDBC Sample Programs

## IMPORTANT PRE-REQUISITES

1. Manually Configure the Sample DSN for the Sample Programs. Refer to _quickstart/classic/html/developer/sample\_dsn\_setup.html_.

2. Set up the sample database and user accounts. The following build_sampledb script should be run once to set up the sample database and user accounts.
  
     Set up the Instance Environment Variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt221 directory, execute the command

     `source /home/timesten/instance/tt221/bin/ttenv.sh`

     Run the _quickstart/classic/sample\_scripts/createdb/build\_sampledb_ script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

      Unix/Linux:
      
      `cd  quickstart/classic/sample_scripts/createdb`
      
      `./build_sampledb.sh`

3. Set up environment to compile and run the sample application

     The following scripts must be run for each of your terminal sessions...

    Set up the Instance environment variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt221 directory, execute the command

    `source /home/timesten/instance/tt221/bin/ttenv.sh`

    Set up quickstart environment variables:
    
    Unix/Linux: 	  	
    
    `. quickstart/classic/ttquickstartenv.sh`
    
    or
    
    `source quickstart/classic/ttquickstartenv.csh`

## How to compile the sample JDBC programs

To compile the sample programs in the sample\_code/jdbc directory, use the relevant TimesTen supported Java compiler for your platform (eg Sun, HP, JRocket or IBM JDK) to compile each sample program. Refer to the [OracleTimesTen In-Memory Database Installation Guide](https://docs.oracle.com/cd/E21901_01/timesten.1122/e21632/toc.htm) for the list of supported JDKs for your preferred platform.

For JDK 11, 17, 21, and 25, the TimesTen JDBC JAR is timesten_home/install/lib/ttjdbc<jdk_version>.jar, where <jdk_version> indicates the JDK version, 11, 17, 21, or 25, for example, ttjdbc25.jar for JDK 25. The JAR is also packaged as a Java module with the Java module name, timesten.jdbc, so you can use it for module compilation, if your JDK supports module. 

### Compile with JDK 11, 17 and 21

To compile specific program:

`javac -d out <progname>.java`

To compile the sample programs all at once:

`javac -d out *.java`

The "-d" is necessary to build the module application in "out" directory. If you check the "out" directory you will find package directories.

### Compile using the TimesTen JDBC Module JDK 11, 17, 21 and 25

#### Compile as a non-modular or mixed application:

To compile specific program:

`javac -cp $CLASSPATH --module-path <module_path> -d out <progname>.java`

To compile the sample programs all at once:

`javac -cp $CLASSPATH --module-path <module_path> -d out *.java`

Where <module_path> is timesten_home/install/lib/ttjdbc<jdk_version>.jar:<existing_path_to_modules>. The "-d" is necessary to build the module application in "out" directory. If you check the "out" directory you will find package directories. 

#### Compile as a modular application:

To compile specific program:

`javac --module-path <module_path> -d out module-info.java <progname>.java`

To compile the sample programs all at once:

`javac --module-path <module_path> -d out *.java`

Where <module_path> is timesten_home/install/lib/ttjdbc<jdk_version>.jar:<existing_path_to_modules>. The "-d" is necessary to build the module application in "out" directory. If you check the "out" directory you will find module-info.class and package directories. The module name, dependencies and exports are defined in module-info.java file (you need this to compile a module), and all java files you are going to compile into a module need package name as shown in java files ("jdbc.demo" and "jms.demo"). For this demos when you want to **compile as a module application** you can use $CLASSPATH (set in **IMPORTANT PRE-REQUISITES** section) as your <module_path> to ensure all dependendies are imported as modules and have "out" directories of jdbc and jms demos included. When you make your own module application (not this demos) make sure to define your own module-info.java (module name, dependencies, exports, etc.) and name your packages accordingly.

**NOTE:** Since XLA does not support applications linked with a driver manager library or the client/server library, the asyncJMS and syncJMS demos cannot be compiled or run in client-only installations. Additionally, asyncJMS2 and syncJMS2 which are sample programs using Jakarta JMS rather than Javax JMS, require separate download of jakarta.jms.jar file. Once this jar file is downloaded, location to this jar will need to be added to environment variable CLASSPATH. Support for Jakarta JMS has been added to the TimesTen release from version 22.1.1.20.0 onward.

## How to run the sample JDBC programs

**NOTE:** On platforms where both the 32-bit and 64-bit JDKs are installed in
the same directory (e.g. Solaris), java must be invoked with the -d64
option in order to run the 64-bit JVM.

**NOTE:** On some platforms, such as macOS, you may need to explicitly pass a setting for java.library path to the JVM in order to run the samples. For example:

    java -Djava.library.path=${TIMESTEN_HOME}/install/lib <packagename><progname>

**NOTE:** When you want to run using the TimesTen JDBC Module with JDK 11, 17, 21 and 25. The module-path and enable-native-access need to be added. For example:
    
Run as a non-modular or mixed application

    java -cp $CLASSPATH --module-path <module_path> --enable-native-access="timesten.jdbc" jdbc.demo.<progname> …

    java -cp $CLASSPATH --module-path <module_path> --enable-native-access="timesten.jdbc,timesten.jmsxla" jms.demo.<progname> …


Run as a modular application

    java --module-path <module_path> --enable-native-access="timesten.jdbc" --module my.jdbc.app.module/jdbc.demo.<progname> …

    java --module-path <module_path> --enable-native-access="timesten.jdbc,timesten.jmsxla" --module my.jms.app.module/jms.demo.<progname> …


Make sure to also include **timesten.jmsxla** in **--enable-native-access** for jms programs like **--enable-native-access=timesten.jdbc,timesten.jmsxla**. As shown in command, the name of the module you compiled is **my.jdbc.app.module** or **my.jms.app.module** (if they are jms programs) while the package of the class you are going to execute is **jdbc.demo** or **jms.demo** (for jms programs), the name of the module and dependencies are defined in module-info.java file. As a reminder for this demos when you want to **run this as a module application** you can use $CLASSPATH (set in **IMPORTANT PRE-REQUISITES** section) as your <module_path>. When you build and run your own module application (not this demos) make sure to define your own module-info.java (module name, dependencies, exports, etc.) and name your packages accordingly.

**asyncJMS** or **asyncJMS2**

These two programs use the TimesTen JMS/XLA implementation to process messages.  asynJMS is using Javax JMS for processing JMS/XLA whereas asynJMS2 is using Jakarta JMS. The functions of these two programs are:

a) Connect to the database as an XLA user

b) Listen to committed changes to the CUSTOMER table

c) Use a JMS Subscriber

d) ASYNCHRONOUSLY receive messages via onMessage() and a JMS MessageListener

e) Display the change records to the console

f) Disconnect from the database when the used enters CTRL-C

Either the level1 - level4 JDBC programs or ttIsql can be used to apply committed changes to the CUSTOMER table.

Examples:

  Connect using the default DSN sampledb, uid=xlauser, prompted password, listen to APPUSER.CUSTOMER
  
  `java jms.demo.asyncJMS`

  Connect using the default DSN sampledb, uid=xlauser, listen to APPUSER.CUSTOMER
 
  `java jms.demo.asyncJMS -xlauser <xlausername>`

  Connect using the default DSN sampledb, uid=xlauser and listen to the MYUSER.CUSTOMER table
  
  `java jms.demo.asyncJMS -xlauser <xlausername> -schema myUser`

  For the full syntax of the program, use "java jms.demo.asyncJMS -h" or "java jms.demo.asyncJMS2 -h".

**level1**

This program uses the TimesTen JDBC DriverManager interface to show basic operations on a TimesTen database: 

a) Connect using the com.timesten.jdbc.timesTenDriver interface

b) Insert into the CUSTOMER table all the data read from input file input1.dat file

c) Call ttOptUpdateStats to update statistics

d) SELECT from the CUSTOMER table, fetch, and print the result set to stdout

e) Disconnect from the database

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java jdbc.demo.level1`

  Connect using default dsn and uid, and client/server mode
  
  `java jdbc.demo.level1 -c`

  Connect using the dsn **my_dsn**
  
  `java jdbc.demo.level1 my_dsn`

  For the full syntax of the program, use "java jdbc.demo.level1 -h".

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
  
  `java jdbc.demo.level2`

  Connect using default dsn and uid, and client/server mode
  
  `java jdbc.demo.level2 -c`

  Connect using the dsn **my_dsn**
  
  `java jdbc.demo.level2 my_dsn`

  For the full syntax of the program, use "java jdbc.demo.level2 -h".

**level3**

This program uses the TimesTen JDBC driver to perform order processing operations:

a) Connect to the database using the TimesTenDataSource interface

b) Process all orders in the input3.dat file by inserting into the ORDERS and ORDER_ITEM tables, select from the Inventory table to check available quantity, and update the INVENTORY table to debit the quantity from the order. If there is not enough quantity in the inventory, the program rolls back the transaction and issues the message.

c) Disconnect from the database.

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java jdbc.demo.level3`

  Connect using default dsn and uid, and client/server mode
  
  `java jdbc.demo.level3 -c`

  Connect using the dsn **my_dsn**
  
  `java jdbc.demo.level3 my_dsn`

  For the full syntax of the program, use "java jdbc.demo.level3 -h".

**level4**

This program is the multi-threaded version of the level3.java program. This program uses multiple threads to increase the application throughput.

Examples:

  Connect using default dsn sampledb, uid=appuser, and direct-linked
  
  `java jdbc.demo.level4`

  Connect using default dsn and uid, and client/server mode
  
  `java jdbc.demo.level4 -c`

  Connect using the dsn **my_dsn**
  
  `java jdbc.demo.level4 my_dsn`

  For the full syntax of the program, use "java jdbc.demo.level4 -h".

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
  
  `java jdbc.demo.plsqlJDBC`

For the full syntax of the program, use "java jdbc.demo.plsqlJDBC -help".

**syncJMS** or **syncJMS2**

These two programs use the TimesTen JMS/XLA implementation to process messages.  synJMS is using Javax JMS for processing JMS/XLA whereas synJMS2 is using Jakarta JMS. The functions of these two programs are:

a) Connects to the database as an XLA user

b) Listen to committed changes for ANY table (defaults to the appuser.customer table)

c) Use a JMS Subscriber

d) SYNCHRONOUSLY receive messages via the receive method in the get routine

e) Display the change records to the console

f) Disconnect from the database when the used enters CTRL-C

Either the level1 - level4 JDBC programs or ttIsql can be used to apply committed changes to the CUSTOMER, PRODUCT, ORDERS, ORDER_ITEM and INVENTORY tables

Examples:

  Connect using the default DSN sampledb, uid=xlauser, prompted password, listen to APPUSER.CUSTOMER
  
  `java jms.demo.syncJMS`

  Connect using the default DSN sampledb, uid=xlauser, listen to APPUSER.CUSTOMER
  
  `java jms.demo.syncJMS -xlauser <xlausername>`

  Connect using the default DSN sampledb, uid=xlauser and listen to the MYUSER.PRODUCT table
  
  `java jms.demo.syncJMS -xlauser <xlausername> -schema myUser -table product `

For the full syntax of the program, use "java jms.demo.syncJMS -h" or "java jms.demo.syncJMS2 -h".

**Tptbm**

This program implements a multi-user throughput benchmark. By default, the transaction mix consists of 80% SELECT (read) transactions and 20% UPDATE (write) transactions. In addition to SELECTs and UPDATEs, INSERTs can also be included in the transaction mix. The ratio of SELECTs, UPDATEs and INSERTs is specified at the command line. Each transaction consists of one or more SQL operations.

The benchmark initially populates the data store, and then executes the transaction mix on it. The number of tuples inserted as part of the transaction mix cannot exceed the number of tuples with which the database is populated initially.

The measurement error for the benchmark is at most 2 seconds. This will
be negligible at loads with a duration in excess of 200 seconds. A suggested load for the benchmark is one that lasts at least 600 seconds.

The schema for this test is described in the program source file tptbm.java.

Examples:

  
Run the program using default workload mix of 80% reads, 20% updates, dsn=sampledb, uid=appuser
  
  `java jdbc.demo.Tptbm`

80% reads, 20% updates, 2 threads, populate the table with 400,000 rows, and run for 60 seconds with a 10 second ramp-upand ramp-down time.
  
  `java jdbc.demo.Tptbm -threads 2 -key 200 -sec 60`

85% reads, 10% inserts, 5% updates, 4 threads.
  
  `java jdbc.demo.Tptbm -threads 4 -read 85 -insert 10 `

For the full syntax of the program, use "java jdbc.demo.Tptbm -h".


**TTJdbcExamples**

This program demonstrates the following operations using the TimesTen JDBC DriverManager interface to:

a) Connect and disconnect

b) Perform some DDL and DML operations: create table, create index, prepare statements, insert into and select from a table

c) Make changes to influence the query plan

d) Execute batch updates

Examples:

  Run all examples, connect using the default DSN (sampledb) as user appuser
  
  `java jdbc.demo.TTJdbcExamples`

  To run only example 2
  
  `java jdbc.demo.TTJdbcExamples -run 2`

  To run the 64-bit JDK for example 2
  
  `java -d64 jdbc.demo.TTJdbcExamples -run 2`

  For the full syntax of the program, use "java jdbc.demo.TTJdbcExamples -h".


For more information on Java programming with Oracle TimesTen, refer to the [Oracle TimesTen In-Memory Database Java Developer's Guide](https://docs.oracle.com/en/database/other-databases/timesten/22.1/java-developer/index.html).
