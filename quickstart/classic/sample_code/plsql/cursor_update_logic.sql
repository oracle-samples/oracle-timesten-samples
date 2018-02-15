--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

Rem
Rem    NAME
Rem      pls_sample4.sql - plsql sample file
Rem    DESCRIPTION
Rem      <short description of component this file declares/defines>
Rem    RETURNS
Rem
Rem    NOTES
Rem      renamed from sample4.sql to pls_sample4.sql
Rem

/*
** This program modifies the ACCOUNT table based on instructions
** stored in the ACTION table.  Each row of the ACTION table
** contains an account number to act upon, an action to be taken
** (insert, update, or delete), an amount  by which to update the
** account, and a time tag.
**
** On an insert, if the account already exists, an update is
** performed instead.  On an update, if the account does not exist,
** it is created by an insert.  On a delete, if the row does not
** exist, no action is taken.
**
** Copyright (c) 1989,1992, 1999 by Oracle Corporation
*/

DECLARE
    CURSOR c1 IS
        SELECT account_id, oper_type, new_value FROM action
        ORDER BY time_tag
        FOR UPDATE OF status;

BEGIN
    FOR acct IN c1 LOOP   -- process each row one at a time

        acct.oper_type := upper(acct.oper_type);

        /*----------------------------------------*
         * Process an UPDATE.  If the account to  *
         * be updated doesn't exist, create a new *
         * account.                               *
         *----------------------------------------*/
        IF acct.oper_type = 'U' THEN
            UPDATE account SET bal = acct.new_value
                WHERE account_id = acct.account_id;

            IF SQL%NOTFOUND THEN  -- account didn't exist.  Create it.
                INSERT INTO account
                    VALUES (acct.account_id, acct.new_value);
                UPDATE action SET status =
                    'Update: ID not found. Value inserted.'
                    WHERE CURRENT OF c1;
            ELSE
                UPDATE action SET status = 'Update: Success.'
                    WHERE CURRENT OF c1;
            END IF;
        /*--------------------------------------------*
         * Process an INSERT.  If the account already *
         * exists, do an update of the account        *
         * instead.                                   *
         *--------------------------------------------*/
        ELSIF acct.oper_type = 'I' THEN
            BEGIN
                INSERT INTO account
                    VALUES (acct.account_id, acct.new_value);
                UPDATE action set status = 'Insert: Success.'
                    WHERE CURRENT OF c1;

            EXCEPTION
                WHEN DUP_VAL_ON_INDEX THEN   -- account already exists
                    UPDATE account SET bal = acct.new_value
                        WHERE account_id = acct.account_id;
                    UPDATE action SET status =
                        'Insert: Acct exists. Updated instead.'
                        WHERE CURRENT OF c1;
            END;

        /*--------------------------------------------*
         * Process a DELETE.  If the account doesn't  *
         * exist, set the status field to say that    *
         * the account wasn't found.                  *
         *--------------------------------------------*/
        ELSIF acct.oper_type = 'D' THEN
            DELETE FROM account
                WHERE account_id = acct.account_id;

            IF SQL%NOTFOUND THEN   -- account didn't exist.
                UPDATE action SET status = 'Delete: ID not found.'
                    WHERE CURRENT OF c1;
            ELSE
                UPDATE action SET status = 'Delete: Success.'
                    WHERE CURRENT OF c1;
            END IF;

        /*--------------------------------------------*
         * The requested operation is invalid.        *
         *--------------------------------------------*/
        ELSE    -- oper_type is invalid
            UPDATE action SET status =
               'Invalid operation. No action taken.'
               WHERE CURRENT OF c1;

        END IF;

    END LOOP;
    COMMIT;
END;
/

