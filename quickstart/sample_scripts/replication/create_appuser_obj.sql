--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

drop table customers;

create table customers (
   cust_number number,
   first_name  varchar2(12) not null,
   last_name   varchar2(12) not null,
   address     varchar2(100) not null,
   primary key (cust_number));

insert into customers values (3700,'Peter','Burchard','882 Osborne Avenue, Boston, MA 02122');
insert into customers values (1121,'Saul','Mendoza','721 Stardust Street, Mountain View, CA 94043');

drop table orders;

create table orders (
   order_number number   not null,
   cust_number  number   not null,
   prod_number  char(10) not null,
   order_date   date     not null,
   primary key (order_number),
   foreign key (cust_number) references customers (cust_number));

insert into ORDERS values (6853036,3700,'0028616731',to_date('2008-04-05','yyyy-mm-dd'));
insert into ORDERS values (6853041,3700,'0198612710',to_date('2009-01-12','yyyy-mm-dd'));
insert into ORDERS values (6853169,1121,'0003750299',to_date('2008-08-01','yyyy-mm-dd'));
insert into ORDERS values (6853174,1121,'0789428741',to_date('2008-10-25','yyyy-mm-dd'));
insert into ORDERS values (6853179,1121,'0198612583',to_date('2009-02-02','yyyy-mm-dd'));

commit

/
