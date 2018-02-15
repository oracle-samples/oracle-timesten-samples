--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

REM
REM Create program units for the PL/SQL sample programs
REM

prompt
create or replace package personnel as
    type charArrayTyp is table of varchar2(10)
        index by binary_integer;
    type numArrayTyp is table of float
        index by binary_integer;
    procedure get_employees(
        dept_number in     integer,
        batch_size  in     integer,
        found       in out integer,
        done_fetch  out    integer,
        emp_name    out    charArrayTyp,
        job_title   out    charArrayTyp,
        salary      out    numArrayTyp);
end personnel;
/
create or replace package body personnel as
    cursor get_emp (dept_number integer) is
        select ename, job, sal from emp
            where deptno = dept_number order by ename;
    procedure get_employees(
        dept_number in     integer,
        batch_size  in     integer,
        found       in out integer,
        done_fetch  out    integer,
        emp_name    out    charArrayTyp,
        job_title   out    charArrayTyp,
        salary      out    numArrayTyp) is
    begin
        if not get_emp%isopen then
            open get_emp(dept_number);
        end if;
        done_fetch := 0;
        found := 0;
        for i in 1..batch_size loop
            fetch get_emp into emp_name(i),
                    job_title(i), salary(i);
            if get_emp%notfound then
                close get_emp;
                done_fetch := 1;
                exit;
            else
                found := found + 1;
            end if;
        end loop;
    end get_employees;
end personnel;
/
CREATE OR REPLACE PACKAGE emp_pkg AS

  -- Declare a record for the desired EMP fields
  TYPE empRecType IS RECORD (
    r_empno  EMP.EMPNO%TYPE,
    r_ename  EMP.ENAME%TYPE,
    r_salary EMP.SAL%TYPE
  );

  -- Declare a Ref Cursor type
  TYPE EmpCurType IS REF CURSOR RETURN empRecType;

  -- A parameterized cursor
  CURSOR low_paid (num PLS_INTEGER) IS
    SELECT empno 
      FROM emp
      WHERE rownum <= num
      ORDER BY sal ASC;

  PROCEDURE ddl_dml
    (myComment IN  VARCHAR2,
     errCode   OUT PLS_INTEGER,
     errText   OUT VARCHAR2); 

  PROCEDURE givePayRaise
    (num       IN  PLS_INTEGER,
     name      OUT EMP.ENAME%TYPE,
     errCode   OUT PLS_INTEGER,
     errText   OUT VARCHAR2); 

  PROCEDURE OpenSalesPeopleCursor
    (salesRefCur OUT EmpCurType,
     errCode     OUT PLS_INTEGER,
     errText     OUT VARCHAR2); 

END emp_pkg;
/


