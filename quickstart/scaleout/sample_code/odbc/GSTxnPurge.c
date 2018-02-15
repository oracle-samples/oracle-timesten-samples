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
 *  GSTxnPurge.c - Delete 'old' rows from the TRANSACTIONS table.
 *
 ****************************************************************************/

#include "GridSample.h"

static char const * const 
purgeDML = "DELETE FIRST 128 FROM transactions WHERE CAST(SYSDATE AS TIMESTAMP) - transaction_ts > NUMTODSINTERVAL( :age, 'SECOND')";

static long
rowCount = -1;

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

/*
 * Allocate and initialise a txnPurge_t
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
              )
{
    txnPurge_t * ptxn = NULL;

    if (  ctxt == NULL  )
        return NULL;

    debugMessage( ctxt, "DEBUG: ENTER: createTxnPurge" );
    ptxn = (txnPurge_t *)calloc( 1, sizeof( txnPurge_t ) );
    if (  ptxn == NULL  )
        goto error;
    ptxn->txn = (apptxn_t *)calloc( 1, sizeof( apptxn_t ) );
    if (  ptxn->txn == NULL  )
        goto error;

    ptxn->purgeAge = DFLT_PURGE_AGE;
    ptxn->txn->parent = (void *)ptxn;
    ptxn->txn->init = (FPinit_t)initTxnPurge;
    ptxn->txn->execute = (FPexecute_t)executeTxnPurge;
    ptxn->txn->cleanup = (FPcleanup_t)cleanupTxnPurge;
    ptxn->txn->rowcount = (FProwcount_t)rowcountTxnPurge;
    ptxn->txn->ctxt = ctxt;
    ptxn->txn->txntype = TXN_PURGE;
    LIST_LINK( ctxt->apptxns, ptxn->txn );
    ctxt->ptxn = ptxn;
    
    debugMessage( ctxt, "DEBUG: EXITOK: createTxnPurge" );
    return ptxn;

error:
    if (  ptxn != NULL  )
        free( (void *)ptxn );
    ctxt->errNumber = ERR_NOMEM;
    displayError( ctxt, errorMessage( ERR_NOMEM, ctxt ), NULL );

    debugMessage( ctxt, "DEBUG: EXITERR: createTxnPurge" );
    return NULL;
} // createTxnPurge

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
            )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (ptxn == NULL) || (ptxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ptxn->txn->ctxt, "DEBUG: ENTER: initTxnPurge" );
    if (  ptxn->purgeStmt != NULL  )
        ODBCFreeStmt( &(ptxn->purgeStmt) );

    debugMessage( ptxn->txn->ctxt, "DEBUG: ODBCAllocStmt(purgeStmt)" );
    errStack = ptxn->txn->ctxt->dbconn->errors;
    ret = ODBCAllocStmt( ptxn->txn->ctxt->dbconn, &(ptxn->purgeStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ptxn->txn->ctxt, "DEBUG: ODBCPrepare(purgeDML)" );
    errStack = ptxn->purgeStmt->errors;
    ret = ODBCPrepare( ptxn->purgeStmt, purgeDML );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ptxn->txn->ctxt, "DEBUG: ODBCBindParameter(purgeStmt,1)" );
    ret = ODBCBindParameter( ptxn->purgeStmt,
                             1,
                             SQL_C_SLONG,
                             SQL_NUMERIC,
                             10, 0 );
    if (  ret != SUCCESS )
        goto error;

    LIST_LINK( ptxn->txn->stmts, ptxn->purgeStmt );

    debugMessage( ptxn->txn->ctxt, "DEBUG: EXITOK: initTxnPurge" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( ptxn->txn->ctxt,
                      errorMessage( ret, ptxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( ptxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( ptxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( ptxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( ptxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

    if (  ptxn->purgeStmt != NULL  )
        ODBCFreeStmt( &(ptxn->purgeStmt) );

#if defined(DEBUG)
    sprintf( ptxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: initTxnPurge: %d", ret );
    debugMessage( ptxn->txn->ctxt, NULL );
#endif

    return ret;
} // initTxnPurge

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
               )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (ptxn == NULL) || (ptxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ptxn->txn->ctxt, "DEBUG: ENTER: executeTxnPurge" );
    if (  ptxn->purgeStmt == NULL  )
        return ERR_STATE_INTERNAL;

    debugMessage( ptxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_INTEGER(purgeStmt,1)" );
    errStack = NULL;
    ret = ODBCSetParam_INTEGER( ptxn->purgeStmt, 1, ptxn->purgeAge );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ptxn->txn->ctxt, "DEBUG: ODBCExecute(purgeStmt)" );
    errStack = ptxn->purgeStmt->errors;
    ret = ODBCExecute( ptxn->purgeStmt );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ptxn->txn->ctxt, "DEBUG: ODBCRowCount(purgeStmt)" );
    ret = ODBCRowCount( ptxn->purgeStmt, &rowCount );
    if (  ret != SUCCESS )
        goto error;
#if defined(DEBUG)
    sprintf( ptxn->txn->ctxt->msgBuff, "DEBUG: Row count = %ld", rowCount );
    debugMessage( ptxn->txn->ctxt, NULL );
#endif

    debugMessage( ptxn->txn->ctxt, "DEBUG: ODBCCommit" );
    errStack = ptxn->txn->ctxt->dbconn->errors;
    ret = ODBCCommit( ptxn->txn->ctxt->dbconn );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ptxn->txn->ctxt, "DEBUG: EXITOK: executeTxnPurge" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( ptxn->txn->ctxt,
                      errorMessage( ret, ptxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( ptxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( ptxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( ptxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( ptxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( ptxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: executeTxnPurge: %d", ret );
    debugMessage( ptxn->txn->ctxt, NULL );
#endif

    return ret;
} // executeTxnPurge

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
                )
{
    if (  (ptxn == NULL) || (ptxn->txn == NULL) || (rcount == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ptxn->txn->ctxt, "DEBUG: ENTER: executeTxnPurge" );

    if (  ptxn->purgeStmt == NULL  )
    {
        debugMessage( ptxn->txn->ctxt, "DEBUG: EXITERR: executeTxnPurge" );
        return ERR_STATE_INTERNAL;
    }

    *rcount = rowCount;

    debugMessage( ptxn->txn->ctxt, "DEBUG: EXITOK: executeTxnPurge" );
    return SUCCESS;
} // rowcountTxnPurge

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
               )
{
    if (  ptxn != NULL  )
    {
        debugMessage( ptxn->txn->ctxt, "DEBUG: ENTER: cleanupTxnPurge" );
        if (  ptxn->txn->stmts != NULL  )
        {
            if ( ptxn->purgeStmt != NULL  )
                LIST_UNLINK( ptxn->txn->stmts, ptxn->purgeStmt );
        }
        ODBCFreeStmt( &(ptxn->purgeStmt) );
        debugMessage( ptxn->txn->ctxt, "DEBUG: EXITOK: cleanupTxnPurge" );
        free( (void *)ptxn );
    }
} // cleanupTxnPurge

