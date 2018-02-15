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
 *  GSUtil.c - various utility functions
 *
 ****************************************************************************/

#include "GridSample.h"

/*
 * Array of transaction names indexed by transaction type.
 *
 * Used to convert between a transaction type and a transatcion name.
 * See 'getTxnType()' and 'getTxnName()' below.
 */
static char * 
txnTypeMap[] =
{
    TXN_UPDGRIDINFO_NAME,
    TXN_GETCOUNTS_NAME,
    TXN_CLEARHISTORY_NAME,
    TXN_AUTHORIZE_NAME,
    TXN_QUERY_NAME,
    TXN_CHARGE_NAME,
    TXN_TOPUP_NAME,
    TXN_PURGE_NAME
};

// number of entries in the txnTypeMap array
static int
numTxnTypes = (sizeof(txnTypeMap) / sizeof(char *));

/*
 * The maximum allowed random number. Also used to indicate if the random
 * number mechanism has been initialised (> 0).
 */
static long maxRand = 0L;

/*
 * Signal receievd indicator. If != 0 then the value indicates the signal
 * that was last received.
 */
static volatile int 
sigReceived = 0;

/****************************************************************************
 *
 * Internal functions
 *
 ****************************************************************************/

/*
 * Signal handler function.
 *
 * This is the function invoked when a handled signal is received.
 *
 * INPUTS:
 *     sig - signal value
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
static void
sigHandler(
           int sig
          )
{
    sigReceived = sig;
} // sigHandler

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

/*
 * Sleep for a specified number of milliseconds.
 *
 * INPUTS:
 *     ms - number of ms to sleep for
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
sleepMs(
        unsigned int ms
       )
{
    struct timespec rqtm, rmtm;

    rqtm.tv_sec = (time_t)(ms / 1000);
    rqtm.tv_nsec = (long)(ms % 1000000);

    while (  nanosleep( &rqtm, &rmtm )  )
        rqtm = rmtm;
} // sleepMs

/*
 * Have we received a signal?
 *
 * INPUTS: None
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Number of signal last received or 0.
 */
int
signalReceived(
               void
              )
{
    return sigReceived;
} // signalReceived

/*
 * Install signal handlers for SIGHUP, SIGINT and SIGTERM.
 *
 * INPUTS: None
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or error code.
 */
int
installSignalHandlers(
                      void
                     )
{
    struct sigaction sa;

    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (  sigaction( SIGHUP, &sa, NULL )  )
        return ERR_SIGNAL;
    if (  sigaction( SIGINT, &sa, NULL )  )
        return ERR_SIGNAL;
    if (  sigaction( SIGTERM, &sa, NULL )  )
        return ERR_SIGNAL;

    return SUCCESS;
} // installSignalHandlers

/*
 * Global cleanup and termination function.
 *
 * INPUTS:
 *    ctxt     - pointer to a pointer to a context_t
      exitcode - process exit code to use for exit()
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing, does not return.
 */
void
cleanup(
        context_t * * ctxt,
        int           exitcode
       )
{
    if (  (ctxt != NULL) && (*ctxt != NULL) )
        freeContext( ctxt );

    exit( exitcode );
} // cleanup

/*
 * Check if a string might be a simple positive integer value.
 *
 * INPUTS:
 *     s - the string
 *
 * OUTPUTS: Nothing
 *
 * RETURNS:
 *      true or false
 */
boolean
isSimpleInt(
            char const * s
           )
{
    char * p;

    if (  (s == NULL) || (*s == '\0') || (strlen(s) > 9)  )
        return false;

    while (  *s  )
        if (  (*s < '0') || (*s > '9')  )
            return false;
        else
           s++;

    return true;
} // isSimpleInt

/*
 * Check if a string might be a simple positive long long value.
 *
 * INPUTS:
 *     s - the string
 *
 * OUTPUTS: Nothing
 *
 * RETURNS:
 *      true or false
 */
