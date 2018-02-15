--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

rem
Rem    NAME
Rem      pls_examp4.sql - <one-line expansion of the name>
Rem    DESCRIPTION
Rem      <short description of component this file declares/defines>
Rem    RETURNS
Rem
Rem    NOTES
Rem      <other useful comments, qualifications, etc.>

/*
** This block finds all employees whose monthly wages (salary plus
** commission) are higher than $2000.
**
** An alias is used in the cursor declaration so that the subsequent
** use of %ROWTYPE is allowed.  (Column names in a cursor declaration
** must have aliases if they are not simple names.)
**
** Copyright (c) 1989,1992 by Oracle Corporation
*/

DECLARE
    CURSOR my_cursor IS SELECT sal + NVL(comm, 0) wages, ename
        FROM emp;
    my_rec  my_cursor%ROWTYPE;
BEGIN
    OPEN my_cursor;
    LOOP
        FETCH my_cursor INTO my_rec;
        EXIT WHEN my_cursor%NOTFOUND;
        IF my_rec.wages > 2000 THEN
            INSERT INTO temp VALUES (NULL, my_rec.wages,
                my_rec.ename);
        END IF;
    END LOOP;
    CLOSE my_cursor;
END;
/

