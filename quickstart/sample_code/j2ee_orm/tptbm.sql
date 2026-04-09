--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

DROP TABLE SCOTT.TPTBM;
DROP USER SCOTT;

CREATE USER SCOTT IDENTIFIED BY 'tiger';
GRANT CONNECT, CREATE TABLE TO SCOTT;

CREATE TABLE SCOTT.TPTBM (
    VPN_ID INT NOT NULL,
    VPN_NB INT NOT NULL,
    DIRECTORY_NB CHAR (10) NOT NULL,
    LAST_CALLING_PARTY CHAR (10) NOT NULL,
    DESCR CHAR (100) NOT NULL,
    PRIMARY KEY (VPN_ID, VPN_NB))
    UNIQUE HASH ON (VPN_ID, VPN_NB) PAGES = 400;


