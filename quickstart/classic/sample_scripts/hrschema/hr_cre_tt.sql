--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

#hr_cre_tt.sql - creates the table definitions

CREATE TABLE regions
    ( region_id      NUMBER NOT NULL PRIMARY KEY
    , region_name    VARCHAR2(25)
    );

CREATE TABLE countries
    ( country_id      CHAR(2) NOT NULL PRIMARY KEY
    , country_name    VARCHAR2(40)
    , region_id       NUMBER
    );

ALTER TABLE countries
ADD CONSTRAINT countr_reg_fk
                FOREIGN KEY (region_id)
                 REFERENCES regions(region_id)
    ;

CREATE TABLE locations
    ( location_id    NUMBER(4) PRIMARY KEY
    , street_address VARCHAR2(40)
    , postal_code    VARCHAR2(12)
    , city       VARCHAR2(30) NOT NULL
    , state_province VARCHAR2(25)
    , country_id     CHAR(2)
    ) ;

ALTER TABLE locations
     ADD CONSTRAINT loc_c_id_fk
                     FOREIGN KEY (country_id)
                      REFERENCES countries(country_id)
     ;

CREATE SEQUENCE locations_seq
 START WITH     3300
 INCREMENT BY   100
 MAXVALUE       9900;

CREATE TABLE departments
    ( department_id    NUMBER(4) PRIMARY KEY
    , department_name  VARCHAR2(30) NOT NULL
    , manager_id       NUMBER(6)
    , location_id      NUMBER(4)
    ) ;

ALTER TABLE departments
     ADD CONSTRAINT dept_loc_fk
                     FOREIGN KEY (location_id)
                      REFERENCES locations (location_id)
     ;

CREATE SEQUENCE departments_seq
 START WITH     280
 INCREMENT BY   10
 MAXVALUE       9990;

CREATE TABLE jobs
    ( job_id         VARCHAR2(10) PRIMARY KEY
    , job_title      VARCHAR2(35) NOT NULL
    , min_salary     NUMBER(6)
    , max_salary     NUMBER(6)
    ) ;

CREATE TABLE employees
    ( employee_id    NUMBER(6) PRIMARY KEY
    , first_name     VARCHAR2(20)
    , last_name      VARCHAR2(25) NOT NULL
    , email          VARCHAR2(25) NOT NULL UNIQUE
    , phone_number   VARCHAR2(20)
    , hire_date      DATE NOT NULL
    , job_id         VARCHAR2(10) NOT NULL
    , salary         NUMBER(8,2)
    , commission_pct NUMBER(2,2)
    , manager_id     NUMBER(6)
    , department_id  NUMBER(4)
    ) ;

ALTER TABLE employees
    ADD CONSTRAINT   emp_dept_fk
                      FOREIGN KEY (department_id)
                       REFERENCES departments;
ALTER TABLE employees
   ADD CONSTRAINT    emp_job_fk
                      FOREIGN KEY (job_id)
                       REFERENCES jobs (job_id);

CREATE SEQUENCE employees_seq
 START WITH     207
 INCREMENT BY   1;

CREATE TABLE job_history
    ( employee_id   NUMBER(6) NOT NULL
    , start_date    DATE NOT NULL
    , end_date      DATE NOT NULL
    , job_id        VARCHAR2(10) NOT NULL
    , department_id NUMBER(4)
    , PRIMARY KEY (employee_id, start_date)
    ) ;

ALTER TABLE job_history
     ADD CONSTRAINT  jhist_job_fk
                      FOREIGN KEY (job_id)
                       REFERENCES jobs;
ALTER TABLE job_history
    ADD CONSTRAINT   jhist_emp_fk
                      FOREIGN KEY (employee_id)
                       REFERENCES employees;
ALTER TABLE job_history
    ADD CONSTRAINT   jhist_dept_fk
                      FOREIGN KEY (department_id)
                       REFERENCES departments
    ;

CREATE VIEW emp_details_view
  (employee_id,
   job_id,
   manager_id,
   department_id,
   location_id,
   country_id,
   first_name,
   last_name,
   salary,
   commission_pct,
   department_name,
   job_title,
   city,
   state_province,
   country_name,
   region_name)
AS SELECT
  e.employee_id,
  e.job_id,
  e.manager_id,
  e.department_id,
  d.location_id,
  l.country_id,
  e.first_name,
  e.last_name,
  e.salary,
  e.commission_pct,
  d.department_name,
  j.job_title,
  l.city,
  l.state_province,
  c.country_name,
  r.region_name
FROM
  employees e,
  departments d,
  jobs j,
  locations l,
  countries c,
  regions r
WHERE e.department_id = d.department_id
  AND d.location_id = l.location_id
  AND l.country_id = c.country_id
  AND c.region_id = r.region_id
  AND j.job_id = e.job_id;


