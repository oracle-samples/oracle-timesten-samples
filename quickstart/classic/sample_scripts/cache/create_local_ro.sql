--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

create readonly cache group ro 
   autorefresh 
       interval 5 seconds
       mode incremental
from
    hr.departments (
            department_id   number(4) not null primary key,
            department_name varchar2(30) not null,
            manager_id      number(6),
            location_id     number(4));
  
