/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

/************************************************************************
 * Main header file for GridSample sample program
 ************************************************************************/

#if !defined(_GRIDSAMPLE_H)
#define _GRIDSAMPLE_H

/************************************************************************
 * Include files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>

/* Set ODBC API version before including timesten.h */
/* Default is ODBC 3.5 */
#if defined(USEODBC25)
#define  ODBCVER              0x0250   // ODBC 2.5
#else
#define  ODBCVER              0x0350   // ODBC 3.5
#endif

#include "timesten.h"

/************************************************************************
 * Constants
 */

/*
 * Boolean values
 */
#define  false                0
#define  true                 1

/*
 * Default DSN
 */
#define DIRECT_DSN  "sampledb"
#define CLIENT_DSN  "sampledbCS"
#ifdef TTCLIENTSERVER
#define DFLT_DSN    CLIENT_DSN
#else
#define DFLT_DSN    DIRECT_DSN
#endif

/*
 * Connection related things
 */
#define  DFLT_UID             "appuser"
#define  DFLT_PWD             NULL
#define  ATTR_DSN             "DSN"
#define  ATTR_UID             "UID"
#define  ATTR_PWD             "PWD"
#define  ATTR_CONN_NAME       "ConnectionName"
#define  DFLT_CONN_NAME       "GridSample"

/*
 * Grid retries and failover retries
 */
#define  DFLT_RETRY_LIMIT      30  // max retries for transient errors
#define  DFLT_RETRY_DELAY      0   // retry delay (ms) for transient errors
#define  DFLT_FAILOVER_LIMIT   100 // max retries for failover errors
#define  DFLT_FAILOVER_DELAY   25  // retry delay (ms) for failover errors

/*
 * Transaction names for internal 'transactions'.
 */
#define  TXN_UPDGRIDINFO_NAME  "UpdateGridInfo"
#define  TXN_GETCOUNTS_NAME    "GetCounts"
#define  TXN_CLEARHISTORY_NAME "ClearHistory"
#define  TXN_AUTHORIZE_NAME    "Authorize"
#define  TXN_QUERY_NAME        "Query"
#define  TXN_CHARGE_NAME       "Charge"
#define  TXN_TOPUP_NAME        "Topup"
#define  TXN_PURGE_NAME        "Purge"

/*
 * Transaction types - numeric constants matching transaction names.
 */
#define  TXN_INVALID           0
#define  TXN_UPDGRIDINFO       1
#define  TXN_GETCOUNTS         2
#define  TXN_CLEARHISTORY      3
#define  TXN_AUTHORIZE         4
#define  TXN_QUERY             5
#define  TXN_CHARGE            6
#define  TXN_TOPUP             7
#define  TXN_PURGE             8

/*
 * Default workload mix - transaction percentages. These can be overridden 
 * via the -txnmix command line parameter.
 */
#define  DFLT_PCT_AUTHORIZE    70
#define  DFLT_PCT_QUERY        5
#define  DFLT_PCT_CHARGE       15
#define  DFLT_PCT_TOPUP        5
#define  DFLT_PCT_PURGE        5

/*
 * Other defaults
 */
#define  DFLT_PROGNAME         "GridSample"
#define  DFLT_PURGE_AGE        30 // age (seconds) for TRANSACTIONS rows to
                                  // be a candidate to purge
#define  DFLT_TOPUP_VALUE      10.0
#define  MIN_CHARGE_VALUE      0.1
#define  MAX_CHARGE_VALUE      1.0

/*
 * Command line options
 */
#define  OPT_HELP              "-help"
#define  OPT_DSN               "-dsn"
#define  OPT_UID               "-uid"
#define  OPT_PWD               "-pwd"
#define  OPT_NOCLEANUP         "-nocleanup"
#define  OPT_NUMTXN            "-numtxn"
#define  OPT_TXNMIX            "-txnmix"
#define  OPT_DURATION          "-duration"
#define  OPT_VERBOSE           "-verbose"
#define  OPT_SILENT            "-silent"
#define  OPT_LOG               "-log"
#define  OPT_DEBUG             "-debug"
#if defined(SPECIAL_FEATURES)
#define  OPT_COMMITROTXN       "-commitrotxn"
#endif // SPECIAL_FEATURES

/*
 * Various lengths.
 */
#define  MAX_LEN_DSN           30
#define  MAX_LEN_UID           30
#define  MAX_LEN_PWD           30
#define  MAX_LEN_CONNNAME      30
#define  MAX_LEN_CONNSTR       256
#define  MAX_MSG_LEN           1024

/*
 * Verbosity levels and reporting intervals
 */
#define  VB_SILENT             0
#define  VB_NORMAL             1
#define  VB_VERBOSE            2
#define  VB_MIN_RPT_INTERVAL   10
#define  VB_DFLT_RPT_INTERVAL  30

/************************************************************************
 * Code macros.
 */

/*
 * Link an item on a doubly linked list.
 *
 * NOTE: Error checking is limited; mis-use at your peril.
 */
#define   LIST_LINK(l,e) if ((l != NULL) && (e != NULL)) \
                         { \
                             if (l->first == NULL) \
                             { \
                                 l->first = l->last = e; \
                                 e->next = e->prev = NULL; \
                             } \
                             else \
                             { \
                                 e->next = NULL; \
                                 e->prev = l->last; \
                                 l->last = l->last->next = e; \
                             } \
                             l->numentries++; \
                          }

