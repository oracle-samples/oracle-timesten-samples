Copyright (c) 2010, 2021, Oracle and/or its affiliates. All rights reserved.

# TimesTen Driver Manager

# User Guide

# Version: 18.1.4.9.0

# Date: 7th April 2021

# Introduction to the TimesTen Driver Manager

This is the User Guide for the TimesTen Driver Manager (TTDM). TTDM is a lightweight ODBC Driver Manager analogue specifically developed and optimized for use with the Oracle TimesTen In-Memory Database.

## What is an ODBC Driver Manager and why might I need one?

When an ODBC based application connects to a datasource (typically a database) it connects to a logical name, the Data Source Name (DSN), which identifies the datasource to which it wants to connect. It does this by calling various ODBC functions provided by an ODBC driver. Some external repository of configuration information holds the various DSN values that are available, together with the necessary configuration and control information needed by the ODBC driver to establish a connection and manage usage of the datasource.

An ODBC driver is a piece of software (typically a library of some kind) that implements the ODBC API and provides the functionality to connect to a specific kind of datasource. For example, there are ODBC drivers available for most popular databases; Oracle, TimesTen, MySQL, SQL Server, Sybase, DB2, Informix etc. Each type of database (datasource) requires a different ODBC driver. Herein lies a potential problem...

Since each ODBC library defines and exposes (largely) the same set of functions (those defined by the ODBC API), an application can only be directly linked with one ODBC driver library at any one time. This means that if the application has to support different kinds of datasource, different
versions of the application must be built for each type of datasource. If the application needs to connect concurrently to different types of datasource (i.e. needs to concurrently use more than one ODBC driver) then this becomes impossible.

A solution to this issue is to use a Driver Manager (DM). A DM itself implements and exposes the ODBC API , thus the application can link directly to the DM library instead of the individual ODBC driver libraries. Based on configuration data or some other mechanism, the DM will dynamically load the relevant ODBC driver libraries at runtime as the application requires them. The DM sits between the application and the individual ODBC drivers and passes application ODBC calls to the correct underlying ODBC driver. The application can now connect to multiple datasources, using different ODBC drivers, concurrently.

## Advantages and disadvantages of using a Driver Manager?

The advantages of a DM are:

1.  It enables an application process to concurrently use multiple, different ODBC drivers.

2.  It can _sometimes_ enable an application that expects a newer version of the ODBC API standard (e.g. 3.5) to work with a driver that implements an older version of the API (e.g. 2.5). However this is not guaranteed and depends on how the application has been written.

The disadvantages of a DM are:

1.  Although there is a DM built into the Windows O/S, for Unix/Linux O/S there is no standard DM available. There are a few Open Source DMs and some commercial DMs (which require a license fee).

2.  DMs, by their nature, are generic. They focus on providing the full diverse functionality of ODBC in order to cater for all possible ODBC drivers and their capabilities. As a result, they typically impose a significant performance penalty; sometimes as much as 20% or more.

3.  Generic DMs often do not provide access to special capabilities or optimisations offered by the underlying ODBC drivers.

## Why is this relevant to Oracle TimesTen?

Oracle TimesTen is a high performance, relational, In-Memory Database (IMDB). Its native API is ODBC and its access language is SQL. TimesTen provides two connection mechanisms for applications; direct mode and client/server. Here is a brief summary of the similarities and differences between them.

**Direct mode**

-   The application and the TimesTen database are tightly coupled. They must both reside on the same physical computer system.

-   There is no IPC involved in application \<-\> database communication resulting in reduced response times and increased throughput for database (SQL) operations.

-   The API used is ODBC.

-   There is a proprietary event notification and change tracking API called XLA. This API is part of the direct mode ODBC driver library.

-   There is a proprietary API called the Routing API that can be used with TimesTen Scaleout.

-   There is a proprietary API (the utility API) to various administrative and utility functions. This is provided by a separate utility library.

**Client/server**

-   The application and the TimesTen database are loosely coupled. They can reside on the same physical computer system or different computer systems.

-   Database access is performed by a server proxy process on behalf of the client application. The server proxy always executes on the computer system where the TimesTen database is located.

-   The communication between the application and the server proxy is via a TCP/IP connection. The server proxy itself is a direct mode application. The performance of client/server access is significantly less than that of direct mode access.

-   The API used is ODBC.

-   The XLA API is **not** available.

-   The Routing API **is** available.

-   The Utility API is **not** available.

