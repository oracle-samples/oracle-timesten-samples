--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

REM
REM Create a demo application user appuser for running the demo programs 
REM

PROMPT Please choose a password for the applicaton user 'appuser' 
ACCEPT password char prompt 'Enter password: ' hide 
CREATE USER appuser IDENTIFIED BY '&&password';
GRANT CREATE SESSION TO appuser; 
GRANT CREATE TABLE, CREATE SEQUENCE, CREATE PROCEDURE TO appuser;
DEFINE appuser_password=&&password;
UNDEFINE password;
 
REM
REM Create a XLA user xlauser for running the xla demos
REM

prompt Please choose a password for the XLA subscriber user 'xlauser' 
ACCEPT password char prompt 'Enter password: ' hide 
CREATE USER xlauser IDENTIFIED BY '&password';
GRANT XLA, CREATE SESSION TO xlauser;

REM
REM Return all database users in the sample database
REM

SELECT username, user_id, TO_CHAR(created,'YYYY-MM-DD HH24:MI:SS') 
FROM   sys.all_users;
PROMPT

REM
REM Return the system privileges assigned to each user
REM

SELECT * FROM sys.dba_sys_privs;

