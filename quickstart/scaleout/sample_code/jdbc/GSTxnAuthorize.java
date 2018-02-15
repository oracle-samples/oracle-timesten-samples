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
 *  A class that represents the 'Authorize' transaction
 *
 * This transaction checks to see if a given accountID exists with
 * a pre-defined type and status and if so it returns the associated
 * customerID and phone number.
 */
public class GSTxnAuthorize
    extends GSAppTransaction
{
    // Static stuff
    private static final long INVALID_VALUE = -1;
    private static final String sqlAuthorize =
         "SELECT cust_id, phone FROM accounts WHERE account_id = :acc_id" +
                 " AND account_type = :acc_type AND status = :acc_status";
    private static final String accountType = "P";
    private static final int accountStatus = 10;
    
    // DB stuff
    private GSDbStatement stmtAuthorize = null;
    private long accountID = INVALID_VALUE;

    /**
     * Constructor
     */
    GSTxnAuthorize(
                   GSDbConnection dbcon,
                   GSGlobal cx
                  )
    {
        super( dbcon, cx );
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, 
                "DEBUG: ENTER: GSTxnAuthorize: Constructor" );
        myName = GSConstants.nameAuthorize;
        returnsResultSet = true;
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, 
                "DEBUG: EXIT: GSTxnAuthorize: Constructor" );
    } // Constructor

    /**
     * Set 'accountID' input value
     */
    public boolean setAccountID(
                                long aID
                               )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: ENTER: GSTxnAuthorize: setAccountID" );
        if (  aID < 1  )
        {
            errMsg = GSErrors.M_PARAM_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                          "DEBUG: EXITERR: GSTxnAuthorize: setAccountID" );
            return false;
        }

        errMsg = null;
        accountID = aID;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: EXITOK: GSTxnAuthorize: setAccountID" );
        return true;
    } // setAccountID

    /**
     * Cleanup the transaction
     */
    public void cleanup()
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                "DEBUG: ENTER: GSTxnAuthorize: cleanup" );
        errMsg = null;
        if (  stmtAuthorize != null  )
        {
            stmtAuthorize.close();
            stmtAuthorize = null;
        }
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                "DEBUG: EXIT: GSTxnAuthorize: cleanup" );
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
                "DEBUG: ENTER: GSTxnAuthorize: init" );
        cleanup();
        try {
            stmtAuthorize = dbConn.prepare( "TxnAuthorize_Authorize",
                                                 sqlAuthorize );
            if (  stmtAuthorize != null  )
                result = true;
        } catch (  GSGridRetryException gre  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXIT: GSTxnAuthorize: init: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXIT: GSTxnAuthorize: init: Failover" );
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
                          "DEBUG: EXITOK: GSTxnAuthorize: init" );
            else
                GSUtil.debugMessage( ctxt, 
                          "DEBUG: EXITERR: GSTxnAuthorize: init" );
        }
        return result;
    } // init

    /**
     * Execute the transaction
     */
    public GSDbResultSet executeRS()
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        boolean result = false;
        GSDbResultSet rs = null;

        if (  GSConstants.debugEnabled  )
        {
            GSUtil.debugMessage( ctxt, 
                "DEBUG: ENTER: GSTxnAuthorize: executeRS" );
            GSUtil.debugMessage( ctxt, 
                "DEBUG: accountID = " + accountID );
        }

        if (  accountID == INVALID_VALUE  )
        {
            errMsg = GSErrors.M_NO_INPUT;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                          "DEBUG: EXITERR: GSTxnAuthorize: executeRS" );
            return rs;
        }

        errMsg = null;

        // Process the query
        try {
            // set parameters
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                    "DEBUG: setLong() for stmtAuthorize/accountID" );
            result = setLong( stmtAuthorize, 1, accountID );
            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                        "DEBUG: setString() for stmtAuthorize/accountType" );
                result = setString( stmtAuthorize, 2, accountType );
            }

            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                        "DEBUG: setInt() for stmtAuthorize/accountStatus" );
                setInt( stmtAuthorize, 3, accountStatus );
            }

            if (  result  ) // execute query
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                                     "DEBUG: executeStmt(stmtAuthorize)" );
                result = executeStmt( stmtAuthorize );
            }
            if (  result  )
            {
                // retrieve results
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                        "DEBUG: stmtAuthorize.getResultSet()" );
                rs = stmtAuthorize.getResultSet();
                if (  rs == null  )
                {
                    errMsg = GSErrors.M_NO_RESULTS;
                    if (  GSConstants.debugEnabled  )
                        GSUtil.debugMessage( ctxt, 
                           "DEBUG: EXITERR: GSTxnAuthorize: executeRS:" +
                           " No results" );
                    return rs;
                }
            }
        } catch (  GSGridRetryException gre  ) {
            if (  rs != null  ) { rs.close(); rs = null; }
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXIT: GSTxnAuthorize: executeRS: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  rs != null  ) { rs.close(); rs = null; }
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXIT: GSTxnAuthorize: executeRS: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                "DEBUG: EXITOK: GSTxnAuthorize:executeRS" );
        return rs;
    } // executeRS

} // GSTxnAuthorize
