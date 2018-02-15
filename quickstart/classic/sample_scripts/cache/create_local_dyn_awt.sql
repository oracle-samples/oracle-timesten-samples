--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

create dynamic asynchronous writethrough cache group d_awt from 
hr.employees ( employee_id number (6) not null,
               first_name    varchar2(20),
               last_name     varchar2(25) not null,
               email         varchar2(25) not null,
               phone_number  varchar2(20),
               hire_date     date not null,
               job_id        varchar2(10) not null,
               salary        number (8,2),
               commission_pct number (2,2),
               manager_id    number (6),
               department_id number(4),
   primary key (employee_id)),
hr.job_history (employee_id  number(6) not null,
                start_date   date  not null,
                end_date     date  not null,
                job_id       varchar2(10) not null,
                department_id number(4), 
   primary key (employee_id,start_date),
   foreign key (employee_id) 
   references hr.employees (employee_id));

