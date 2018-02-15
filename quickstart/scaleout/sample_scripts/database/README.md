Copyright (c) 1997, 2018, Oracle and/or its affiliates. All rights reserved.

# Database scripts for sample database

This directory contains the following files and directories which can be used to create and populate the Mobile Payments sample database used by the Java/JDBC and C/ODBC GridSample programs.

| File/Directory | Description |
| :------------- | :---------- |
| [sql](./sql)   | Various SQL scripts used by the utility scripts. |
| [data](./data) | The data files and a SQL script used to populate the database. |
| [createuser](./createuser) | Script to create the database user based on input provided by the user. |
| [createdb](./createdb) | Script to create the database objects that comprise the Mobile Payments sample database. |
| [loaddata](./loaddata) | Script to load the demo data set into the sample database. |
| [dropdb](./dropdb) | Script to drop the database objects that comprise the Mobile Payments sample database. |

## IMPORTANT NOTES:

* Type \<script-name\> -help for detailed help for each script.

* The **createuser** script must be executed directly on one of the data hosts as the instance administrator user.

* The other scripts can be executed on a data host or on the client host.

* Before executing any of the scripts you should 'source' the necessary environment scripts to set your environment correctly:

````
      <instance_home>/bin/ttenv.[c]sh

      followed by

      <quickstart_install_dir>/quickstart/scaleout/ttquickstartenv.[c]sh
````