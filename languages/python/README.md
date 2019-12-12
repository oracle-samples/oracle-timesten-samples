Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

#TimesTen Python Samples

This folder contains Python samples that illustrate database connection and operations using the cx_Oracle driver against the TimesTen database. 

##Software & Platform Support
The following table describes the tested operating systems, cx_Oracle driver and TimesTen software versions.

OS  | Python Version | cx_Oracle Driver Version | TimesTen Client Driver	| TimesTen Direct Driver
------------- | --------- | --------- | ------------| ------
Linux 64-bit  |  3.7.5   | 7.2.2+    | 18.1.3.1.0+	| 18.1.3.1.0+
macOS  	    |  3.7.5   |7.2.2+    | 18.1.3.1.0+	| N/A
MS Windows 64-bit   | 3.7.5  |7.2.2+    | 18.1.3.1.0+| N/A

**NOTE**: Access to TimesTen Databases on any supported TimesTen server platforms can be achieved using the TimesTen client driver from any of the platforms listed above. For more information on supported TimesTen platforms, see [TimesTen Release Notes](https://docs.oracle.com/database/timesten-18.1/TTREL/toc.htm).

**NOTE2**: Python version 2.7 also works against TimesTen databases even though it's not listed in the official tested chart above.



##PRE-REQUISITES
 
1. Python language is installed. 
2. The cx_Oracle driver for Python is installed. 
3. A TimesTen database is created and data source is setup to access that database. 
4. Environment to access Python, cx_Oracle driver and TimesTen data source are set up (i.e. the TimesTen environment script ttenv.sh/ttenv.csh has been executed)

For more information on setup, see [TimesTen In-Memory Database Open Source Languages Support Guide](https://docs.oracle.com/database/timesten-18.1/TTOSL/toc.htm).

## Known Problems and Limitations
* REF CURSORs are currently not supported.
* Using the NVARCHAR/NCHAR data types for input parameters in a PL/SQL procedure is currently not supported.
* Large objects (LOBs) are currently not supported.
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

###simple.py

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

###queriesAndPlsql.py

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


## Documentation
You can find the online documentation for Oracle TimesTen In-Memory Database in the [Documentation Library](https://docs.oracle.com/database/timesten-18.1/). Online documenation for the cx_Oracle driver can be found [here](https://cx-oracle.readthedocs.io/en/latest/).
