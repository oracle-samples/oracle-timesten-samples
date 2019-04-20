Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

# How to Run the PL/SQL Sample Code

PL/SQL support in Oracle TimesTen is seamlessly integrated within the TimesTen database and is available from all supported TimesTen programming interfaces, ODBC, JDBC, OCI, Pro*C, and TTClasses (C++). TimesTen PL/SQL uses the same language syntax and semantics as supported
in the Oracle database. In this release, a subset of the PL/SQL packages targeted for performance critical OLTP applications are available. For details of supported PL/SQL functionality, refer to the [Oracle TimesTen In-Memory Database PL/SQL Developer's Guide](https://docs.oracle.com/cd/E21901_01/timesten.1122/e21639/toc.htm).

## IMPORTANT PRE-REQUISITES

1. Manually Configure Sample DSN for Sample Programs refer to _quickstart/classic/html/developer/sample\_dsn\_setup.html_

2. Set up sample database and user accounts

   The following build_sampledb script should be run once to set up the sample database and user accounts; first set up the Instance Environment Variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

    `source /home/timesten/instance/tt181/bin/ttenv.sh`

    Run the quickstart/classic/sample\_scripts/createdb/build\_sampledb script, which creates the sample database and user accounts that are used by the sample programs. This script creates the TimesTen user accounts and prompts you for the desired user passwords.

    Unix/Linux:
    
    `cd quickstart/classic/sample\_scripts/createdb`
    
    `./build\_sampledb.sh`

3. Set up the environment to compile and run the sample application

    The following scripts must be run in each of your terminal sessions...

    Set up the Instance environment variables e.g. If your TimesTen instance location is under /home/timesten/instance/tt181 directory, execute the command

    `source /home/timesten/instance/tt181/bin/ttenv.sh`

    Set up quickstart environment variables:
    
    Unix/Linux:
    
    `. quickstart/classic/ttquickstartenv.sh`
    
    or
    
    `source quickstart/classic/ttquickstartenv.csh`

## How to run the sample PL/SQL code

To run the PL/SQL sample code using ttIsql, do

`ttIsql "dsn=<name>;uid=<username>"
...
Command\> @\<plsql_filename\>;`

or

`ttIsql -f <plsql_filename> "dsn=<name>;uid=<username>"`

Examples:

`ttIsql "dsn=sampledb; uid=appuser"
Enter the password for appuser
Command\> @basics.sql;`

or

`ttIsql -f basics.sql "dsn=sampledb; uid=appuser"`

_Sample Descriptions_

**basics.sql**

This PL/SQL script gives an overview of the functionality of TimesTen PL/SQL via ttIsql.

Features shown include:

* SET SERVEROUTPUT ON
* Creating stored procedures
* Executing stored procedures from a block
* Error output with SHOW ERRORs
* Examples of INPUT and OUTPUT bind variables
* PL/SQL querying the TimesTen SYSTEM tables
* Shows the stored procedures in the database using the PROCEDURES
  command
* Describes a PL/SQL object
* Displays the USER_OBJECTS view

This PL/SQL script should be executed from ttIsql.

**case\_procedures.sql**

This PL /SQL block demonstrates the usage of the CASE statement and CASE expression. The PL/SQL CASE statement includes the simple CASE statement and the searched CASE statement. The CASE expression includes the simple CASE expression, the searched CASE expression, plus two syntactic shorthands, COALESCE and NULLIF.

For comparison, each stored procedure is also implemented using traditional language elements, such as a sequence of IF statements or the SQL DECODE function.

This PL/SQL block should be executed from ttIsql and output is displayed to the console using DBMS_OUTPUT.

**cursor\_loop.sql**

This PL /SQL block uses a CURSOR and a LOOP to find and conditionally insert all employees whose monthly wages (salary plus commission) are higher than $2000. A variable of type %ROWTYPE is used to FETCH a row into. EXIT is used to quit the LOOP.

This example can be executed from ttIsql (or programatically via a TimesTen API) and output is inserted into the TEMP table. Check the values in the TEMP table before and after running this program.

**cursor\_loop\_types.sql**

This PL /SQL block show the use of the %TYPE datatype to declare variables of the same type as database columns. These types are then used in the CURSOR and some simple math is performed on the fetched data. WHEN %NOTFOUND is used to exit the LOOP.

This example can be executed from ttIsql (or via a TimesTen API) and output is inserted into the TEMP table. Check the values in the TEMP table before and after running this program.


**cursor\_loop\_types2.sql**

This PL/SQL block show the use of a FOR-LOOP based on a CURSOR. The CURSOR resultset is used to exit the LOOP.

This program can be executed from ttIsql [or from a TimesTen API] and output is inserted into the TEMP table. Check the values in the TEMP table before and after running this program.

**cursor\_update\_logic.sql**

This program modifies the ACCOUNT table based on instructions stored in the ACTION table. Each row of the ACTION table contains an account number to act upon, an action to be taken (insert, update, or delete), an amount by which to update the account, and a time tag.

On an insert, if the account already exists, an update is performed instead. On an update, if the account does not exist, it is created by an insert. On a delete, if the row does not exist, no action is taken.

This example can be executed from ttIsql (or via a TimesTen API) and output goes to the ACCOUNT and ACTION tables. Check the values in the ACTION and ACCOUNT tables before and after running this program.

**inner\_loop\_block.sql**

This PL/SQL block illustrates block structure and scope rules. An outer block declares two variables named X and COUNTER, and loops four times. Inside the loop is a sub-block that also declares a variable named X. The values inserted into the TEMP table show that the two Xs are indeed different.

This example can be executed from ttIsql (or via a TimesTen API) and output is inserted into the TEMP table. Check the values in the TEMP table before and after running this program.

**loop\_insert.sql**

This sample PL/SQL block uses a simple FOR loop to insert 10 rows into a table. The values of a loop index, counter variable, and either of two character strings are inserted. The inserted string depends on the value of the loop index.

This program can be executed from ttIsql (or via a TimesTen API) and output is inserted into the TEMP table. Check the values in the TEMP table before and after running this program.


**select\_exception.sql**

This sample block shows an example of PL/SQL EXCEPTION handling.

It calculates the ratio between the X and Y columns of the RATIO table. If the ratio is greater than 0.72, the block inserts the ratio into RESULT\_TABLE. Otherwise, it inserts -1. If the denominator is zero, ZERO\_DIVIDE is raised, and a zero is inserted into RESULT\_TABLE.

This program can be executed from ttIsql (or via a TimesTen API) and output is inserted into the RATIO table. Check the values in the RATIO table before and after running this program.


**update\_inventory.sql**

This PL/SQL block processes orders for tennis rackets. It decrements the quantity of rackets on hand only if there is at least one racket left in stock.

This program can be executed from ttIsql (or from a TimesTen API) and output goes to the INVENTORY2 and PURCHASE\_RECORD tables. Check the values in the INVENTORY2 and PURCHASE\_RECORD tables before and after running this program.

For more information on PL/SQL support in TimesTen, refer to the [Oracle TimesTen In-Memory Database PL/SQL Developer's Guide](https://docs.oracle.com/database/timesten-18.1/TTPLS/toc.htm). 
