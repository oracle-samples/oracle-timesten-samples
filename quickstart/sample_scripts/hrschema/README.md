Copyright (c) 1998, 2017, Oracle and/or its affiliates. All rights reserved.

# Readme for HrSchema demo

## 1. Introduction

The purpose of the files in the hrschema directory is to create and populate the HR (Human Resources) sample schema. The HR sample schema is used for many of the examples in the 
TimesTen documentation and training materials. The TimesTen HR sample schema is based on the Oracle Database 10g HR Sample Schema. The HR objects in TimesTen are created under the current user and not under a user called HR as in an Oracle database.

The HR sample schema illustrates compatibility with the Oracle Database 10G HR Sample Schema. It is not meant to illustrate best practices for using TimesTen for optimal performance.

## 2. Creating the HR sample schema

You can create the HR sample schema from within ttIsql. After you connect to your data store, use the ttIsql RUN command to execute the following three files, in the order specified:

**hr\_cre_tt.sql** - Creates the tables,sequences and view.
	           
**hr\_idx_tt.sql** - Creates the indexes.
		   
**hr\_popul_tt.sql** - Populates the tables with data.
		     
For example:

````
Command> run hr_cre_tt.sql
Command> run hr_idx_tt.sql
Command> run hr_popul_tt.sql
````

## 3. Information about the HR sample schema

The HR sample schema contains data about the Human Resources department of a fictitious company.

The Human Resource department tracks information about the company employees and facilities. Each employee has an identification number, email address, job identification code, salary, and manager. Some employees earn a commission in addition to their salary. This information is stored in the EMPLOYEES table.

The JOBS table is used to track information about jobs within the organization. Each job has an identification code, job title, and a minimum and maximum salary range for the job.

The JOB_HISTORY table is used for employees that change jobs. The start and end date of the former job is stored as well as the job identification number and the department.

The DEPARTMENTS table identifies departments within the company. Each department has a department ID and name. Each employee is assigned to a department. Each department is associated with one location.

The LOCATIONS table is used to store the location ID, street address, postal code, city, state or province, and country code for each location.

For each location, the COUNTRIES table is used to store information about the country name and the region where the country is located geographically.

The REGIONS table is used to store the region ID and the region name.

## 4. The HR sample schema objects

a. There are seven tables in the HR sample schema:

````
   REGIONS
   COUNTRIES
   LOCATIONS
   DEPARTMENTS
   JOBS
   EMPLOYEES
   JOB_HISTORY
````

b. There are three sequences in the HR sample schema:

````
   LOCATIONS_SEQ
   DEPARTMENTS_SEQ
   EMPLOYEES_SEQ
````

c. There is one view in the HR sample schema:

````
   EMP_DETAILS_VIEW
````

d. There are 20 indexes in the HR sample schema:

````
   COUNTRIES
   COUNTR_REG_FK
   DEPARTMENTS
   DEPT_LOC_FK                
   EMPLOYEES
   EMP_DEPT_FK
   EMP_JOB_FK
   EMP_MANAGER_IX
   EMP_NAME_IX  
   JHIST_DEPT_FK
   JHIST_EMP_FK
   JHIST_JOB_FK 
   JOBS
   JOB_HISTORY 
   LOCATIONS
   LOC_CITY_IX
   LOC_C_ID_FK
   LOC_STATE_PROVINCE_IX
   REGIONS
   TTUNIQUE_0 
````
                               
## 5. Describing the tables

You can use ttIsql to describe the created tables. For example, after you connect to the data store containing the HR sample schema objects, use the ttIsql DESCRIBE command:  

````
Command> describe regions;

Table SAMPLEUSER.REGIONS
  Columns:
   *REGION_ID                       NUMBER NOT NULL
    REGION_NAME                     VARCHAR2 (25) INLINE

1 table found.
(primary key columns are indicated with *)

Command> describe countries;

Table SAMPLEUSER.COUNTRIES:
  Columns:
   *COUNTRY_ID                      CHAR (2) NOT NULL
    COUNTRY_NAME                    VARCHAR2 (40) INLINE
    REGION_ID                       NUMBER

1 table found.
(primary key columns are indicated with *)

Command> describe locations;

Table SAMPLEUSER.LOCATIONS:
  Columns:
   *LOCATION_ID                     NUMBER (4) NOT NULL
    STREET_ADDRESS                  VARCHAR2 (40) INLINE
    POSTAL_CODE                     VARCHAR2 (12) INLINE
    CITY                            VARCHAR2 (30) INLINE NOT NULL
    STATE_PROVINCE                  VARCHAR2 (25) INLINE
    COUNTRY_ID                      CHAR (2)

1 table found.
(primary key columns are indicated with *)

Command> describe departments;

