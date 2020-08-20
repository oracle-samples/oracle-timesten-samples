/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/*************************************************************************
 *
 * This is the 'Simple' TimesTen Grid Sample application.
 *
 *************************************************************************/

import java.io.*;
import java.sql.*;

/**
 * The main class for the GridSample application.
 */
public class GridSample
{
    //////////////////////////////////////////////////////////////////
    // Things relating to the shutdown hook to allow us to terminate
    // cleanly when we get interrupted (within reason)
    //////////////////////////////////////////////////////////////////

    // Set to shut things down
    private static boolean shuttingDown = false;

    // Set when finished shutting things down
    private static boolean shutdownDone = false;

    /**
     * Call to indicate that we want to shut down
     */
    private static synchronized void setShuttingDown()
    {
        shuttingDown = true;
    } // setShuttingDown

    /**
     * Are / should we be shutting down?
     */
    private static synchronized boolean isShuttingDown()
    {
        return shuttingDown;
    } // isShuttingDown

    /**
     * Call to indicate that we have finished shutting down
     */
    // Indicate that we have finished shutting down
    private static synchronized void setShutdown()
    {
        shutdownDone = true;
    } // setShutdown

    /**
     * Have we finished shutting down?
     */
    private static synchronized boolean isShutdown()
    {
        return shutdownDone;
    } // isShutdown