CREATE OR REPLACE PACKAGE BODY emp_pkg AS


  PROCEDURE ddl_dml
    (myComment IN  VARCHAR2,
     errCode   OUT PLS_INTEGER,
     errText   OUT VARCHAR2) IS

    sql_str                    VARCHAR2(256);
    name_already_exists        EXCEPTION;
    insufficient_privileges    EXCEPTION;
    PRAGMA EXCEPTION_INIT(name_already_exists,     -0955);
    PRAGMA EXCEPTION_INIT(insufficient_privileges, -1031);
    seq_value                  number;


  BEGIN


    BEGIN
      sql_str := 'create table foo (COL1 VARCHAR2 (20),COL2 NVARCHAR2 (60))';
      DBMS_OUTPUT.PUT_LINE(sql_str);
      execute immediate sql_str;
    EXCEPTION
      WHEN name_already_exists THEN
        DBMS_OUTPUT.PUT_LINE('  Ignore existing table errors');
      WHEN insufficient_privileges THEN
        DBMS_OUTPUT.PUT_LINE('  Ignore insufficient privileges errors');
    END;

    -- Cast num_col1 and char_col values
    insert into temp values (1, 1, myComment);

    commit;

    errCode := 0;
    errtext := 'OK';

  EXCEPTION
  
    WHEN name_already_exists THEN

      errCode := 0;
      errtext := 'OK';

    WHEN OTHERS THEN

      errCode  := SQLCODE;
      errText  := SUBSTR(SQLERRM, 1, 200);

  END ddl_dml;



  PROCEDURE givePayRaise
    (num       IN  PLS_INTEGER,
     name      OUT EMP.ENAME%TYPE,
     errCode   OUT PLS_INTEGER,
     errText   OUT VARCHAR2) IS

    number_type_overflow    EXCEPTION;
    PRAGMA EXCEPTION_INIT(number_type_overflow, -57000);

   -- Can use PLSQL collections within TimesTen PLSQL
   TYPE lowest_paid_type IS TABLE OF emp.empno%TYPE;
   lowest_paid lowest_paid_type;

   i           PLS_INTEGER; 
   numRows     PLS_INTEGER;
   lucky_index PLS_INTEGER; 
   lucky_emp   EMP.EMPNO%TYPE; 

  BEGIN

    -- Initialize the output variable
    name := 'Nobody';

    -- Initialize the collection
    lowest_paid := lowest_paid_type(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    i := 1;
    
    -- Constrain the resultset size
    IF num < 1 OR num > 10 THEN

      -- If bad inputs, default to 5 rows
      numRows := 5;
    ELSE
      numRows := num;
    END IF;


    -- Create the cursor resultset with up to 'numRows' rows
    OPEN low_paid( numRows );

    LOOP

      -- Get the current empid
      FETCH low_paid INTO lowest_paid(i);

      EXIT WHEN low_paid%NOTFOUND;

      -- Increment the PLSQL table index
      i := i + 1;

    END LOOP;

    -- Close the cursor
    CLOSE low_paid;


    -- List the subset of lowest paid employees
    FOR j in lowest_paid.FIRST .. numRows LOOP
      DBMS_OUTPUT.PUT_LINE('  Lowest paid empno ' || j || ' is ' || lowest_paid(j) );
    END LOOP;

    -- Randomly choose one of the lowest paid employees for a 10% pay raise.
    lucky_index := trunc(dbms_random.value(lowest_paid.FIRST, numRows)); 
    lucky_emp := lowest_paid(lucky_index);


    -- Give lucky_emp a 10% pay raise and return their name
    UPDATE emp
      SET sal = sal * 1.1
      WHERE empno = lucky_emp
      RETURNING ename INTO name;

    COMMIT;

    errCode := 0;
    errtext := 'OK';

  EXCEPTION
  
    WHEN number_type_overflow THEN

      -- The employees were paid too much, start again
      UPDATE emp
        SET sal = 42;

      COMMIT;

      name    := 'KING';
      errCode := 0;
      errText := 'OK';


    WHEN OTHERS THEN

      errCode  := SQLCODE;
      errText  := SUBSTR(SQLERRM, 1, 200);

  END givePayRaise;



  PROCEDURE OpenSalesPeopleCursor
    (salesRefCur OUT EmpCurType,
     errCode     OUT PLS_INTEGER,
     errText     OUT VARCHAR2) IS

  BEGIN 

    errCode := 0;
    errtext := 'OK';

    OPEN salesRefCur FOR
      SELECT empno, ename, sal
      FROM emp
      WHERE comm IS NOT NULL;
    
  EXCEPTION

    WHEN OTHERS THEN

      errCode  := SQLCODE;
      errText  := SUBSTR(SQLERRM, 1, 200);
      salesRefCur := null;

  END OpenSalesPeopleCursor;


BEGIN  -- package initialization goes here
  DBMS_OUTPUT.PUT_LINE('Initialized package emp_pkg');
END emp_pkg;
/

CREATE OR REPLACE PACKAGE sample_pkg AS

  FUNCTION getEmpName
    (empNum      IN  EMP.EMPNO%TYPE,
     errCode     OUT PLS_INTEGER,
     errText     OUT VARCHAR2) RETURN EMP.ENAME%TYPE;

END sample_pkg;
/


CREATE OR REPLACE PACKAGE BODY sample_pkg AS

  FUNCTION getEmpName
    (empNum      IN  EMP.EMPNO%TYPE,
     errCode     OUT PLS_INTEGER,
     errText     OUT VARCHAR2) RETURN EMP.ENAME%TYPE

  IS
    -- emp_name is used to return the name of the employee
    emp_name EMP.ENAME%TYPE;

  BEGIN

    -- Initialize the output variables
    emp_name := 'Nobody yet';
    errCode  := 0;
    errtext  := 'OK';

    -- Primary key select to the output string (emp_name)
    SELECT ename
      INTO emp_name
      FROM emp
      WHERE empno = empNum;

    RETURN emp_name;


  -- In case something goes wrong
  EXCEPTION

    WHEN NO_DATA_FOUND THEN

      errCode := 0;
      errtext := 'OK';
      RETURN 'No employee for that employee number';

    WHEN TOO_MANY_ROWS THEN

      errCode := 0;
      errtext := 'OK';
      RETURN 'Non unique id used';

    WHEN OTHERS THEN

      errCode  := SQLCODE;
      errText  := SUBSTR(SQLERRM, 1, 200);
      RETURN null;

  END getEmpName;


BEGIN  -- package initialization goes here

  DBMS_OUTPUT.PUT_LINE('Initialized the sample_pkg package');

END sample_pkg;
/

PACKAGES;

REM
REM A function to return blank spaces 
REM This is used in the generation of statement 
REM

CREATE OR REPLACE FUNCTION sp(n IN INTEGER) RETURN VARCHAR2
AS
num INTEGER;
space VARCHAR2(80);
BEGIN
  space := '';
  FOR num IN 1..n 
  LOOP
    space := space||' ';
  END LOOP;
  RETURN space;
END;
/

FUNCTIONS;

REM Create PL/SQL procedures required for Pro*C sample program
REM from psq01.sql
REM

CREATE OR REPLACE PROCEDURE 
GETEMPNAME_UNIQUE_ID_ (EMPNAME OUT VARCHAR2) AS
BEGIN
  EMPNAME := 'JONES';
END;
/

CREATE OR REPLACE PROCEDURE 
getempno_unique_id_ (empno out number) AS
BEGIN
  empno := 7566;
END;
/
PROCEDURES;
/
PROMPT
SHOW ERRORS
/

CREATE OR REPLACE PACKAGE sample_pkg AS

  FUNCTION getEmpName
    (empNum      IN  EMP.EMPNO%TYPE,
     errCode     OUT PLS_INTEGER,
     errText     OUT VARCHAR2) RETURN EMP.ENAME%TYPE;

END sample_pkg;
/
