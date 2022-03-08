# Container content

The following files from the [content](./content) directory are provisioned into the container at build time, and are essential for the correct functioning of the container.

| File name            | Description                                     |
| :-------------------------- | :---------------------------------------------- |
| [bash_profile](./bash_profile) | The .bash_profile file for the timesten user. |
| [bashrc](./bashrc) | The .bashrc file for the timesten user. |
| [init.sql](./init.sql) | When creating a database, this SQL script is run against the newly created database to provision users and any other database objects that may be desired. You should customize this script to suit your requirements. |
| [init-tt](./init-tt) | Script that is used as the entrypoint for the container. |
| [remenv](./remenv) | Helper script for the ttconnect script. Installed into /usr/local/bin within the container. |
| [start-tt](./start-tt) | Script to create and/or start the TimesTen instance and then create and/or start the TimesTen database. Invoked from the **init-tt** script. |
| [stop-tt](./stop-tt) | Script to cleanly shutdown the TimesTen database and instance. Invoked from the **init-tt** script when the shutdown signal is received. |
| [sys.odbc.ini](./sys.odbc.ini) | The TimesTen sys.odbc.ini file to be provisioned into the container's TimesTen instance. |