Although both connection mechanisms use the ODBC API, they are implemented as separate driver libraries. Thus, without a Driver Manager, a TimesTen application can use one or the other, but not both concurrently.

If one uses a commercial, or Open Source, DM with TimesTen in order to allow use of both direct mode and client/server connections concurrently from the same process, what problems or issues might be encountered?

1.  The DM will not have specific support for TimesTen so it might not work reliably with TimesTen.

2.  The DM will not support any TimesTen specific optimisations and due to its generic nature the performance overhead may be quite high.

3.  When using the DM to access TimesTen, TimesTen specific features and functions (such as the Routing API, XLA and the Utility Library) are not available.

The TimesTen Driver Manager (TTDM) is a lightweight DM analogue focussed specifically on overcoming these limitations. Its main features are:

-   No application source code changes are needed to use TTDM. TTDM provides 100% transparency of operation.

-   No additional configuration is required to use TTDM.

-   TTDM allows a single process to concurrently use both client/server and direct mode connections. TTDM dynamically determines the correct connection type, based on the DSN, or connection string, that is being used for a connection, and routes the application's calls to the correct TimesTen driver.

-   TTDM provides the same level of ODBC API support as TimesTen, including all TimesTen extensions.

-   For direct mode connections, TTDM provides access to the full set of XLA functionality.

-   For direct mode connections, TTDM provides access to the full set of TimesTen Utility API functionality.

-   TTDM provides access to the Routing API for both direct mode and client/server connections.

-   TTDM has a low performance overhead; see later for details.

-   TTDM operates with both full and client-only TimesTen instances without any special configuration. For example, in a client only installation, attempts to access a direct mode DSN will simply return an ODBC error.

Things that TTDM does **not** do are:

-   Emulate higher levels of the ODBC API than that supported by the TimesTen drivers.

-   Provide/emulate ODBC functions not provided by the TimesTen drivers.

-   Support the use of non-TimesTen ODBC drivers.

-   Provide driver manager specific functions not provided by the TimesTen drivers.

# TTDM Performance Overhead

The performance overhead of using TTDM has been measured in various configurations on a number of platforms. For the platforms evaluated, the results were as follows.

For direct mode connections the overhead of TTDM typically ranges from 2% to 5%.

For remote client/server connections the overhead of TTDM typically ranges from 1% to 3%.

For most real world applications, the difference in performance between using TTDM and linking directly with a specific TimesTen driver is negligible.

# TimesTen Version and O/S Support

This version of TTDM currently supports the following TimesTen versions:

**TimesTen 18.1.4.9.0 and later**   - 64 bit only

O/S platforms currently supported are:

- Linux x86-64

- AIX

- Solaris SPARC

- Solaris x86-64

# Building and Running TTDM Based Applications

## Source code

No source code changes are needed in order to use TTDM. TTDM based applications should include **timesten.h**, just like regular TimesTen applications.

If your application uses XLA then you should continue to also include **tt\_xla.h** in your code.

If your application uses the TimesTen Utility Library then you should continue to also include **ttutillib.h** and **ttutil.h** as appropriate.

## Linking changes

Currently you will be linking with one of the TimesTen ODBC libraries and maybe also with the TimesTen Utility Library, or possibly with some other driver manager library.

You should change the linker & library options in your Makefile(s) to instead link **only** with the TTDM library (**libttdrvmgr.so**). For example, on Unix/Linux platforms the linker option required will be **-lttdrvmgr**. Do **not** include **any** of the other TimesTen libraries, or any third party driver manager libraries, when linking.

## Runtime changes

At run time the TTDM shared library, plus all the other TimesTen shared libraries, must be located in directories defined in LD\_LIBRARY\_PATH (or its equivalent for your platform). This is most easily accomplished by using the script that TimesTen provides to set the environment (**\<instance_home\>/bin/ttenv.[c]sh**).

# TTDM ODBC Extensions

## SQLGetConnectionOption[W] and SQLGetConnectAttr[W]

TTDM provides an extension to the ODBC **SQLGetConnectOption\[W\]()** and **SQLGetConnectAttr\[W\]()** functions. If you pass the value **TT_TTDM\_CONNECTION\_TYPE** for the **fOption/Attribute** parameter and a pointer to a SQLINTEGER for the **pvParam/ValuePtr** parameter, as follows:

````
SQLINTEGER connType = 0;

rc = SQLGetConnectAttr(hdbc, TT_TTDM_CONNECTION_TYPE, &connType, SQL_IS_INTEGER, NULL);
````