Table SAMPLEUSER.DEPARTMENTS:
  Columns:
   *DEPARTMENT_ID                   NUMBER (4) NOT NULL
    DEPARTMENT_NAME                 VARCHAR2 (30) INLINE NOT NULL
    MANAGER_ID                      NUMBER (6)
    LOCATION_ID                     NUMBER (4)

1 table found.
(primary key columns are indicated with *)

Command> describe jobs;

Table SAMPLEUSER.JOBS:
  Columns:
   *JOB_ID                          VARCHAR2 (10) INLINE NOT NULL
    JOB_TITLE                       VARCHAR2 (35) INLINE NOT NULL
    MIN_SALARY                      NUMBER (6)
    MAX_SALARY                      NUMBER (6)

1 table found.
(primary key columns are indicated with *)

Command> describe employees;

Table SAMPLEUSER.EMPLOYEES:
  Columns:
   *EMPLOYEE_ID                     NUMBER (6) NOT NULL
    FIRST_NAME                      VARCHAR2 (20) INLINE
    LAST_NAME                       VARCHAR2 (25) INLINE NOT NULL
    EMAIL                           VARCHAR2 (25) INLINE UNIQUE NOT NULL
    PHONE_NUMBER                    VARCHAR2 (20) INLINE
    HIRE_DATE                       DATE NOT NULL
    JOB_ID                          VARCHAR2 (10) INLINE NOT NULL
    SALARY                          NUMBER (8,2)
    COMMISSION_PCT                  NUMBER (2,2)
    MANAGER_ID                      NUMBER (6)
    DEPARTMENT_ID                   NUMBER (4)

1 table found.
(primary key columns are indicated with *)

Command> describe job_history;

Table SAMPLEUSER.JOB_HISTORY:
  Columns:
   *EMPLOYEE_ID                     NUMBER (6) NOT NULL
   *START_DATE                      DATE NOT NULL
    END_DATE                        DATE NOT NULL
    JOB_ID                          VARCHAR2 (10) INLINE NOT NULL
    DEPARTMENT_ID                   NUMBER (4)

1 table found.
(primary key columns are indicated with *)
````

## 6. Sequences and View definitions

You can use ttIsql to describe the sequences and views. For example, after you connect to the data store containing the HR sample schema objects, use the ttIsql commands:

````
Command> sequences;

Sequence SAMPLEUSER.DEPARTMENTS_SEQ:
  Minimum Value: 1
  Maximum Value: 9990
  Current Value: 280
  Increment:     10
  Cache:         20
  Cycle:         Off

Sequence SAMPLEUSER.EMPLOYEES_SEQ:
  Minimum Value: 1
  Maximum Value: 9223372036854775807
  Current Value: 207
  Increment:     1
  Cache:         20
  Cycle:         Off

Sequence SAMPLEUSER.LOCATIONS_SEQ:
  Minimum Value: 1
  Maximum Value: 9900
  Current Value: 3300
  Increment:     100
  Cache:         20
  Cycle:         Off

3 sequences found.

Command> views;

View SAMPLEUSER.EMP_DETAILS_VIEW is defined as:

CREATE VIEW SAMPLEUSER.EMP_DETAILS_VIEW (EMPLOYEE_ID, JOB_ID, MANAGER_ID, DEPARTMENT_ID, LOCATION_ID, COUNTRY_ID, FIRST_NAME, LAST_NAME, SALARY, COMMISSION_PCT, DEPARTMENT_NAME, JOB_TITLE, CITY, STATE_PROVINCE, COUNTRY_NAME, REGION_NAME) AS SELECT E.EMPLOYEE_ID, E.JOB_ID, E.MANAGER_ID, E.DEPARTMENT_ID, D.LOCATION_ID, L.COUNTRY_ID, E.FIRST_NAME, E.LAST_NAME, E.SALARY, E.COMMISSION_PCT, D.DEPARTMENT_NAME, J.JOB_TITLE, L.CITY, L.STATE_PROVINCE, C.COUNTRY_NAME, R.REGION_NAME FROM SAMPLEUSER.EMPLOYEES E, SAMPLEUSER.DEPARTMENTS D, SAMPLEUSER.JOBS J, SAMPLEUSER.LOCATIONS L, SAMPLEUSER.COUNTRIES C, SAMPLEUSER.REGIONS R WHERE ((((E.DEPARTMENT_ID = D.DEPARTMENT_ID) AND (D.LOCATION_ID = L.LOCATION_ID)) AND (L.COUNTRY_ID = C.COUNTRY_ID)) AND (C.REGION_ID = R.REGION_ID)) AND (J.JOB_ID = E.JOB_ID);

1 view found.
````

## 7. Detail on indexes created

You can use ttIsql to describe the index definitions. For example, after you connect to the data store containing the HR sample schema objects, use the ttIsql indexes command. (Assume SAMPLEUSER has created the objects):

