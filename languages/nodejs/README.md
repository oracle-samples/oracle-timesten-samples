Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

#TimesTen Node.js Samples

This folder contains Node.js samples that illustrate database connection and operations using the node-oracledb driver against  TimesTen databases. 

##Software & Platform Support
The following table describes the tested operating systems, node-oracledb driver and TimesTen software versions.

OS  | Node.js Version  | node-oracledb Driver Version | TimesTen Client Driver	| TimesTen Direct Driver
------------- | ------- | -------------	| ------------ | ------
Linux 64-bit  | 12.7.0  |4.0.1    | 18.1.3.1.0+	| 18.1.3.1.0+
macOS      |  12.7.0  |4.0.1   | 18.1.3.1.0+	| N/A
MS Windows 64-bit    | 12.7.0  | 4.0.1    | 18.1.3.1.0+ | N/A

**NOTE**: Access to TimesTen Databases on any supported TimesTen server platforms can be achieved using the TimesTen client driver from any of the platforms listed above. For more information on supported TimesTen platforms, see [TimesTen Release Notes](https://docs.oracle.com/database/timesten-18.1/TTREL/toc.htm).


## PRE-REQUISITES
 
1. Node.js language is installed. 
2. The node-oracledb driver is installed. 
3. A TimesTen database is created and data source is setup to access that database. 
4. Environment to access Node.js, node-oracledb driver and TimesTen data source are set up (i.e. the TimesTen environment script ttenv.sh/ttenv.csh has been executed)

For more information on setup, see [TimesTen In-Memory Database Open Source Languages Support Guide](https://docs.oracle.com/database/timesten-18.1/TTOSL/toc.htm).

## Known Problems and Limitations
* REF CURSORs are currently not supported.
* Using the NVARCHAR/NCHAR data types for input parameters in a PL/SQL procedure is currently not supported.
* Large objects (LOBs) are currently not supported.
* DML statements with RETURN INTO are currently not supported.
* The value returned for the sub-second field of a PL/SQL output parameter of type Timestamp may be incorrect. 



## Node.js sample programs
### Download Samples

Node.js sample programs to access TimesTen databases can be downloaded from [Oracle/oracle-timesten-samples/languages/nodejs on github](https://github.com/oracle/oracle-timesten-samples/tree/master/languages/nodejs). Command-line command such as "git clone" can be used for github download. For example,

```
% git clone https://github.com/oracle/oracle-timesten-samples
```

Once the samples are downloaded locally, you can change to languages/nodejs subdirectory and run the samples directly from the local machine.  Descriptions of the sample programs and examples of how to run them are below.

###simple.js

This simple sample program connects to a TimesTen database and performs the following operations:

* creates a table named Employees
*  inserts 3 rows to the table
*  select the rows from the table
*  drops the table
*  disconnects from the database.

Example:

```
% node simple.js -u username -p password
Table has been created
Inserted 3 employees into the table
Selected employee: ROBERT ROBERTSON
Selected employee: ANDY ANDREWS
Selected employee: MICHAEL MICHAELSON
Table has been dropped
Connection has been released
```


### sql.js
The sql sample program connects to a TimesTen database and performs the following operations:


* Creates a table called "vpn_users"
* Populates the table with records
* Performs a number of Selects, Updates and Deletes against the table
* Drops the table
* Disconnects from the database

Example:

```
% node sql.js -u username -p password
Table has been created
Populating table
  Inserted  10 rows
  Inserted  20 rows
  Inserted  30 rows
  Inserted  40 rows
  Inserted  50 rows
  Inserted  60 rows
  Inserted  70 rows
  Inserted  80 rows
  Inserted  90 rows
  Inserted  100 rows
Performing selects
  select(ed) 10 rows
  select(ed) 20 rows
  select(ed) 30 rows
  select(ed) 40 rows
  select(ed) 50 rows
  select(ed) 60 rows
  select(ed) 70 rows
  select(ed) 80 rows
Performing updates
  update(ed) 10 rows
  update(ed) 20 rows
Performing deletes
  delete(ed) 10 rows
  delete(ed) 20 rows
Connection has been released
```
###queriesAndPlsql.js

The queriesAndPlsql sample program connects to a TimesTen database and performs a number of database operations with PL/SQL: 


* Creates a table named "items"
* Populates the table
* Performs Selects fetching 100 rows
* Calls a PL/SQL procedure to update a row
* Calls a PL/SQL procedure to delete a row
* Drops the table
* Disconnects from the database

Example:

```
% node queriesAndPlsql.js -u username -p password
Table has been created
Inserting with executeMany ...
  100 Registries added
Select some rows with one select ...
  20 Rows have been fetched and iterated
Updating a row using an anonymous block ...
  Value before update:  [ 'descr_0' ]
  Value after update:  [ 'updated description' ]
Delete a row using an anonymous block ...
  Rows after delete =  [ 99 ]
Connection has been released

```



## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/database/timesten-18.1/).  Online documenation for the node-oracledb driver can be found [here](https://oracle.github.io/node-oracledb/doc/api.html).
