--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

rem
Rem    NAME
Rem      pls_examp5.sql - <one-line expansion of the name>
Rem    DESCRIPTION
Rem      <short description of component this file declares/defines>
Rem    RETURNS
Rem
Rem    NOTES
Rem      <other useful comments, qualifications, etc.>

/*
** This block does some numeric processing on data that
** comes from experiment #1.  The results are stored in
** the TEMP table.
**
** Copyright (c) 1989, 2017, Oracle and/or its affiliates. 
** All rights reserved.*/

DECLARE
    num1    data_table.n1%TYPE;   -- Declare variables
    num2    data_table.n2%TYPE;   -- to be of same type as
    num3    data_table.n3%TYPE;   -- database columns
    result  temp.num_col1%TYPE;
    CURSOR c1 IS
        SELECT n1, n2, n3 FROM data_table
            WHERE exper_num = 1;
BEGIN
    OPEN c1;
    LOOP
        FETCH c1 INTO num1, num2, num3;
        EXIT WHEN c1%NOTFOUND;
            -- the c1%NOTFOUND condition evaluates
            -- to TRUE when FETCH finds no more rows
        /* calculate and store the results */
        result := num2/(num1 + num3);
        INSERT INTO temp VALUES (result, NULL, NULL);
    END LOOP;
    CLOSE c1;
    COMMIT;
END;
/