boolean
isSimpleLong(
             char const * s
            )
{
    char * p;

    if (  (s == NULL) || (*s == '\0') || (strlen(s) > 19)  )
        return false;

    while (  *s  )
        if (  (*s < '0') || (*s > '9')  )
            return false;
        else
           s++;

    return true;
} // isSimpleLong

/*
 * Free a 'context_t' and everything it contains.
 *
 * INPUTS:
 *     ctxt - a pointer to a pointer to a context_t
 *
 * OUTPUTS: Nothing
 *
 * RETURNS: Nothing
 */
void
freeContext(
            context_t ** ctxt
           )
{
    apptxn_t * txn = NULL;
    
    if (  (ctxt == NULL) || (*ctxt == NULL)  )
        return; // already freed

    // set all specific txn pointers to NULL
    (*ctxt)->ugitxn = NULL;
    (*ctxt)->gctxn = NULL;
    (*ctxt)->chtxn = NULL;
    (*ctxt)->ptxn = NULL;
    (*ctxt)->qtxn = NULL;
    (*ctxt)->atxn = NULL;
    (*ctxt)->ctxn = NULL;
    (*ctxt)->ttxn = NULL;

    // cleanup all transactions, free memory
    if (  (*ctxt)->apptxns != NULL  )
    {
        while (  (*ctxt)->apptxns->first != NULL  )
        {
            txn = (*ctxt)->apptxns->first;
            LIST_UNLINK( (*ctxt)->apptxns, txn );
            if (  txn->cleanup != NULL  )
                txn->cleanup( txn->parent );
            free( (void *)txn );
        }
        free( (void *)((*ctxt)->apptxns) );
    }

    // disconnect from the database amnd cleanup ODBC resources
    if (  (*ctxt)->dbconn != NULL  )
    {
        if (  (*ctxt)->dbconn->connected  )
        {
            ODBCRollback( (*ctxt)->dbconn );
            ODBCDisconnect( (*ctxt)->dbconn );
        }
        ODBCFreeConn( &((*ctxt)->dbconn) );
    }

    // free error list
    if (  (*ctxt)->errors != NULL  )
    {
        ODBCFreeErrors( &((*ctxt)->errors) );
        free( (void *)(*ctxt)->errors);
        (*ctxt)->errors = NULL;
    }

    // close log file
    if (  (*ctxt)->logOut != NULL  )
    {
        fclose( (*ctxt)->logOut );
        (*ctxt)->logOut = NULL;
    }

    // free memory and mark as freed
    free( (void *)(*ctxt) );
    *ctxt = NULL;
} // freeContext

/*
 * Allocate and initialise a 'context_t'
 *
 * INPUTS:
 *     nctxt - a pointer to a pointer to a context_t
 *
 * OUTPUTS:
 *     nctxt set to point to the allocated and initialized context_t
 *
 * RETURNS:
 *     Success or an error code.
 */
