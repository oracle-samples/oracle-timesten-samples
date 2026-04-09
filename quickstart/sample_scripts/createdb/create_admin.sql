--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

SET DEFINE ON

REM
REM Create a database administration user adm for managing the 
REM TimesTen sample database
REM

Prompt Please choose a password for the database administration user 'adm' 
accept password char prompt 'Enter password: ' hide 
CREATE USER adm IDENTIFIED BY '&&password';
GRANT ADMIN TO adm;
DEFINE adm_password=&&password;
UNDEFINE password;
