--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

/*
** This block uses a simple FOR loop to insert 10 rows into a table.
** The values of a loop index, counter variable, and either of two
** character strings are inserted.  Which string is inserted
** depends on the value of the loop index.
**
** Copyright (c) 1989,1992, 1999 by Oracle Corporation
*/

DECLARE
    x  NUMBER := 100;
BEGIN
    FOR i IN 1..10 LOOP
        IF MOD(i,2) = 0 THEN     -- i is even
            INSERT INTO temp VALUES (i, x, 'i is even');
        ELSE
            INSERT INTO temp VALUES (i, x, 'i is odd');
        END IF;

        x := x + 100;
    END LOOP;
    COMMIT;
END;
/

