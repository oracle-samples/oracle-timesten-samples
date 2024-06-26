Copyright (c) 2019, 2024 Oracle and/or its affiliates. All rights reserved.

# TimesTen Node.js Samples

This folder contains Node.js samples that illustrate database connection and operations using the node-oracledb driver against  TimesTen databases. 

## Software & Platform Support
The following table describes the tested operating systems, node-oracledb driver and TimesTen software versions.

OS  | Node.js Version  | node-oracledb Driver Version | TimesTen Client Driver	| TimesTen Direct Driver
------------- | ------- | -------------	| ------------ | ------
Linux 64-bit  | 14+  |6.0+    | 22.1.1.25.0+	| 22.1.1.25.0+
macOS      |  14+  |6.0+  | 22.1.1.25.0+	| N/A
MS Windows 64-bit    | 14+  | 6.0+    | 22.1.1.25.0+ | N/A

**NOTE**: Access to TimesTen Databases on any supported TimesTen server platforms can be achieved using the TimesTen client driver from any of the platforms listed above. For more information on supported TimesTen platforms, see [TimesTen Release Notes](https://docs.oracle.com/en/database/other-databases/timesten/22.1/release-notes/toc.htm).


## PRE-REQUISITES
 
1. Node.js language is installed. 
2. The node-oracledb driver is installed. 
3. A TimesTen database is created and data source is setup to access that database. 
4. Environment to access Node.js, node-oracledb driver and TimesTen data source are set up (i.e. the TimesTen environment script ttenv.sh/ttenv.csh has been executed)

For more information on setup, see [TimesTen In-Memory Database Open Source Languages Support Guide](https://docs.oracle.com/en/database/other-databases/timesten/22.1/open-source-languages/index.html).

## Known Problems and Limitations

* NVARCHAR/NCHAR data types in a Node.js application are encoded as UTF-16, the [same difference between Oracle and TimesTen](https://docs.oracle.com/en/database/other-databases/timesten/22.1/cache/compatibility-timesten-and-oracle-databases.html#GUID-13FF0E9B-9250-49DB-810A-89EE48605E5E) as noted in the TimesTen Documentation.
* Fetching a clob inside a stored procedure through a Node.js application can result in error ORA-00600.
* DML statements with RETURN INTO are currently not supported.
* The value returned for the sub-second field of a PL/SQL output parameter of type Timestamp may be incorrect. 



## Node.js sample programs
### Download Samples

Node.js sample programs to access TimesTen databases can be downloaded from [Oracle/oracle-timesten-samples/languages/nodejs on github](https://github.com/oracle/oracle-timesten-samples/tree/master/languages/nodejs). Command-line command such as "git clone" can be used for github download. For example,

```
% git clone https://github.com/oracle/oracle-timesten-samples
```

Once the samples are downloaded locally, you can change to languages/nodejs subdirectory and run the samples directly from the local machine.  Descriptions of the sample programs and examples of how to run them are below.

### simple.js

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
### queriesAndPlsql.js

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
### lobs.js

The lobs sample program connects to a TimesTen database and performs a number of database operations against a CLOB data type table:


* Creates a table named "CLOB"
* Populates the table
* Performs Select/fetching row
* Disconnects from the database

Example:

```
% node lobs.js -u username -p password
> Connecting
> Creating table with CLOB column
> Reading file
> Populating CLOB from file
> Querying CLOB column
> Reading CLOB
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce facilisis lacinia mauris et sodales. Ut non ligula eget lorem elementum maximus. Fusce pretium, felis a ultrices sodales, ligula augue ullamcorper eros, nec accumsan risus diam id justo. Mauris sed dictum nunc, vitae vehicula felis. Praesent et sodales odio. Nunc pulvinar ipsum ac erat iaculis efficitur. Nunc in aliquet est. Donec porta est est, nec iaculis leo lacinia at.

Nunc quis sodales sem. Nam felis dolor, cursus volutpat cursus vel, tempus nec mauris. Morbi bibendum urna nec leo commodo, id pellentesque nisi dictum. Vivamus venenatis velit nec orci imperdiet sagittis. Praesent fermentum, tortor sed tempus condimentum, lectus ipsum condimentum diam, at facilisis ligula purus vitae neque. Quisque erat quam, tristique at nisl sed, mattis feugiat diam. Curabitur ipsum nibh, mollis at venenatis nec, rutrum eu quam. Fusce augue tellus, porta ut dapibus at, ornare ut nisi. In imperdiet elit non dolor pharetra, quis bibendum odio aliquam. Quisque porttitor tempus augue eu consequat. Cras ac metus malesuada, pellentesque tortor in, porta leo.

Aliquam erat volutpat. Duis vitae quam id est maximus commodo. Morbi dolor lacus, bibendum ac nisl nec, fringilla eleifend nibh. Proin a tellus rhoncus, fermentum magna sed, imperdiet nibh. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae; Nullam sed consequat nisi. Morbi tortor ipsum, consequat a suscipit sed, ullamcorper ut massa.

Aliquam erat volutpat. Maecenas porttitor vel sapien non viverra. Sed dignissim luctus lectus at cursus. Sed condimentum at massa in egestas. In sed dui eget augue posuere finibus. Etiam pulvinar libero sit amet magna efficitur scelerisque. Sed pretium, turpis in condimentum blandit, risus tortor venenatis nisl, facilisis luctus mauris metus ut sem. Proin dapibus sit amet nunc a ultricies. Phasellus interdum lobortis leo sed fermentum. Phasellus ac aliquam erat. Duis at ultricies urna. Nulla at pharetra dolor, id sodales sapien. Ut auctor mi cras amet.

> Finished reading CLOB
> Connection released


```



## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/en/database/other-databases/timesten/).  Online documentation for the node-oracledb driver can be found [here](https://oracle.github.io/node-oracledb/doc/api.html).
