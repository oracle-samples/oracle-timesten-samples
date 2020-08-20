/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/****************************************************************************
 *
 * TimesTen Scaleout sample programs - GridSample
 *
 * GridSample.c - main source file
 *
 ****************************************************************************/

#include "GridSample.h"

/*
 * Display and/or log the execution summary report.
 */
void
displaySummary(
               context_t * ctxt
              )
{
    long nsecs;

    if (  ctxt == NULL  )
        return;

    nsecs = (ctxt->stopTime - ctxt->startTime) / 1000;

    sprintf( ctxt->msgBuff,
             "Execution time: %ld seconds", nsecs );
    displayMessage( ctxt, NULL );

    sprintf( ctxt->msgBuff,
             "Total transactions: %ld", ctxt->totalTxnExecuted );
    displayMessage( ctxt, NULL );

    if (  ctxt->totalTxnExecuted > 0  )
    {
        sprintf( ctxt->msgBuff,
                 "    Authorize : %ld", ctxt->totalTxnAuthorize );
        displayMessage( ctxt, NULL );

        sprintf( ctxt->msgBuff,
                 "    Charge    : %ld", ctxt->totalTxnCharge );
        displayMessage( ctxt, NULL );

        sprintf( ctxt->msgBuff,
                 "    Topup     : %ld", ctxt->totalTxnTopup );
        displayMessage( ctxt, NULL );

        sprintf( ctxt->msgBuff,
                 "    Query     : %ld", ctxt->totalTxnQuery );
        displayMessage( ctxt, NULL );

        sprintf( ctxt->msgBuff,
                 "    Purge     : %ld", ctxt->totalTxnPurge );
        displayMessage( ctxt, NULL );

        sprintf( ctxt->msgBuff,
                 "ACCOUNTS table rows: %ld updated", 
                 ctxt->totalAccountUpdates );
        displayMessage( ctxt, NULL );

        sprintf( ctxt->msgBuff,
                 "TRANSACTIONS table rows: %ld inserted, %ld deleted", 
                 ctxt->totalTransactionInserts,
                 ctxt->totalTransactionDeletes );
        displayMessage( ctxt, NULL );
    }

    sprintf( ctxt->msgBuff,
             "Total retries: %ld", ctxt->totalRetries );
    displayMessage( ctxt, NULL );

    sprintf( ctxt->msgBuff,
             "Total C/S failovers: %ld", ctxt->totalCSFailovers );
    displayMessage( ctxt, NULL );
} // displaySummary

/*
 * dbDisconnect - disconnect from the database performing all
 *                necessary cleanup
 */
void
dbDisconnect(
             context_t * ctxt,
             boolean     rollback
            )
{
    int ret = SUCCESS;

    if (  ctxt == NULL  )
        return;

    debugMessage( ctxt, "DEBUG: ENTER: dbDisconnect" );

    if (  (ctxt->dbconn != NULL) && ctxt->dbconn->connected  )
    { // connected
        if (  rollback  )
        { // rollback flag is set
            ret = ODBCRollback( ctxt->dbconn );
#if defined(DEBUG)
            sprintf( ctxt->msgBuff, 
                     "DEBUG: dbDisconnect: ODBCRollback: %d", ret );
            debugMessage( ctxt, NULL );
#endif
        }
        ret = ODBCDisconnect( ctxt->dbconn );
#if defined(DEBUG)
        sprintf( ctxt->msgBuff, 
                 "DEBUG: dbDisconnect: ODBCDisconnect: %d", ret );
        debugMessage( ctxt, NULL );
#endif
    }

    debugMessage( ctxt, "DEBUG: EXIT: dbDisconnect" );
} // dbDisconnect

/*
 * dbConnect - connect to the database and initialise a bunch
 *             of stuff
 */
