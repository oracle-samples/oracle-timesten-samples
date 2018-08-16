/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.math.BigDecimal;

/**
 *  Constants used by the GridSample application.
 */
public class GSConstants
{
    /* BUILD options */
    // Is debug allowed? Optimization to omit debug code elsewhere if it is 
    // globally disabled here.
    public static final boolean debugEnabled = false;

    // use TRUNCATE TABLE instead of DELETE FROM
    public static final boolean useTruncate = false;

    // enable 'commit read-only transactions' option
    public static final boolean enableCommitRO = false;
    /* end of BUILD options */
    
    // General constants
    public static final String progName = "GridSample"; // program name

    // Constants for database access

    // Default direct and C/S DSN names
    public static final String dfltDSN = "sampledb";    
    public static final String dfltDSNcs = "sampledbCS";    

    // default credentials
    public static final String dfltUID = "appuser";
    public static final String dfltPWD = null; // no default for password

    // Various things used to assemble a JDBC connection URL
    public static final String ttURLPrefix = "jdbc:timesten:";
    public static final String ttDirect = "direct:";
    public static final String ttClient = "client:";
    public static final String ttConnName = "ConnectionName";
    public static final String dfltConnName = progName;

    // Controls for retrying grid retryable errors
    public static final int DFLT_RETRY_LIMIT = 30;
    public static final int DFLT_RETRY_DELAY = 5; //ms

    // Controls for retrying on client connection failover
    public static final int DFLT_FAILOVER_LIMIT = 100;
    public static final int DFLT_FAILOVER_DELAY = 100; //ms

    // Workload related constants

    // Transaction names
    public static final String nameGetCounts = "GetCounts";
    public static final String nameClearHistory = "ClearHistory";
    public static final String nameAuthorize = "Authorize";
    public static final String nameUnknown = "Unknown";
    public static final String nameQuery = "Query";
    public static final String nameCharge = "Charge";
    public static final String nameTopup = "Topup";
    public static final String namePurge = "Purge";

    // Transaction type codes for workload transactions
    public static final int TXN_AUTHORIZE = 1;
    public static final int TXN_QUERY = 2;
    public static final int TXN_CHARGE = 3;
    public static final int TXN_TOPUP = 4;
    public static final int TXN_PURGE = 5;

    // Default transaction percentages (workload mix)
    public static final int dfltPctAuthorize = 70;
    public static final int dfltPctQuery = 5;
    public static final int dfltPctCharge = 15;
    public static final int dfltPctTopUp = 5;
    public static final int dfltPctPurge = 5;

    // Default parameter values and associated things
    public static final int dfltPurgeAge = 30;
    public static final BigDecimal dfltTopupValue = 
                                       new BigDecimal( 10 ).setScale( 2 );
    public static final double minChargeValue = 0.1;
    public static final double maxChargeValue = 1.0;
    public static final BigDecimal bdZero = new BigDecimal( 0 );

    // Command line option strings
    public static final String optHelp = "-help";
    public static final String optCS = "-cs";
    public static final String optDSN = "-dsn";
    public static final String optUID = "-uid";
    public static final String optPWD = "-pwd";
    public static final String optNoCleanup = "-nocleanup";
    public static final String optNumTxn = "-numtxn";
    public static final String optTxnMix = "-txnmix";
    public static final String optDuration = "-duration";
    public static final String optCommitRO = "-commitrotxn";
    public static final String optVerbose = "-verbose";
    public static final String optSilent = "-silent";
    public static final String optLog = "-log";
    public static final String optDebug = "-debug";

    // Verbosity levels and reporting intervals
    public static final int vbSilent = 0;
    public static final int vbNormal = 1;
    public static final int vbVerbose = 2;
    public static final int dfltReportInterval = 30;
    public static final int minReportInterval = 10;

} // GSConstants