int
allocContext(
             context_t ** nctxt
            )
{
    int ret = SUCCESS;
    context_t * ctxt;

    // check parameter
    if (  nctxt == NULL  )
        return ERR_PARAM_INTERNAL;
    if (  *nctxt != NULL  )
        return ERR_STATE_INTERNAL;

    // Allocate context
    ctxt = calloc( 1, sizeof(context_t) );
    if (  ctxt == NULL  )
        return ERR_NOMEM;

    // Allocate 'apptxns' list
    ctxt->apptxns = (apptxnlist_t *)calloc( 1, sizeof(apptxnlist_t) );
    if (  ctxt->apptxns == NULL  )
    {
        free( (void *)ctxt );
        return ERR_NOMEM;
    }

    // Initialize all non-zero, non-NULL fields
    ctxt->progName = DFLT_PROGNAME;
    ctxt->dbDSN = DFLT_DSN;
    ctxt->dbUID = DFLT_UID;
    ctxt->dbPWD = DFLT_PWD;
    ctxt->doCleanup = true;
    ctxt->vbLevel = VB_NORMAL;
    ctxt->vbInterval = VB_DFLT_RPT_INTERVAL;
    ctxt->pctAuthorize = DFLT_PCT_AUTHORIZE;
    ctxt->pctQuery = DFLT_PCT_QUERY;
    ctxt->pctCharge = DFLT_PCT_CHARGE;
    ctxt->pctTopUp = DFLT_PCT_TOPUP;
    ctxt->pctPurge = DFLT_PCT_PURGE;

    ctxt->purgeAge = DFLT_PURGE_AGE;
    ctxt->connName = DFLT_CONN_NAME;
    ctxt->retryLimit = DFLT_RETRY_LIMIT;
    ctxt->failoverLimit = DFLT_FAILOVER_LIMIT;

    ret = ODBCAllocConn( &(ctxt->dbconn) );
    if (  ret == ERR_ODBC_NORMAL  )
        ctxt->errors = ctxt->dbconn->errors;

    if (  ret == SUCCESS  )
        *nctxt = ctxt;
    else
    {
        if (  ctxt->dbconn != NULL  )
            ODBCFreeConn( &(ctxt->dbconn) );
        if (  ctxt->apptxns != NULL  )
            free( (void *)ctxt->apptxns );
        free( (void *)ctxt );
    }

    return ret;
} // allocContext

/*
 * Get current clock time with millisecond precision.
 *
 * INPUTS: None
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Current time since the epoch expressed as milliseconds.
 */
long
getTimeMS(
          void
         )
{
    struct timeval tv;
    long currtim;

    if (  gettimeofday( &tv, NULL )  )
        return -1L;

    currtim = (long)tv.tv_sec * (long long)1000;
    currtim += (long)tv.tv_usec / (long long)1000;

    return currtim;
} // getTimeMS

/*
 * Generate a timestamp string for messages.
 *
 * INPUTS: None
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to a static buffer containing the timestamp string in the
 *     format YYYY-MM-DD hh:mm:ss.fff.
 */
char *
getTS(
      void
     )
{
static char tsstring[32];
    struct timeval tv;
    struct tm * tval;

    if (  gettimeofday( &tv, NULL )  )
        return NULL;

    tval = localtime( &(tv.tv_sec) );
    if (  tval == NULL  )
        return NULL;

    sprintf( tsstring,
             "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d.%3.3d",
             tval->tm_year + 1900,
             tval->tm_mon + 1,
             tval->tm_mday,
             tval->tm_hour,
             tval->tm_min,
             tval->tm_sec,
             tv.tv_usec / 1000 );

    return tsstring;
} // getTS