int
dbConnect(
          context_t * ctxt
         )
{
    int ret;
    txnUpdateGridInfo_t * txnUGI = NULL;
    txnGetCounts_t * txnGC = NULL;
    txnClearHistory_t * txnCH = NULL;
    txnPurge_t * txnP = NULL;
    txnQuery_t * txnQ = NULL;
    txnAuthorize_t * txnA = NULL;
    txnCharge_t * txnC = NULL;
    txnTopup_t * txnT = NULL;

    if (  (ctxt == NULL) || (ctxt->dbconn == NULL)  )
        return ERR_PARAM_INTERNAL;

    if (  ctxt->dbconn->connected  )
        return ERR_STATE_INTERNAL;  // already connected

    debugMessage( ctxt, "DEBUG: ENTER: dbConnect" );

    // Connect to the database
    ret = ODBCConnect( ctxt->dbconn,
                       ctxt->dbDSN,
                       ctxt->dbUID,
                       ctxt->dbPWD,
                       ctxt->connName );
    if (  ret != SUCCESS )
    {
        displayError( ctxt, errorMessage( ret, ctxt ),
                      (ret==ERR_ODBC_NORMAL)?ctxt->dbconn->errors:NULL );
        goto error;
    }

    // Determine grid info.
    txnUGI = createTxnUpdateGridInfo( ctxt );
    if (  ret != SUCCESS )
        goto error;

    ret = initTxnUpdateGridInfo( txnUGI );
    if (  ret != SUCCESS )
        goto error;

    ret = executeTxnUpdateGridInfo( txnUGI );
    if (  ret != SUCCESS )
        goto error;

    // Get table row counts
    txnGC = createTxnGetCounts( ctxt );
    if (  ret != SUCCESS )
        goto error;

    ret = initTxnGetCounts( txnGC );
    if (  ret != SUCCESS )
        goto error;

    ret = executeTxnGetCounts( txnGC );
    if (  ret != SUCCESS )
        goto error;

    // Prepare for truncate TRANSACTIONS table, if required
    if (  ctxt->doCleanup )
    {
        txnCH = createTxnClearHistory( ctxt );
        if (  ret != SUCCESS )
            goto error;

        ret = initTxnClearHistory( txnCH );
        if (  ret != SUCCESS )
            goto error;
    }

    // Prepare all other transactions as required

    // Purge
    if (  ctxt->pctPurge > 0  )
    {
        txnP = createTxnPurge( ctxt );
        if (  ret != SUCCESS )
            goto error;

        ret = initTxnPurge( txnP );
        if (  ret != SUCCESS )
            goto error;
    }

    // Query
    if (  ctxt->pctQuery > 0  )
    {
        txnQ = createTxnQuery( ctxt );
        if (  ret != SUCCESS )
            goto error;

        ret = initTxnQuery( txnQ );
        if (  ret != SUCCESS )
            goto error;
    }

    // Authorize
    if (  ctxt->pctAuthorize > 0  )
    {
        txnA = createTxnAuthorize( ctxt );
        if (  ret != SUCCESS )
            goto error;

        ret = initTxnAuthorize( txnA );
        if (  ret != SUCCESS )
            goto error;
    }

    // Charge
    if (  ctxt->pctCharge > 0  )
    {
        txnC = createTxnCharge( ctxt );
        if (  ret != SUCCESS )
            goto error;

        ret = initTxnCharge( txnC );
        if (  ret != SUCCESS )
            goto error;
    }

    // TopUp
    if (  ctxt->pctTopUp > 0  )
    {
        txnT = createTxnTopup( ctxt );
        if (  ret != SUCCESS )
            goto error;

        ret = initTxnTopup( txnT );
        if (  ret != SUCCESS )
            goto error;
    }

    debugMessage( ctxt, "DEBUG: EXITOK: dbConnect" );

    return SUCCESS;

error:
#if defined(DEBUG)
    sprintf( ctxt->msgBuff, "DEBUG: EXITERR: dbConnect: %d", ret);
    debugMessage( ctxt, NULL );
#endif
    return ret;
} // dbConnect

/*
 * Display (and perhaps log) the interval report.
 */
