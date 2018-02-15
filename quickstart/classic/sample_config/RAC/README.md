Copyright (c) 1998, 2017, Oracle and/or its affiliates. All rights reserved.

This directory contains Oracle SQL script, checkRAC.sql, to help verify that TAF and FAN have been setup appropriately for TimesTen's IMDB Cache. Specifically, the script checks for:

- The ability to connect to a RAC instance successfully

- The TAF setting to be one of none, select or session

- If FAN is enabled.

The script is intended to be used with SQL*Plus.