/*
 * Unlink an item from a doubly linked list.
 *
 * NOTE: Error checking is limited; mis-use at your peril.
 */
#define   LIST_UNLINK(l,e) if ((l != NULL) && (l->first != NULL) && \
                               (e != NULL)) \
                           { \
                               if (l->last == l->first) \
                               { \
                                   e->next = e->prev = NULL; \
                                   l->first = l->last = NULL; \
                               } \
                               else \
                               if (e == l->first) \
                               { \
                                   l->first = l->first->next; \
                                   if ( l->first != NULL ) \
                                       l->first->prev = NULL; \
                                   e->next = e->prev = NULL; \
                               } \
                               else \
                               if (e == l->last) \
                               { \
                                   l->last = l->last->prev; \
                                   if ( l->last != NULL ) \
                                       l->last->next = NULL; \
                                   e->next = e->prev = NULL; \
                               } \
                               else \
                               { \
                                   e->prev->next = e->next; \
                                   e->next->prev = e->prev; \
                                   e->next = e->prev = NULL; \
                               } \
                               l->numentries--; \
                           }

/************************************************************************
 * Error codes and messages.
 */

/*
 * Exit codes
 */
#define  EXIT_SUCCESS               0 // all good
#define  EXIT_PARAM                 1 // parameter error
#define  EXIT_HELP                  2 // user requested help on command line
#define  EXIT_INTERRUPT             3 // execution interrupted
#define  EXIT_ERROR                 4 // an execution error occurred

/*
 * Error codes and related things.
 */
#define  SUCCESS                    0 // no error
// Internal errors
#define  ERR_HELP                   1 // help requested
#define  ERR_PARAM_INTERNAL         2 // internal error - bad parameter passed
#define  ERR_STATE_INTERNAL         3 // internal error - state mismatch
#define  ERR_TYPE_MISMATCH_INTERNAL 4 // internal error - data type mismatch
#define  ERR_NOMEM                  5 // out of memory
// Other errors
#define  ERR_PARAM                  6 // parameter error (command line)
#define  ERR_SIGNAL                 7 // failed to setup signal handlers
#define  ERR_USER                   8 // not currently used
#define  ERR_DATA                   9 // data error
// ODBC error classifications
#define  ERR_ODBC_NO_DATA          10 // no data found
#define  ERR_ODBC_NOINFO           11 // no additional info available
#define  ERR_ODBC_NORMAL           12 // includes warnings
#define  ERR_ODBC_RETRYABLE        13 // retryable error
#define  ERR_ODBC_FAILOVER         14 // failover error

/*
 * Error messages for the above error codes.
 */
#define  ERRM_UNKNOWN                "* unnkown error *"
#define  ERRM_NO_ERROR               "* no error *"
#define  ERRM_HELP                   "* help requested *"
#define  ERRM_PARAM_INTERNAL         "internal error - invalid parameter"
#define  ERRM_STATE_INTERNAL         "internal error - invalid state"
#define  ERRM_TYPE_MISMATCH_INTERNAL "internal error - data type mismatch"
#define  ERRM_NOMEM                  "internal error - out of memory"
#define  ERRM_PARAM                  "parameter error"
#define  ERRM_SIGNAL                 "failed to install signal handlers"
#define  ERRM_USER                   "user defined error"
#define  ERRM_DATA                   "data error"
#define  ERRM_ODBC_NO_DATA           "ODBC error - no data returned"
#define  ERRM_ODBC_NOINFO            "ODBC error - no additional info"
#define  ERRM_ODBC_NORMAL            "ODBC error"
#define  ERRM_ODBC_RETRYABLE         "ODBC error - retryable"
#define  ERRM_ODBC_FAILOVER          "ODBC error - failover"

/*
 * Other messages
 */
#define  MSG_INTERRUPT              "*** Interrupt"
#define  MSG_TXN_LIMIT              "Transaction count limit reached"
#define  MSG_TIME_LIMIT             "Run time limit reached"
#define  MSG_WKL_STARTED            "Workload started"
#define  MSG_WKL_ENDED              "Workload ended"

/*
 * TimesTen error related things. Used for identifying and classifying
 * retryable errors.
 */
#define  TT_ERR_SQLSTATE_NULL        ""
#define  TT_ERR_SQLSTATE_RETRY       "TT005"
#define  TT_ERR_SQLSTATE_GENERAL     "S1000"

#define  TT_ERR_NATIVE_NONE          0
#define  TT_ERR_NATIVE_DEADLOCK      6002
#define  TT_ERR_NATIVE_LOCKTIMEOUT   6003
#define  TT_ERR_NATIVE_FAILOVER      47137

/************************************************************************
 * Structures and typedefs
 */

/*
 * Typedefs
 */