void
displayReport(
              context_t * ctxt
             )
{
    long now;

    if (  ctxt == NULL  )
        return;

    // if not logging and not verbose then return
    if (  (ctxt->vbLevel != VB_VERBOSE) && (ctxt->logOut == NULL)  )
        return;

    // check if reporting interval has elapsed
    now = getTimeMS() / 1000;
    if (  now < (ctxt->lastReportTime + ctxt->vbInterval)  )
        return;

    // create message and log/display it as required
    sprintf( ctxt->msgBuff,
             "Eid=%d Txn=%ld A=%ld C=%ld T=%ld Q=%ld P=%ld R=%ld F=%ld",
             ctxt->currElementID,
             ctxt->intvlTxnExecuted,
             ctxt->intvlTxnAuthorize,
             ctxt->intvlTxnCharge,
             ctxt->intvlTxnTopup,
             ctxt->intvlTxnQuery,
             ctxt->intvlTxnPurge,
             ctxt->intvlRetries,
             ctxt->intvlCSFailovers );

    displayMessage( ctxt, ctxt->msgBuff );

    // reset interval statistics
    ctxt->lastReportTime = now;
    ctxt->intvlTxnExecuted = 0;
    ctxt->intvlTxnAuthorize = 0;
    ctxt->intvlTxnQuery = 0;
    ctxt->intvlTxnCharge = 0;
    ctxt->intvlTxnTopup = 0;
    ctxt->intvlTxnPurge = 0;
    ctxt->intvlRetries = 0;
    ctxt->intvlCSFailovers = 0;
} // displayReport

/*
 * Maintain statistics
 */
void
incrementCounters(
                  context_t * ctxt,
                  int         txnType,
                  apptxn_t  * apptxn
                 )
{
    int  ret = SUCCESS;
    long rowcount = 0;

    if (  (ctxt == NULL) || (apptxn == NULL)  )
        return;

    ctxt->totalTxnExecuted++;
    ctxt->intvlTxnExecuted++;
    switch (  txnType  )
    {
        case TXN_AUTHORIZE:
            ctxt->totalTxnAuthorize++;
            ctxt->intvlTxnAuthorize++;
            break;

        case TXN_QUERY:
            ctxt->totalTxnQuery++;
            ctxt->intvlTxnQuery++;
            break;

        case TXN_CHARGE:
            ctxt->totalTxnCharge++;
            ctxt->intvlTxnCharge++;
            ctxt->totalAccountUpdates++;
            ctxt->totalTransactionInserts++;
            break;

        case TXN_TOPUP:
            ctxt->totalTxnTopup++;
            ctxt->intvlTxnTopup++;
            ctxt->totalAccountUpdates++;
            ctxt->totalTransactionInserts++;
            break;

        case TXN_PURGE:
            ctxt->totalTxnPurge++;
            ctxt->intvlTxnPurge++;
            // get rowcount
            ret = apptxn->rowcount( apptxn->parent, &rowcount );
            if (  (ret == SUCCESS) && (rowcount > 0)  )
                ctxt->totalTransactionDeletes += rowcount;
            break;
    }
} // increment counters

/*
 * executeTxnGrid - Choose a transaction based on the workload profile,
 *                  generate input parameters (if required) and then
 *                  execute it. If it fails due to a grid transient error
 *                  or a client failover, perform the necessary recovery /
 *                  retry actions.
 */
