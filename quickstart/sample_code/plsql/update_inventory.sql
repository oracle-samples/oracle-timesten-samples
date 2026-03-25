--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

Rem
Rem    NAME
Rem      pls_examp1.sql - plsql example file
Rem    DESCRIPTION
Rem      <short description of component this file declares/defines>
Rem    RETURNS
Rem
Rem    NOTES
Rem      renamed from examp1.sql to pls_examp1.sql
Rem

/*
** This block processes an order for tennis rackets.  It decrements
** the quantity of rackets on hand only if there is at least one
** racket left in stock.
**
** Copyright (c) 1989,1992 by Oracle Corporation
*/

DECLARE
    qty_on_hand NUMBER(5);
BEGIN
    SELECT quantity INTO qty_on_hand FROM inventory2
        WHERE product = 'TENNIS RACKET'
        FOR UPDATE OF quantity;

    IF qty_on_hand > 0 THEN  -- check quantity
        UPDATE inventory2 SET quantity = quantity - 1
            WHERE product = 'TENNIS RACKET';
        INSERT INTO purchase_record
            VALUES ('Tennis racket purchased', SYSDATE);
    ELSE
        INSERT INTO purchase_record
            VALUES ('Out of tennis rackets', SYSDATE);
    END IF;

    COMMIT;
END;
/
