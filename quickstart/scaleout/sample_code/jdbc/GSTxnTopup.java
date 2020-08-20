/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.math.BigDecimal;
import java.util.*;
import java.sql.*;
import com.timesten.jdbc.*;

/**
 *  A class that represents the 'TopUp' transaction.
 *
 * This transaction applies a topup value to replenish the balance of an
 * account. it also logs an 'audit' record describing the transatcion in
 * the TRANSACTIONS table.
 */
public class GSTxnTopup
    extends GSAppTransaction
{
    // Static stuff
    private static final int INVALID_VALUE = -1;
    private static final String sqlQuery =
         "SELECT current_balance, prev_balance FROM accounts" +
         " WHERE account_id = :acc_id";
    private static final String sqlUpdate =
         "UPDATE accounts SET prev_balance = current_balance," +
                            " current_balance = current_balance + :adj_amt" +
                " WHERE account_id = :acc_id";
    private static final String sqlInsert =
         "INSERT INTO transactions (transaction_id, account_id," +
                                  " transaction_ts, description," +
                                  " optype, amount) VALUES" +
                                  " (txn_seq.NEXTVAL, :acc_id, SYSDATE," +
                                  " :trx_desc, 'T', :adj_amt)";
    
    // DB stuff
    private GSDbStatement stmtQuery = null;
    private GSDbStatement stmtUpdate = null;
    private GSDbStatement stmtInsert = null;
    private long accountID = INVALID_VALUE;
    private BigDecimal adjustAmount = GSConstants.bdZero;
    private String txnDescription = "Account Topup";
    private double currBalance = 0.0;
    private double prevBalance = 0.0;

    /**
     * Constructor
     */
    GSTxnTopup(
               GSDbConnection dbcon,
               GSGlobal cx
              )
    {
        super( dbcon, cx );
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, "DEBUG: ENTER: GSTxnTopup: Constructor" );
        myName = GSConstants.nameTopup;
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, "DEBUG: EXIT: GSTxnTopup: Constructor" );
    } // Constructor

    /**
     * Set 'accountID' parameter value
     */
    public boolean setAccountID(
                                long aID
                               )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: ENTER: GSTxnTopup: setAccountID" );
        if (  aID <= 0  )
        {
            errMsg = GSErrors.M_PARAM_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                             "DEBUG: EXITERR: GSTxnTopup: setAccountID" );
            return false;
        }

        errMsg = null;
        accountID = aID;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: EXITOK: GSTxnTopup: setAccountID" );
        return true;
    } // setAccountID

    /**
     * Set 'adjustAmount' parameter value
     */
    public boolean setAmount(
                             BigDecimal amt
                            )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: ENTER: GSTxnTopup: setAmount" );
        if (  amt.compareTo( GSConstants.bdZero ) <= 0  )
        {
            errMsg = GSErrors.M_PARAM_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                             "DEBUG: EXITERR: GSTxnTopup: setAmount" );
            return false;
        }

        errMsg = null;
        adjustAmount = amt;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: EXITOK: GSTxnTopup: setAmount" );
        return true;
    } // setamount

    /**
     * Cleanup the transaction
     */
    public void cleanup()
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: ENTER: GSTxnTopup: cleanup" );
        errMsg = null;
        if (  stmtQuery != null  )
        {
            stmtQuery.close();
            stmtQuery = null;
        }
        if (  stmtUpdate != null  )
        {
            stmtUpdate.close();
            stmtUpdate = null;
        }
        if (  stmtInsert != null  )
        {
            stmtInsert.close();
            stmtInsert = null;
        }
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: EXIT: GSTxnTopup: cleanup" );
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
                             "DEBUG: ENTER: GSTxnTopup: init" );
        cleanup();
        try {
            stmtQuery = dbConn.prepare( "TxnTopup_Query", sqlQuery );
            if (  stmtQuery != null  )
            {
                stmtUpdate = dbConn.prepare( "TxnTopup_Update", 
                                             sqlUpdate );
                if (  stmtUpdate == null  )
                {
                    stmtQuery.close();
                    stmtQuery = null;
                }
                else
                {
                    stmtInsert = dbConn.prepare( "TxnTopup_Insert", 
                                                 sqlInsert );
                    if (  stmtInsert == null  )
                    {
                        stmtQuery.close();
                        stmtQuery = null;
                        stmtUpdate.close();
                        stmtUpdate = null;
                    }
                    else
                        result = true;
                }
            }
        } catch (  GSGridRetryException gre  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                             "DEBUG: GSTxnTopup: init: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                             "DEBUG: GSTxnTopup: init: Failover" );
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
                             "DEBUG: EXITOK: GSTxnTopup: init" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnTopup: init" );
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
        {
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSTxnTopup: execute" );
            GSUtil.debugMessage( ctxt, "DEBUG: accountID = " + accountID );
            GSUtil.debugMessage( ctxt, "DEBUG: adjustAmount = " + 
                                       adjustAmount.toString() );
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: txnDescription >" + txnDescription + "<" );
        }
        if (  (accountID == INVALID_VALUE) ||
              (adjustAmount.compareTo( GSConstants.bdZero ) == 0) ||
              (txnDescription == null)  )
        {
            errMsg = GSErrors.M_NO_INPUT;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnTopup: execute" );
            return false;
        }

        errMsg = null;

        // Execute the Transaction
        try {
            // set parameters
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: setLong() for stmtQuery/accountID" );
            result = setLong( stmtQuery, 1, accountID );
            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: setBigDecimal() for stmtUpdate/adjustAmount" );
                result = setBigDecimal( stmtUpdate, 1, adjustAmount );
            }
            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: setBigDecimal() for stmtUpdate/adjustAmount" );
                result = setLong( stmtUpdate, 2, accountID );
            }

            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: setLong() for stmtInsert/accountID" );
                result = setLong( stmtInsert, 1, accountID );
            }
            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: setString() for stmtInsert/txnDescription" );
                result = setString( stmtInsert, 2, txnDescription );
            }
            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: setBigDecimal() for stmtInsert/adjustAmount" );
                result = setBigDecimal( stmtInsert, 3, adjustAmount );
            }

            // if successful, execute the query
            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: execute(stmtQuery)" );
                result = executeStmt( stmtQuery );
                if (  result  )
                {
                    // process results
                    if (  GSConstants.debugEnabled  )
                        GSUtil.debugMessage( ctxt, 
                            "DEBUG: stmtQuery.getResultSet()" );
                    rs = stmtQuery.getResultSet();
                    if (  rs == null  )
                        errMsg = GSErrors.M_NO_RESULTS;
                    else
                    {
                        // get the returned values
                        if (  GSConstants.debugEnabled  )
                            GSUtil.debugMessage( ctxt, 
                                "DEBUG: stmtQuery next()" );
                        if (  next( rs )  )
                        {
                            if (  GSConstants.debugEnabled  )
                                GSUtil.debugMessage( ctxt,
                                    "DEBUG: getDouble( rs, 1 )" );
                            currBalance = getDouble( rs, 1 );
                            if (  GSConstants.debugEnabled  )
                                GSUtil.debugMessage( ctxt,
                                    "DEBUG: getDouble( rs, 2 )" );
                            prevBalance = getDouble( rs, 2 );
                        }
                        else
                        {
                            errMsg = GSErrors.M_NO_RESULTS;
                            result = false;
                        }
                        rs.close();
                        rs = null;
                    }
                }
            }

            // if successful, execute the update
            if (  result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: execute(stmtUpdate)" );
                result = executeStmt( stmtUpdate );
                if (  result  )
                {
                    // check update count
                    int ucount = stmtUpdate.getUpdateCount();
                    if (  GSConstants.debugEnabled  )
                        GSUtil.debugMessage( ctxt,
                            "DEBUG: stmtUpdate.getUpdateCount() = " + ucount );
                    if (  ucount != 1  )
                    {
                        errMsg = GSErrors.M_ROWCOUNT_ERR + " (" + ucount + ")";
                        result = false;
                    }
                }
            }

            // if successful, execute the insert
            if (  result  )
            {
                GSUtil.debugMessage( ctxt, "DEBUG: execute stmtInsert" );
                result = executeStmt( stmtInsert );
                if (  result  )
                {
                    // check update count
                    int ucount = stmtInsert.getUpdateCount();
                    if (  GSConstants.debugEnabled  )
                        GSUtil.debugMessage( ctxt,
                            "DEBUG: stmtInsert.getUpdateCount() = " + ucount );
                    if (  ucount != 1  )
                    {
                        errMsg = GSErrors.M_ROWCOUNT_ERR + " (" + ucount + ")";
                        result = false;
                    }
                }
            }

            // if successful then commit, otherwise rollback
            if (  ! result  )
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: rollback" );
                dbConn.rollback();
            }
            else
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: commit" );
                result = dbConn.commitEx();
                if (  ! result  )
                    errMsg = dbConn.getLastError();
            }
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: GSTxnTopup: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, "DEBUG: GSTxnTopup: Failover" );
            throw cfe;
        } catch (  SQLException sqe  ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            dbConn.rollback();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, "DEBUG: GSTxnTopup: Exception" );
            result = false;
        } finally {
            if (  rs != null )
            {
                rs.close();
                rs = null;
             }
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITOK: GSTxnTopup: execute" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnTopup: execute" );
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

} // GSTxnTopup
