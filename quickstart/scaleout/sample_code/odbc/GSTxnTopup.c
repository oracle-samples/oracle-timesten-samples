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
 *  GSTxnTopup.c - Transaction to apply a top-up to an account.
 *
 ****************************************************************************/

#include "GridSample.h"

static char const * const 
topupQuery = 
    "SELECT current_balance, prev_balance FROM accounts WHERE account_id = :acc_id";

static char const * const 
topupUpdate = 
    "UPDATE accounts SET prev_balance = current_balance, current_balance = current_balance + :adj_amt WHERE account_id = :acc_id";

static char const * const
topupInsert =
    "INSERT INTO transactions (transaction_id, account_id, transaction_ts, description, optype, amount) VALUES (txn_seq.NEXTVAL, :acc_id, SYSDATE, :trx_desc, 'C', :adj_amt)";

static char const * const
txnDescription = "Account Topup";

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

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
               )
{
    txnTopup_t * ttxn = NULL;

    if (  ctxt == NULL  )
        return NULL;

    debugMessage( ctxt, "DEBUG: ENTER: createTxnTopup" );
    ttxn = (txnTopup_t *)calloc( 1, sizeof( txnTopup_t ) );
    if (  ttxn == NULL  )
        goto error;
    ttxn->txn = (apptxn_t *)calloc( 1, sizeof( apptxn_t ) );
    if (  ttxn->txn == NULL  )
        goto error;

    ttxn->txn->parent = (void *)ttxn;
    ttxn->txn->init = (FPinit_t)initTxnTopup;
    ttxn->txn->execute = (FPexecute_t)executeTxnTopup;
    ttxn->txn->cleanup = (FPcleanup_t)cleanupTxnTopup;
    ttxn->txn->ctxt = ctxt;
    ttxn->txn->txntype = TXN_TOPUP;
    LIST_LINK( ctxt->apptxns, ttxn->txn );
    ctxt->ttxn = ttxn;

    debugMessage( ctxt, "DEBUG: EXITOK: createTxnTopup" );
    return ttxn;

error:
    if (  ttxn != NULL  )
        free( (void *)ttxn );
    ctxt->errNumber = ERR_NOMEM;
    displayError( ctxt, errorMessage( ERR_NOMEM, ctxt ), NULL );

    debugMessage( ctxt, "DEBUG: EXITERR: createTxnTopup" );
    return NULL;
} // createTxnTopup

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
              txnTopup_t * ttxn
             )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (ttxn == NULL) || (ttxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ENTER: initTxnTopup" );
    if (  ttxn->selectStmt != NULL  )
        ODBCFreeStmt( &(ttxn->selectStmt) );
    if (  ttxn->updateStmt != NULL  )
        ODBCFreeStmt( &(ttxn->updateStmt) );
    if (  ttxn->insertStmt != NULL  )
        ODBCFreeStmt( &(ttxn->insertStmt) );

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCAllocStmt(selectStmt)" );
    errStack = ttxn->txn->ctxt->dbconn->errors;
    ret = ODBCAllocStmt( ttxn->txn->ctxt->dbconn, &(ttxn->selectStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCAllocStmt(updateStmt)" );
    ret = ODBCAllocStmt( ttxn->txn->ctxt->dbconn, &(ttxn->updateStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCAllocStmt(insertStmt)" );
    ret = ODBCAllocStmt( ttxn->txn->ctxt->dbconn, &(ttxn->insertStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCPrepare(topupQuery)" );
    errStack = ttxn->selectStmt->errors;
    ret = ODBCPrepare( ttxn->selectStmt, topupQuery  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCBindParameter(selectStmt,1)" );
    ret = ODBCBindParameter( ttxn->selectStmt, 1, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 10, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCPrepare(topupUpdate)" );
    errStack = ttxn->updateStmt->errors;
    ret = ODBCPrepare( ttxn->updateStmt, topupUpdate  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCBindParameter(updateStmt,1)" );
    ret = ODBCBindParameter( ttxn->updateStmt, 1, SQL_C_DOUBLE, 
                             SQL_NUMERIC, 10, 2 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCBindParameter(updateStmt,2)" );
    ret = ODBCBindParameter( ttxn->updateStmt, 2, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 10, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCPrepare(topupInsert)" );
    errStack = ttxn->insertStmt->errors;
    ret = ODBCPrepare( ttxn->insertStmt, topupInsert  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCBindParameter(insertStmt,1)" );
    ret = ODBCBindParameter( ttxn->insertStmt, 1, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 10, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCBindParameter(insertStmt,2)" );
    ret = ODBCBindParameter( ttxn->insertStmt, 2, SQL_C_CHAR, 
                             SQL_CHAR, 60, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCBindParameter(insertStmt,3)" );
    ret = ODBCBindParameter( ttxn->insertStmt, 3, SQL_C_DOUBLE, 
                             SQL_NUMERIC, 10, 2 );
    if (  ret != SUCCESS )
        goto error;

    LIST_LINK( ttxn->txn->stmts, ttxn->selectStmt );
    LIST_LINK( ttxn->txn->stmts, ttxn->updateStmt );
    LIST_LINK( ttxn->txn->stmts, ttxn->insertStmt );

    debugMessage( ttxn->txn->ctxt, "DEBUG: EXITOK: initTxnTopup" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( ttxn->txn->ctxt, 
                      errorMessage( ret, ttxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( ttxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( ttxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( ttxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

    if (  ttxn->selectStmt != NULL  )
        ODBCFreeStmt( &(ttxn->selectStmt) );
    if (  ttxn->updateStmt != NULL  )
        ODBCFreeStmt( &(ttxn->updateStmt) );
    if (  ttxn->insertStmt != NULL  )
        ODBCFreeStmt( &(ttxn->insertStmt) );

#if defined(DEBUG)
    sprintf( ttxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: initTxnTopup: %d", ret );
    debugMessage( ttxn->txn->ctxt, NULL );
#endif
    return ret;
} // initTxnTopup

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
                 txnTopup_t * ttxn
                )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (ttxn == NULL) || (ttxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ENTER: executeTxnTopup" );
#if defined(DEBUG)
    sprintf( ttxn->txn->ctxt->msgBuff,
             "DEBUG: accountID = %ld, adjustAmount = %.2f, txnDescription >%s<",
             ttxn->accountID, ttxn->adjustAmount, (char *)txnDescription );
    debugMessage( ttxn->txn->ctxt, NULL );
#endif

    if (  (ttxn->selectStmt == NULL) || 
          (ttxn->updateStmt == NULL) || 
          (ttxn->insertStmt == NULL)  )
        return ERR_STATE_INTERNAL;

    debugMessage( ttxn->txn->ctxt,
                  "DEBUG: ODBCSetParam_BIGINT(updateStmt, accountID)" );
    errStack = NULL;
    ret = ODBCSetParam_BIGINT( ttxn->selectStmt, 1, ttxn->accountID );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt,
                  "DEBUG: ODBCSetParam_BIGINT(updateStmt, adjustAmount)" );
    ret = ODBCSetParam_DOUBLE( ttxn->updateStmt, 1, ttxn->adjustAmount );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt,
                  "DEBUG: ODBCSetParam_BIGINT(updateStmt, accountID)" );
    ret = ODBCSetParam_BIGINT( ttxn->updateStmt, 2, ttxn->accountID );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt,
                  "DEBUG: ODBCSetParam_BIGINT(insertStmt, accountID)" );
    ret = ODBCSetParam_BIGINT( ttxn->insertStmt, 1, ttxn->accountID );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt,
                  "DEBUG: ODBCSetParam_CHAR(insertStmt, txnDescription)" );
    ret = ODBCSetParam_CHAR( ttxn->insertStmt, 2, (char *)txnDescription );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt,
                  "DEBUG: ODBCSetParam_DOUBLE(insertStmt, adjustAmount)" );
    ret = ODBCSetParam_DOUBLE( ttxn->insertStmt, 3, ttxn->adjustAmount );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCExecute(selectStmt)" );
    errStack = ttxn->selectStmt->errors;
    ret = ODBCExecute( ttxn->selectStmt );
    if (  ret != SUCCESS )
        goto error;
    ttxn->cursor = ttxn->selectStmt;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCFetch(selectStmt)" );
    ret = ODBCFetch( ttxn->cursor );
    if (  ret != SUCCESS )
        goto error;
    // We don't care what the retrieved data actually is in this example

   debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCCloseCursor(selectStmt)" );
    ret = ODBCCloseCursor( ttxn->cursor );
    if (  ret != SUCCESS )
        goto error;
    ttxn->cursor = NULL;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCExecute(updateStmt)" );
    errStack = ttxn->updateStmt->errors;
    ret = ODBCExecute( ttxn->updateStmt );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCExecute(insertStmt)" );
    errStack = ttxn->insertStmt->errors;
    ret = ODBCExecute( ttxn->insertStmt );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCCommit" );
    errStack = ttxn->txn->ctxt->dbconn->errors;
    ret = ODBCCommit( ttxn->txn->ctxt->dbconn );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ttxn->txn->ctxt, "DEBUG: EXITOK: executeTxnTopup" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( ttxn->txn->ctxt,
                      errorMessage( ret, ttxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( ttxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( ttxn->txn->ctxt->dbconn );
    ttxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( ttxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( ttxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( ttxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: executeTxnTopup: %d", ret );
    debugMessage( ttxn->txn->ctxt, NULL );
#endif
     return ret;
} // executeTxnTopup

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
                txnTopup_t * ttxn
               )
{
    if (  ttxn != NULL  )
    {
        debugMessage( ttxn->txn->ctxt, "DEBUG: ENTER: cleanupTxnTopup" );
        if (  ttxn->txn->stmts != NULL  )
        {
            if ( ttxn->selectStmt != NULL  )
                LIST_UNLINK( ttxn->txn->stmts, ttxn->selectStmt );
            if ( ttxn->updateStmt != NULL  )
                LIST_UNLINK( ttxn->txn->stmts, ttxn->updateStmt );
            if ( ttxn->insertStmt != NULL  )
                LIST_UNLINK( ttxn->txn->stmts, ttxn->insertStmt );
        }
        ODBCFreeStmt( &(ttxn->selectStmt) );
        ODBCFreeStmt( &(ttxn->updateStmt) );
        ODBCFreeStmt( &(ttxn->insertStmt) );
        debugMessage( ttxn->txn->ctxt, "DEBUG: EXITOK: cleanupTxnTopup" );
        free( (void *)ttxn );
    }
} // cleanupTxnTopup

