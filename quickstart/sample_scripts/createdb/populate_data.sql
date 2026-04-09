--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

REM
REM Populate tables for PL/SQL, OCI and PRO*C sample programs
REM

DELETE FROM account ;
INSERT INTO account VALUES (1,1000.00) ;
INSERT INTO account VALUES (2,2000.00) ;
INSERT INTO account VALUES (3,1500.00) ;
INSERT INTO account VALUES (4,6500.00) ;
INSERT INTO account VALUES (5,500.00) ;

DELETE FROM action ;
INSERT INTO action VALUES (3,'u',599,null,sysdate) ;
INSERT INTO action VALUES (6,'i',20099,null, sysdate) ;
INSERT INTO action VALUES (5,'d',null,null, sysdate) ;
INSERT INTO action VALUES (7,'u',1599,null, sysdate) ;
INSERT INTO action VALUES (1,'i',399,null,sysdate) ;
INSERT INTO action VALUES (9,'d',null,null,sysdate) ;
INSERT INTO action VALUES (10,'x',null,null,sysdate) ;

DELETE FROM data_table ;
INSERT INTO data_table VALUES (1, 10, 167, 17) ;
INSERT INTO data_table VALUES (1, 16, 223, 35) ;
INSERT INTO data_table VALUES (2, 34, 547, 2) ;
INSERT INTO data_table VALUES (3, 23, 318, 11) ;
INSERT INTO data_table VALUES (1, 17, 266, 15) ;
INSERT INTO data_table VALUES (1, 20, 117, 9) ;

DELETE FROM dept;
INSERT INTO dept VALUES (10,'ACCOUNTING','NEW YORK');
INSERT INTO dept VALUES (20,'RESEARCH','DALLAS');
INSERT INTO dept VALUES (30,'SALES','CHICAGO');
INSERT INTO dept VALUES (40,'OPERATIONS','BOSTON');

DELETE FROM emp ; 
INSERT INTO emp VALUES (7369,'SMITH','CLERK',7902,to_date('17-12-1980','dd-mm-yyyy'),800,NULL,20) ;
INSERT INTO emp VALUES (7499,'ALLEN','SALESMAN',7698,to_date('20-2-1981','dd-mm-yyyy'),1600,300,30) ; 
INSERT INTO emp VALUES (7521,'WARD','SALESMAN',7698,to_date('22-2-1981','dd-mm-yyyy'),1250,500,30) ;
INSERT INTO emp VALUES (7566,'JONES','MANAGER',7839,to_date('2-4-1981','dd-mm-yyyy'),2975,NULL,20) ;
INSERT INTO emp VALUES (7654,'MARTIN','SALESMAN',7698,to_date('28-9-1981','dd-mm-yyyy'),1250,1400,30) ;
INSERT INTO emp VALUES (7698,'BLAKE','MANAGER',7839,to_date('1-5-1981','dd-mm-yyyy'),2850,NULL,30) ;
INSERT INTO emp VALUES (7782,'CLARK','MANAGER',7839,to_date('9-6-1981','dd-mm-yyyy'),2450,NULL,10) ;
INSERT INTO emp VALUES (7788,'SCOTT','ANALYST',7566,to_date('13-JUL-1987','dd-mon-yyyy')-numtodsinterval(85,'day'),3000,NULL,20) ;
INSERT INTO emp VALUES (7839,'KING','PRESIDENT',NULL,to_date('17-11-1981','dd-mm-yyyy'),5000,NULL,10) ;
INSERT INTO emp VALUES (7844,'TURNER','SALESMAN',7698,to_date('8-9-1981','dd-mm-yyyy'),1500,0,30) ;
INSERT INTO emp VALUES (7876,'ADAMS','CLERK',7788,to_date('13-JUL-1987','dd-mon-yyyy')-numtodsinterval(51,'day'),1100,NULL,20) ;
INSERT INTO emp VALUES (7900,'JAMES','CLERK',7698,to_date('3-12-1981','dd-mm-yyyy'),950,NULL,30) ;
INSERT INTO emp VALUES (7902,'FORD','ANALYST',7566,to_date('3-12-1981','dd-mm-yyyy'),3000,NULL,20) ;
INSERT INTO emp VALUES (7934,'MILLER','CLERK',7782,to_date('23-1-1982','dd-mm-yyyy'),1300,NULL,10) ;

DELETE FROM inventory2 ;
INSERT INTO inventory2 VALUES (1234, 'TENNIS RACKET', 3) ;
INSERT INTO inventory2 VALUES (8159, 'GOLF CLUB', 4) ;
INSERT INTO inventory2 VALUES (2741, 'SOCCER BALL', 2) ;

DELETE FROM purchase_record ;

DELETE FROM ratio ;

DELETE FROM result_table ;
INSERT INTO result_table VALUES (130, 70, 87) ;
INSERT INTO result_table VALUES (131, 77, 194) ;
INSERT INTO result_table VALUES (132, 73, 0) ;
INSERT INTO result_table VALUES (133, 81, 98) ;

DELETE FROM temp ;

REM
REM Populate tables for Java sample programs
REM

INSERT INTO product VALUES (100, 'Beef Flavor', 1.10, 0.25, 'Tasty artificial beef flavor crystals',NULL, 'Not for use in soft drinks');
INSERT INTO product VALUES (101, 'Chicken Flavor', 0.95, 0.25, 'Makes everything taste like chicken',NULL, 'Not for use in soft drinks');
INSERT INTO product VALUES (102, 'Garlic extract', 0.10, 0.22, 'Pure essence of garlic',NULL, 'Keeps vampires away');
INSERT INTO product VALUES (103, 'Oat bran', 4.00, 5.055, 'All natural oat bran',NULL, 'May reduce cholesterol');

INSERT INTO customer VALUES (1, 'South', 'Fiberifics', '123 any street');
INSERT INTO customer VALUES (2, 'West', 'Natural Foods Co.', '5150 Johnson Rd');
INSERT INTO customer VALUES (3, 'North', 'Happy Food Inc.', '4004 Happy Lane');
INSERT INTO customer VALUES (4, 'East', 'Nakamise', '6-6-2 Nishi Shinjuku'); 

INSERT INTO orders VALUES (1001, 1, sysdate, null, null);
INSERT INTO order_item VALUES (1001, 103, 50);

INSERT INTO orders VALUES (1002, 2, '2003-04-11 09:17:32.148000', null, null);
INSERT INTO order_item VALUES (1002, 102, 6000);
INSERT INTO order_item VALUES (1002, 103, 500);

INSERT INTO orders VALUES (1003, 3, '2003-03-09 08:15:12.100000', null, null);
INSERT INTO order_item VALUES (1003, 101, 40);
INSERT INTO order_item VALUES (1003, 102, 500);

INSERT INTO orders VALUES (1008, 3, sysdate, null, null);INSERT INTO order_item VALUES (1008, 102, 300);

INSERT INTO orders VALUES (1009, 4, sysdate, null, null);INSERT INTO order_item VALUES (1009, 103, 79);

INSERT INTO inventory VALUES (100, 'London', 10000);
INSERT INTO inventory VALUES (101, 'New York', 5000);
INSERT INTO inventory VALUES (102, 'Gilroy', 1000);
INSERT INTO inventory VALUES (103, 'Gilroy', 4000);

REM
REM Update statistics for all tables
REM

CALL ttOptUpdateStats;
