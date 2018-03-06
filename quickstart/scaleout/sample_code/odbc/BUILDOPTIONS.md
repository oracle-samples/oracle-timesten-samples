Copyright (c) 1997, 2018, Oracle and/or its affiliates. All rights reserved.

# Compile time program options

When building this sample program you can control some optional features at build time by defining additional macros using the OPTDEFS variable in the Makefile. For example, to enable DEBUG mode and switch to using the older ODBC 2.5 API you could use:

OPTDEFS = -DDEBUG -DUSEODBC25

The specific features that can be controlled are as follows:

**DEBUG**

By default debug mode is not enabled for performance reasons. If you wish to be able to obtain a much more detailed execution log (essentially a trace of program execution including ODBC API calls, data values etc.) then you can enable this by adding -DDEBUG to the makefile OPTDEFS variable. 

When running the program you can activate debug mode using the '-debug' command line flag. See the online help for information on that flag (the help info for this flag is only displayed if the program has been built with debug enabled).

**USEODBC25**

TimesTen 18.1 introduces support fo the ODBC 3.5 API compared to previous versions of TimesTen which only supported the older ODBC 2.5 API. By default this sample program uses the newer ODBC 3.5 API but it can also be built to use ODBC 2.5 by specifying -DUSEODBC25 in the makefile's OPTDEFS variable.

The main reasons for including this capability are:

1.    The program can be used with older TimesTen versions if desired.

2.    The source code serves as a useful comparison between ODBC 2.5 and 3.5       (at least for the functions used by this program) and may also serve as a helpful guide for those wishing to migrate from ODBC 2.5 to 3.5.

**USE_TRUNCATE**

If this macro is defined then the clear down of the TRANSACTIONS table is done using a TRUNCATE TABLE statement otherwise it is done using DELETE FROM (the default).

**SPECIAL_FEATURES**

The program supports the following spcial features, which can be enabled by adding -DSPECIAL_FEATURES to the makefile OPTDEFS variable:

_Always commit read-only transactions:_

TimesTen does not require that applications commit SQL operations / transactions that have only performed read (query) operations. The sample program follows this convention. If you build with SPECIAL_FEATURES enabled then you have the option to use an additional command line flag, **\-commitrotxn**. If this is specified at run-time then all transactions will be committed on completion regardless of whether they made any database updates or not.