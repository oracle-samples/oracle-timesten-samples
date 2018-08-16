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
 *  A class that represents the 'Query' transaction
 *
 * This transaction returns information about a customer and their accounts.
 */
public class GSTxnQuery
    extends GSAppTransaction
{
    // Static stuff
    private static final int INVALID_VALUE = -1;
    private static final String sqlQuery =
         "SELECT c.cust_id, c.last_name, c.member_since, a.account_id," +
               " a.phone, s.status, a.current_balance" +
         " FROM customers c, accounts a, account_status s" +
        " WHERE c.cust_id = :cst_id AND c.cust_id = a.cust_id" +
          " AND a.status = s.status";
    
    // DB stuff
    private GSDbStatement stmtQuery = null;
    private GSDbResultSet queryRs = null;
    private long custID = INVALID_VALUE;

    /**
     * Constructor
     */
    GSTxnQuery(
               GSDbConnection dbcon,
               GSGlobal cx
              )
    {
        super( dbcon, cx );
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, "DEBUG: ENTER: GSTxnQuery: Constructor" );
        myName = GSConstants.nameQuery;
        returnsResultSet = true;
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx, "DEBUG: EXIT: GSTxnQuery: Constructor" );
    } // Constructor

    /**
     * Set 'custID' input value
     */
    public boolean setCustID(
                             long cID
                            )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, "DEBUG: ENTER: GSTxnQuery: setCustID" );
        if (  cID < 1  )
        {
            errMsg = GSErrors.M_PARAM_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                                 "DEBUG: EXITERR: GSTxnQuery: setCustID" );
            return false;
        }

        errMsg = null;
        custID = cID;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                             "DEBUG: EXITOK: GSTxnQuery: setCustID" );
        return true;
    } // setCustID

    /**
     * Cleanup the transaction
     */
    public void cleanup()
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, "DEBUG: ENTER: GSTxnQuery: cleanup" );
        errMsg = null;
        if (  stmtQuery != null  )
        {
            stmtQuery.close();
            stmtQuery = null;
        }
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, "DEBUG: EXIT: GSTxnQuery: cleanup" );
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
            GSUtil.debugMessage( ctxt, "DEBUG: ENTER: GSTxnQuery: init" );

        cleanup();
        try {
            stmtQuery = dbConn.prepare( "TxnQuery_Query", sqlQuery );
            if (  stmtQuery != null  )
                result = true;
        } catch (  GSGridRetryException gre  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                                 "DEBUG: GSTxnQuery: init: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            cleanup();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                                 "DEBUG: GSTxnQuery: init: Failover" );
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
                             "DEBUG: EXITOK: GSTxnQuery: init" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSTxnQuery: init" );
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
            GSUtil.debugMessage( ctxt, "DEBUG: custID = " + custID );

        if (  queryRs != null  )
        {
            try { queryRs.close(); } catch ( Exception e ) { ; }
            queryRs = null;
        }

        if (  custID == INVALID_VALUE  )
        {
            errMsg = GSErrors.M_NO_INPUT;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: EXITERR: GSTxnQuery: executeRS" );
            return rs;
        }

        errMsg = null;

        // Process the query
        try {
            // set parameters
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                    "DEBUG: setLong() for stmtQuery/custID" );
            result = setLong( stmtQuery, 1, custID );
            if (  result  ) // execute query
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: execute stmtQuery" );
                result = executeStmt( stmtQuery );
            }
            if (  result  )
            {
                // retrieve results
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                        "DEBUG: stmtQuery.getResultSet()" );
                rs = stmtQuery.getResultSet();
                if (  rs == null  )
                {
                    errMsg = GSErrors.M_NO_RESULTS;
                    if (  GSConstants.debugEnabled  )
                        GSUtil.debugMessage( ctxt,
                           "DEBUG: EXITERR: GSTxnQuery: executeRS:" +
                           " No results" );
                }
                else
                    queryRs = rs;
            }
        } catch (  GSGridRetryException gre  ) {
            if (  rs != null  ) { rs.close(); rs = null; }
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSTxnQuery: executeRS: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  rs != null  ) { rs.close(); rs = null; }
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSTxnQuery: executeRS: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  rs != null  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: EXITOK: GSTxnQuery: executeRS" );
            else
                GSUtil.debugMessage( ctxt,
                         "DEBUG: EXITERR: GSTxnQuery: executeRS" );
        }
        return rs;
    } // executeRS

    /**
     * Close any open result set.
     */
    public boolean close()
        throws GSGridRetryException,
               GSConnectionFailoverException
    {
        if (  queryRs != null  )
        {
            try { queryRs.close(); } catch ( Exception e ) { ; }
            queryRs = null;
        }
        return true;
    } // close

} // GSTxnQuery
