--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

create dynamic readonly cache group d_ro 
   autorefresh interval 1 second
from 
hr.jobs ( job_id       varchar2(10) not null primary key,
          job_title    varchar2(35) not null,
          min_salary   number(6),
          max_salary   number(6));
  