````
Command> indexes regions%;

Indexes on table SAMPLEUSER.REGIONS:
  REGIONS: unique Range index on columns:
    REGION_ID
    (referenced by foreign key index COUNTR_REG_FK on table SAMPLEUSER.COUNTRIES)
  1 index found.

1 table found.

Command> indexes countries%;

Indexes on table SAMPLEUSER.COUNTRIES:
  COUNTRIES: unique Range index on columns:
    COUNTRY_ID
    (referenced by foreign key index LOC_C_ID_FK on table SAMPLEUSER.LOCATIONS)
  COUNTR_REG_FK: non-unique Range index on columns:
    REGION_ID
    (foreign key index references table SAMPLEUSER.REGIONS(REGION_ID))
  2 indexes found.

1 table found.

Command> indexes locations%;

Indexes on table SAMPLEUSER.LOCATIONS:
  LOCATIONS: unique Range index on columns:
    LOCATION_ID
    (referenced by foreign key index DEPT_LOC_FK on table SAMPLEUSER.DEPARTMENTS)
  LOC_CITY_IX: non-unique Range index on columns:
    CITY
  LO\C_C_ID_FK: non-unique Range index on columns:
    COUNTRY_ID
    (foreign key index references table SAMPLEUSER.COUNTRIES(COUNTRY_ID))
  LOC_STATE_PROVINCE_IX: non-unique Range index on columns:
    STATE_PROVINCE
  4 indexes found.

1 table found.

Command> indexes departments%;

Indexes on table SAMPLEUSER.DEPARTMENTS:
  DEPARTMENTS: unique Range index on columns:
    DEPARTMENT_ID
    (referenced by foreign key index EMP_DEPT_FK on table SAMPLEUSER.EMPLOYEES)
    (referenced by foreign key index JHIST_DEPT_FK on table SAMPLEUSER.JOB_HISTORY
)
  DEPT_LOC_FK: non-unique Range index on columns:
    LOCATION_ID
    (foreign key index references table SAMPLEUSER.LOCATIONS(LOCATION_ID))
  2 indexes found.

1 table found.

Command> indexes jobs%;

Indexes on table SAMPLEUSER.JOBS:
  JOBS: unique Range index on columns:
    JOB_ID
    (referenced by foreign key index EMP_JOB_FK on table SAMPLEUSER.EMPLOYEES)
    (referenced by foreign key index JHIST_JOB_FK on table SAMPLEUSER.JOB_HISTORY)
  1 index found.

1 table found.

Command> indexes employees%;

Indexes on table SAMPLEUSER.EMPLOYEES:
  EMPLOYEES: unique Range index on columns:
    EMPLOYEE_ID
    (referenced by foreign key index JHIST_EMP_FK on table SAMPLEUSER.JOB_HISTORY)
  TTUNIQUE_0: unique Range index on columns:
    EMAIL
  EMP_DEPT_FK: non-unique Range index on columns:
    DEPARTMENT_ID
    (foreign key index references table SAMPLEUSER.DEPARTMENTS(DEPARTMENT_ID))
  EMP_JOB_FK: non-unique Range index on columns:
    JOB_ID
    (foreign key index references table SAMPLEUSER.JOBS(JOB_ID))
  EMP_MANAGER_IX: non-unique Range index on columns:
    MANAGER_ID
  EMP_NAME_IX: non-unique Range index on columns:
    LAST_NAME
    FIRST_NAME
  6 indexes found.

1 table found.

Command> indexes job_history%;

Indexes on table SAMPLEUSER.JOB_HISTORY:
  JOB_HISTORY: unique Range index on columns:
    EMPLOYEE_ID
    START_DATE
  JHIST_DEPT_FK: non-unique Range index on columns:
    DEPARTMENT_ID
    (foreign key index references table SAMPLEUSER.DEPARTMENTS(DEPARTMENT_ID))
  JHIST_EMP_FK: non-unique Range index on columns:
    EMPLOYEE_ID
    (foreign key index references table SAMPLEUSER.EMPLOYEES(EMPLOYEE_ID))
  JHIST_JOB_FK: non-unique Range index on columns:
    JOB_ID
    (foreign key index references table SAMPLEUSER.JOBS(JOB_ID))
  4 indexes found.

1 table found.
````

## 8. Performing queries

You can use ttIsql to perform queries. For example, each table should have the following number of rows:

````
Command> select count (*) from regions;
< 4 >
1 row found.
Command> select count (*) from countries;
< 25 >
1 row found.
Command> select count (*) from locations;
< 23 >
1 row found.
Command> select count (*) from departments;
< 27 >
1 row found.
Command> select count (*) from jobs;
< 19 >
1 row found.
Command> select count (*) from employees;
< 107 >
1 row found.
Command> select count (*) from job\_history;
< 10 >
1 row found.
````