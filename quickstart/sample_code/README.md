Copyright (c) 1998, 2026, Oracle and/or its affiliates. All rights reserved.

# Sample Code for TimesTen Classic

This directory contains the TimesTen sample-code collection. The samples are kept in their existing locations for compatibility with QuickStart documentation, scripts, and long-standing user workflows. Most of these samples will also work with TimesTen Scaleout.

Each sample directory contains a README file with the necessary information to help you use the associated sample(s).

## Recommended application-development paths

For new application-development work, the following entry points are usually the easiest places to start:

| Area | Location | Description |
| :--- | :------- | :---------- |
| Java/JDBC | [jdbc](./jdbc) | Java samples using JDBC, including JSON and PL/SQL examples. |
| Python | [../../languages/python](../../languages/python) | Python samples using the `python-oracledb` driver. |
| Node.js | [../../languages/nodejs](../../languages/nodejs) | Node.js samples using the `node-oracledb` driver. |

## API and compatibility samples

The rest of this tree remains available for customers who need specific APIs, older integration patterns, performance tests, or compatibility references.

| File/Directory                               | Description                              |
| :------------------------------------------- | :----------                           |
| [common](./common) | Code and data used by multiple samples. |
| [oci](./oci)     | C samples using the Oracle Call Interface (OCI). |
| [odbc](./odbc)   | C samples using ODBC (TimesTen's native API). |
| [plsql](./plsql) | PL/SQL samples. |
| [ttclasses](./ttclasses) | C++ samples using TimesTen's TTClasses API. |
| [odp.net](./odp.net) | C# sample using Oracle ODP.NET. |
| [odpi-c](./odpi-c) | ODPI-C sample using Oracle Database Programming Interface for C. |
| [j2ee_orm](./j2ee_orm)  | J2EE application-server and ORM samples, including WebLogic, JBoss, WebSphere, GlassFish, and Hibernate examples. |

## Full directory map

| File/Directory                               | Description                              |
| :------------------------------------------- | :----------                           |
| [common](./common) | Code used by multiple samples. |
| [j2ee_orm](./j2ee_orm)  | Samples relating to various J2EE appservers (such as WebLogic, JBoss etc.) and Object Relational Mapping frameworks (such as Hibernate). |
| [jdbc](./jdbc)   | Java samples using JDBC. |
| [oci](./oci)     | C samples using the Oracle Call Interface (OCI). |
| [odbc](./odbc)   | C samples using ODBC (TimesTen's native API). |
| [plsql](./plsql) | PL/SQL samples. |
| [ttclasses](./ttclasses) | C++ samples using TimesTen's TTClasses API. |
| [odp.net](./odp.net) | C# sample using Oracle ODP.NET. |
| [odpi-c](./odpi-c) | ODPI-C sample using Oracle Database Programming Interface for C (ODPI-C). |