/*
 * Display (and log) an error message. Errors are always displayed and
 * also are always logged if logging or debug is enabled. If an ODBC error
 * list is passed, display the list of errors.
 *
 * INPUTS:
 *     ctxt     - a pointer to a valid context_t
 *     msg      - pointer to a message to display. If NULL is passed,
 *                uses ctxt->msgBuff.
 *     odbcerrs - an ODBC error list or NULL
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
displayError(
             context_t const     * ctxt,
             char const          * msg,
             odbcerrlist_t const * odbcerrs
            )
{
    char * ts;

    if (  ctxt != NULL  )
    {
        if (  msg == NULL  )
            msg = ctxt->msgBuff;
        ts = getTS();
        fprintf( stdout, "%s: %s\n", ts, msg );
        if (  ctxt->logOut != NULL  )
            fprintf( ctxt->logOut, "%s: %s\n", ts, msg );
        if (  odbcerrs != NULL  )
        {
            odbcerr_t * err = odbcerrs->first;
            while (  err != NULL  )
            {
                fprintf( stdout, "%s: %s / %d / %s\n",
                         ts, err->sqlstate, 
                         err->native_error, err->error_msg );
                if (  ctxt->logOut != NULL  )
                    fprintf( ctxt->logOut, "%s: %s / %d / %s\n",
                             ts, err->sqlstate, 
                             err->native_error, err->error_msg );
                err = err->next;
            }
        }
    }
} // displayError

/*
 * Log a message (if logging is enabled). Nothing is displayed.
 *
 * INPUTS:
 *     ctxt     - a pointer to a valid context_t
 *     msg      - pointer to a message to log. If NULL is passed,
 *                uses ctxt->msgBuff.
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
logMessage(
           context_t const * ctxt,
           char      const * msg
          )
{
    char * ts;

    if (  (ctxt != NULL) && (ctxt->logOut != NULL)  )
    {
        if (  msg == NULL  )
            msg = ctxt->msgBuff;
        ts = getTS();
        fprintf( ctxt->logOut, "%s: %s\n", ts, msg );
    }
} // logMessage

/*
 * Display and/or log a message. Messages are only displayed if
 * silent mode is not in effect. Messages are always logged if logging
 * or debug is in effect.
 *
 * INPUTS:
 *     ctxt     - a pointer to a valid context_t
 *     msg      - pointer to a message to display/log. If NULL is passed,
 *                uses ctxt->msgBuff.
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
displayMessage(
               context_t const * ctxt,
               char      const * msg
              )
{
    char * ts;

    if (  ctxt != NULL  )
    {
        if (  msg == NULL  )
            msg = ctxt->msgBuff;
        ts = getTS();
        if (  ctxt->vbLevel > VB_SILENT  )
            fprintf( stdout, "%s: %s\n", ts, msg );
        if (  ctxt->logOut != NULL  )
            fprintf( ctxt->logOut, "%s: %s\n", ts, msg );
    }
} // displayMessage

/*
 * Log a message if debug mode is enabled. Nothing is displayed.
 *
 * INPUTS:
 *     ctxt     - a pointer to a valid context_t
 *     msg      - pointer to a message to log. If NULL is passed,
 *                uses ctxt->msgBuff.
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
#if defined(DEBUG)
void
debugMessage(
             context_t const * ctxt,
             char      const * msg
            )
{
    char * ts;

    if (  (ctxt != NULL) && ctxt->debugMode && (ctxt->logOut != NULL)   )
    {
        if (  msg == NULL  )
            msg = ctxt->msgBuff;
        ts = getTS();
        fprintf( ctxt->logOut, "%s: %s\n", ts, msg );
    }
} // debugMessage
#endif // DEBUG

/*
 * Map a transaction name to a type.
 *
 * INPUTS:
 *     txnname  - a transaction name
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     The txn type code matching the name or TXN_INVALID (0) if the name
 *     cannot be matched (case sensitive).
 */
int
getTxnType(
           char const * txnname
          )
{
    int txntype = 0;

    if (  txnname != NULL  )
    {
        while (  txntype < numTxnTypes  )
        {
            if (  strcmp( txnname, txnTypeMap[txntype] ) == 0  )
                return txntype + 1;
            txntype += 1;
        }
    }

    return 0;
} // getTxnType

/*
 * Map a transaction type to a name.
 *
 * INPUTS:
 *     txntype  - a transaction code
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     The txn name matching the passed in code or NULL if the code is invalid.
 */
char *
getTxnName(
           int txntype
          )
{
    txntype--;
    if (  (txntype < 0) || (txntype >= numTxnTypes)  )
        return NULL;

    return txnTypeMap[txntype];
} // getTxnName

/*
 * Should we stop?
 *
 * Examines the signal state and the execution limit parameters and indicates
 * if the time has come to cease execution.
 *
 * INPUTS:
 *     ctxt - a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     true or false.
 */