typedef int                        boolean;
typedef struct s_odbcerr           odbcerr_t;     // an ODBC error/warning
typedef struct s_odbcerrlist       odbcerrlist_t; // list of ODBC errors
typedef struct s_parambinding      parambinding_t; // ODBC parameter
typedef struct s_paramlist         paramlist_t;    // list of ODBC parameters
typedef struct s_colbinding        colbinding_t;   // ODBC column
typedef struct s_collist           collist_t;      // list of ODBC columns
typedef struct s_sqlstmt           sqlstmt_t;      // a SQL statement
typedef struct s_sqlstmtlist       sqlstmtlist_t;  // list of SQL statements
typedef struct s_dbconnection      dbconnection_t; // a database connection
typedef struct s_apptxn            apptxn_t;       // generic app transaction
typedef struct s_apptxnlist        apptxnlist_t;   // list of generix txns
typedef struct s_txnUpdateGridInfo txnUpdateGridInfo_t; // txn to get grid
                                                        // info
typedef struct s_txnGetCounts      txnGetCounts_t;      // txn to get data 
                                                        // counts
typedef struct s_txnClearHistory   txnClearHistory_t;   // txn to truncate
                                                        // TRANSACTIONS table
typedef struct s_txnPurge          txnPurge_t;     // workload Purge txn
typedef struct s_txnQuery          txnQuery_t;     // workload Query txn
typedef struct s_txnAuthorize      txnAuthorize_t; // workload Authorize txn
typedef struct s_txnCharge         txnCharge_t;    // workload Charge txn
typedef struct s_txnTopup          txnTopup_t;     // workload Topup txn
typedef struct s_context           context_t;      // app context

/*
 * ODBC error/warning structures
 */
#define  LEN_SQLSTATE          5
struct s_odbcerr    // represents a single ODBC error
{
    odbcerr_t      * next;
    odbcerr_t      * prev;
    char             sqlstate[LEN_SQLSTATE+1];
    char           * error_msg;
    int              native_error;
    int              retry_delay; // ms
}; // s_odbcerr

struct s_odbcerrlist // a  list of ODBC errors
{
    odbcerr_t      * first;
    odbcerr_t      * last;
    int              numentries;
}; // s_odbcerrlist

/*
 * Parameter binding structures
 */
struct s_parambinding // represents a bound parameter
{
    parambinding_t * next;
    parambinding_t * prev;
    sqlstmt_t      * stmt;
    SQLSMALLINT      paramno;
    SQLSMALLINT      InputOutputType;
    SQLSMALLINT      ValueType;
    SQLSMALLINT      ParameterType;
    SQLULEN          ColumnSize;
    SQLSMALLINT      DecimalDigits;
    SQLPOINTER       ParameterValuePtr;
    SQLLEN           BufferLength;
    SQLLEN           StrLen_or_IndPtr;
}; // s_parambinding

struct s_paramlist  // a list of bound parameters
{
    parambinding_t * first;
    parambinding_t * last;
    int              numentries;
}; // s_paramlist

/*
 * Column binding structures
 */
struct s_colbinding  // represents a bound column
{
    colbinding_t   * next;
    colbinding_t   * prev;
    sqlstmt_t      * stmt;
    SQLSMALLINT      colno;
    SQLSMALLINT      TargetType;
    SQLPOINTER       TargetValuePtr;
    SQLLEN           BufferLength;
    SQLLEN           StrLen_or_Ind;
}; // s_colbinding

struct s_collist     // a list of bound columns
{
    colbinding_t   * first;
    colbinding_t   * last;
    int              numentries;
}; // s_collist

/*
 * SQL statement structures
 */
struct s_sqlstmt     // represents a SQL statement
{
    sqlstmt_t      * next;
    sqlstmt_t      * prev;
    dbconnection_t * conn;    // associated database connection
    SQLHANDLE        hStmt;   // ODBC statement handle
    char const     * sqltext; // text of SQL statement
    paramlist_t    * params;  // list of ODBC parameters (input)
    collist_t      * cols;    // list of columns (result set)
    odbcerrlist_t  * errors;  // associated errors
}; // s_sqlstmt

struct s_sqlstmtlist  // a list of SQL statements
{
    sqlstmt_t      * first;
    sqlstmt_t      * last;
    int              numentries;
}; // s_sqlstmtlist

/*
 * Database Connection
 */
struct s_dbconnection
{
    SQLHANDLE        hEnv;      // ODBC environment handle (per connection)
    SQLHANDLE        hDbc;      // ODBC connection handle
    sqlstmtlist_t  * stmts;     // associated statements
    odbcerrlist_t  * errors;    // associated errors
    char           * dsn;       // DSN value
    char           * uid;       // UID value
    char           * pwd;       // PWD value
    char           * cname;     // ConnectionName value
    boolean          connected; // is it connected?
}; // s_dbconnection

/*
 * Application Transaction (generic). Structures to hold things common to all
 * application transactions.
 */
typedef int (*FPinit_t)(void *);            // init function
typedef int (*FPexecute_t)(void *);         // execute function
typedef int (*FPfetch_t)(void *);           // fetch function
typedef int (*FPclose_t)(void *);           // cursor close function
typedef int (*FProwcount_t)(void *,long *); // get rowcount function
typedef void (*FPcleanup_t)(void *);        // cleanup function

struct s_apptxn      // a generic transaction
{
    apptxn_t       * next;
    apptxn_t       * prev;
    context_t      * ctxt;     // associated context
    sqlstmtlist_t  * stmts;    // associated statements
    int              txntype;  // txn type code
    void           * parent;   // parent (specific) transaction
    FPinit_t       init;       // init function, never NULl
    FPexecute_t    execute;    // execute function, never NULl
    FPfetch_t      fetch;      // fetch function, can be NULL
    FPclose_t      close;      // cursor close function, can be NULL
    FProwcount_t   rowcount;   // get rowcount function, can be NULL
    FPcleanup_t    cleanup;    // cleanup function, never NULl
}; // s_apptxn

