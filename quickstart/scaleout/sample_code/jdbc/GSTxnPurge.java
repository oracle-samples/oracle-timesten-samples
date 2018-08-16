/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.util.*;
import java.sql.*;
import com.timesten.jdbc.*;

/**
 *  A class that represents the 'Purge' transaction.
 *
 * Thus transaction deletes a number of 'old' rows from the TRANSACTIONS
 * table in order to prevent it growing too large.
 */
public class GSTxnPurge
    extends GSAppTransaction
{
    // Static stuff
    private static final String sqlPurge =
         "DELETE FIRST 128 FROM transactions WHERE" +
                " CAST(SYSDATE AS TIMESTAMP) - transaction_ts >" +
                " NUMTODSINTERVAL( :age, 'SECOND')";
    
    // DB stuff
    private GSDbStatement stmtPurge = null;
    private int age = GSConstants.dfltPurgeAge;

    /**
     * Constructor
     */
    GSTxnPurge(
               GSDbConnection dbcon,
               GSGlobal cx
              )
    {
        super( dbcon, cx );
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, "DEBUG: ENTER: GSTxnPurge: Constructor" );
        myName = GSConstants.namePurge;
        age = cx.purgeAge;
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, "DEBUG: EXIT: GSTxnPurge: Constructor" );
    } // Constructor

    /**
     * Set 'age' parameter - how old rows must be (in seconds) before they
     * can be purged.
     */
    public boolean setAge(
                          int a
                         )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, "DEBUG: ENTER: GSTxnPurge: setAge" );
        if (  a <= 0  )
        {
            errMsg = GSErrors.M_PARAM_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: EXITERR: GSTxnPurge: setAge" );
            return false;
        }

        errMsg = null;
        age = a;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, "DEBUG: EXITOK: GSTxnPurge: setAge" );
        return true;
    } // setAge

    /**
     * Cleanup the transaction
     */
    public void cleanup()
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSTxnPurge: cleanup" );
        errMsg = null;
        if (  stmtPurge != null  )
        {
            stmtPurge.close();
            stmtPurge = null;
        }
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: EXIT: GSTxnPurge: cleanup" );
    } // cleanup

    /**
     * Initialize the transaction
     */
    public boolean init()
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSTxnPurge: init" );
        cleanup();
        try {
            stmtPurge = dbConn.prepare( "TxnPurge_Purge", sqlPurge );
            if (  stmtPurge != null  )
                result = true;
        } catch (  GSGridRetryException gre  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSTxnPurge: init: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSTxnPurge: init: Failover" );
            throw cfe;
        }

        if (  ! result  )
        {
            String tmp = dbConn.getLastError();
            cleanup();
            errMsg = tmp;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITOK: GSTxnPurge: init" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnPurge: init" );
        }
        return result;
    } // init

    /**
     * Execute the transaction
     */
    public boolean execute()
        throws GSGridRetryException, GSConnectionFailoverException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, "DEBUG: ENTER: GSTxnPurge: execute" );

        errMsg = null;

        // Execute the DELETE
        try {
            rowCount = 0;
            // set parameters
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: setInt() for stmtPurge/age" );
            result = setInt( stmtPurge, 1, age );
            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: execute stmtPurge" );
                result = executeStmt( stmtPurge );
            }
            if (  result  )
            {
                rowCount = stmtPurge.getUpdateCount();
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: commit" );
                result = commitTxn();
            }
            else
            {
                GSUtil.debugMessage( ctxt, "DEBUG: rollback" );
                errMsg = stmtPurge.getLastError();
                dbConn.rollback();
            }
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSTxnPurge: execute: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSTxnPurge: execute: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITOK: GSTxnPurge: execute" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnPurge: execute" );
        }
        return result;
    } // execute

    /**
     * Close any open result set.
     */
    public boolean close()
        throws GSGridRetryException,
               GSConnectionFailoverException
    {
        return true;
    } // close

} // GSTxnPurge
