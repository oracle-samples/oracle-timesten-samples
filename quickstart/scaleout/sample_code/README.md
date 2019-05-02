Copyright (c) 1998, 2018, Oracle and/or its affiliates. All rights reserved.

# Sample programs for Scaleout

This directory contains the files necessary to build and execute the C/ODBC and Java/JDBC 'GridSample' sample programs. These programs illustrate how to write applications to connect to and execute transactions against a TimesTen Scaleout database. They also illustrate best practice for handling transient errors and client connection failover events.

The sub-directories contained here are as follows:

| Directory | Description |
| :-------- | :----------|
| [odbc](./odbc) | Source code, Makefile and README for the C/ODBC version of the GridSample sample program and the TptBm benchmark program. See the [README](./odbc/README.md) file for full instructions. |
| [jdbc](./jdbc) | Source code, Makefile and README for the Java/JDBC version of the GridSample sample program. See the [README](./jdbc/README.md) file for full instructions. |