    /**
     * Shutdown thread class. Gets run by the shutdown hook
     */
    public static class shutdownThread
        extends Thread
    {
        private GSGlobal ctxt = null;

        /**
          * Constructor
          */
        public shutdownThread(
                              GSGlobal cx // needs access to the context
                             )
        {
            super("shutdown");
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( cx, 
                   "DEBUG: ENTER: shutdownThread: Constructor" );
            ctxt = cx;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( cx, 
                   "DEBUG: EXIT: shutdownThread: Constructor" );
        } // Constructor

        /**
          * Fired when shutdown hook is engaged. This may happen due to 
          * an interrupt (Ctrl-C etc.) or on normal termination.
          */
        public void run()
        {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: shutdownThread: run" );
            if (  ! isShutdown()  )
            { // We were interrupted, log/output message
                String msg = "*** Interrupt";
                if (  ctxt.vbLevel != GSConstants.vbSilent  )
                {
                    System.out.print( "\010\010" );
                    GSUtil.printMessage( ctxt, msg, true );
                }
                else
                    GSUtil.logMessage( ctxt, msg );
            }
            setShuttingDown(); // indicate that it is time to shut down
            while (  ! isShutdown()  ) // wait until shutdown complete
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: waiting for shutdown" );
                GSUtil.sleep( 10 ); // don't spin
            }
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXIT: shutdownThread: run" );
        } // run
    } // shutdownThread

    /**
     * Install shutdown hook
     */
    public static void setShutdownHook(
                                       GSGlobal ctxt
                                      )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: setShutdownHook" );
        Runtime.getRuntime().addShutdownHook(new shutdownThread( ctxt ));
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXIT: GridSample: setShutdownHook" );
    } // setShutdownHook

    /**
     * Main - does all the work
     */
    public static void main(
                            String arg[]
                           )
    {
        // create the context
        GSGlobal ctxt = new GSGlobal();
        String msg = null;

        // Process command line arguments, if any
        if (  ! parseArgs( ctxt, arg )  )
            gsCleanup( ctxt, 1 ); // error, so exit

        // Set the start time
        ctxt.startTime = System.currentTimeMillis();

        // display pre-connect message
        msg = "Connecting to '" + ctxt.dbDSN + 
                     "' as '" + ctxt.dbUID + "'";
        if (  ctxt.vbLevel != GSConstants.vbSilent  )
            GSUtil.printMessage( ctxt, msg, true );
        else
            GSUtil.logMessage( ctxt, msg );

        // Connect to the database
        if (  ! dbConnect( ctxt )  )
            gsCleanup( ctxt, 3 ); // error, so exit

        // display post-connect message
        msg = "Connected";
        if (  ctxt.dbConn.isGrid()  )
             msg = msg + " to element " + ctxt.dbConn.getGridElementID();
        if (  ctxt.vbLevel != GSConstants.vbSilent  )
            GSUtil.printMessage( ctxt, msg, true );
        else
            GSUtil.logMessage( ctxt, msg );

        // get table counts etc.
        if (  ! getTableData( ctxt )  )
            gsCleanup( ctxt, 4 ); // error, so exit

        // if requested, cleanup TRANSACTIONS table
        if (  ctxt.doCleanup && ! cleanupHistory( ctxt )  )
            gsCleanup( ctxt, 5 ); // error, so exit

        // install shutdown hook
        setShutdownHook( ctxt );

        // now run the workload
        runWorkload( ctxt );

        // display pre-disconnection message
        msg = "Disconnecting";
        if (  ctxt.vbLevel != GSConstants.vbSilent  )
            GSUtil.printMessage( ctxt, msg, true );
        else
            GSUtil.logMessage( ctxt, msg );

        dbDisconnect( ctxt );

        // display post-disconnection message
        msg = "Disconnected";
        if (  ctxt.vbLevel != GSConstants.vbSilent  )
            GSUtil.printMessage( ctxt, msg, true );
        else
           GSUtil.logMessage( ctxt, msg );

        // record stop time
        ctxt.stopTime = System.currentTimeMillis();

        // display/log summary
        displaySummary( ctxt );

        // indicate that we have shutdown, to suppress shutdown hook
        setShutdown();

        // Exit indicating 'success'
        gsCleanup( ctxt, 0 );
    } // main

    /**
     * Run the workload
     */
    private static void runWorkload(
                                    GSGlobal ctxt
                                   )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: runWorkload" );

        String msg = "Workload started (A=" + ctxt.pctAuthorize + "%,C=" +
                     ctxt.pctCharge + "%,T=" + ctxt.pctTopUp + "%,Q=" +
                     ctxt.pctQuery + "%,P=" + ctxt.pctPurge + "%)";
        if (  ctxt.vbLevel != GSConstants.vbSilent  )
            GSUtil.printMessage( ctxt, msg, true );
        else
            GSUtil.logMessage( ctxt, msg );

        // initialize last report time
        ctxt.lastReportTime = System.currentTimeMillis() / 1000;

        // run until finished or error
        while (  ! shouldTerminate( ctxt ) && 
                 executeTxnGrid( ctxt )  )
            displayReport( ctxt );

        msg = "Workload ended";
        if (  ctxt.vbLevel != GSConstants.vbSilent  )
            GSUtil.printMessage( ctxt, msg, true );
        else
            GSUtil.logMessage( ctxt, msg );

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXIT: GridSample: runWorkload" );
    } // runWorkload

    /**
     * Choose a transaction based on the workload profile, generate input
     * parameters if required and then execute it. If it fails due to a
     * GridRetryException or a ConnectionFailoverException, perform
     * retries.
     */
    private static boolean executeTxnGrid(
                                   GSGlobal ctxt
                                  )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: executeTxnGrid" );

        boolean result = true;
        boolean needReprepare = false;
        long delay = 0;
        GSAppTransaction txn = null; // 'generic' transaction
        GSDbResultSet rs = null;
        int txnType = GSUtil.chooseTxnType( ctxt ); // choose txn type

        switch (  txnType  )
        {
            case GSConstants.TXN_AUTHORIZE:
                txn = ctxt.txnAuthorize;
                ctxt.txnAuthorize.setAccountID( 
                                     GSUtil.getRandomAccount( ctxt ) );
                break;

            case GSConstants.TXN_QUERY:
                txn = ctxt.txnQuery;
                ctxt.txnQuery.setCustID( 
                                    GSUtil.getRandomCustomer( ctxt ) );
                break;

            case GSConstants.TXN_CHARGE:
                txn = ctxt.txnCharge;
                ctxt.txnCharge.setAccountID( 
                                     GSUtil.getRandomAccount( ctxt ) );
                ctxt.txnCharge.setAmount( 
                                     GSUtil.getRandomAmount( 
                                         GSConstants.minChargeValue,
                                         GSConstants.maxChargeValue).negate() );
                break;

            case GSConstants.TXN_TOPUP:
                txn = ctxt.txnTopup;
                ctxt.txnTopup.setAccountID( 
                                     GSUtil.getRandomAccount( ctxt ) );
                ctxt.txnTopup.setAmount( GSConstants.dfltTopupValue );
                break;

            case GSConstants.TXN_PURGE:
                txn = ctxt.txnPurge;
                break;
        }

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, "DEBUG: txnType = " + txnType +
                                           ", " + txn.getTxnName() );

        // Reset retry counts for transaction
        ctxt.rtCount = ctxt.gridRetryLimit;
        ctxt.foCount = ctxt.failoverLimit;

        while (  (ctxt.rtCount > 0) &&
                 (ctxt.foCount > 0)  ) // repeat up to retry limit
        {
            if (  delay > 0  ) // retry delay
            {
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: txn retry, delay = " + delay );
                GSUtil.sleep( delay );
            }
            try {
                if (  needReprepare  ) // failover occurred, need to reprepare
                {
                    if (  GSConstants.debugEnabled  )
                        GSUtil.debugMessage( ctxt, 
                            "DEBUG: invoking reprepareAll" );
                    result = ctxt.dbConn.reprepareAll();
                    if (  result  )
                    {
                        if (  GSConstants.debugEnabled  )
                            GSUtil.debugMessage( ctxt, 
                                "DEBUG: reprepareAll successful" );
                        needReprepare = false;

                        // count failovers
                        ctxt.intvlCSFailovers++;
                        ctxt.totalCSFailovers++;

                        // display/log failover message 
                        ctxt.prevElementID = ctxt.currElementID;
                        result = ctxt.dbConn.updateGridInfoEx();
                        if (  result  )
                        {
                            ctxt.currElementID = ctxt.dbConn.getGridElementID();
                            String msg = "Failover from element " + 
                                         ctxt.prevElementID +
                                         " to element " + 
                                         ctxt.currElementID;
                            if (  ctxt.vbLevel != GSConstants.vbSilent  )
                                GSUtil.printMessage( ctxt, msg, true );
                            else
                                GSUtil.logMessage( ctxt, msg );
                        }
                    }
                    else
                         GSUtil.printError( ctxt,
                                     "*** 'reprepareAll' failed: " + 
                                         ctxt.dbConn.getLastError(),
                                     true );
                }

                if (  result  ) // okay to execute
                {
                    if (  GSConstants.debugEnabled  )
                        GSUtil.debugMessage( ctxt, "DEBUG: invoking execute" );
                    if (  txn.resultIsResultSet()  )
                    {
                        rs = txn.executeRS();
                        result = (rs != null);
                    }
                    else
                        result = txn.execute();
                    if ( ! result ) // execute failed
                         GSUtil.printError( ctxt,
                                     "*** Transaction " + txn.getTxnName() +
                                     " execute failed: " + txn.getLastError(),
                                     true );
                    else
                    {
                        if (  GSConstants.debugEnabled  )
                            GSUtil.debugMessage( ctxt, 
                                             "DEBUG: execute successful" );

                        // maintain stats
                        incrementCounters( ctxt, txnType, txn );

                        // if we cared, we would process any results here
                        if (  txn.resultIsResultSet()  )
                        { // process result set results
                            if (  rs != null  )
                            {
                                while (  rs.next()  ) ;
                                result = txn.close();
                                rs = null;
                            }
                            if (  result && ctxt.commitRO  )
                                result = txn.commitTxn();
                        }
                    }
                }

                if (  GSConstants.debugEnabled  )
                {
                    if (  result  )
                        GSUtil.debugMessage( ctxt, 
                          "DEBUG: EXITOK: GridSample: executeTxnGrid" );
                    else
                        GSUtil.debugMessage( ctxt, 
                          "DEBUG: EXITERR: GridSample: executeTxnGrid" );
                }
                return result; 

            } catch (  GSConnectionFailoverException cfe  ) {
                // handle client connection failover
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: Client Failover Exception" );
                ctxt.foCount--;
                delay = cfe.failoverDelay();
                if (  rs != null  ) { rs.close(); rs = null; }
                needReprepare = true;

            } catch (  GSGridRetryException gre  ) {
                // handle grid retry
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt,
                        "DEBUG: Grid Retry Exception: " +
                                      gre.getSQLState() + "/" +
                                      gre.getErrorCode() );
                if (  ctxt.rtCount > 0  )
                {
                    ctxt.rtCount--;
                    ctxt.intvlGridRetries++;
                    ctxt.totalGridRetries++;
                    delay = gre.retryDelay();
                    if (  GSConstants.debugEnabled && (delay == 0)  )
                        GSUtil.debugMessage( ctxt, 
                            "DEBUG: txn retry immediately" );
                }
                if (  rs != null  ) { rs.close(); rs = null; }

            } catch (  SQLException sqe ) {
                // handle other exceptions - rs.next()
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, "DEBUG: SQL Exception" );
                GSUtil.printError( ctxt,
                                   "*** Transaction " + txn.getTxnName() +
                                   " rs.next() failed: " + rs.getLastError(),
                                   true );
                if (  rs != null  ) { rs.close(); rs = null; }
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: EXITERR: GridSample: executeTxnGrid" );
                return false;
            }
        }

        // if we getr here we have run out of retries
        GSUtil.printError( ctxt, "*** Transaction " + txn.getTxnName() +
                                 " retries exhausted", true );
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXITERR: GridSample: executeTxnGrid" );
        return false;
    } // executeTxnGrid

    /**
     * Maintain various transaction related statistics
     */
    private static void incrementCounters(
                                          GSGlobal ctxt,
                                          int txnType,
                                          GSAppTransaction txn
                                         )
    {
        ctxt.totalTxnExecuted++;
        ctxt.intvlTxnExecuted++;
        switch (  txnType  )
        {
            case GSConstants.TXN_AUTHORIZE:
                ctxt.totalTxnAuthorize++;
                ctxt.intvlTxnAuthorize++;
                break;

            case GSConstants.TXN_QUERY:
                ctxt.totalTxnQuery++;
                ctxt.intvlTxnQuery++;
                break;

            case GSConstants.TXN_CHARGE:
                ctxt.totalTxnCharge++;
                ctxt.intvlTxnCharge++;
                ctxt.totalAccountUpdates++;
                ctxt.totalTransactionInserts++;
                break;

            case GSConstants.TXN_TOPUP:
                ctxt.totalTxnTopup++;
                ctxt.intvlTxnTopup++;
                ctxt.totalAccountUpdates++;
                ctxt.totalTransactionInserts++;
                break;

            case GSConstants.TXN_PURGE:
                ctxt.totalTxnPurge++;
                ctxt.intvlTxnPurge++;
                ctxt.totalTransactionDeletes +=
                    txn.getRowCount();
                break;
        }
    }

    /**
     * Log interval report  and also display it (if in verbose mode)
     */
    private static void displayReport(
                                      GSGlobal ctxt
                                     )
    {
        // if not logging and not verbose then return
        if (  (ctxt.vbLevel != GSConstants.vbVerbose) &&
              (ctxt.logOut == null)  )
            return;

        // check if reporting interval has elapsed
        long now = System.currentTimeMillis() / 1000;
        if (  now < (ctxt.lastReportTime + ctxt.vbInterval)  )
            return;

        // create message and log/display it as required
        String msg = "Eid=" + ctxt.currElementID +
                     " Txn=" + ctxt.intvlTxnExecuted +
                     " A=" + ctxt.intvlTxnAuthorize +
                     " C=" + ctxt.intvlTxnCharge +
                     " T=" + ctxt.intvlTxnTopup +
                     " Q=" + ctxt.intvlTxnQuery +
                     " P=" + ctxt.intvlTxnPurge +
                     " R=" + ctxt.intvlGridRetries +
                     " F=" + ctxt.intvlCSFailovers;

        if (  ctxt.vbLevel == GSConstants.vbVerbose  )
            GSUtil.printMessage( ctxt, msg, true );
        else
            GSUtil.logMessage( ctxt, msg );

        // zero all interval statistics
        ctxt.lastReportTime = now;
        ctxt.intvlTxnExecuted = 0;
        ctxt.intvlTxnAuthorize = 0;
        ctxt.intvlTxnQuery = 0;
        ctxt.intvlTxnCharge = 0;
        ctxt.intvlTxnTopup = 0;
        ctxt.intvlTxnPurge = 0;
        ctxt.intvlGridRetries = 0;
        ctxt.intvlCSFailovers = 0;
    } // displayReport

    /**
     * Display/log execution summary report
     */
    private static void displaySummary(
                                       GSGlobal ctxt
                                      )
    {
        boolean silent = (ctxt.vbLevel == GSConstants.vbSilent);
        long nsecs = (ctxt.stopTime - ctxt.startTime) / 1000;

        if (  silent  )
        {
            GSUtil.logMessage( ctxt, "Execution time: " + nsecs + " seconds" );
            GSUtil.logMessage( ctxt, 
                               "Total transactions: " + ctxt.totalTxnExecuted );
            if (  ctxt.totalTxnExecuted > 0  )
            {
                GSUtil.logMessage( ctxt, 
                               "    Authorize : " + ctxt.totalTxnAuthorize );
                GSUtil.logMessage( ctxt, 
                               "    Charge    : " + ctxt.totalTxnCharge );
                GSUtil.logMessage( ctxt, 
                               "    Topup     : " + ctxt.totalTxnTopup );
                GSUtil.logMessage( ctxt, 
                               "    Query     : " + ctxt.totalTxnQuery );
                GSUtil.logMessage( ctxt, 
                               "    Purge     : " + ctxt.totalTxnPurge );
                GSUtil.logMessage( ctxt, "ACCOUNTS table rows: " + 
                              ctxt.totalAccountUpdates +
                              " updated" );
                GSUtil.logMessage( ctxt, "TRANSACTIONS table rows: " + 
                              ctxt.totalTransactionInserts +
                              " inserted, " +
                              ctxt.totalTransactionDeletes +
                              " deleted" );
            }
            GSUtil.logMessage( ctxt, 
                               "Total retries: " + ctxt.totalGridRetries );
            GSUtil.logMessage( ctxt, 
                               "Total C/S failovers: " + 
                               ctxt.totalCSFailovers );
        }
        else
        {
            GSUtil.printMessage( ctxt, 
                           "Execution time: " + nsecs + " seconds", true );
            GSUtil.printMessage( ctxt, 
                          "Total transactions: " + ctxt.totalTxnExecuted,
                          true );
            if (  ctxt.totalTxnExecuted > 0  )
            {
                GSUtil.printMessage( ctxt, 
                          "    Authorize : " + ctxt.totalTxnAuthorize,
                          true );
                GSUtil.printMessage( ctxt, 
                          "    Charge    : " + ctxt.totalTxnCharge,
                          true );
                GSUtil.printMessage( ctxt, 
                          "    Topup     : " + ctxt.totalTxnTopup,
                          true );
                GSUtil.printMessage( ctxt, 
                          "    Query     : " + ctxt.totalTxnQuery,
                          true );
                GSUtil.printMessage( ctxt, 
                          "    Purge     : " + ctxt.totalTxnPurge,
                          true );
                GSUtil.printMessage( ctxt, 
                          "ACCOUNTS table rows: " + 
                          ctxt.totalAccountUpdates +
                          " updated",
                          true );
                GSUtil.printMessage( ctxt, 
                          "TRANSACTIONS table rows: " + 
                          ctxt.totalTransactionInserts +
                          " inserted, " +
                          ctxt.totalTransactionDeletes +
                          " deleted",
                          true );
            }
            GSUtil.printMessage( ctxt, 
                          "Total retries: " + ctxt.totalGridRetries,
                          true );
            GSUtil.printMessage( ctxt, 
                          "Total C/S failovers: " + ctxt.totalCSFailovers,
                          true );
        }
    } // displaySummary

    /**
     * Get table min/max and row counts
     */
    private static boolean getTableData(
                                        GSGlobal ctxt
                                       )
    {
        boolean result = true;
        
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: getTableData" );

        try {
            ctxt.rtCount = ctxt.gridRetryLimit;
            ctxt.foCount = ctxt.failoverLimit;
            if (  ! ctxt.txnGetCounts.execute()  )
            {
                GSUtil.printError(  ctxt,
                    "*** GetCounts transaction failed: " +
                        ctxt.txnGetCounts.getLastError(),
                    true  );
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: EXITERR: GridSample: getTableData" );
                result = false;
            }
        } catch ( SQLException sqe ) {
                String msg = sqe.getSQLState() + ":"  +
                             sqe.getErrorCode() + ":" +
                             sqe.getMessage();
                GSUtil.printError(  ctxt,
                                    "*** GetCounts transaction failed: " + msg,
                                    true  );
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: EXITERR: GridSample: getTableData" );
                result = false;
        }

        if (  result  )
        {
            // retrieve values
            ctxt.minCustID = ctxt.txnGetCounts.getMinCustID();
            ctxt.maxCustID = ctxt.txnGetCounts.getMaxCustID();
            ctxt.numCustomers = ctxt.txnGetCounts.getNumCustomers();
            ctxt.minAccountID = ctxt.txnGetCounts.getMinAccountID();
            ctxt.maxAccountID = ctxt.txnGetCounts.getMaxAccountID();
            ctxt.numAccounts = ctxt.txnGetCounts.getNumAccounts();

            // log them if we are in DEBUG mode
            if (  GSConstants.debugEnabled  )
            {
                GSUtil.debugMessage( ctxt, 
                                     "minCustID    = " + ctxt.minCustID);
                GSUtil.debugMessage( ctxt, 
                                     "maxCustID    = " + ctxt.maxCustID);
                GSUtil.debugMessage( ctxt, 
                                     "numCustomers = " + ctxt.numCustomers);
                GSUtil.debugMessage( ctxt, 
                                     "minAccountID = " + ctxt.minAccountID);
                GSUtil.debugMessage( ctxt, 
                                     "maxAccountID = " + ctxt.maxAccountID);
                GSUtil.debugMessage( ctxt, 
                                     "numAccounts  = " + ctxt.numAccounts);
            }

            // validate them
            if (  (ctxt.minCustID < 1) || (ctxt.minAccountID < 1) ||
                  (ctxt.maxCustID <= ctxt.minCustID) ||
                  (ctxt.maxAccountID <= ctxt.minAccountID) ||
                  ((ctxt.maxCustID-ctxt.minCustID+1) != ctxt.numCustomers) ||
                  ((ctxt.maxAccountID-ctxt.minAccountID+1) != 
                        ctxt.numAccounts)  )
            {
                GSUtil.printError(  ctxt,
                                    "*** GetCounts transaction failed: " + 
                                    GSErrors.M_DATA_ERR,
                                    true  );
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                        "DEBUG: EXITERR: GridSample: getTableData" );
                result = false;
            }
        }

        ctxt.txnGetCounts.cleanup();

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITOK: GridSample: getTableData" );
            else
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITERR: GridSample: getTableData" );
        }
        return result;
    } // getTableData

    /**
     * Execute the 'ClearHistory' transaction
     */
    private static boolean cleanupHistory(
                                          GSGlobal ctxt
                                         )
    {
        boolean result = true;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: cleanupHistory" );

        GSUtil.logMessage( ctxt, "Truncating TRANSACTIONS table" );

        ctxt.rtCount = ctxt.gridRetryLimit;
        ctxt.foCount = ctxt.failoverLimit;
        try {
            if (  ! ctxt.txnClearHistory.execute()  )
            {
                GSUtil.printError(  ctxt,
                        "*** Unable to execute ClearHistory transaction: " +
                        ctxt.txnClearHistory.getLastError(),
                        true  );
                result = false;
            }
        } catch ( SQLException sqe ) {
                String msg = sqe.getSQLState() + ":"  +
                             sqe.getErrorCode() + ":" +
                             sqe.getMessage();
                GSUtil.printError(  ctxt,
                        "*** Unable to execute ClearHistory transaction: " +
                        msg,
                        true  );
                result = false;
        }

        GSUtil.logMessage( ctxt, "Truncate completed" );

        ctxt.txnClearHistory.cleanup();

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITOK: GridSample: cleanupHistory" );
            else
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITERR: GridSample: cleanupHistory" );
        }
        return result;
    } // cleanupHistory

    /**
     * Nicely cleanup all resources and then terminate with a specific 
     * exit code
     */
    private static void gsCleanup(
                                  GSGlobal ctxt,
                                  int exitCode
                                 )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: gsCLeanup" );

        // cleanup
        ctxt.cleanup();

        // Exit!
        System.exit( exitCode );
    } // gsCleanup

    /**
     * Connect to the database and initialize all transactions etc.
     */
    private static boolean dbConnect(
                                     GSGlobal ctxt
                                    )
    {
         boolean ret = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: dbConnect" );

         // Connect
         ctxt.dbConn = new GSDbConnection(
                                          ctxt.dbDSN,
                                          ctxt.doCS,
                                          ctxt.dbUID,
                                          ctxt.dbPWD,
                                          ctxt.connName
                                    );
        if (  ! ctxt.dbConn.connect()  )
        {
            GSUtil.printError(  ctxt,
                         "*** Failed to connect to database: " +
                         ctxt.dbConn.getLastError(),
                         true  );
            return false;
        }
        ctxt.currElementID = ctxt.dbConn.getGridElementID();
        ctxt.prevElementID = ctxt.currElementID;

        // Create all necessary Transaction objects
        ctxt.txnGetCounts = new GSTxnGetCounts( ctxt.dbConn, ctxt );
        if (  ctxt.doCleanup  )
            ctxt.txnClearHistory = new GSTxnClearHistory( ctxt.dbConn, ctxt );
        ctxt.txnAuthorize = new GSTxnAuthorize( ctxt.dbConn, ctxt );
        ctxt.txnQuery = new GSTxnQuery( ctxt.dbConn, ctxt );
        ctxt.txnCharge = new GSTxnCharge( ctxt.dbConn, ctxt );
        ctxt.txnTopup = new GSTxnTopup( ctxt.dbConn, ctxt );
        ctxt.txnPurge = new GSTxnPurge( ctxt.dbConn, ctxt );

        // Initialize all transactions. Any failiure, including failover
        // or grid retry, is considered fatal to the connection attempt

        // Reset retry counts for transaction
        ctxt.rtCount = ctxt.gridRetryLimit;
        ctxt.foCount = ctxt.failoverLimit;

        // GetCounts
        try {
            ret = ctxt.txnGetCounts.init();
        } catch ( SQLException sqe ) { ret = false; }
        if (  ! ret  )
        {
            GSUtil.printError(  ctxt,
                         "*** Failed to initialize GetCounts transaction: " +
                         ctxt.txnGetCounts.getLastError(),
                         true  );
            ctxt.dbConn.close(true);
            ctxt.dbConn = null;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXITERR: GridSample: dbConnect" );
            return false;
        }

        // ClearHistory
        if (  ctxt.doCleanup  )
        {
            // Reset retry counts for transaction
            ctxt.rtCount = ctxt.gridRetryLimit;
            ctxt.foCount = ctxt.failoverLimit;

            try {
                ret = ctxt.txnClearHistory.init();
            } catch ( SQLException sqe ) { ret = false; }
            if (  ! ret  )
            {
                GSUtil.printError(  ctxt,
                       "*** Failed to initialize ClearHistory transaction: " +
                       ctxt.txnClearHistory.getLastError(),
                       true  );
                ctxt.dbConn.close(true);
                ctxt.dbConn = null;
                if (  GSConstants.debugEnabled  )
                    GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITERR: GridSample: dbConnect" );
                return false;
            }
        }

        // Reset retry counts for transaction
        ctxt.rtCount = ctxt.gridRetryLimit;
        ctxt.foCount = ctxt.failoverLimit;

       // Authorize
        try {
            ret = ctxt.txnAuthorize.init();
        } catch ( SQLException sqe ) { ret = false; }
        if (  ! ret  )
        {
            GSUtil.printError(  ctxt,
                         "*** Failed to initialize Authorize transaction: " +
                         ctxt.txnAuthorize.getLastError(),
                         true  );
            ctxt.dbConn.close(true);
            ctxt.dbConn = null;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITERR: GridSample: dbConnect" );
            return false;
        }

        // Reset retry counts for transaction
        ctxt.rtCount = ctxt.gridRetryLimit;
        ctxt.foCount = ctxt.failoverLimit;

        // Query
        try {
            ret = ctxt.txnQuery.init();
        } catch ( SQLException sqe ) { ret = false; }
        if (  ! ret  )
        {
            GSUtil.printError(  ctxt,
                         "*** Failed to initialize Query transaction: " +
                         ctxt.txnQuery.getLastError(),
                         true  );
            ctxt.dbConn.close(true);
            ctxt.dbConn = null;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITERR: GridSample: dbConnect" );
            return false;
        }
  
        // Reset retry counts for transaction
        ctxt.rtCount = ctxt.gridRetryLimit;
        ctxt.foCount = ctxt.failoverLimit;

        // Charge
        try {
            ret = ctxt.txnCharge.init();
        } catch ( SQLException sqe ) { ret = false; }
        if (  ! ret  )
        {
            GSUtil.printError(  ctxt,
                         "*** Failed to initialize Charge transaction: " +
                         ctxt.txnCharge.getLastError(),
                         true  );
            ctxt.dbConn.close(true);
            ctxt.dbConn = null;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITERR: GridSample: dbConnect" );
            return false;
        }

        // Reset retry counts for transaction
        ctxt.rtCount = ctxt.gridRetryLimit;
        ctxt.foCount = ctxt.failoverLimit;

        // Topup
        try {
            ret = ctxt.txnTopup.init();
        } catch ( SQLException sqe ) { ret = false; }
        if (  ! ret  )
        {
            GSUtil.printError(  ctxt,
                         "*** Failed to initialize Topup transaction: " +
                         ctxt.txnTopup.getLastError(),
                         true  );
            ctxt.dbConn.close(true);
            ctxt.dbConn = null;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITERR: GridSample: dbConnect" );
            return false;
        }

        // Reset retry counts for transaction
        ctxt.rtCount = ctxt.gridRetryLimit;
        ctxt.foCount = ctxt.failoverLimit;

        // Purge
        try {
            ret = ctxt.txnPurge.init();
        } catch ( SQLException sqe ) { ret = false; }
        if (  ! ret  )
        {
            GSUtil.printError(  ctxt,
                         "*** Failed to initialize Purge transaction: " +
                         ctxt.txnPurge.getLastError(),
                         true  );
            ctxt.dbConn.close(true);
            ctxt.dbConn = null;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                       "DEBUG: EXITERR: GridSample: dbConnect" );
            return false;
        }

        // success
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXITOK: GridSample: dbConnect" );
        return true;
    } // dbConnect

    /**
     * Disconnect from the database
     */
    private static void dbDisconnect(
                                     GSGlobal ctxt
                                    )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: dbDisconnect" );
        // Close database connection with rollback
        if (  ! ctxt.dbConn.close( true )  )
            GSUtil.printError(  ctxt,
                         "*** Database disconnect reported an error: " +
                         ctxt.dbConn.getLastError(),
                         true  );
        ctxt.dbConn = null;
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXIT: GridSample: dbDisconnect" );
    }

    /**
     * Check if it is time to stop the workload
     */
    private static boolean shouldTerminate(
                                           GSGlobal ctxt
                                          )
    {
        String msg = null;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: ENTER: GridSample: shouldTerminate" );

        if (  isShuttingDown()  ) // interrupt?
        {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt, 
                    "DEBUG: EXIT: GridSample: shouldTerminate: true" );
            return true;
        }

        if (  ctxt.limitExecution  )
        { // execution limit is defined
            if (  ctxt.runNumTxn > 0  )
            {
                // check if we have done the required number of txns
                if (  ctxt.totalTxnExecuted >= ctxt.runNumTxn  )
                {
                    msg = "Transaction count limit reached";
                    if (  ctxt.vbLevel != GSConstants.vbSilent  )
                        GSUtil.printMessage( ctxt, msg, true );
                    else
                        GSUtil.logMessage( ctxt, msg );
                    if (  GSConstants.debugEnabled  )
                          GSUtil.debugMessage( ctxt, 
                            "DEBUG: EXIT: GridSample: shouldTerminate: true" );
                    return true;
                }
            }
            else
            {
                // check if we have run for the required time
                long now = System.currentTimeMillis();
                if (  ( ( now - ctxt.startTime ) / 1000 ) >= ctxt.runDuration  )
                {
                    msg = "Run time limit reached";
                    if (  ctxt.vbLevel != GSConstants.vbSilent  )
                        GSUtil.printMessage( ctxt, msg, true );
                    else
                        GSUtil.logMessage( ctxt, msg );
                    if (  GSConstants.debugEnabled  )
                        GSUtil.debugMessage( ctxt, 
                           "DEBUG: EXIT: GridSample: shouldTerminate: true" );
                    return true;
                }
            }
        }

        // not time to stop yet
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt, 
                   "DEBUG: EXIT: GridSample: shouldTerminate: false" );
        return false;
    } // shouldTerminate

    /**
     * Display help information
     */
    private static void displayUsage()
    {
        System.out.println("\nUsage:\n");
        System.out.println("  java " + GSConstants.progName + " [-help] [-cs] [-dsn dsname] [-uid username] [-pwd password]");
        System.out.println("           [-txnmix A,C,T,Q,P] [-nocleanup] [{-numtxn t | -duration s}]");
        if (  GSConstants.enableCommitRO  )
            System.out.println("           [-commitrotxn]\n");
        if (  GSConstants.debugEnabled  )
            System.out.println("           [{-silent | -verbose [n]}] [{-log logpath | -debug logpath}]\n");
        else
            System.out.println("           [{-silent | -verbose [n]}] [-log logpath]\n");
        System.out.println("Parameters:\n");
        System.out.println("  -help        - Displays this usage information and then exits. Any");
        System.out.println("                 other options specified are ignored.\n");
        System.out.println("  -cs          - Connect using client/server mode (default is direct mode).\n");
        System.out.println("  -dsn         - Connect to 'dsname' instead of the default DSN (" + 
                           GSConstants.dfltDSN + " or");
        System.out.println("                 " + GSConstants.dfltDSNcs + 
                           "). The type of the DSN must match the requested");
        System.out.println("                 connection type (direct or client/server).\n");
        System.out.println("  -uid         - Connect as user 'username' instead of the default user");
        System.out.println("                 (" + GSConstants.dfltUID + ").\n");
        System.out.println("  -pwd         - Connect using 'password'. If omitted then the user will be");
        System.out.println("                 prompted for the password.\n");
        System.out.println("  -txnmix      - Sets the transaction mix for the workload; A = percentage");
        System.out.println("                 of Authorize, C = percentage of Charge, T = percentage of");
        System.out.println("                 TopUp, Q = percentage of Query and P = percentage of Purge."); 
        System.out.println("                 All values are integers >= 0 and <= 100 and the sum of the");
        System.out.println("                 values must equal 100. If not specified, the default mix");
        System.out.println("                 is A=" + GSConstants.dfltPctAuthorize + ", C=" + 
                           GSConstants.dfltPctCharge + ", T=" + GSConstants.dfltPctTopUp + ", Q=" +
                           GSConstants.dfltPctQuery + " and P=" + GSConstants.dfltPctPurge + ".\n");
        System.out.println("  -nocleanup   - The transaction history table will not be truncated prior");
        System.out.println("                 to starting the workload.\n");
        System.out.println("  -numtxn      - The workload will run until it has completed 't' transactions");
        System.out.println("                 (t > 0) and then the program will terminate.\n");
        System.out.println("  -duration    - The workload will run for approximately 's' seconds (s > 0)");
        System.out.println("                 and then the program will terminate.\n");
        if (  GSConstants.enableCommitRO  )
        {
        System.out.println("  -commitrotxn - Commit read-only transactions (normally these are not");
        System.out.println("                 committed).\n");
        }
        System.out.println("  -silent      - The program will not produce any output other than to report");
        System.out.println("                 errors. Normally the program will report significant events");
        System.out.println("                 (connect, failover, ...) as it runs plus a workload summary");
        System.out.println("                 when it terminates.\n");
        System.out.println("  -verbose     - Execution statistics will be reported every 'n' seconds");
        System.out.println("                 (n > 0, default is " + GSConstants.dfltReportInterval +
                           ") as the program runs, in addition to");
        System.out.println("                 the normal reporting.\n");
        System.out.println("  -log         - Write an execution log to 'logpath'.\n");
        if (  GSConstants.debugEnabled  )
        {
            System.out.println("  -debug       - Write a debug log to 'logpath'. This log includes much");
            System.out.println("                 more detailed information than the regular log; the");
            System.out.println("                 extra lines include the flag 'DEBUG:' in the text.\n");
            System.out.println("  The '-log' and '-debug' options are mutually exclusive.\n");
        }
        System.out.println("  The '-numtxn' and '-duration' options are mutually exclusive. If neither");
        System.out.println("  is specified then the program will run until it is manually terminated");
        System.out.println("  using Ctrl-C, SIGTERM etc.\n");
        System.out.println("  The '-silent' and '-verbose' options are mutually exclusive.\n");
        System.out.println("Exit status:\n");
        System.out.println("  The exit code reflects the outcome of execution as follows:");
        System.out.println("    0  - Success");
        System.out.println("    1  - Parameter error");
        System.out.println("    2  - Help requested");
        System.out.println("   >2  - Fatal error encountered\n");
    } // displayUsage

    /**
     * Parse the -txnmix command line option
     */
    private static boolean parseTxnMix(
                                       GSGlobal ctxt,
                                       String txnmix
                                      )
    {
        String sVals[] = null;
        int iVals[] = null, n, total = 0;

        sVals = txnmix.split(",");
        n = sVals.length;
        if (  n != 5  )
            return false;
        iVals = new int[n];

        for (int i = 0; i < n; i++)
        {
            try {
                iVals[i] = Integer.parseInt(sVals[i]);
            } catch (NumberFormatException nfe) {
                return false;
            }
            if (  (iVals[i] < 0) || (iVals[i] > 100)  )
                return false;
            total += iVals[i];
        }
        if (  total != 100  )
            return false;
            
        ctxt.pctAuthorize = iVals[0];
        ctxt.pctCharge = iVals[1];
        ctxt.pctTopUp = iVals[2];
        ctxt.pctQuery = iVals[3];
        ctxt.pctPurge = iVals[4];

        return true;
    } // parseTxnMix

    /**
     * Parse and validate command line arguments, set defaults etc.
     */
    private static boolean parseArgs(
                                     GSGlobal ctxt,
                                     String arg[]
                                    )
    {
        int argno = 0;
        boolean foundCS = false;
        boolean foundDSN = false;
        boolean foundUID = false;
        boolean foundPWD = false;
        boolean foundTxnMix = false;
        boolean foundNoCleanup = false;
        boolean foundNumTxn = false;
        boolean foundDuration = false;
        boolean foundCommitRO = false;
        boolean foundSilent = false;
        boolean foundVerbose = false;
        boolean foundLog = false;
        boolean foundDebug = false;

        while (  argno < arg.length  )
        {
            if (  arg[argno].equals( GSConstants.optHelp )  )
            {
                displayUsage();
                gsCleanup( ctxt, 2 );
            }
            else
            if (  arg[argno].equals( GSConstants.optCS )  )
            {
                if (  foundCS  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optCS +
                                        "' options not permitted\n" );
                    return false;
                }
                foundCS = true;
                ctxt.doCS = true;
            }
            else
            if (  arg[argno].equals( GSConstants.optVerbose )  )
            {
                if (  foundVerbose  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optVerbose +
                                        "' options not permitted\n" );
                    return false;
                }
                if (  foundSilent  )
                {
                    System.err.println( "\n*** Cannot specify both '" + 
                                        GSConstants.optVerbose +
                                        "' and '" +
                                        GSConstants.optSilent +
                                        "' options\n" );
                    return false;
                }
                foundVerbose = true;
                ctxt.vbLevel = GSConstants.vbVerbose;
                if (  ++argno < arg.length  )
                {
                    int rptIntvl;
                    try {
                        rptIntvl = Integer.parseInt(arg[argno]);
                        ctxt.vbInterval = rptIntvl;
                        if ( rptIntvl < GSConstants.minReportInterval )
                        {
                            System.err.println( "\n*** Value for '" +
                                       GSConstants.optVerbose +
                                       "' must be at least " +
                                       GSConstants.minReportInterval +
                                       "\n" );
                            return false;
                        }
                    } catch (NumberFormatException nfe) {
                         argno--;
                    }
                }
            }
            else
            if (  GSConstants.enableCommitRO && arg[argno].equals( GSConstants.optCommitRO )  )
            {
                if (  foundCommitRO  )
                {
                    System.err.println( "\n*** Multiple '" +
                                        GSConstants.optCommitRO +
                                        "' options not permitted\n" );
                    return false;
                }
                foundCommitRO = true;
                ctxt.commitRO = true;
            }
            else
            if (  arg[argno].equals( GSConstants.optSilent )  )
            {
                if (  foundSilent  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optSilent +
                                        "' options not permitted\n" );
                    return false;
                }
                if (  foundVerbose  )
                {
                    System.err.println( "\n*** Cannot specify both '" + 
                                        GSConstants.optVerbose +
                                        "' and '" +
                                        GSConstants.optSilent +
                                        "' options\n" );
                    return false;
                }
                foundSilent = true;
                ctxt.vbLevel = GSConstants.vbSilent;
            }
            else
            if (  arg[argno].equals( GSConstants.optNoCleanup )  )
            {
                if (  foundNoCleanup  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optNoCleanup +
                                        "' options not permitted\n" );
                    return false;
                }
                foundNoCleanup = true;
                ctxt.doCleanup = false;
            }
            else
            if (  arg[argno].equals( GSConstants.optTxnMix )  )
            {
                if (  foundTxnMix  )
                {
                    System.err.println( "\n*** Multiple '" +
                                        GSConstants.optTxnMix +
                                        "' options not permitted\n" );
                    return false;
                }
                foundTxnMix = true;
                if (  ++argno >= arg.length  )
                {
                    System.err.println( "\n*** Missing value for '" +
                                        GSConstants.optTxnMix +
                                        "' option\n" );
                    return false;
                }
                if (  ! parseTxnMix( ctxt, arg[argno] )  )
                {
                    System.err.println( "\n*** Invalid value for '" +
                                        GSConstants.optTxnMix +
                                        "' option\n" );
                    return false;
                }
            }
            else
            if (  arg[argno].equals( GSConstants.optDSN )  )
            {
                if (  foundDSN  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optDSN +
                                        "' options not permitted\n" );
                    return false;
                }
                foundDSN = true;
                if (  ++argno >= arg.length  )
                {
                    System.err.println( "\n*** Missing value for '" +
                                        GSConstants.optDSN +
                                        "' option\n" );
                    return false;
                }
                ctxt.dbDSN = arg[argno];
            }
            else
            if (  arg[argno].equals( GSConstants.optUID )  )
            {
                if (  foundUID  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optUID +
                                        "' options not permitted\n" );
                    return false;
                }
                foundUID = true;
                if (  ++argno >= arg.length  )
                {
                    System.err.println( "\n*** Missing value for '" +
                                        GSConstants.optUID +
                                        "' option\n" );
                    return false;
                }
                ctxt.dbUID = arg[argno];
            }
            else
            if (  arg[argno].equals( GSConstants.optPWD )  )
            {
                if (  foundPWD  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optPWD +
                                        "' options not permitted\n" );
                    return false;
                }
                foundPWD = true;
                if (  ++argno >= arg.length  )
                {
                    System.err.println( "\n*** Missing value for '" +
                                        GSConstants.optPWD +
                                        "' option\n" );
                    return false;
                }
                ctxt.dbPWD = arg[argno];
            }
            else
            if (  arg[argno].equals( GSConstants.optLog )  )
            {
                if (  foundLog  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optLog +
                                        "' options not permitted\n" );
                    return false;
                }
                if (  foundDebug  )
                {
                    System.err.println( "\n*** Cannot specify both '" + 
                                        GSConstants.optLog +
                                        "' and '" +
                                        GSConstants.optDebug +
                                        "' options\n" );
                    return false;
                }
                foundLog = true;
                if (  ++argno >= arg.length  )
                {
                    System.err.println( "\n*** Missing value for '" +
                                        GSConstants.optLog +
                                        "' option\n" );
                    return false;
                }
                ctxt.logPath = arg[argno];
            }
            else
            if (  arg[argno].equals( GSConstants.optDebug )  )
            {
                if (  GSConstants.debugEnabled  )
                {
                    if (  foundDebug  )
                    {
                        System.err.println( "\n*** Multiple '" +
                                            GSConstants.optDebug +
                                            "' options not permitted\n" );
                        return false;
                    }
                    if (  foundLog  )
                    {
                        System.err.println( "\n*** Cannot specify both '" +
                                            GSConstants.optLog +
                                            "' and '" +
                                            GSConstants.optDebug +
                                            "' options\n" );
                        return false;
                    }
                    foundDebug = true;
                    if (  ++argno >= arg.length  )
                    {
                        System.err.println( "\n*** Missing value for '" +
                                            GSConstants.optDebug +
                                            "' option\n" );
                        return false;
                    }
                    ctxt.logPath = arg[argno];
                    ctxt.debugMode = true;
                }
                else
                {
                        System.err.println( "\n*** The '" + 
                                            GSConstants.optDebug +
                                            "' option is not enabled\n" );
                        return false;
                }
            }
            else
            if (  arg[argno].equals( GSConstants.optNumTxn )  )
            {
                if (  foundNumTxn  )
                {
                    System.err.println( "\n*** Multiple '" + 
                                        GSConstants.optNumTxn +
                                        "' options not permitted\n" );
                    return false;
                }
                if (  foundDuration  )
                {
                    System.err.println( "\n*** Cannot specify both '" + 
                                        GSConstants.optNumTxn +
                                        "' and '" +
                                        GSConstants.optDuration +
                                        "' options\n" );
                    return false;
                }
                if (  ++argno >= arg.length  )
                {
                    System.err.println( "\n*** Missing value for '" +
                                        GSConstants.optNumTxn +
                                        "' option\n" );
                    return false;
                }
                try {
                    ctxt.runNumTxn = Long.parseLong( arg[argno] );
                } catch ( NumberFormatException nfe ) { ; }
                if (  ctxt.runNumTxn <= 0  )
                {
                    System.err.println( "\n*** Invalid value for '" +
                                        GSConstants.optNumTxn +
                                        "' option\n" );
                    return false;
                }
                foundNumTxn = true;
                ctxt.limitExecution = true;
            }
            else
            if (  arg[argno].equals( GSConstants.optDuration )  )
            {
                if (  foundDuration  )
                {
                    System.err.println( "\n*** Multiple '" +
                                        GSConstants.optDuration +
                                        "' options not permitted\n" );
                    return false;
                }
                if (  foundNumTxn  )
                {
                    System.err.println( "\n*** Cannot specify both '" + 
                                        GSConstants.optNumTxn +
                                        "' and '" +
                                        GSConstants.optDuration +
                                        "' options\n" );
                    return false;
                }
                if (  ++argno >= arg.length  )
                {
                    System.err.println( "\n*** Missing value for '" +
                                        GSConstants.optDuration +
                                        "' option\n" );
                    return false;
                }
                try {
                    ctxt.runDuration = Long.parseLong( arg[argno] );
                } catch ( NumberFormatException nfe ) { ; }
                if (  ctxt.runDuration <= 0  )
                {
                    System.err.println( "\n*** Invalid value for '" +
                                        GSConstants.optDuration +
                                        "' option\n" );
                    return false;
                }
                foundDuration = true;
                ctxt.limitExecution = true;
            }
            else
            {
                System.err.println( "\n*** Invalid option '" + 
                                    arg[argno] + "'\n" );
                return false;
            }

            argno += 1;
        }

        // If no password specified, then prompt for one
        if (  ! foundPWD  )
        {
            java.sql.Timestamp ct =
                new java.sql.Timestamp(System.currentTimeMillis());
            String msg = ct + ": Password for '" + ctxt.dbUID + "'? ";
            GSPasswordReader pwdReader = new GSPasswordReader();
            ctxt.dbPWD = pwdReader.readPassword( msg );
        }

        // Check for 'null' password (empty string)
        if (  (ctxt.dbPWD != null) &&
              ctxt.dbPWD.equals("")  )
            ctxt.dbPWD = null;

        // If logging or debug was specified, open the log file
        if (  ctxt.logPath != null  )
        {
            try {
                ctxt.logOut =
                    new PrintStream( 
                        new FileOutputStream( ctxt.logPath ), true 
                                   );
            } catch ( FileNotFoundException fnfe ) {
                System.err.println("\n*** Unable to open log file '" + 
                                   ctxt.logPath + "'\n");
                return false;
            }
        }

        // If client-server mode but no DSN specified, use the default
        // C/S DSN instead of the default direct mode one
        if (  ctxt.doCS && ! foundDSN  )
            ctxt.dbDSN = GSConstants.dfltDSNcs;

        return true;
    } // parseArgs

} // GridSample

