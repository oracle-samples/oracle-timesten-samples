/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.io.*;
import java.util.*;

/**
 * Global variables used by the GridSample application plus 
 * a 'cleanup' function. These are gathered together here in a 
 * 'context' to make it easier to make the application multi-threaded 
 * in the future if so desired.
 */
public class GSGlobal
{
    // Values from command line options
    public boolean doCS = false;
    public String dbDSN = GSConstants.dfltDSN;
    public String dbUID = GSConstants.dfltUID;
    public String dbPWD = GSConstants.dfltPWD;
    public boolean doCleanup = true;
    public long runNumTxn = 0;
    public long runDuration = 0;
    public int vbLevel = GSConstants.vbNormal;
    public int vbInterval = GSConstants.dfltReportInterval;
    public String logPath = null;
    public boolean debugMode = false;
    public int pctAuthorize = GSConstants.dfltPctAuthorize;
    public int pctQuery = GSConstants.dfltPctQuery;
    public int pctCharge = GSConstants.dfltPctCharge;
    public int pctTopUp = GSConstants.dfltPctTopUp;
    public int pctPurge = GSConstants.dfltPctPurge;

    // Other globals
    public int purgeAge = GSConstants.dfltPurgeAge;
    public String connName = GSConstants.dfltConnName;
    public boolean limitExecution = false;
    public int gridRetryLimit = GSConstants.DFLT_RETRY_LIMIT;
    public int failoverLimit = GSConstants.DFLT_FAILOVER_LIMIT;
    public PrintStream logOut = null; // log file
    // statistics and timers
    public long startTime = 0;
    public long stopTime = 0;
    public long totalGridRetries = 0;
    public long totalCSFailovers = 0;
    public long totalTxnExecuted = 0;
    public long totalTxnAuthorize = 0;
    public long totalTxnQuery = 0;
    public long totalTxnCharge = 0;
    public long totalTxnTopup = 0;
    public long totalTxnPurge = 0;
    public long totalAccountUpdates = 0;
    public long totalTransactionInserts = 0;
    public long totalTransactionDeletes = 0;
    public long intvlGridRetries = 0;
    public long intvlCSFailovers = 0;
    public long intvlTxnExecuted = 0;
    public long intvlTxnAuthorize = 0;
    public long intvlTxnQuery = 0;
    public long intvlTxnCharge = 0;
    public long intvlTxnTopup = 0;
    public long intvlTxnPurge = 0;
    public long lastReportTime  = 0;

    // last error message
    public String errMsg = null;

    // DB stuff
    public int rtCount = 0; // current transient error retry count
    public int foCount = 0; // current connection failover retry count
    public GSDbConnection dbConn =  null; // DB connection
    // specific transactions
    public GSTxnGetCounts txnGetCounts = null;
    public GSTxnClearHistory txnClearHistory = null;
    public GSTxnAuthorize txnAuthorize = null;
    public GSTxnQuery txnQuery = null;
    public GSTxnCharge txnCharge = null;
    public GSTxnTopup txnTopup = null;
    public GSTxnPurge txnPurge = null;
    public long minCustID = 0;    // Minimum cust_id value in CUSTOMERS table
    public long maxCustID = 0;    // Maximum cust_id value in CUSTOMERS table
    public long numCustomers = 0; // Number of rows in CUSTOMERS table
    public long minAccountID = 0; // Minimum account_id value in ACCOUNTS table
    public long maxAccountID = 0; // Minimum account_id value in ACCOUNTS table
    public long numAccounts = 0;  // Number of rows in ACCOUNTS table
    public int prevElementID = 0; // previous grid ElementID
    public int currElementID = 0; // current grid ElementID

    // List of transactions associated with this context
    private ArrayList<GSAppTransaction> appTxns = null;

    /**
     * Constructor
     */
    public GSGlobal()
    {
        appTxns = new ArrayList<GSAppTransaction>();
    } // Constructor

    /**
     * Add a txn to the list for this context
     */
    public void addTxn(
                       GSAppTransaction txn
                      )
    {
        errMsg = null;
        if (  txn != null  )
            appTxns.add(txn);
    }


    /**
     * Cleanup
     */
    public void cleanup()
    {
        // Remove all transactions associated with the context
        if (  appTxns != null  )
        {
            for (int i = 0; i < appTxns.size(); i++)
                appTxns.get(i).cleanup();
            appTxns.clear();
        }

        // disconnect from database
        if (  dbConn != null  )
        {
            dbConn.close( true );
            dbConn = null;
        }

        // Close log file, if open
        if (  logOut != null  )
        {
            try {
                logOut.close();
            } catch ( Exception e ) { ; }
            logOut = null;
        }
    } // cleanup

} // GSGlobal
