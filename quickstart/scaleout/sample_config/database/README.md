Copyright (c) 1997, 2018, Oracle and/or its affiliates. All rights reserved.

# Configuring the Scaleout sample database

This directory contains the following files which can be used to configure the Scaleout sample database and, optionally, client connectivity.

## Prerequisites

In order to create a Scaleout database you need to have an already deployed and functional TimesTen Scaleout grid; a grid exists independantly of any databases that it may be hosting. 

If you deploy your grid using ttGridRollout then database creation is normally performed at the same time as grid deployment whereas if you are deploying using ttGridAdmin or SQL Developer then you can create the database subsequent to deploying the grid. 

Regardless of deployment method the sample files provided here, suitably customized, can be used to create a database suitable for use with the Scaleout samples.

If you plan to build and execute the sample programs on one of the data hosts within the grid and to connect to the grid database using TimesTen direct mode connectivity then you are ready to go once the physical database has been created.

If you plan to use client/server connectivity to access the grid database, for example if you plan to build and execute the sample programs on a  computer system that is not hosting one of your grid's data instances, then you will need to perform some additional steps to configure client connectivity.

The steps to install, configure and deploy a grid are not covered in this README; consult the [TimesTen In-Memory Database documentation](https://docs.oracle.com/database/timesten-18.1) for the necessary steps.

## Creating the physical sample database

### At the primary management instance

This action needs to be performed at the primary management instance as the instance administrator OS user. Your environment must be set to access the management instance:

```text
. <mgmt_instance_home>/bin/ttenv.sh       OR
source <mgmt_instance_home>/bin/ttenv.csh
```

Take the provided sample database definition file, [sampledb.dbdef](./sampledb.dbdef), and copy it to a suitable location on the primary management host. Here are the contents of that file:

```text
DataStore=/<customize>/<this>/<path>/sampledb
PermSize=128
TempSize=128
LogBufMB=128
LogFileSize=128
DatabaseCharacterSet=AL32UTF8
ConnectionCharacterSet=AL32UTF8
Durability=0
```

With this configuration, ___each___ database element will require a little under 400 MB of shared memory. This sample configuration assumes that the sample database consists of 3 replica sets; if you configure your database with more or less replica sets then you will need to adjust the memory allocation (PermSize) for each database element in proportion before creating the database.

In order to use this database definition file you must decide where you will be placing the database files (checkpoint and log files) in each database element and after creating those locations on every element you should customise the value for the DataStore attribute in the dbdef file accordingly.

Once you have customized the dbdef file you can either reference it in your ttGridRollout master configuration file (in order to create the database as part of grid deployment) or you can use it with the ttGridAdmin utility to create the database in your already deployed grid. For example:

```text
ttGridAdmin dbdefCreate sampledb.dbdef
ttGridAdmin modelApply
ttGridAdmin dbCreate sampledb -wait
```

If you will be using exclusively direct mode connectivity then you are done. If you plan to use client/server connectivity to access your grid database then there are some additional steps needed.

## Configuring client/server connectivity

If your application will be running on the same computer system as one of the grid data instances then in general you will use direct mode connectivity for optimal performance. If you will be running your application, or any other database tools, on a separate computer system (the client system) which has network connectivity to the grid data instances then you will need to perform the following additional steps:

### At the primary management instance

1. Create a client connectable using the provided sampledbCS.connect file. Again this can either be done at grid rollout time, if using ttGridRollout, or at some later time if using ttGridAdmin. For example:

```text
ttGridAdmin connectableCreate -dbdef sampledb -cs sampledbCS.connect
ttGridAdmin modelApply
```

2. Export a client sys.odbc.ini file containing the client DSN definition(s).

```text
ttGridAdmin gridClientExport client_sys.odbc.ini
```

3. Copy the exported file to the client system.

### On the client machine

As the desired instance administrator OS user:

4.  Install the TimesTen In-Memory Database software.

5.  Create a ___client___ instance using the **ttInstanceCreate** utility.

6.  Rename the file from step 3 as 'sys.odbc.ini' and copy it to the client instance's 'conf' directory:

```text
cp client_sys.odbc.ini <client_instance_home>/conf/sys.odbc.ini 
```

Client connectivity is now configured.