boolean
executeTxnGrid(
               context_t * ctxt
              )
{
    boolean needReprepare = false;
    apptxn_t * apptxn = NULL;
    apptxn_t * tapptxn = NULL;
    int txnType = 0;
    int ret;

    if (  ctxt == NULL  )
        return false;

    debugMessage( ctxt, "DEBUG: ENTER: executeTxnGrid" );

    // choose a transaction type ramndpmly based on specified ratios
    txnType = chooseTxnType( ctxt );
    
    // generate input values as required
    switch (  txnType  )
    {
        case TXN_AUTHORIZE:
            apptxn = ctxt->atxn->txn;
            ctxt->atxn->accountID = getRandomAccount( ctxt );
            break;

        case TXN_QUERY:
            apptxn = ctxt->qtxn->txn;
            ctxt->qtxn->custID = getRandomCustomer( ctxt );
            break;

        case TXN_CHARGE:
            apptxn = ctxt->ctxn->txn;
            ctxt->ctxn->accountID = getRandomAccount( ctxt );
            ctxt->ctxn->adjustAmount = 
                -getRandomDouble( MIN_CHARGE_VALUE, MAX_CHARGE_VALUE );
            break;

        case TXN_TOPUP:
            apptxn = ctxt->ttxn->txn;
            ctxt->ttxn->accountID = getRandomAccount( ctxt );
            ctxt->ctxn->adjustAmount = DFLT_TOPUP_VALUE;
            break;

        case TXN_PURGE:
            apptxn = ctxt->ptxn->txn;
            break;
    }

    // reset retry and failover counts
    ctxt->rtCount = ctxt->retryLimit;
    ctxt->foCount = ctxt->failoverLimit;

    // execute transaction handling retries and failovers
    while (  (ctxt->rtCount > 0) && (ctxt->foCount > 0)  )
    {
        debugMessage( ctxt, "DEBUG: Start of retry loop" );
        ret = SUCCESS;
        if (  needReprepare  ) // connection has failed over
        {
            debugMessage( ctxt, "DEBUG: Re-preparing" );
            // try and pro-actively re-prepare *all* transactions
            tapptxn = ctxt->apptxns->first;
            while (  tapptxn != NULL  )
            {
                if (  (tapptxn->init != NULL) && (ret == SUCCESS)  )
                    ret = tapptxn->init( tapptxn->parent );
                tapptxn = tapptxn->next;
            }
            if (  ret == SUCCESS  ) // if okay, update grid info
                ret = ctxt->ugitxn->txn->execute( ctxt->ugitxn );
            if (  ret == SUCCESS  )
            { // failover completed successfully
                needReprepare = false;
                sprintf( ctxt->msgBuff, 
                         "Failover from element %d to element %d",
                         ctxt->prevElementID, ctxt->currElementID );
                displayMessage( ctxt, ctxt->msgBuff );
            }
        }
        if (  ret == SUCCESS  )
        {   // execute the selected transaction
            debugMessage( ctxt, "DEBUG: Executing" );
            ret = apptxn->execute( apptxn->parent );
        }
        if (  (ret == SUCCESS) && (apptxn->fetch != NULL)  )
        {  // if okay and txn returns a result set, retrieve the rows
            debugMessage( ctxt, "DEBUG: Fetching" );
            ret = apptxn->fetch( apptxn->parent );
            while (  ret == SUCCESS  )
                ret = apptxn->fetch( apptxn->parent );
            if (  (ret == SUCCESS) || (ret == ERR_ODBC_NO_DATA)  )
            { // close the cursor
                debugMessage( ctxt, "DEBUG: Closing" );
                ret = apptxn->close( apptxn->parent );
            }
        }

        if (  ret == SUCCESS  )
        { // maintain stats
            incrementCounters( ctxt, txnType, apptxn );
            return true; // return success indication
        }
        else
        if (  ret == ERR_ODBC_RETRYABLE  )
        { // update retry stats
            ctxt->intvlRetries++;
            ctxt->totalRetries++;
            if (  --ctxt->rtCount > 0  )
                logMessage( ctxt, "RETRY" );
        }
        else
        if (  ret == ERR_ODBC_FAILOVER  )
        { // update failover stats
            ctxt->intvlCSFailovers++;
            ctxt->totalCSFailovers++;
            needReprepare = true; // mark for re-prepare
            if (  --ctxt->foCount > 0  )
                logMessage( ctxt, "FAILOVER" );
        }
        else
        { // fatal error
            sprintf( ctxt->msgBuff, 
                     "*** Transaction %s failed", getTxnName(txnType) );
            displayMessage( ctxt, ctxt->msgBuff );
            return false; // return failure indication
        }
    }

    // if we reach here, we have run out of retries
    sprintf( ctxt->msgBuff, 
             "*** Transaction %s retries exhausted", getTxnName(txnType) );
    displayMessage( ctxt, ctxt->msgBuff );

    return false; // return failure indication
} // executeGridTxn