struct s_apptxnlist  // a list of generic transactions
{
    apptxn_t       * first;
    apptxn_t       * last;
    int              numentries;
}; // s_apptxnlist

/*
 * Structure representing the UpdateGridInfo transaction. 
 *
 * Retrieves information about the connected database (is it a grid database
 * or a regular database) and if it is a grid database, the database element 
 * that the application is currently connected to.
 */
struct s_txnUpdateGridInfo
{
    apptxn_t       * txn;              // associated generic txn
    sqlstmt_t      * isGridStmt;       // SQL statement to determine if DB
                                       // is a grid DB
    sqlstmt_t      * localElementStmt; // SQL statement to determine the
                                       // connected element's ID
    sqlstmt_t      * cursor;           // current cursor
    boolean          griddb;           // is DB a grid DB?
    int              localelement;     // id of connected element
}; // s_txnUpdateGridInfo

/*
 * Structure representing the GetCounts transaction.
 *
 * Retries the rowcount and minimum/maximum primary key values for the
 * CUSTOMERS and ACCOUNTS tables. These values are used by the workload code 
 * when generating random customer and account IDs .
 */
struct s_txnGetCounts
{
    apptxn_t       * txn;                // associated generic txn
    sqlstmt_t      * stmtCountCustomers; // SQL statement to get CUSTOMER counts
    sqlstmt_t      * stmtCountAccounts;  // SQL statement to get ACCOUNT counts
    sqlstmt_t      * cursor;             // current cursor
    long             minCustID;          // min customer ID
    long             maxCustID;          // max customer ID
    long             numCustomers;       // number of customers
    long             minAccountID;       // min account ID
    long             maxAccountID;       // max account ID
    long             numAccounts;        // number of accounts
}; // s_txnGetCounts

/*
 * Structure representing the ClearHistory transaction.
 *
 * Clears down the TRANSACTIONS table using TRUNCATE TABLE.
 */
struct s_txnClearHistory
{
    apptxn_t       * txn;          // associated generic txn
    sqlstmt_t      * truncateStmt; // SQL statement for TRUNCATE
}; // s_txnClearHistory

/*
 * Structure representing the Purge transaction.
 *
 * Removes 'old' rows from the TRANSACTIONS table to avoid unbounded growth.
 */
struct s_txnPurge
{
    apptxn_t       * txn;          // associated generic txn
    sqlstmt_t      * purgeStmt;    // SQL statement for purge operation
    int              purgeAge;     // age (seconds) used to determine 'old' rows
}; // s_txnPurge

/*
 * Structure representing the Query transaction.
 *
 * Retrieves information about a customer and all of their accounts.
 */
struct s_txnQuery
{
    apptxn_t       * txn;              // associated generic txn
    sqlstmt_t      * queryStmt;        // SQL statment for query
    long             custID;           // the customer ID for the query
    sqlstmt_t      * cursor;           // the cursor
    // row data; populated on each successful fetch
    boolean          rdvalid;          // is the data valid?
    long             cust_id;          // customer ID
    char           * last_name;        // last name
    char           * member_since;     // member_since; date
    long             account_id;       // account ID
    char           * phone;            // phone number
    short            status;           // status
    double           current_balance;  // current account balance
}; // s_txnQuery

/*
 * Structure representing the Authorize transaction.
 *
 * Checks that an account exists with the correct attributes and returns the
 * associated customer ID and phone number.
 */
struct s_txnAuthorize
{
    apptxn_t       * txn;            // associated generic txn
    sqlstmt_t      * authorizeStmt;  // SQL statement for the query
    long             accountID;      // the account ID for the query
    sqlstmt_t      * cursor;         // the cursor
    // row data; populated on each successful fetch
    boolean          rdvalid;        // is the data valid?
    long             cust_id;        // customer ID
    char           * phone;          // phone number
}; // s_txnAuthorize

/*
 * Structure representing the Charge transaction.
 *
 * Depletes a specific account's balance by a specified amount. Logs a
 * 'jurnal' record in the TRANSACTIONS table.
 */
struct s_txnCharge
{
    apptxn_t       * txn;          // associated generic txn
    sqlstmt_t      * selectStmt;   // SQL statement for the query
    sqlstmt_t      * updateStmt;   // SQL statement for the balance update
    sqlstmt_t      * insertStmt;   // SQL statement for the TRANSACTIONS insert
    sqlstmt_t      * cursor;       // the cursor
    long             accountID;    // target account ID
    double           adjustAmount; // amount to adjust balance by
}; // s_txnCharge

/*
 * Structure representing the Topup transaction.
 *
 * Replenishes a specified account by a specified amount.
 */
struct s_txnTopup
{
    apptxn_t       * txn;          // associated generic txn
    sqlstmt_t      * selectStmt;   // SQL statement for the query
    sqlstmt_t      * updateStmt;   // SQL statement for the balance update
    sqlstmt_t      * insertStmt;   // SQL statement for the TRANSACTIONS insert
    sqlstmt_t      * cursor;       // the cursor
    long             accountID;    // target account ID
    double           adjustAmount; // amount to adjust balance by
}; // s_txnTopup

