--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

Rem
Rem    NAME
Rem      pls_examp11.sql - <one-line expansion of the name>
Rem    DESCRIPTION
Rem      <short description of component this file declares/defines>
Rem    RETURNS
Rem
Rem    NOTES
Rem      <other useful comments, qualifications, etc.>
Rem

/*
** This block calculates the ratio between the X and Y columns of
** the RESULT_TABLE table.  If the ratio is greater than 0.72, the block
** inserts the ratio into RATIO.  Otherwise, it inserts -1.
** If the denominator is zero, ZERO_DIVIDE is raised, and a
** zero is inserted into RATIO.
**
** Copyright (c) 1989,1992 by Oracle Corporation
*/

DECLARE
    numerator    NUMBER;
    denominator  NUMBER;
    the_ratio    NUMBER;
    lower_limit  CONSTANT NUMBER := 0.72;
    samp_num     CONSTANT NUMBER := 132;
BEGIN
    SELECT x, y INTO numerator, denominator FROM result_table
        WHERE sample_id = samp_num;
    the_ratio := numerator/denominator;
    IF the_ratio > lower_limit THEN
        INSERT INTO ratio VALUES (samp_num, the_ratio);
    ELSE
        INSERT INTO ratio VALUES (samp_num, -1);
    END IF;
    COMMIT;
EXCEPTION
    WHEN ZERO_DIVIDE THEN
        INSERT INTO ratio VALUES (samp_num, 0);
        COMMIT;
    WHEN OTHERS THEN
        ROLLBACK;
END;
/


/*
** This block uses a cursor to fetch and display the employees
** in a given department. If no employee is found, the exception
** NO_DATA_FOUND is raised.
*/

SET SERVEROUTPUT ON
DECLARE
   CURSOR emp_cursor is select empno, ename from emp where deptno=10;
   emp_rec emp_cursor%rowtype;
BEGIN
   OPEN emp_cursor;
   LOOP
     FETCH emp_cursor INTO emp_rec;
     EXIT WHEN emp_cursor%NOTFOUND;
     DBMS_OUTPUT.PUT_LINE (emp_rec.empno||','||emp_rec.ename);
   END LOOP;
   IF emp_cursor%ROWCOUNT = 0 THEN 
     RAISE NO_DATA_FOUND;
   END IF;
  CLOSE emp_cursor; 
EXCEPTION 
   WHEN NO_DATA_FOUND THEN
     DBMS_OUTPUT.PUT_LINE ('No Employee Found');
END;
/