then on successful return (rc == SQL\_SUCCESS), **connType** will contain a value that indicates the type of connection represented by **hdbc** as follows:

**TT\_TTDM\_CONN\_NONE** - the hdbc is not currently connected

**TT\_TTDM\_CONN\_DIRECT** - the hdbc is connected in direct mode

**TT\_TTDM\_CONN\_CLIENT** - the hdbc is connected in client/server mode

## SQLGetEnvAttr

TTDM also provides some extensions to the ODBC **SQLGetEnvAttr()** function.

If you pass the value **TT\_TTDM\_VERSION** for the **Attribute** parameter and a pointer to a SQLINTEGER for the **ValuePtr** parameter, as follows:

````
SQLCHAR ttdmver[21];

rc = SQLGetEnvAttr(henv, TT_TTDM_VERSION, (SQLPOINTER)ttdmver, sizeof(ttdmver), NULL);
````

Then the value returned in ttdmver indicates the version of the TTDM library that the application is using. The version is a string with a value of *'AA.BB.CC.DD.EE'* , for example '18.1.4.9.0'.

If you pass the value **TT\_TTDM\_CAPABILITIES** for the **Attribute** parameter and a pointer to a SQLINTEGER for the **ValuePtr** parameter, as follows:

````
SQLINTEGER ttdmcap = 0;

rc = SQLGetEnvAttr(henv, TT_TTDM_CAPABILITIES, (SQLPOINTER)&ttdmcap, 0, NULL);
````

Then the value returned in **ttdmcap** indicates what capabilities are currently available via TTDM. The returned value is a bitwise OR of the following constants:

**TT\_TTDM\_CLIENT**  - Client driver functions are available

**TT\_TTDM\_DIRECT**  - Direct driver functions are available

**TT\_TTDM\_XLA**     - XLA functions are available

**TT\_TTDM\_ROUTING** - Routing API functions are available

**TT\_TTDM\_UTILITY** - Utility API functions are available

**TT\_TTDM\_APIVER** - API function versioning is active 

The available capabilities depend both on how TTDM was built (some capabilities can be disabled at build time), and what is actually currently available based on the TimesTen environment under which the TTDM based application is being executed.

These extensions can also be used by an application to programatically determine if it is using TTDM or if it is linked directly with one of the TimesTen driver libraries. When using any of these extensions, a return of SQL\_ERROR means that the program is not using TTDM.

## API function versioning

Very occasionally, usually in a new TimesTen major release, new API functions may be added to, or removed from, one of the APIs that are mediated by TTDM (ODBC, XLA, Routing, Utility). Although this is not a common occurrence, it does have some implications for applications in terms of cross release comptibility. 

For example:

- If an API function is removed in TimesTen release Y, an application that calls that function and which was linked against TimesTen release X will not be able to run against TimesTen release Y or later; the binary will not even be able to start as it will incur a dynamic linking error on startup.

- If an API function is added in TimesTen release Y, an application that calls that function and which was linked against TimesTen release Y will not be able to run against TimesTen release X or earlier; the binary will not even be able to start as it will incur a dynamic linking error on startup.

TTDM provides some mitigation to reduce this impact. API functions for all TimesTen releases from the first release that includes TTDM onwards will be present in, and exported by, TTDM. If an application calls an API function that is not supported by the specific TimesTen release that it is running against then a meaningful and specific error code and message isreturned by TTDM. This feature may help to insulate applications from API changes over time.

# TTDM Specific Errors & Warnings

TTDM can return any of the standard TimesTen errors and warnings. In some cases TTDM may return the following additional native errors and warnings, in conjunction with the appropriate SQLSTATE.

## Errors

  **tt\_ErrDMNoMemory** - TTDM was unable to allocate some required memory.
  
  **tt\_ErrDMDriverLoad** - TTDM was unable to dynamically load a required library (direct mode library, client library or utility library).
  
  **tt\_ErrDMNotDisconnected** - An attempt was made to call SQLFreeConnect() on a connection that is still connected. Disconnect the connection first by calling SQLDisconnect().
  
  **tt\_ErrDMInvalidArg** - TTDM detected an invalid argument passed to a function.
  
  **tt\_ErrDMApiVersion** - The application called an API versioned function that does not exist in the version of the underlying TimesTen driver that is currently in use.

## Warnings

No additional warnings are currently defined.

# Limitations and Restrictions

TTDM does not support, and hence does not export, the driver manager functions **SQLDrivers[W]** and **SQLDataSources[W]**.
