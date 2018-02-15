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
 *  GSTxnCharge.c - Transaction to apply a charge to an account.
 *
 ****************************************************************************/

#include "GridSample.h"

static char const * const 
chargeQuery = 
    "SELECT current_balance, prev_balance FROM accounts WHERE account_id = :acc_id";

static char const * const 
chargeUpdate = 
    "UPDATE accounts SET prev_balance = current_balance, current_balance = current_balance + :adj_amt WHERE account_id = :acc_id";

static char const * const
chargeInsert =
    "INSERT INTO transactions (transaction_id, account_id, transaction_ts, description, optype, amount) VALUES (txn_seq.NEXTVAL, :acc_id, SYSDATE, :trx_desc, 'C', :adj_amt)";

static char const * const
txnDescription = "Call Charge";

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

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
               )
{
    txnCharge_t * ctxn = NULL;

    if (  ctxt == NULL  )
        return NULL;

    debugMessage( ctxt, "DEBUG: ENTER: createTxnCharge" );
    ctxn = (txnCharge_t *)calloc( 1, sizeof( txnCharge_t ) );
    if (  ctxn == NULL  )
        goto error;
    ctxn->txn = (apptxn_t *)calloc( 1, sizeof( apptxn_t ) );
    if (  ctxn->txn == NULL  )
        goto error;

    ctxn->txn->parent = (void *)ctxn;
    ctxn->txn->init = (FPinit_t)initTxnCharge;
    ctxn->txn->execute = (FPexecute_t)executeTxnCharge;
    ctxn->txn->cleanup = (FPcleanup_t)cleanupTxnCharge;
    ctxn->txn->ctxt = ctxt;
    ctxn->txn->txntype = TXN_CHARGE;
    LIST_LINK( ctxt->apptxns, ctxn->txn );
    ctxt->ctxn = ctxn;

    debugMessage( ctxt, "DEBUG: EXITOK: createTxnCharge" );
    return ctxn;

error:
    if (  ctxn != NULL  )
        free( (void *)ctxn );
    ctxt->errNumber = ERR_NOMEM;
    displayError( ctxt, errorMessage( ERR_NOMEM, ctxt ), NULL );

    debugMessage( ctxt, "DEBUG: EXITERR: createTxnCharge" );
    return NULL;
} // createTxnCharge

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
              txnCharge_t * ctxn
             )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (ctxn == NULL) || (ctxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ENTER: initTxnCharge" );
    if (  ctxn->selectStmt != NULL  )
        ODBCFreeStmt( &(ctxn->selectStmt) );
    if (  ctxn->updateStmt != NULL  )
        ODBCFreeStmt( &(ctxn->updateStmt) );
    if (  ctxn->insertStmt != NULL  )
        ODBCFreeStmt( &(ctxn->insertStmt) );

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCAllocStmt(selectStmt)" );
    errStack = ctxn->txn->ctxt->dbconn->errors;
    ret = ODBCAllocStmt( ctxn->txn->ctxt->dbconn, &(ctxn->selectStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCAllocStmt(updateStmt)" );
    ret = ODBCAllocStmt( ctxn->txn->ctxt->dbconn, &(ctxn->updateStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCAllocStmt(insertStmt)" );
    ret = ODBCAllocStmt( ctxn->txn->ctxt->dbconn, &(ctxn->insertStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCPrepare(chargeQuery)" );
    errStack = ctxn->selectStmt->errors;
    ret = ODBCPrepare( ctxn->selectStmt, chargeQuery  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCBindParameter(selectStmt,1)" );
    ret = ODBCBindParameter( ctxn->selectStmt, 1, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 10, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCPrepare(chargeUpdate)" );
    errStack = ctxn->updateStmt->errors;
    ret = ODBCPrepare( ctxn->updateStmt, chargeUpdate  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCBindParameter(updateStmt,1)" );
    ret = ODBCBindParameter( ctxn->updateStmt, 1, SQL_C_DOUBLE, 
                             SQL_NUMERIC, 10, 2 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCBindParameter(updateStmt,2)" );
    ret = ODBCBindParameter( ctxn->updateStmt, 2, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 10, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCPrepare(chargeInsert)" );
    errStack = ctxn->insertStmt->errors;
    ret = ODBCPrepare( ctxn->insertStmt, chargeInsert  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCBindParameter(insertStmt,1)" );
    ret = ODBCBindParameter( ctxn->insertStmt, 1, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 10, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCBindParameter(insertStmt,2)" );
    ret = ODBCBindParameter( ctxn->insertStmt, 2, SQL_C_CHAR, 
                             SQL_CHAR, 60, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCBindParameter(insertStmt,3)" );
    ret = ODBCBindParameter( ctxn->insertStmt, 3, SQL_C_DOUBLE, 
                             SQL_NUMERIC, 10, 2 );
    if (  ret != SUCCESS )
        goto error;

    LIST_LINK( ctxn->txn->stmts, ctxn->selectStmt );
    LIST_LINK( ctxn->txn->stmts, ctxn->updateStmt );
    LIST_LINK( ctxn->txn->stmts, ctxn->insertStmt );

    debugMessage( ctxn->txn->ctxt, "DEBUG: EXITOK: initTxnCharge" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( ctxn->txn->ctxt, 
                      errorMessage( ret, ctxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( ctxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( ctxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( ctxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

    if (  ctxn->selectStmt != NULL  )
        ODBCFreeStmt( &(ctxn->selectStmt) );
    if (  ctxn->updateStmt != NULL  )
        ODBCFreeStmt( &(ctxn->updateStmt) );
    if (  ctxn->insertStmt != NULL  )
        ODBCFreeStmt( &(ctxn->insertStmt) );

#if defined(DEBUG)
    sprintf( ctxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: initTxnCharge: %d", ret );
    debugMessage( ctxn->txn->ctxt, NULL );
#endif

    return ret;
} // initTxnCharge

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
                 txnCharge_t * ctxn
                )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (ctxn == NULL) || (ctxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ENTER: executeTxnCharge" );
#if defined(DEBUG)
    sprintf( ctxn->txn->ctxt->msgBuff, 
             "DEBUG: accountID = %ld, adjustAmount = %.2f, txnDescription >%s<",
             ctxn->accountID, ctxn->adjustAmount, (char *)txnDescription );
    debugMessage( ctxn->txn->ctxt, NULL );
#endif

    if (  (ctxn->selectStmt == NULL) || 
          (ctxn->updateStmt == NULL) || 
          (ctxn->insertStmt == NULL)  )
        return ERR_STATE_INTERNAL;


    debugMessage( ctxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_BIGINT(updateStmt, accountID)" );
    errStack = NULL;
    ret = ODBCSetParam_BIGINT( ctxn->selectStmt, 1, ctxn->accountID );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_BIGINT(updateStmt, adjustAmount)" );
    ret = ODBCSetParam_DOUBLE( ctxn->updateStmt, 1, ctxn->adjustAmount );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_BIGINT(updateStmt, accountID)" );
    ret = ODBCSetParam_BIGINT( ctxn->updateStmt, 2, ctxn->accountID );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_BIGINT(insertStmt, accountID)" );
    ret = ODBCSetParam_BIGINT( ctxn->insertStmt, 1, ctxn->accountID );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_CHAR(insertStmt, txnDescription)" );
    ret = ODBCSetParam_CHAR( ctxn->insertStmt, 2, (char *)txnDescription );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_DOUBLE(insertStmt, adjustAmount)" );
    ret = ODBCSetParam_DOUBLE( ctxn->insertStmt, 3, ctxn->adjustAmount );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCExecute(selectStmt)" );
    errStack = ctxn->selectStmt->errors;
    ret = ODBCExecute( ctxn->selectStmt );
    if (  ret != SUCCESS )
        goto error;
    ctxn->cursor = ctxn->selectStmt;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCFetch(selectStmt)" );
    ret = ODBCFetch( ctxn->cursor );
    if (  ret != SUCCESS )
        goto error;
    // We don't care what the retrieved data actually is in this example

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCCloseCursor(selectStmt)" );
    ret = ODBCCloseCursor( ctxn->cursor );
    if (  ret != SUCCESS )
        goto error;
    ctxn->cursor = NULL;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCExecute(updateStmt)" );
    errStack = ctxn->updateStmt->errors;
    ret = ODBCExecute( ctxn->updateStmt );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCExecute(insertStmt)" );
    errStack = ctxn->insertStmt->errors;
    ret = ODBCExecute( ctxn->insertStmt );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCCommit" );
    errStack = ctxn->txn->ctxt->dbconn->errors;
    ret = ODBCCommit( ctxn->txn->ctxt->dbconn );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( ctxn->txn->ctxt, "DEBUG: EXITOK: executeTxnCharge" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( ctxn->txn->ctxt,
                      errorMessage( ret, ctxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( ctxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( ctxn->txn->ctxt->dbconn );
    ctxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( ctxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( ctxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( ctxn->txn->ctxt->msgBuff,
             "DEBUG: EXITERR: executeTxnCharge: %d", ret );
    debugMessage( ctxn->txn->ctxt, NULL );
#endif
     return ret;
} // executeTxnCharge

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
                txnCharge_t * ctxn
               )
{
    if (  ctxn != NULL  )
    {
        debugMessage( ctxn->txn->ctxt, "DEBUG: ENTER: cleanupTxnCharge" );
        if (  ctxn->txn->stmts != NULL  )
        {
            if ( ctxn->selectStmt != NULL  )
                LIST_UNLINK( ctxn->txn->stmts, ctxn->selectStmt );
            if ( ctxn->updateStmt != NULL  )
                LIST_UNLINK( ctxn->txn->stmts, ctxn->updateStmt );
            if ( ctxn->insertStmt != NULL  )
                LIST_UNLINK( ctxn->txn->stmts, ctxn->insertStmt );
        }
        ODBCFreeStmt( &(ctxn->selectStmt) );
        ODBCFreeStmt( &(ctxn->updateStmt) );
        ODBCFreeStmt( &(ctxn->insertStmt) );
        debugMessage( ctxn->txn->ctxt, "DEBUG: EXITOK: cleanupTxnCharge" );
        free( (void *)ctxn );
    }
} // cleanupTxnCharge

