Copyright (c) 2019, 2020 Oracle and/or its affiliates. All rights reserved.

# TimesTen Python Samples

This folder contains Python samples that illustrate database connection and operations using the cx_Oracle driver against the TimesTen database. 

## Software & Platform Support
The following table describes the tested operating systems, cx_Oracle driver and TimesTen software versions.

OS  | Python Version | cx_Oracle Driver Version | TimesTen Client Driver	| TimesTen Direct Driver
------------- | --------- | --------- | ------------| ------
Linux 64-bit  |  3.7.5   | 7.2.2+    | 18.1.4.1.0+	| 18.1.4.1.0+
macOS  	    |  3.7.5   |7.2.2+    | 18.1.4.1.0+	| N/A
MS Windows 64-bit   | 3.7.5  |7.2.2+    | 18.1.4.1.0+| N/A

**NOTE**: Access to TimesTen Databases on any supported TimesTen server platforms can be achieved using the TimesTen client driver from any of the platforms listed above. For more information on supported TimesTen platforms, see [TimesTen Release Notes](https://docs.oracle.com/en/database/other-databases/timesten/22.1/release-notes/toc.htm).

**NOTE2**: Python version 2.7 also works against TimesTen databases even though it's not listed in the chart above.



## PRE-REQUISITES
 
1. Python language is installed. 
2. The cx_Oracle driver for Python is installed. 
3. A TimesTen database is created and data source is setup to access that database. 
4. Environment to access Python, cx_Oracle driver and TimesTen data source are set up (i.e. the TimesTen environment script ttenv.sh/ttenv.csh has been executed)

For more information on setup, see [TimesTen In-Memory Database Open Source Languages Support Guide](https://docs.oracle.com/en/database/other-databases/timesten/22.1/open-source-languages/index.html).

## Known Problems and Limitations

* NVARCHAR/NCHAR data types in a Python application are encoded as UTF-16, the [same difference between Oracle and TimesTen](https://docs.oracle.com/en/database/other-databases/timesten/22.1/cache/compatibility-timesten-and-oracle-databases.html#GUID-13FF0E9B-9250-49DB-810A-89EE48605E5E) as noted in the TimesTen Documentation.
* DML statements with RETURN INTO are currently not supported.
* The value returned for the sub-second field of a PL/SQL output parameter of type Timestamp may incorrect. 
* When using the built-in procedures ttRepStateSave() & ttRepSubscriberWait() to set the replication state from a Python applications, the operation may take some time to take effect. Your application should wait much longer than the set waitTime specified in the call to ttRepSubscriberWait() to avoid timeouts.


## Python Sample Programs
### Download Samples

Python sample programs to access TimesTen databases can be downloaded from [Oracle/oracle-timesten-samples/languages/python on github](https://github.com/oracle/oracle-timesten-samples/tree/master/languages/python). Command-line command such as "git clone" can be used for github download. For example,

```
% git clone https://github.com/oracle/oracle-timesten-samples
```

Once the samples are downloaded locally, you can change to languages/python subdirectory and run the samples directly from the local machine.  Descriptions of the sample programs and examples of how to run them are below.

### simple.py

This simple sample program connects to a TimesTen database and performs the following operations:

* creates a table named Employees
*  inserts 3 rows to the table
*  select the rows from the table
*  drops the table
*  disconnects from the database.

Example:

```
% python3 simple.py -u username -p password
Table has been created
Inserted  3 employees into the table
Selected employee: ROBERT ROBERTSON
Selected employee: ANDY ANDREWS
Selected employee: MICHAEL MICHAELSON
Table has been dropped
Connection has been released

```

### sql.py

This Python sample program connects to a TimesTen Database and performs the following operations:


* Creates a table called "vpn_users"
* Populates the table
* Performs a number of Selects, Updates and Deletes against the table
* Drops the table
* Disconnects from the database

Example:

```
% python3 sql.py -u username -p password
Table has been created
Populating table
  Inserted 10 rows
  Inserted 20 rows
  Inserted 30 rows
  Inserted 40 rows
  Inserted 50 rows
  Inserted 60 rows
  Inserted 70 rows
  Inserted 80 rows
  Inserted 90 rows
  Inserted 100 rows
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

### queriesAndPlsql.py

This Python sample program connects to a TimesTen Database and performs a number of database operations with PL/SQL: 


* Creates a table named "items"
* Populates the table
* Performs Selects fetching 100 rows
* Calls a PL/SQL procedure to update a row
* Calls a PL/SQL procedure to delete a row
* Drops the table
* Disconnects from the database

Example:

```
% python3 queriesAndPlsql.py -u username -p password
Table has been created
Inserting with executeMany ...
  100  Registries added
Select some rows with one select ...
  20 Rows have been fetched and iterated
Updating a row using an anonymous block ...
  Value before update:  descr_0
  Value before update:  updated description
Delete a row using an anonymous block ...
  Rows after delete =  99
Connection has been released

```
### lobs.py

The lobs sample program connects to a TimesTen database and performs a number of database operations against a CLOB data type table:


* Creates a table named "CLOB"
* Populates the table
* Performs Select/fetching row
* Disconnects from the database

Example:

```
% python3 lobs.py -u appuser -p appuser
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
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/en/database/other-databases/timesten/). Online documenation for the cx_Oracle driver can be found [here](https://cx-oracle.readthedocs.io/en/latest/).
