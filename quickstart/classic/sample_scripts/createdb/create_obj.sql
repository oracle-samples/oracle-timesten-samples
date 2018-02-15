--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

REM
REM Create mydata table for xla demos
REM

REM DROP TABLE mydata;

CREATE TABLE mydata ( 
   name    CHAR(30 BYTE) NOT NULL,
   address VARCHAR2(50 BYTE) INLINE,
   custno  NUMBER(38),
   service NCHAR(20),
   tstamp  TIMESTAMP(6),
   price   NUMBER(10,2),
   primary key (name))
UNIQUE HASH ON (name) PAGES = 100;

REM
REM Create tables for PLSQL, OCI and PRO*C sample programs
REM

REM DROP TABLE account;

CREATE TABLE account(
    account_id  number(4) not null,
    bal         number(11,2));

create unique index account_index on account (account_id);

REM DROP TABLE action;

CREATE TABLE action(
    account_id  number(4) not null,
    oper_type   char(1) not null,
    new_value   number(11,2),
    status      char(45),
    time_tag    date not null);

REM DROP TABLE data_table;

CREATE TABLE data_table(
    exper_num  number(2),
    n1         number(5),
    n2         number(5),
    n3         number(5));

REM DROP TABLE emp;
REM DROP TABLE dept;

CREATE TABLE dept(
    deptno     number(2) not null primary key,
    dname      varchar2(14),
    loc        varchar2(13));

CREATE TABLE emp(
    empno      number(4) not null primary key,
    ename      varchar2(10),
    job        varchar2(9),
    mgr        number(4),
    hiredate   date,
    sal        number(7,2),
    comm       number(7,2),
    deptno     number(2),
    foreign key (deptno) references dept(deptno));

REM
REM PLSQL sample table inventory renamed to inventory2
REM to avoid conflict with inventory used in Java sample programs 
REM

REM DROP TABLE inventory2;

CREATE TABLE inventory2(
    prod_id     number(5) not null,
    product     char(15),
    quantity    number(5));

REM DROP TABLE purchase_record;

CREATE TABLE purchase_record(
    mesg        char(45),
    purch_date  date);

REM DROP TABLE ratio;

CREATE TABLE ratio(
    sample_id  number(3) not null,
    ratio      number);

REM DROP TABLE result_table;

CREATE TABLE result_table(
    sample_id  number(3) not null,
    x          number,
    y          number);

REM DROP TABLE temp;

CREATE TABLE temp(
    num_col1   number(9,4),
    num_col2   number(9,4),
    char_col   char(55));

REM
REM Sequence is created to get unique account numbers for the
REM different account types.
REM

REM DROP SEQUENCE orderID;
REM DROP MATERIALIZED VIEW order_summary;
REM DROP TABLE order_item;
REM DROP SEQUENCE orderID;
REM DROP TABLE orders;
REM DROP TABLE inventory;
REM DROP TABLE product;
REM DROP TABLE customer;

CREATE TABLE product (
  prod_num    integer       not null primary key,
  name        varchar(40)   not null,
  price       decimal(10,3) not null,  
  ship_weight float         not null,
  description varchar(255)  null,
  picture     varbinary(10240) null,
  notes       varchar(1024) null
)
UNIQUE HASH ON (prod_num) PAGES=100;

CREATE TABLE inventory (
  prod_num   integer        not null,
  warehouse  char(15)       not null,
  quantity   integer        not null,
  primary key (prod_num, warehouse),
  foreign key (prod_num) references product (prod_num)
)
UNIQUE HASH ON (prod_num, warehouse) PAGES=300;

CREATE TABLE customer (
  cust_num          integer       not null primary key,
  region            char(5)       not null,
  name              varchar(80), 
  address           varchar(255)  not null
) 
UNIQUE HASH ON (cust_num) PAGES=100;

CREATE TABLE orders (
  ord_num      integer        not null primary key,
  cust_num     integer        not null,
  when_placed  timestamp      not null,
  when_shipped timestamp,
  notes        varchar(1024),
  foreign key (cust_num) references customer (cust_num)
)
UNIQUE HASH ON (ord_num) PAGES=1000;

CREATE SEQUENCE orderID MINVALUE 300;

CREATE TABLE order_item (
  ord_num    integer        not null,
  prod_num   integer        not null,
  quantity   integer        not null,
  primary key (ord_num, prod_num),
  foreign key (prod_num) references product (prod_num),
  foreign key (ord_num) references orders (ord_num)
)
UNIQUE HASH ON (ord_num, prod_num) PAGES=500;

REM
REM Create mytable for the sample.cpp 
REM

CREATE TABLE mytable (
   name char(10) not null primary key,
   i tt_integer)
   UNIQUE HASH ON (name) PAGES=100;

CREATE TABLE icf (
  i tt_integer not null,
  c char(32) not null,
  f binary_double not null);

CREATE TABLE abc (
  double_ binary_double not null,
  real_ binary_float not null,
  tinyint_ tt_tinyint not null,
  smallint_ tt_smallint not null,
  time_ time,
  date_ tt_date,
  timestamp_ timestamp not null,
  binary_ binary(5),
  varbinary_ varbinary(10),
  varchar2_ varchar2(132),
  number_ number(11),
  number_two_ number(17,5),
  nchar_   nchar(10),
  nvarchar2_ nvarchar2(100)
);

REM
REM Create table to include sample LOB data
REM

CREATE TABLE print_media (
        product_id    number(6),
        ad_id         number(6),
        ad_composite  blob,
        ad_sourcetext clob,
        ad_finaltext  clob,
        ad_fltextn    nclob,
        ad_photo      blob);

REM
REM Return a list of tables created in the APPUSER schema
REM
tables;
