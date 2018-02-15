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
 *  GSTxnClearHistory.c - Transaction to truncate the TRANSACTIONS table.
 *
 ****************************************************************************/

#include "GridSample.h"

static char const * const 
truncateDDL = "TRUNCATE TABLE transactions";

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

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
                     )
{
    txnClearHistory_t * chtxn = NULL;

    if (  ctxt == NULL  )
        return NULL;

    debugMessage( ctxt, "DEBUG: ENTER: createTxnClearHistory" );
    chtxn = (txnClearHistory_t *)calloc( 1, sizeof( txnClearHistory_t ) );
    if (  chtxn == NULL  )
        goto error;
    chtxn->txn = (apptxn_t *)calloc( 1, sizeof( apptxn_t ) );
    if (  chtxn->txn == NULL  )
        goto error;

    chtxn->txn->parent = (void *)chtxn;
    chtxn->txn->init = (FPinit_t)initTxnClearHistory;
    chtxn->txn->execute = (FPexecute_t)executeTxnClearHistory;
    chtxn->txn->cleanup = (FPcleanup_t)cleanupTxnClearHistory;
    chtxn->txn->ctxt = ctxt;
    chtxn->txn->txntype = TXN_CLEARHISTORY;
    LIST_LINK( ctxt->apptxns, chtxn->txn );
    ctxt->chtxn = chtxn;
    
    debugMessage( ctxt, "DEBUG: EXITOK: createTxnClearHistory" );
    return chtxn;

error:
    if (  chtxn != NULL  )
        free( (void *)chtxn );
    ctxt->errNumber = ERR_NOMEM;
    displayError( ctxt, errorMessage( ERR_NOMEM, ctxt ), NULL );

    debugMessage( ctxt, "DEBUG: EXITERR: createTxnClearHistory" );
    return NULL;
} // createTxnClearHistory

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
                   )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (chtxn == NULL) || (chtxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( chtxn->txn->ctxt, "DEBUG: ENTER: initTxnClearHistory" );
    if (  chtxn->truncateStmt != NULL  )
        ODBCFreeStmt( &(chtxn->truncateStmt) );

    debugMessage( chtxn->txn->ctxt, "DEBUG: ODBCAllocStmt(truncateStmt)" );
    errStack = chtxn->txn->ctxt->dbconn->errors;
    ret = ODBCAllocStmt( chtxn->txn->ctxt->dbconn, &(chtxn->truncateStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( chtxn->txn->ctxt, "DEBUG: ODBCPrepare(truncateDDL)" );
    errStack = chtxn->truncateStmt->errors;
    ret = ODBCPrepare( chtxn->truncateStmt, truncateDDL );
    if (  ret != SUCCESS )
        goto error;

    LIST_LINK( chtxn->txn->stmts, chtxn->truncateStmt );

    debugMessage( chtxn->txn->ctxt, "DEBUG: EXITOK: initTxnClearHistory" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( chtxn->txn->ctxt,
                      errorMessage( ret, chtxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( chtxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( chtxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( chtxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( chtxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

    if (  chtxn->truncateStmt != NULL  )
        ODBCFreeStmt( &(chtxn->truncateStmt) );

#if defined(DEBUG)
    sprintf( chtxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: initTxnClearHistory: %d", ret );
    debugMessage( chtxn->txn->ctxt, NULL );
#endif

    return ret;
} // initTxnClearHistory

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
                      )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (chtxn == NULL) || (chtxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( chtxn->txn->ctxt, "DEBUG: ENTER: executeTxnClearHistory" );
    if (  chtxn->truncateStmt == NULL  )
        return ERR_STATE_INTERNAL;

    errStack = chtxn->truncateStmt->errors;
    ret = ODBCExecute( chtxn->truncateStmt );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( chtxn->txn->ctxt, "DEBUG: EXITOK: executeTxnClearHistory" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( chtxn->txn->ctxt,
                      errorMessage( ret, chtxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( chtxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( chtxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( chtxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( chtxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( chtxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: executeTxnClearHistory: %d", ret );
    debugMessage( chtxn->txn->ctxt, NULL );
#endif

    return ret;
} // executeTxnClearHistory

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
                      )
{
    if (  chtxn != NULL  )
    {
        debugMessage( chtxn->txn->ctxt, 
                      "DEBUG: ENTER: cleanupTxnClearHistory" );
        if (  chtxn->txn->stmts != NULL  )
        {
            if ( chtxn->truncateStmt != NULL  )
                LIST_UNLINK( chtxn->txn->stmts, chtxn->truncateStmt );
        }
        ODBCFreeStmt( &(chtxn->truncateStmt) );
        debugMessage( chtxn->txn->ctxt, 
                      "DEBUG: EXITOK: cleanupTxnClearHistory" );
        free( (void *)chtxn );
    }
} // cleanupTxnClearHistory

