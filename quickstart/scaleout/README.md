Copyright (c) 1998, 2018, Oracle and/or its affiliates. All rights reserved.

# Sample database and programs for TimesTen Scaleout

This directory contains the files necessary to create the Mobile Payments sample database and then to build and execute the C/ODBC and Java/JDBC 'GridSample' sample programs.

These programs illustrate how to write applications to connect to and execute transactions against a TimesTen database deployed using TimesTen Scaleout. They also illustrate best practice for handling transient errors and client connection failover events.

The files and directories contained here are as follows:

| File/Directory                               | Description                              |
| :------------------------------------------- | :----------                           |
| [ttquickstartenv.cmd](./ttquickstartenv.cmd) | Windows command file to configure the required environment variables on Windows. |
| [ttquickstartenv.csh](./ttquickstartenv.csh) | Script to configure the required environment variables for the C shell and compatible shells (csh, tcsh). |
| [ttquickstartenv.zsh](./ttquickstartenv.zsh) | Script to configure the required environment variables for the Z shell (zsh). This is primarily intended for macOS users. |
| [ttquickstartenv.sh](./ttquickstartenv.sh)   | Script to configure the required environment variables for the Bourne shell and compatible shells (sh, ksh, bash, ...). |
| [sample_config](./sample_config)           | Example configuration files including an example database definition and an example client connectable for use with ttGridAdmin. |
| [sample_scripts](./sample_scripts)           | Scripts, SQL files and data files for building and populating the Mobile Payments demo database. |
| [sample_code](./sample_code)                 | The Scaleout sample programs. |

In order to use the sample programs you must perform the following steps:

1. Install, configure and deploy a TimesTen Scaleout grid, create the physical sample database and optionally configure client/server connectivity. See  [sample\_config/database/README.md](./sample\_config/database/README.md) for detailed instructions.

2. Create the database user then create and and populate the Mobile Payments sample database (populating the database is not necessary for the **TptBm** sample program). See [sample\_scripts/database/README.md](./sample\_scripts/database/README.md) for detailed instructions.

3. Compile and execute the sample programs. See [sample\_code/odbc/README.md](./sample\_code/odbc/README.md) and [sample\_code/jdbc/README.md](./sample\_code/jdbc/README.md) for detailed instructions.

**IMPORTANT**

Although you can build and run the sample programs on a variety of platforms using a suitabvle TimesTen Client instance, TimesTen Scaleout itself is only supported on the Linux x86-64 platform.
