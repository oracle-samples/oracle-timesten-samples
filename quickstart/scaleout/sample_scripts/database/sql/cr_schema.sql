--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

CREATE TABLE account_type
(
    type            CHAR(1) NOT NULL PRIMARY KEY,
    description     VARCHAR2(100) NOT NULL
)
DUPLICATE;

CREATE TABLE account_status
(
    status          NUMBER(2,0) NOT NULL PRIMARY KEY,
    description     VARCHAR2(100) NOT NULL
)
DUPLICATE;

CREATE TABLE customers
(
    cust_id            NUMBER(10,0) NOT NULL PRIMARY KEY,
    first_name         VARCHAR2(30) NOT NULL,
    last_name          VARCHAR2(30) NOT NULL,
    addr1              VARCHAR2(64),
    addr2              VARCHAR2(64),
    zipcode            VARCHAR2(5),
    member_since       DATE NOT NULL
)
DISTRIBUTE BY HASH;

CREATE TABLE accounts
(
    account_id         NUMBER(10,0) NOT NULL PRIMARY KEY,
    phone              VARCHAR2(16) NOT NULL,
    account_type       CHAR(1) NOT NULL,
    status             NUMBER(2,0) NOT NULL,
    current_balance    NUMBER(10,2) NOT NULL,
    prev_balance       NUMBER(10,2) NOT NULL,
    date_created       DATE NOT NULL,
    cust_id            NUMBER(10,0) NOT NULL,
    CONSTRAINT fk_customer
        FOREIGN KEY (cust_id)
            REFERENCES customers(cust_id),
    CONSTRAINT fk_acct_type
        FOREIGN KEY (account_type)
            REFERENCES account_type(type),
    CONSTRAINT fk_acct_status
        FOREIGN KEY (status)
            REFERENCES account_status(status)
)
DISTRIBUTE BY REFERENCE (fk_customer);

CREATE TABLE transactions
(
    transaction_id      NUMBER(10,0) NOT NULL,
    account_id          NUMBER(10,0) NOT NULL ,
    transaction_ts      TIMESTAMP NOT NULL,
    description         VARCHAR2(60),
    optype              CHAR(1) NOT NULL,
    amount              NUMBER(6,2) NOT NULL,
    PRIMARY KEY (account_id, transaction_id, transaction_ts),
    CONSTRAINT fk_accounts
        FOREIGN KEY (account_id)
            REFERENCES accounts(account_id)
)
DISTRIBUTE BY REFERENCE (fk_accounts);

CREATE SEQUENCE txn_seq CACHE 100 BATCH 1000000;