/*
 * Application Context.
 *
 * All 'global' variables gathered together in a single structure for ease 
 * of management and access.
 */

struct s_context
{
    // Values from program arguments
    char const     * progName;       // program name
    char const     * dbDSN;          // DSN value
    char const     * dbUID;          // UID value
    char const     * dbPWD;          // PWD value
    boolean          doCleanup;      // clear down TRANSACTIONS at start?
    boolean          commitReadTxn;  // commit read-only transactions?
    long             runNumTxn;      // number of transactions to execute
    long             runDuration;    // time to run for (seconds)
    int              vbLevel;        // verbosity level
    int              vbInterval;     // verbosity reporting interval (seconds)
    char const     * logPath;        // path of log file
    boolean          debugMode;      // is debug mode enabled?
    int              pctAuthorize;   // % of Authorize txns in workload
    int              pctQuery;       // % of Query txns in workload
    int              pctCharge;      // % of Charge txns in workload
    int              pctTopUp;       // % of Topup txns in workload
    int              pctPurge;       // % of Purge txns in workload

    // Other 'globals'
    int              purgeAge;       // mnimum age for 'old' records (seconds)
    char           * connName;       // connection name
    boolean          limitExecution; // apply runtime limits?
    int              retryLimit;     // maximum retries for retryable errors
    int              failoverLimit;  // maximum retries for failovers
    FILE           * logOut;         // log file
    long             startTime;      // start time (ms)
    long             stopTime;       // stop time (ms)
    // totals for statistics
    long             totalRetries;   
    long             totalCSFailovers;
    long             totalTxnExecuted;
    long             totalTxnAuthorize;
    long             totalTxnQuery;
    long             totalTxnCharge;
    long             totalTxnTopup;
    long             totalTxnPurge;
    long             totalAccountUpdates;
    long             totalTransactionInserts;
    long             totalTransactionDeletes;
    // interval statistics
    long             intvlRetries;
    long             intvlCSFailovers;
    long             intvlTxnExecuted;
    long             intvlTxnAuthorize;
    long             intvlTxnQuery;
    long             intvlTxnCharge;
    long             intvlTxnTopup;
    long             intvlTxnPurge;
    long             lastReportTime ;

    // DB related stuff
    dbconnection_t * dbconn;        // database connection
    odbcerrlist_t  * errors;        // last error stack
    long             minCustID;     // min and max customer IDs
    long             maxCustID;
    long             numCustomers;  // number of customers
    long             minAccountID;  // min and max account IDs
    long             maxAccountID;
    long             numAccounts;   // number of accounts
    boolean          isGrid;        // a grid DB?
    int              prevElementID; // the previously connected element
    int              currElementID; // the current connected element
    int              rtCount;       // current retry count
    int              foCount;       // current failover count

    // Transactions
    apptxnlist_t        * apptxns;  // list of generic transactions
    txnUpdateGridInfo_t * ugitxn;   // UpdateGridInfo txn
    txnGetCounts_t      * gctxn;    // GetCounts txn
    txnClearHistory_t   * chtxn;    // ClearHistory txn
    txnPurge_t          * ptxn;     // Purge txn
    txnQuery_t          * qtxn;     // Query txn
    txnAuthorize_t      * atxn;     // Authorize txn
    txnCharge_t         * ctxn;     // Charge txn
    txnTopup_t          * ttxn;     // Topup ntxn

    // Error handling
    int              errNumber;     // last error code
   
    // Message buffer
    char             msgBuff[MAX_MSG_LEN+1]; // general usage message buffer
}; // s_context


/************************************************************************
 * Functions
 *************************************************************************/

/*************************************************************************
 * Utility Functions
 */

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
       );

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
              );

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
                     );

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
       );

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
           );

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
            );

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
         );

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
     );

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
           );

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
             context_t ** ctxt
            );

/*
 * Translate an error code into an error message.
 *
 * INPUTS:
 *     errorCode - a valid error code
 *     ctxt      - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the error message textt that corresponds to the
 *     error code. The data pointed to is read only and statically allocated.
 */
char const *
errorMessage(
             int         errorCode,
             context_t * ctxt
            );

/*
 * Check if an error and associated error stack represents a retryable error.
 *
 * INPUTS:
 *     err      - error code
 *     errstack - ODBC error stack
 *
 * OUTPUTS:
 *     delay - pointer to an integer to receive the retry delay value if this
 *             error is retryable (can be NULL if the value is not required)
 *
 * RETURNS:
 *     True if the error is a retryable error, false otherwise.
 */
boolean
isRetryable(
            int             err,
            odbcerrlist_t * errstack,
            int           * delay
           );

/*
 * Check if an error and associated error stack represents a connection
 * failover error (client/server mode only).
 *
 * INPUTS:
 *     err      - error code
 *     errstack - ODBC error stack
 *
 * OUTPUTS:
 *     delay - pointer to an integer to receive the retry delay value if this
 *             error is a failover (can be NULL if the value is not required)
 *
 * RETURNS:
 *     True if the error is a failover error, false otherwise.
 */
boolean
isFailover(
           int             err,
           odbcerrlist_t * errstack,
           int           * delay
          );

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
            );

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
          );

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
              );

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
            );
