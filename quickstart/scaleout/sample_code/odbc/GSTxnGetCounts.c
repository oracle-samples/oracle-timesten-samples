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
 *  GSTxnGetCounts.c - Transaction to get table row counts.
 *
 ****************************************************************************/

#include "GridSample.h"

static char const * const 
customersQuery = 
    "SELECT MIN(cust_id), MAX(cust_id), COUNT(*) FROM customers";

static char const * const 
accountsQuery = 
    "SELECT MIN(account_id), MAX(account_id), COUNT(*) FROM accounts";

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

/*
 * Allocate a txnGetCounts_t
 *
 * INPUTS:
 *     ctxt  - a pointer to a valid context_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the allocated and initialized structure, or NULl if error.
 */
txnGetCounts_t *
createTxnGetCounts(
                   context_t * ctxt
                  )
{
    txnGetCounts_t * gctxn = NULL;

    if (  ctxt == NULL  )
        return NULL;

    debugMessage( ctxt, "DEBUG: ENTER: createTxnGetCounts" );
    gctxn = (txnGetCounts_t *)calloc( 1, sizeof( txnGetCounts_t ) );
    if (  gctxn == NULL  )
        goto error;
    gctxn->txn = (apptxn_t *)calloc( 1, sizeof( apptxn_t ) );
    if (  gctxn->txn == NULL  )
        goto error;

    gctxn->txn->parent = (void *)gctxn;
    gctxn->txn->init = (FPinit_t)initTxnGetCounts;
    gctxn->txn->execute = (FPexecute_t)executeTxnGetCounts;
    gctxn->txn->cleanup = (FPcleanup_t)cleanupTxnGetCounts;
    gctxn->txn->ctxt = ctxt;
    gctxn->txn->txntype = TXN_GETCOUNTS;
    LIST_LINK( ctxt->apptxns, gctxn->txn );
    ctxt->gctxn = gctxn;

    debugMessage( ctxt, "DEBUG: EXITOK: createTxnGetCounts" );
    return gctxn;

error:
    if (  gctxn != NULL  )
        free( (void *)gctxn );
    ctxt->errNumber = ERR_NOMEM;
    displayError( ctxt, errorMessage( ERR_NOMEM, ctxt ), NULL );

    debugMessage( ctxt, "DEBUG: EXITERR: createTxnGetCounts" );
    return NULL;
} // createTxnGetCounts

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
                )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (gctxn == NULL) || (gctxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( gctxn->txn->ctxt, "DEBUG: ENTER: initTxnGetCounts" );
    if (  gctxn->stmtCountCustomers != NULL  )
        ODBCFreeStmt( &(gctxn->stmtCountCustomers) );
    if (  gctxn->stmtCountAccounts != NULL  )
        ODBCFreeStmt( &(gctxn->stmtCountAccounts) );

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCAllocStmt(stmtCountCustomers)" );
    errStack = gctxn->txn->ctxt->dbconn->errors;
    ret = ODBCAllocStmt( gctxn->txn->ctxt->dbconn, 
                         &(gctxn->stmtCountCustomers) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCAllocStmt(stmtCountAccounts)" );
    ret = ODBCAllocStmt( gctxn->txn->ctxt->dbconn, 
                         &(gctxn->stmtCountAccounts) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCPrepare(customersQuery)" );
    errStack = gctxn->stmtCountCustomers->errors;
    ret = ODBCPrepare( gctxn->stmtCountCustomers, customersQuery  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCBindColumn(stmtCountCustomers, 1)" );
    ret = ODBCBindColumn( gctxn->stmtCountCustomers, 1, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCBindColumn(stmtCountCustomers, 2)" );
    ret = ODBCBindColumn( gctxn->stmtCountCustomers, 2, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCBindColumn(stmtCountCustomers, 3)" );
    ret = ODBCBindColumn( gctxn->stmtCountCustomers, 3, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCPrepare(accountsQuery)" );
    errStack = gctxn->stmtCountAccounts->errors;
    ret = ODBCPrepare( gctxn->stmtCountAccounts, accountsQuery );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCBindColumn(stmtCountAccounts, 1)" );
    ret = ODBCBindColumn( gctxn->stmtCountAccounts, 1, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCBindColumn(stmtCountAccounts, 2)" );
    ret = ODBCBindColumn( gctxn->stmtCountAccounts, 2, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCBindColumn(stmtCountAccounts, 3)" );
    ret = ODBCBindColumn( gctxn->stmtCountAccounts, 3, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    LIST_LINK( gctxn->txn->stmts, gctxn->stmtCountCustomers );
    LIST_LINK( gctxn->txn->stmts, gctxn->stmtCountAccounts );

    debugMessage( gctxn->txn->ctxt, "DEBUG: EXITOK: initTxnGetCounts" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( gctxn->txn->ctxt, 
                      errorMessage( ret, gctxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( gctxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( gctxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( gctxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( gctxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

    if (  gctxn->stmtCountCustomers != NULL  )
        ODBCFreeStmt( &(gctxn->stmtCountCustomers) );
    if (  gctxn->stmtCountAccounts != NULL  )
        ODBCFreeStmt( &(gctxn->stmtCountAccounts) );

#if defined(DEBUG)
    sprintf( gctxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: initTxnGetCounts: %d", ret );
    debugMessage( gctxn->txn->ctxt, NULL );
#endif

    return ret;
} // initTxnGetCounts

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
                   )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (gctxn == NULL) || (gctxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( gctxn->txn->ctxt, "DEBUG: ENTER: executeTxnGetCounts" );
    if (  (gctxn->stmtCountCustomers == NULL) || 
          (gctxn->stmtCountAccounts == NULL) ||
          (gctxn->cursor != NULL)  )
        return ERR_STATE_INTERNAL;

    gctxn->minCustID = 0;
    gctxn->maxCustID = 0;
    gctxn->numCustomers = 0;

    debugMessage( gctxn->txn->ctxt, "DEBUG: ODBCExecute(stmtCountCustomers)" );
    errStack = gctxn->stmtCountCustomers->errors;
    ret = ODBCExecute( gctxn->stmtCountCustomers );
    if (  ret != SUCCESS )
        goto error;
    gctxn->cursor = gctxn->stmtCountCustomers;

    debugMessage( gctxn->txn->ctxt, "DEBUG: ODBCFetch(stmtCountCustomers)" );
    ret = ODBCFetch( gctxn->stmtCountCustomers );
    if (  ret == ERR_ODBC_NO_DATA  )
    {
        errStack = NULL;
        goto error;
    }
    else
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCGetCol_BIGINT(stmtCountCustomers,1)" );
    errStack = NULL;
    ret = ODBCGetCol_BIGINT( gctxn->stmtCountCustomers, 1, 
                             &(gctxn->minCustID) );
    if (  ret != SUCCESS  )
        goto error;
        
    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCGetCol_BIGINT(stmtCountCustomers,2)" );
    ret = ODBCGetCol_BIGINT( gctxn->stmtCountCustomers, 2, 
                             &(gctxn->maxCustID) );
    if (  ret != SUCCESS  )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCGetCol_BIGINT(stmtCountCustomers,3)" );
    ret = ODBCGetCol_BIGINT( gctxn->stmtCountCustomers, 3, 
                             &(gctxn->numCustomers) );
    if (  ret != SUCCESS  )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCCloseCursor(stmtCountCustomers)" );
    errStack = gctxn->stmtCountCustomers->errors;
    ret = ODBCCloseCursor( gctxn->stmtCountCustomers );
    if (  ret != SUCCESS )
        goto error;
    gctxn->cursor = NULL;

#if defined(DEBUG)
    sprintf( gctxn->txn->ctxt->msgBuff,
             "DEBUG: minCustID = %ld, maxCustID = %ld, numCustomers = %ld",
             gctxn->minCustID, gctxn->maxCustID, gctxn->numCustomers );
    debugMessage( gctxn->txn->ctxt, NULL );
#endif

    gctxn->minAccountID = 0;
    gctxn->maxAccountID = 0;
    gctxn->numAccounts = 0;

    debugMessage( gctxn->txn->ctxt, "DEBUG: ODBCExecute(stmtCountAccounts)" );
    errStack = gctxn->stmtCountAccounts->errors;
    ret = ODBCExecute( gctxn->stmtCountAccounts );
    if (  ret != SUCCESS )
        goto error;
    gctxn->cursor = gctxn->stmtCountAccounts;

    debugMessage( gctxn->txn->ctxt, "DEBUG: ODBCFetch(stmtCountAccounts)" );
    ret = ODBCFetch( gctxn->stmtCountAccounts );
    if (  ret == ERR_ODBC_NO_DATA  )
    {
        errStack = NULL;
        goto error;
    }
    else
    if (  ret != SUCCESS )
        goto error;

    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCGetCol_BIGINT(stmtCountAccounts,1)" );
    errStack = NULL;
    ret = ODBCGetCol_BIGINT( gctxn->stmtCountAccounts, 1,
                             &(gctxn->minAccountID) );
    if (  ret != SUCCESS  )
        goto error;
        
    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCGetCol_BIGINT(stmtCountAccounts,2)" );
    ret = ODBCGetCol_BIGINT( gctxn->stmtCountAccounts, 2,
                             &(gctxn->maxAccountID) );
    if (  ret != SUCCESS  )
        goto error;
        
    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCGetCol_BIGINT(stmtCountAccounts,3)" );
    ret = ODBCGetCol_BIGINT( gctxn->stmtCountAccounts, 3,
                             &(gctxn->numAccounts) );
    if (  ret != SUCCESS  )
        goto error;
    
    debugMessage( gctxn->txn->ctxt, 
                  "DEBUG: ODBCCloseCursor(stmtCountAccounts)" );
    errStack = gctxn->stmtCountAccounts->errors;
    ret = ODBCCloseCursor( gctxn->stmtCountAccounts );
    if (  ret != SUCCESS )
        goto error;
    gctxn->cursor = NULL;

#if defined(DEBUG)
    sprintf( gctxn->txn->ctxt->msgBuff,
             "DEBUG: minAccountID = %ld, maxAccountID = %ld, numAccounts = %ld",
             gctxn->minAccountID, gctxn->maxAccountID, gctxn->numAccounts );
    debugMessage( gctxn->txn->ctxt, NULL );
#endif

    // validate the retrieved data
    if (  (gctxn->minCustID < 1) || (gctxn->minAccountID < 1) ||
          (gctxn->maxCustID <= gctxn->minCustID) ||
          (gctxn->maxAccountID <= gctxn->minAccountID) ||
          ((gctxn->maxCustID-gctxn->minCustID+1) != gctxn->numCustomers) ||
          ((gctxn->maxAccountID-gctxn->minAccountID+1) != gctxn->numAccounts)  )
    {
        errStack = NULL;
        ret = ERR_DATA;
        goto error;
    }

    gctxn->txn->ctxt->minCustID = gctxn->minCustID;
    gctxn->txn->ctxt->maxCustID = gctxn->maxCustID;
    gctxn->txn->ctxt->numCustomers = gctxn->numCustomers;
    gctxn->txn->ctxt->minAccountID = gctxn->minAccountID;
    gctxn->txn->ctxt->maxAccountID = gctxn->maxAccountID;
    gctxn->txn->ctxt->numAccounts = gctxn->numAccounts;

    if (  gctxn->txn->ctxt->commitReadTxn  )
    {
        debugMessage( gctxn->txn->ctxt, "DEBUG: ODBCCommit" );
        errStack = gctxn->txn->ctxt->dbconn->errors;
        ret = ODBCCommit( gctxn->txn->ctxt->dbconn );
        if (  ret != SUCCESS )
            goto error;
    }

    debugMessage( gctxn->txn->ctxt, "DEBUG: EXITOK: executeTxnGetCounts" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( gctxn->txn->ctxt,
                      errorMessage( ret, gctxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( gctxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( gctxn->txn->ctxt->dbconn );
    gctxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( gctxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( gctxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( gctxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: executeTxnGetCounts: %d", ret );
    debugMessage( gctxn->txn->ctxt, NULL );
#endif

     return ret;
} // executeTxnGetCounts

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
                   )
{
    if (  gctxn != NULL  )
    {
        debugMessage( gctxn->txn->ctxt, "DEBUG: ENTER: cleanupTxnGetCounts" );
        gctxn->cursor = NULL;
        if (  gctxn->txn->stmts != NULL  )
        {
            if ( gctxn->stmtCountCustomers != NULL  )
                LIST_UNLINK( gctxn->txn->stmts, gctxn->stmtCountCustomers );
            if ( gctxn->stmtCountAccounts != NULL  )
                LIST_UNLINK( gctxn->txn->stmts, gctxn->stmtCountAccounts );
        }
        ODBCFreeStmt( &(gctxn->stmtCountCustomers) );
        ODBCFreeStmt( &(gctxn->stmtCountAccounts) );
        debugMessage( gctxn->txn->ctxt, "DEBUG: EXITOK: cleanupTxnGetCounts" );
        free( (void *)gctxn );
    }
} // cleanupTxnGetCounts