boolean
shouldTerminate(
                context_t * ctxt
               )
{
    long now;
    int sig;

    sig = signalReceived(); // interrupt?
    if (  sig  ) // interrupted
    {
        if (  (ctxt->vbLevel != VB_SILENT) && (sig == SIGINT)  )
            printf("\010\010"); // erase '^C' from interrupt
        displayMessage( ctxt, MSG_INTERRUPT );
        return true;
    }

    if (  ! ctxt->limitExecution  )
        return false;

    // execution limit is defined
    if (  ctxt->runNumTxn > 0  )
    {
        // check if we have done the required number of txns
        if (  ctxt->totalTxnExecuted >= ctxt->runNumTxn  )
        {
            displayMessage( ctxt, MSG_TXN_LIMIT );
            return true;
        }
    }
    else
    {
        // check if we have run for the required time
        if (  ((getTimeMS() - ctxt->startTime) / 1000) >= ctxt->runDuration  )
        {
            displayMessage( ctxt, MSG_TIME_LIMIT );
            return true;
        }
    }

    return false;
} // shouldTerminate

/*
 * Get a random long in the range 'min' to 'max'.
 *
 * INPUTS:
 *     min - minimum value ( > 0)
 *     max - maximum value (> min)
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A random long in the range 'min' to 'max' inclusive or 0 if error.
 */
long
getRandomLong(
              long min,
              long max
             )
{
    long rval = 0;

    if (  maxRand == 0L  )
    {
#if defined(RAND_MAX)
        maxRand = RAND_MAX;
#else
        maxRand = (long)exp2(31) - 1L;
#endif
        srandom( (unsigned int)( (int)(getTimeMS() + getpid()) ) );
    }

    if (  (min >= max) || (max < 1) || (min < 0) || ((max-min) >= maxRand)  )
        rval = 0;
    else
        // rval = ( random() % ((max-min)+1) ) + min;
        rval = ( random() % (max-min) ) + min;

    return rval;
} // getRandomLong

/*
 * Get a random double in the range 'min' to 'max'.
 *
 * INPUTS:
 *     min - minimum value ( > 0.0)
 *     max - maximum value (> min)
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A random double in the range 'min' to 'max' inclusive or 0.0 if error.
 */
double
getRandomDouble(
                double min,
                double max
               )
{
    return (double)(getRandomLong( (long)min*100, (long)max*100 ) / 100.0);
} // getRandomDouble

/*
 * Get a random (but valid) CustomerID.
 *
 * INPUTS:
 *     ctxt - pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A random customer ID that is guaranteed to exist in the database.
 */
long
getRandomCustomer(
                  context_t * ctxt
                 )
{
    return getRandomLong( ctxt->minCustID, ctxt->maxCustID );
} // getRandomCustomer

/*
 * Get a random (but valid) AccountID.
 *
 * INPUTS:
 *     ctxt - pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A random account ID that is guaranteed to exist in the database.
 */
long
getRandomAccount(
                 context_t * ctxt
                )
{
    return getRandomLong( ctxt->minAccountID, ctxt->maxAccountID );
} // getRandomCustomer

/*
 * Randomly choose a transaction type based on the relative percentages
 * for each workload transaction.
 *
 * INPUTS:
 *     ctxt - pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A random txn type. The distribution of values follows the relative
 *     percentages specified for the workload transaction mix.
 */
int
chooseTxnType(
              context_t * ctxt
             )
{
    int rnd;

    rnd = (int)getRandomLong(0, 100);

    if (  rnd < (ctxt->pctAuthorize + ctxt->pctQuery +
                 ctxt->pctCharge + ctxt->pctTopUp)  )
    {
        if (  rnd < (ctxt->pctAuthorize + ctxt->pctQuery + ctxt->pctCharge)  )
        {
            if (  rnd < (ctxt->pctAuthorize + ctxt->pctQuery)  )
            {
                if (  rnd < ctxt->pctAuthorize  )
                    return TXN_AUTHORIZE;
                else
                    return TXN_QUERY;
            }
            else
                return TXN_CHARGE;
        }
        else
            return TXN_TOPUP;
    }
    else
        return TXN_PURGE;
} // chooseTxnType