#else
#define debugMessage( ctxt, msg )
#endif // DEBUG

/*
 * Display program usage information.
 *
 * INPUTS:
 *     ctxt - pointer to a valid context_t.
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
displayUsage(
             context_t const * ctxt
            );

/*
 * Parse and validate the command line arguments.
 *
 * Stores the extracted values in the context.
 *
 * INPUTS:
 *     ctxt   - pointer to a valid context_t
 *     argc   - numbver of command line arguments
 *     argv   - array of pointers to the arguments
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code. Also displays error messages as required.
 */
int
parseArgs(
          context_t  * ctxt,
          int          argc,
          char const * argv[]
         );

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
          );

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
          );

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
               );

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
             );

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
               );

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
                 );

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
                );

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
             );

/*************************************************************************
 * ODBC Functions
 */

/*
 * Empty an error list.
 *
 * INPUTS:
 *     errlist - a pointer to a pointer to the list
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
ODBCFreeErrors(
               odbcerrlist_t ** errlist
              );

/*
 * Build an error and warning list for a handle
 *
 * INPUTS:
 *     handleType - the type of the ODBC handle
 *     handle     - the ODBC handle
 *
 * OUTPUTS:
 *     errlist - a pointer to a pointer to the list
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetErrors(
              odbcerrlist_t ** errlist,
              SQLSMALLINT      handleType,
              SQLHANDLE        handle
             );

/*
 * Free a statement structure and unlink it from the associated connection.
 *
 * INPUTS:
 *     stmt - a pointer to a pointer to the statement structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCFreeStmt(
             sqlstmt_t ** stmt
            );

/*
 * Allocate a statement structure and link it to the associated connection.
 *
 * INPUTS:
 *     conn - a pointer to the database connection
 *
 * OUTPUTS:
 *     stmt - a pointer to a pointer to the allocated structure
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCAllocStmt(
              dbconnection_t * conn,
              sqlstmt_t     ** stmt
             );

/*
 * Free memory and ODBC resources for a connection.
 *
 * INPUTS:
 *     conn - a pointer to a pointer to the connection structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCFreeConn(
             dbconnection_t ** conn
            );

/*
 * Allocate and initialize a connection structure including environment and
 * connection handles.
 *
 * INPUTS:
 *     conn - a pointer to a pointer to the connection structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCAllocConn(
              dbconnection_t ** conn
             );

/*
 * Issue a commit on a connection.
 *
 * INPUTS:
 *     conn - a pointer to the connection structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCCommit(
           dbconnection_t * conn
          );

/*
 * Issue a rollback on a connection.
 *
 * INPUTS:
 *     conn - a pointer to the connection structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCRollback(
             dbconnection_t * conn
            );

/*
 * Disconnect a connection.
 * INPUTS:
 *     conn - a pointer to the connection structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCDisconnect(
               dbconnection_t * conn
              );

/*
 * Connect to the database.
 *
 * INPUTS:
 *     conn  - a pointer to the connection structure
 *     dsn   - the target DSN
 *     uid   - the username (can be NULL for direct mode connections)
 *     pwd   - the password (can be NULL for direct mode connections;
 *             must be NULL if username is NULL)
 *     cname - the connection name (can be NULL)
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCConnect(
            dbconnection_t * conn,
            char const     * dsn,
            char const     * uid,
            char const     * pwd,
            char const     * cname
           );

/*
 * Prepare a SQL statement.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     sqlstmt - the SQL statement text
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCPrepare(
            sqlstmt_t  * stmt,
            char const * sqlstmt
           );

/*
 * Execute a prepared statement.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCExecute(
            sqlstmt_t * stmt
           );

/*
 * Fetch from a cursor (i.e. an active executed statement).
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCFetch(
          sqlstmt_t * stmt
         );

/*
 * Close a cursor.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCCloseCursor(
                sqlstmt_t * stmt
               );

/*
 * Get a row count.
 *
 * INPUTS:
 *     stmt     - a pointer to a statement structure
 *
 * OUTPUTS:
 *     rowcount - a pointer to a long to receive the rowcount
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCRowCount(
             sqlstmt_t * stmt,
             long      * rowcount
            );

/*
 * Bind an input parameter. Allocates the necessary value buffer.
 *
 * INPUTS:
 *     stmt          - a pointer to a statement structure
 *     paramno       - the parameter number (> 0)
 *     clientType    - ODBC C data type for the parameter
 *     dbType        - ODBC database data type for the parameter
 *     ColumnSize    - ODBC ColumnSize value for the parameter
 *     DecimalDigits - ODBC DecimalDigits value for the parameter
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCBindParameter(
                  sqlstmt_t * stmt,
                  int         paramno,
                  SQLSMALLINT clientType,
                  SQLSMALLINT dbType,
                  SQLULEN     ColumnSize,
                  SQLSMALLINT DecimalDigits
                 );

/*
 * Get the TINYINT value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val     - pointer to an unsigned char to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetParam_TINYINT(
                   sqlstmt_t     * stmt,
                   int             paramno,
                   unsigned char * val
                  );

/*
 * Get the SMALLINT value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val     - pointer to a short to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetParam_SMALLINT(
                   sqlstmt_t * stmt,
                   int         paramno,
                   short     * val
                  );

/*
 * Get the INTEGER value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val     - pointer to an int to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetParam_INTEGER(
                   sqlstmt_t * stmt,
                   int         paramno,
                   int       * val
                  );

/*
 * Get the BIGINT value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val     - pointer to a long to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetParam_BIGINT(
                  sqlstmt_t * stmt,
                  int         paramno,
                  long      * val
                 );

/*
 * Get the FLOAT value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val     - pointer to a float to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetParam_FLOAT(
                 sqlstmt_t * stmt,
                 int         paramno,
                 float     * val
                );

/*
 * Get the DOUBLE value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val     - pointer to a double to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetParam_DOUBLE(
                  sqlstmt_t * stmt,
                  int         paramno,
                  double    * val
                 );

/*
 * Get the [VAR]CHAR value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val     - pointer to a pointer to receive a pointer to the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetParam_CHAR(
                sqlstmt_t * stmt,
                int         paramno,
                char    * * val
               );

/*
 * Set the TINYINT value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *     val     - the value
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCSetParam_TINYINT(
                   sqlstmt_t     * stmt,
                   int             paramno,
                   unsigned char   val
                  );

/*
 * Set the SMALLINT value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *     val     - the value
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCSetParam_SMALLINT(
                   sqlstmt_t * stmt,
                   int         paramno,
                   short       val
                  );

/*
 * Set the INTEGER value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *     val     - the value
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCSetParam_INTEGER(
                   sqlstmt_t * stmt,
                   int         paramno,
                   int         val
                  );

/*
 * Set the BIGINT value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *     val     - the value
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCSetParam_BIGINT(
                  sqlstmt_t * stmt,
                  int         paramno,
                  long        val
                 );

/*
 * Set the FLOAT value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *     val     - the value
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCSetParam_FLOAT(
                 sqlstmt_t * stmt,
                 int         paramno,
                 float       val
                );

/*
 * Set the DOUBLE value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *     val     - the value
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCSetParam_DOUBLE(
                  sqlstmt_t * stmt,
                  int         paramno,
                  double      val
                 );

/*
 * Set the [VAR]CHAR value for a specific parameter.
 *
 * INPUTS:
 *     stmt    - a pointer to a statement structure
 *     paramno - the parameter number (> 0)
 *     val     - the value
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCSetParam_CHAR(
                sqlstmt_t * stmt,
                int         paramno,
                char      * val
               );

/*
 * Bind an output column. Allocates the necessary value buffer.
 *
 * INPUTS:
 *     stmt          - a pointer to a statement structure
 *     colno         - the column number (> 0)
 *     clientType    - ODBC C data type for the parameter
 *     maxDataLength - the maximum size for the column value
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCBindColumn(
               sqlstmt_t * stmt,
               int         colno,
               SQLSMALLINT clientType,
               SQLLEN      maxDataLength
              );

/*
 * Get the TINYINT value for a specific column in the current resultset row.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *     colno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val   - a pointer to an unsigned char to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetCol_TINYINT(
                   sqlstmt_t     * stmt,
                   int             colno,
                   unsigned char * val
                  );

/*
 * Get the SMALLINT value for a specific column in the current resultset row.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *     colno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val   - a pointer to a short to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetCol_SMALLINT(
                   sqlstmt_t * stmt,
                   int         colno,
                   short     * val
                  );

/*
 * Get the INTEGER value for a specific column in the current resultset row.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *     colno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val   - a pointer to an int to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetCol_INTEGER(
                   sqlstmt_t * stmt,
                   int         colno,
                   int       * val
                  );

/*
 * Get the BIGINT value for a specific column in the current resultset row.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *     colno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val   - a pointer to a long to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetCol_BIGINT(
                  sqlstmt_t * stmt,
                  int         colno,
                  long      * val
                 );

/*
 * Get the FLOAT value for a specific column in the current resultset row.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *     colno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val   - a pointer to a float to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetCol_FLOAT(
                 sqlstmt_t * stmt,
                 int         colno,
                 float     * val
                );

/*
 * Get the DOUBLE value for a specific column in the current resultset row.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *     colno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val   - a pointer to a double to receive the value
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetCol_DOUBLE(
                  sqlstmt_t * stmt,
                  int         colno,
                  double    * val
                 );

/*
 * Get the [VAR]CHAR value for a specific column in the current resultset row.
 *
 * INPUTS:
 *     stmt  - a pointer to a statement structure
 *     colno - the parameter number (> 0)
 *
 * OUTPUTS:
 *     val   - a pointer to a pointer to char to receive a pointer to the value.
 *
 * RETURNS:
 *     Success or an error code.
 */
