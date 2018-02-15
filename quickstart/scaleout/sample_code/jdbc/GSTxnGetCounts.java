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
 *  A class that represents the 'Get Counts' application transaction
 *
 * This transaction returns the max and min customer and account numbers
 * plus the counts of customers and accounts.
 */
public class GSTxnGetCounts
    extends GSAppTransaction
{
    // Static stuff
    private static final long INVALID_VALUE = -1;
    private static final String sqlCountCustomers =
         "SELECT MIN(cust_id), MAX(cust_id), COUNT(*) FROM customers";
    private static final String sqlCountAccounts =
         "SELECT MIN(account_id), MAX(account_id), COUNT(*) FROM accounts";
    
    // DB stuff
    private GSDbStatement stmtCountCustomers = null;
    private GSDbStatement stmtCountAccounts = null;
    private long minCustID = INVALID_VALUE;
    private long maxCustID = INVALID_VALUE;
    private long numCustomers = INVALID_VALUE;
    private long minAccountID = INVALID_VALUE;
    private long maxAccountID = INVALID_VALUE;
    private long numAccounts = INVALID_VALUE;
    private boolean resultsAreValid = false;

    /**
     * Constructor
     */
    GSTxnGetCounts(
                   GSDbConnection dbcon,
                   GSGlobal cx
                  )
    {
        super( dbcon, cx );
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, 
                "DEBUG: ENTER: GSTxnGetCounts: Constructor" );
        myName = GSConstants.nameGetCounts;
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, 
                "DEBUG: EXIT: GSTxnGetCounts: Constructor" );
    } // Constructor

    /**
     * Indicate if results are valid
     */
    public boolean resultsValid()
    {
        errMsg = null;
        return resultsAreValid;
    } // resultsValid

    /**
     * Return minCustID result, if valid
     */
    public long getMinCustID()
    {
        errMsg = null;
        if (  resultsAreValid  )
            return minCustID;
        else
            return INVALID_VALUE;
    } // getMinCustID

    /**
     * Return maxCustID result, if valid
     */
    public long getMaxCustID()
    {
        errMsg = null;
        if (  resultsAreValid  )
            return maxCustID;
        else
            return INVALID_VALUE;
    } // getMaxCustID

    /**
     * Return numCustomers result, if valid
     */
    public long getNumCustomers()
    {
        errMsg = null;
        if (  resultsAreValid  )
            return numCustomers;
        else
            return INVALID_VALUE;
    } // getNumCustomers

    /**
     * Return minAccountID result, if valid
     */
    public long getMinAccountID()
    {
        errMsg = null;
        if (  resultsAreValid  )
            return minAccountID;
        else
            return INVALID_VALUE;
    } // getMinAccountID

    /**
     * Return maxAccountID result, if valid
     */
    public long getMaxAccountID()
    {
        errMsg = null;
        if (  resultsAreValid  )
            return maxAccountID;
        else
            return INVALID_VALUE;
    } // getMaxAccountID

    /**
     * Return numAccounts result, if valid
     */
    public long getNumAccounts()
    {
        errMsg = null;
        if (  resultsAreValid  )
            return numAccounts;
        else
            return INVALID_VALUE;
    } // getNumAccounts

    /**
     * Cleanup the transaction
     */
    public void cleanup()
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                "DEBUG: ENTER: GSTxnGetCounts: cleanup" );
        errMsg = null;
        if (  stmtCountCustomers != null  )
        {
            stmtCountCustomers.close();
            stmtCountCustomers = null;
        }
        if (  stmtCountAccounts != null  )
        {
            stmtCountAccounts.close();
            stmtCountAccounts = null;
        }
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                "DEBUG: EXIT: GSTxnGetCounts: cleanup" );
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
            GSUtil.debugMessage( ctxt, "DEBUG: ENTER: GSTxnGetCounts: init" );
        resultsAreValid = false;
        cleanup();
        try {
            stmtCountCustomers = dbConn.prepare(
                                           "TxnGetCounts_CustomersCount",
                                           sqlCountCustomers );
            if (  stmtCountCustomers != null  )
            {
                stmtCountAccounts = dbConn.prepare(
                                           "TxnGetCounts_AccountsCount",
                                           sqlCountAccounts );
                if (  stmtCountAccounts == null  )
                {
                    stmtCountCustomers.close();
                    stmtCountCustomers = null;
                }
                else
                    result = true;
            }
        } catch (  GSGridRetryException gre  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSTxnGetCounts: init: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSTxnGetCounts: init: Failover" );
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
                             "DEBUG: EXITOK: GSTxnGetCounts: init" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnGetCounts: init" );
        }
        return result;
    } // init

    /**
     * Execute the transaction
     */
    public boolean execute()
        throws GSGridRetryException,
               GSConnectionFailoverException
    {
        boolean result = false;
        GSDbResultSet rs = null;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                "DEBUG: ENTER: GSTxnGetCounts: execute" );

        errMsg = null;
        // Invalidate any existing results
        resultsAreValid = false;

        try {
        // First process the query against CUSTOMERS table
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: execute(stmtCountCustomers)" );
            result = executeStmt( stmtCountCustomers );
            if (  ! result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnGetCounts: execute" );
                return result;
            }
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                    "DEBUG: stmtCountCustomers.getResultSet()" );
            rs = stmtCountCustomers.getResultSet();
            if (  rs == null  )
            {
                errMsg = GSErrors.M_NO_RESULTS;
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnGetCounts: execute" );
                return false;
            }

            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: stmtCountCustomers next()" );
            if (  next( rs )  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: getLong( rs, 1 )" );
                minCustID = getLong( rs, 1 );
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: getLong( rs, 2 )" );
                maxCustID = getLong( rs, 2 );
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: getLong( rs, 3 )" );
                numCustomers = getLong( rs, 3 );
            }
            else
            {
                errMsg = GSErrors.M_NO_RESULTS;
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnGetCounts: execute" );
                return false;
            }

            rs.close();
            rs = null;

       // Now process the query against ACCOUNTS table
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: execute(stmtCountAccounts)" );
            result = executeStmt( stmtCountAccounts );
            if (  ! result  )
                return result;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                    "DEBUG: stmtCountAccounts.getResultSet()" );
            rs = stmtCountAccounts.getResultSet();
            if (  rs == null  )
            {
                errMsg = GSErrors.M_NO_RESULTS;
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnGetCounts: execute" );
                return false;
            }

            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: stmtCountAccounts next()" );
            if (  next( rs )  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: getLong( rs, 1 )" );
                minAccountID = rs.getLong(1);
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: getLong( rs, 2 )" );
                maxAccountID = rs.getLong(2);
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: getLong( rs, 3 )" );
                numAccounts = rs.getLong(3);
            }
            else
            {
                errMsg = GSErrors.M_NO_RESULTS;
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnGetCounts: execute" );
                return false;
            }
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSTxnGetCounts: execute: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSTxnGetCounts: execute: Failover" );
            throw cfe;
        } catch (  SQLException sqe  ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            dbConn.rollback();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnGetCounts: execute" );
            return false;
        } finally {
            if (  rs != null )
            {
                rs.close();
                rs = null;
             }
        }

        // mark results as valid
        resultsAreValid = true;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITOK: GSTxnGetCounts: execute" );
        return true;
    } // execute

} // GSTxnGetCounts
