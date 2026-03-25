--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

set echo off
spool build_db.log

PROMPT
PROMPT Creating users in the sample database
PROMPT 

@create_admin.sql

REM
REM Connect as user ADM to create the other sample database users
REM

connect adding "uid=adm;pwd=&&adm_password" as adm;
@create_users.sql

REM
REM Connect as user APPUSER and create schema objects
REM

connect adding "uid=appuser;pwd=&&appuser_password" as appuser;
PROMPT
PROMPT Creating schema objects in the sample database
PROMPT 

REM
REM Create tables, sequences and indexes 
REM

@create_obj.sql

REM
REM Create PL/SQL program units
REM

@create_obj_plsql.sql

REM
REM Grant object privileges
REM

@grant_obj_privs.sql

REM
REM Pouplate tables and update statistics
REM

@populate_data.sql

PROMPT
PROMPT Sample database created 
PROMPT
spool off