int
ODBCGetCol_CHAR(
                sqlstmt_t * stmt,
                int         colno,
                char    * * val
               );

/*************************************************************************
 * UpdateGridInfo Transaction
 */

/*
 * Allocate a txnUpdateGridInfo_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnUpdateGridInfo_t *
createTxnUpdateGridInfo(
                        context_t * ctxt
                       );

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     ugitxn - a pointer to a valid txnUpdateGridInfo_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnUpdateGridInfo(
                      txnUpdateGridInfo_t * txn
                     );

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     ugitxn - a pointer to a valid txnUpdateGridInfo_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnUpdateGridInfo(
                         txnUpdateGridInfo_t * ugitxn
                        );

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     ugitxn - a pointer to a valid txnUpdateGridInfo_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnUpdateGridInfo(
                         txnUpdateGridInfo_t * ugitxn
                        );

/*************************************************************************
 * GetCounts Transaction
 */

/*
 * Allocate a txnGetCounts_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnGetCounts_t *
createTxnGetCounts(
                   context_t * ctxt
                  );

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     gctxn - a pointer to a valid txnGetCounts_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnGetCounts(
                 txnGetCounts_t * gctxn
                );

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     gctxn - a pointer to a valid txnGetCounts_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnGetCounts(
                    txnGetCounts_t * gctxn
                   );

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     gctxn - a pointer to a valid txnGetCounts_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnGetCounts(
                    txnGetCounts_t * gctxn
                   );

/*************************************************************************
 * ClearHistory Transaction
 */

