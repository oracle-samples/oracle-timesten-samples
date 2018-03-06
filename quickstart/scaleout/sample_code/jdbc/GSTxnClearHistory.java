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
 *  A class that represents the 'Clear History' application transaction.
 *
 * This transatcion truncates (deletes all rows from) the TRANSACTIONS
 * table.
 */
public class GSTxnClearHistory
    extends GSAppTransaction
{
    // Static stuff
    private static final String truncateDDL =
             "TRUNCATE TABLE TRANSACTIONS";
    private static final String truncateDML =
             "DELETE /*+ TT_TblLock(1) TT_RowLock(0) */ FROM transactions";
    
    // DB stuff
    private String sqlTruncate = null;
    private GSDbStatement stmtTruncate = null;

    /**
     * Constructor
     */
    GSTxnClearHistory(
                      GSDbConnection dbcon,
                      GSGlobal cx
                     )
    {
        super( dbcon, cx );
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, 
                             "DEBUG: ENTER: GSTxnClearHistory: Constructor" );
        myName = GSConstants.nameClearHistory;
        if (  GSConstants.useTruncate  )
            sqlTruncate = truncateDDL;
        else
            sqlTruncate = truncateDML;
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, 
                             "DEBUG: EXIT: GSTxnClearHistory: Constructor" );
    } // Constructor

    /**
     * Cleanup the transaction
     */
    public void cleanup()
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: ENTER: GSTxnClearHistory: cleanup" );
        errMsg = null;
        if (  stmtTruncate != null  )
        {
            stmtTruncate.close();
            stmtTruncate = null;
        }
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: ENTER: GSTxnClearHistory: cleanup" );
    } // cleanup

    /**
     * Initialize the transaction
     */
    public boolean init()
        throws GSGridRetryException,
               GSConnectionFailoverException
    {
        boolean result = false;;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: ENTER: GSTxnClearHistory: init" );
        cleanup();
        try {
            stmtTruncate = dbConn.prepare( "TxnClearHistory_Truncate",
                                           sqlTruncate );
            if (  stmtTruncate != null  )
                result = true;
        } catch (  GSGridRetryException gre  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                         "DEBUG: GSTxnClearHistory: init: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                         "DEBUG: GSTxnClearHistory: init: Failover" );
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
                             "DEBUG: EXITOK: GSTxnClearHistory: init" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnClearHistory: init" );
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
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: ENTER: GSTxnClearHistory: execute" );

        errMsg = null;

        // Execute the Truncate statement
        try {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, "DEBUG: execute stmtTruncate" );
            result = executeStmt( stmtTruncate );
            if (  ! GSConstants.useTruncate  ) // need to commit or rollback
            {
                if (  result  )
                    result = commitTxn();
                else
                    dbConn.rollback();
            }
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSTxnClearHistory: execute: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSTxnClearHistory: execute: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITOK: GSTxnClearHistory: execute" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnClearHistory: execute" );
        }
        return result;
    } // execute

} // GSTxnClearHistory