/*
 * runWorkload - runs the workload and accumulates statistics
 */
void
runWorkload(
            context_t * ctxt
           )
{
    sprintf( ctxt->msgBuff,
             "%s (A=%d%%,C=%d%%,T=%d%%,Q=%d%%,P=%d%%)",
             MSG_WKL_STARTED,
             ctxt->pctAuthorize,
             ctxt->pctCharge,
             ctxt->pctTopUp,
             ctxt->pctQuery,
             ctxt->pctPurge );
    displayMessage( ctxt, ctxt->msgBuff );

    // initialize last report time to current time
    ctxt->lastReportTime = getTimeMS() / 1000;

    // run until finished, interrupted or a fatal error occurs
    while (  ! shouldTerminate( ctxt ) && executeTxnGrid( ctxt )  )
        displayReport( ctxt );

    displayMessage( ctxt, MSG_WKL_ENDED );
} // runWorkload

/*
 * main() - drives everything
 */

int
main(
     int          argc,
     char const * argv[]
    )
{
    context_t * ctxt = NULL;
    int ret = SUCCESS;
    int sig;

    // Allocate and initialize the program context
    ret = allocContext( &ctxt );
    if (  ret  )
    {
        fprintf( stderr, "*** Failed to initialize context: %s\n",
                 errorMessage( ret, ctxt ) );
        cleanup( &ctxt, EXIT_ERROR );
    }

    // Parse command line options
    ret = parseArgs( ctxt, argc, argv );
    if (  ret  )
        cleanup( &ctxt, (ret==ERR_HELP)?EXIT_HELP:EXIT_PARAM );

    // Install signal handlers to catch interrupt signals
    ret = installSignalHandlers();
    if (  ret  )
    {
        displayError( ctxt, errorMessage( ret, ctxt ), NULL );
        cleanup( &ctxt, EXIT_ERROR );
    }

    // Record start time
    ctxt->startTime = getTimeMS();

    // display pre-connect message
    sprintf( ctxt->msgBuff,
             "Connecting to '%s' as '%s'",
             ctxt->dbDSN, ctxt->dbUID );
    displayMessage( ctxt, ctxt->msgBuff );

    // Connect to the database
    if (  dbConnect( ctxt ) != SUCCESS  )
        cleanup( &ctxt, EXIT_ERROR );

    sig = signalReceived(); // interrupt?
    if (  sig  ) // interrupted
    {
        if (  (ctxt->vbLevel != VB_SILENT) && (sig == SIGINT)  )
            printf("\010\010"); // erase '^C' from interrupt
        displayMessage( ctxt, MSG_INTERRUPT );
        cleanup( &ctxt, EXIT_INTERRUPT );
    }

    // display post-connect message
    if (  ctxt->isGrid  )
        sprintf( ctxt->msgBuff, 
                 "Connected to element %d",
                 ctxt->currElementID );
    else
        sprintf( ctxt->msgBuff, "Connected" );
    displayMessage( ctxt, ctxt->msgBuff );

    // cleanup TRANSACTIONS table if required
    if (  (ctxt->doCleanup) && (ctxt->chtxn != NULL)  )
    {
        logMessage( ctxt, "Truncating TRANSACTIONS table" );
        ret = executeTxnClearHistory( ctxt->chtxn );
        if (  ret != SUCCESS )
            cleanup( &ctxt, EXIT_ERROR );
        logMessage( ctxt, "Truncate complete" );
    }

    // Run the workload!
    runWorkload( ctxt );

    // display pre-disconnection message
    displayMessage( ctxt, "Disconnecting" );

    // Disconnect from the database
    dbDisconnect( ctxt, false );

    // display post-disconnection message
    displayMessage( ctxt, "Disconnected" );

    // Record stop time
    ctxt->stopTime = getTimeMS();

    // display/log summary
    displaySummary( ctxt );

    cleanup( &ctxt, SUCCESS );
} // main
