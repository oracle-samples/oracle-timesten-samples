--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

create asynchronous writethrough cache group awt from 
hr.regions    ( region_id    number  not null primary key,
                region_name  varchar2(25)),
hr.countries  ( country_id   char (2) not null primary key,
                country_name varchar2(40),
                region_id    number,
   foreign key (region_id)
   references hr.regions (region_id)),
hr.locations  ( location_id  number (4) not null primary key,
                street_address varchar2 (40),
                postal_code    varchar2 (12),   
                city           varchar2 (30) not null,
                state_province varchar2 (25),
                country_id     char(2),
   foreign key (country_id)
   references hr.countries (country_id));

