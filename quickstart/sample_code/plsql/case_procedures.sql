--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

Rem
Rem casedemo.sql
Rem
Rem    NAME
Rem      casedemo.sql - Demo program for CASE statement and CASE expression.
Rem
Rem    DESCRIPTION
Rem      This is a sample program to demonstrate the usage of the CASE
Rem      statement and CASE expression. The PL/SQL CASE statement includes
Rem      the simple CASE statement and the searched CASE statement. The
Rem      CASE expression includes the simple CASE expression, the searched
Rem      CASE expression, plus two syntactic shorthands COALESCE and NULLIF.
Rem      For comparison purpose, each procedure is also implemented using
Rem      traditional language elements, such as a sequence of IF statements
Rem      or the SQL DECODE function.
Rem
Rem    NOTES
Rem      The PL/SQL COALESCE and NULLIF are implemented to allow seamless
Rem      integration between PL/SQL and SQL. It is more advantageous to
Rem      use these two CASE expression shorthands inside a SQL context.
Rem      In a PL/SQL context, they can be easily transformed into PL/SQL
Rem      CASE expression or a sequence of PL/SQL IF statements, sometimes
Rem      results in even simpler code or better performace.
Rem

SET SERVEROUTPUT ON;

-- create a table
DROP TABLE students;
CREATE TABLE students (
  id NUMBER(5) PRIMARY KEY,
  first_name VARCHAR2(20),
  last_name VARCHAR2(20),
  grade CHAR(1)
);

-- insert a row into the table
INSERT INTO students values (10000, 'Scott', 'Smith', 'A');

-- using IF statements
CREATE OR REPLACE PROCEDURE show_appraisal_if (
  grade CHAR)
AS
  appraisal VARCHAR2(20);
BEGIN
  IF grade = 'A' THEN
    appraisal := 'Excellent';
  ELSIF grade = 'B' THEN
    appraisal := 'Very Good';
  ELSIF grade = 'C' THEN
    appraisal := 'Good';
  ELSIF grade = 'D' THEN
    appraisal := 'Fair';
  ELSIF grade = 'F' THEN
    appraisal := 'Poor';
  ELSE
    appraisal := 'No such grade';
  END IF;
  dbms_output.put_line('Appraisal: ' || appraisal);
END;
/
show errors;

-- using simple CASE statements
CREATE OR REPLACE PROCEDURE show_appraisal_simple_case_stm(
  grade CHAR)
AS
  appraisal VARCHAR2(20);
BEGIN
  dbms_output.put('Appraisal: ');
  CASE grade
    WHEN 'A' THEN dbms_output.put_line('Excellent');
    WHEN 'B' THEN dbms_output.put_line('Very Good');
    WHEN 'C' THEN dbms_output.put_line('Good');
    WHEN 'D' THEN dbms_output.put_line('Fair');
    WHEN 'E' THEN dbms_output.put_line('Poor');
    ELSE dbms_output.put_line('No such grade');
  END CASE;
END;
/
show errors;

-- using simple CASE expression
CREATE OR REPLACE PROCEDURE show_appraisal_simple_case_exp (
  grade CHAR)
AS
BEGIN
  dbms_output.put_line('Appraisal: ' ||
    CASE grade
       WHEN 'A' THEN 'Excellent'
       WHEN 'B' THEN 'Very Good'
       WHEN 'C' THEN 'Good'
       WHEN 'D' THEN 'Fair'
       WHEN 'F' THEN 'Poor'
       ELSE 'No such grade'
    END);
END;
/
show errors;

-- using DECODE function
create or replace PROCEDURE show_appraisal_decode (
  student_id NUMBER)
AS
  appraisal VARCHAR2(20);
BEGIN
   select DECODE(grade,
     'A', 'Successful',
     'B', 'Successful',
     'C', 'Successful',
     'D', 'Successful',
     'F', 'Unsuccessful',
     'No such grade')
     into appraisal from students where id = student_id;
   dbms_output.put_line('Appraisal: ' || appraisal);
END;
/
show errors;

-- using searched CASE statements
CREATE OR REPLACE PROCEDURE show_appraisal_srched_case_stm(
  grade CHAR)
AS
  appraisal VARCHAR2(20);
BEGIN
  dbms_output.put('Appraisal: ');
  CASE
    WHEN grade between 'A' and 'D' THEN dbms_output.put_line('Successful');
    WHEN grade = 'F'               THEN dbms_output.put_line('Unsuccessful');
    ELSE dbms_output.put_line('No such grade');
  END CASE;
END;
/
show errors;

-- using searched CASE expression
create or replace PROCEDURE show_appraisal_srched_case_exp (
  student_id NUMBER)
AS
  appraisal VARCHAR2(20);
BEGIN
   select CASE
      WHEN grade between 'A' and 'D' THEN 'Successful'
      WHEN grade = 'F'               THEN 'Unsuccessful'
      ELSE 'No such grade'
   END
     into appraisal from students where id = student_id;
  dbms_output.put_line('Appraisal: ' || appraisal);
END;
/
show errors;

-- using if statements 
create or replace PROCEDURE check_inputs_if (
  p1 NUMBER,
  p2 NUMBER,
  p3 NUMBER,
  p4 NUMBER,
  p5 NUMBER)
AS
BEGIN
  IF (p1 is null OR p1 = 0) AND
     (p2 is null OR p2 = 0) AND
     (p3 is null OR p3 = 0) AND
     (p4 is null OR p4 = 0) AND
     (p5 is null OR p5 = 0)
  THEN
    dbms_output.put_line('invalid inputs');
  ELSE
    dbms_output.put_line('valid inputs');
  END IF;
END;
/
show errors;

-- using COALESCE and NULLIF
create or replace PROCEDURE check_inputs_coalesce_nullif (
  p1 NUMBER,
  p2 NUMBER,
  p3 NUMBER,
  p4 NUMBER,
  p5 NUMBER)
AS
BEGIN
  IF COALESCE(NULLIF(p1, 0),
              NULLIF(p2, 0),
              NULLIF(p3, 0),
              NULLIF(p4, 0),
              NULLIF(p5, 0)) IS NULL
  THEN
    dbms_output.put_line('invalid inputs');
  ELSE
    dbms_output.put_line('valid inputs');
  END IF;
END;
/
show errors;

-- demo execution starts here
DECLARE
  grade CHAR(1);
  id NUMBER(5);
BEGIN
   select id, grade into id, grade from students
      where first_name = 'Scott' and last_name = 'Smith';
   dbms_output.put_line('Calling procedure show_appraisal_if');
   show_appraisal_if(grade);
   dbms_output.put_line('Calling procedure show_appraisal_simple_case_stm');
   show_appraisal_simple_case_stm(grade);
   dbms_output.put_line('Calling procedure show_appraisal_simple_case_exp');
   show_appraisal_simple_case_exp(grade);
   dbms_output.put_line('Calling procedure show_appraisal_decode');
   show_appraisal_decode(id);
   dbms_output.put_line('Calling procedure show_appraisal_srched_case_stm');
   show_appraisal_srched_case_stm(grade);
   dbms_output.put_line('Calling procedure show_appraisal_srched_case_exp');
   show_appraisal_srched_case_exp(id);
   dbms_output.put_line('Calling procedure check_inputs_if');
   check_inputs_if(null, 0, null, 0, 0);
   check_inputs_if(null, 0, null, 0, 1);
   dbms_output.put_line('Calling procedure check_inputs_coalesce_nullif');
   check_inputs_coalesce_nullif(null, 0, null, 0, 0);
   check_inputs_coalesce_nullif(null, 0, null, 0, 1);
end;
/