/*
 * Allocate a txnClearHistory_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnClearHistory_t *
createTxnClearHistory(
                      context_t * ctxt
                     );

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     chtxn - a pointer to a valid txnClearHistory_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnClearHistory(
                    txnClearHistory_t * chtxn
                   );

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     chtxn - a pointer to a valid txnClearHistory_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnClearHistory(
                       txnClearHistory_t * chtxn
                      );

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     chtxn - a pointer to a valid txnClearHistory_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnClearHistory(
                       txnClearHistory_t * chtxn
                      );

/*************************************************************************
 * Purge Transaction
 */

/*
 * Allocate a txnPurge_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnPurge_t *
createTxnPurge(
               context_t * ctxt
              );

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     ptxn - a pointer to a valid txnPurge_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnPurge(
             txnPurge_t * ptxn
            );

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     ptxn - a pointer to a valid txnPurge_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnPurge(
                txnPurge_t * ptxn
               );

/*
 * Get the rowcount.
 *
 * INPUTS:
 *     ptxn  - a pointer to a valid txnPurge_t
 *
 * OUTPUTS:
 *     rcount - pointer to a long to receive the rowcount
 *
 * RETURNS:
 *     Success or an error code.
 */
int
rowcountTxnPurge(
                 txnPurge_t * ptxn,
                 long       * rcount
                );

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     ptxn - a pointer to a valid txnPurge_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnPurge(
                txnPurge_t * ptxn
               );

/*************************************************************************
 * Query Transaction
 */

/*
 * Allocate a txnQuery_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnQuery_t *
createTxnQuery(
               context_t * ctxt
              );

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnQuery(
             txnQuery_t * qtxn
            );

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnQuery(
                txnQuery_t * qtxn
               );

/*
 * Fetch from the cursor.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
fetchTxnQuery(
              txnQuery_t * qtxn
             );

/*
 * Close any open cursor.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
closeTxnQuery(
              txnQuery_t * qtxn
             );

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     qtxn - a pointer to a valid txnQuery_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnQuery(
                txnQuery_t * qtxn
               );

/*************************************************************************
 * Authorize Transaction
 */

/*
 * Allocate a txnAuthorize_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnAuthorize_t *
createTxnAuthorize(
                   context_t * ctxt
                  );

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     atxn - a pointer to a valid txnAuthorize_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnAuthorize(
                 txnAuthorize_t * atxn
                );

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     atxn - a pointer to a valid txnAuthorize_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnAuthorize(
                    txnAuthorize_t * atxn
                   );

/*
 * Fetch from the cursor.
 *
 * INPUTS:
 *     atxn - a pointer to a valid txnAuthorize_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
fetchTxnAuthorize(
                  txnAuthorize_t * atxn
                 );

/*
 * Close any open cursor.
 *
 * INPUTS:
 *     atxn - a pointer to a valid txnAuthorize_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
closeTxnAuthorize(
                  txnAuthorize_t * atxn
                 );

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     atxn - a pointer to a valid txnAuthorize_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnAuthorize(
                    txnAuthorize_t * atxn
                   );

/*************************************************************************
 * Charge Transaction
 */

/*
 * Allocate a txnCharge_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnCharge_t *
createTxnCharge(
                context_t * ctxt
               );

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     ctxn - a pointer to a valid txnCharge_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnCharge(
              txnCharge_t * atxn
             );

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     ctxn - a pointer to a valid txnCharge_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnCharge(
                 txnCharge_t * atxn
                );

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     ctxn - a pointer to a valid txnCharge_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
void
cleanupTxnCharge(
                 txnCharge_t * atxn
                );

/*************************************************************************
 * Topup Transaction
 */

/*
 * Allocate a txnTopup_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated structure, or NULL if error.
 */
txnTopup_t *
createTxnTopup(
               context_t * ctxt
              );

/*
 * Initialize the transaction.
 *
 * INPUTS:
 *     ttxn - a pointer to a valid txnTopup_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
initTxnTopup(
             txnTopup_t * atxn
            );

/*
 * Execute the transaction.
 *
 * INPUTS:
 *     ttxn - a pointer to a valid txnTopup_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
int
executeTxnTopup(
                txnTopup_t * atxn
               );

/*
 * Cleanup the transaction.
 *
 * INPUTS:
 *     ttxn - a pointer to a valid txnTopup_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
void
cleanupTxnTopup(
                txnTopup_t * atxn
               );

#endif /* _GRIDSAMPLE_H */
