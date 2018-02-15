--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

Rem
Rem    NAME
Rem      pls_examp7.sql - <one-line expansion of the name>
Rem    DESCRIPTION
Rem      <short description of component this file declares/defines>
Rem    RETURNS
Rem
Rem    NOTES
Rem      <other useful comments, qualifications, etc.>
Rem

/*
** This block does some numeric processing on data that comes
** from experiment #1.  The results are stored in the TEMP table.
**
** Copyright (c) 1989, 2017, Oracle and/or its affiliates. 
** All rights reserved.*/
*/

DECLARE
    result  temp.num_col1%TYPE;
    CURSOR c1 IS
        SELECT n1, n2, n3 FROM data_table
            WHERE exper_num = 1;
BEGIN
    FOR c1rec IN c1 LOOP
            /* calculate and store the results */
        result := c1rec.n2 / (c1rec.n1 + c1rec.n3);
        INSERT INTO temp VALUES (result, NULL, NULL);
    END LOOP;
    COMMIT;
END;
/

