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
 *  GSTxnAuthorize.c - Transaction to check if an account exists and is
 *                     allowed to make calls. Returns customerID and phone
 *                     number.
 *
 ****************************************************************************/

#include "GridSample.h"

static char const * const 
authorizeQuery = 
    "SELECT cust_id, phone FROM accounts WHERE account_id = :acc_id AND account_type = :acc_type AND status = :acc_status";

static char const * const 
accountType = "P";

static const int
accountStatus = 10;

/****************************************************************************
 *
 * Internal functions
 *
 ****************************************************************************/

/*
 * Clear down the result set variables.
 *
 * INPUTS:
 *     atxn - pointer to a valid txnAuthorize_t
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
static void
clearRowData(
             txnAuthorize_t * atxn
            )
{
    debugMessage( atxn->txn->ctxt, "DEBUG: clearRowData" );
    atxn->rdvalid = false;
    atxn->cust_id = 0;
    atxn->phone = NULL;
} // clearRowData

/*
 * Set the result set variables.
 *
 * INPUTS:
 *     atxn - pointer to a valid txnAuthorize_t
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
static int
setRowData(
           txnAuthorize_t * atxn
          )
{
    int ret;

    debugMessage( atxn->txn->ctxt, "DEBUG: ENTER: setRowData" );
    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCGetCol_BIGINT(cust_id)" );
    ret = ODBCGetCol_BIGINT( atxn->cursor, 1, &(atxn->cust_id) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( atxn->txn->ctxt->msgBuff, "DEBUG: cust_id = %ld", atxn->cust_id );
    debugMessage( atxn->txn->ctxt, NULL );
#endif

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCGetCol_BIGINT(phone)" );
    ret = ODBCGetCol_CHAR( atxn->cursor, 2, &(atxn->phone) );
    if (  ret != SUCCESS  )
        goto error;
#if defined(DEBUG)
    sprintf( atxn->txn->ctxt->msgBuff, "DEBUG: phone >%s<", atxn->phone );
    debugMessage( atxn->txn->ctxt, NULL );
#endif

    atxn->rdvalid = true;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: EXITOK: setRowData" );
    return SUCCESS;

error:
    clearRowData( atxn );

#if defined(DEBUG)
    sprintf( atxn->txn->ctxt->msgBuff, "DEBUG: EXITERR: setRowData: %d", ret );
    debugMessage( atxn->txn->ctxt, NULL );
#endif
    return ret;
} // setRowData

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

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
                  )
{
    txnAuthorize_t * atxn = NULL;

    if (  ctxt == NULL  )
        return NULL;

    debugMessage( ctxt, "DEBUG: ENTER: createTxnAuthorize" );
    atxn = (txnAuthorize_t *)calloc( 1, sizeof( txnAuthorize_t ) );
    if (  atxn == NULL  )
        goto error;
    atxn->txn = (apptxn_t *)calloc( 1, sizeof( apptxn_t ) );
    if (  atxn->txn == NULL  )
        goto error;

    atxn->txn->parent = (void *)atxn;
    atxn->txn->init = (FPinit_t)initTxnAuthorize;
    atxn->txn->execute = (FPexecute_t)executeTxnAuthorize;
    atxn->txn->fetch = (FPfetch_t)fetchTxnAuthorize;
    atxn->txn->close = (FPclose_t)closeTxnAuthorize;
    atxn->txn->cleanup = (FPcleanup_t)cleanupTxnAuthorize;
    atxn->txn->ctxt = ctxt;
    atxn->txn->txntype = TXN_AUTHORIZE;
    LIST_LINK( ctxt->apptxns, atxn->txn );
    ctxt->atxn = atxn;

    debugMessage( ctxt, "DEBUG: EXITOK: createTxnAuthorize" );
    return atxn;

error:
    if (  atxn != NULL  )
        free( (void *)atxn );
    ctxt->errNumber = ERR_NOMEM;
    displayError( ctxt, errorMessage( ERR_NOMEM, ctxt ), NULL );

    debugMessage( ctxt, "DEBUG: EXITERR: createTxnAuthorize" );
    return NULL;
} // createTxnAuthorize

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
                )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (atxn == NULL) || (atxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( atxn->txn->ctxt, "DEBUG: ENTER: initTxnAuthorize" );
    if (  atxn->authorizeStmt != NULL  )
        ODBCFreeStmt( &(atxn->authorizeStmt) );

    debugMessage( atxn->txn->ctxt, "DEBUG: ODBCAllocStmt(authorizeStmt)" );
    errStack = atxn->txn->ctxt->dbconn->errors;
    ret = ODBCAllocStmt( atxn->txn->ctxt->dbconn, &(atxn->authorizeStmt) );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, "DEBUG: ODBCPrepare(authorizeQuery)" );
    errStack = atxn->authorizeStmt->errors;
    ret = ODBCPrepare( atxn->authorizeStmt, authorizeQuery  );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCBindParameter(authorizeStmt,1)" );
    ret = ODBCBindParameter( atxn->authorizeStmt, 1, SQL_C_SBIGINT, 
                             SQL_NUMERIC, 10, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCBindParameter(authorizeStmt,2)" );
    ret = ODBCBindParameter( atxn->authorizeStmt, 2, SQL_C_CHAR, 
                             SQL_CHAR, 1, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCBindParameter(authorizeStmt,3)" );
    ret = ODBCBindParameter( atxn->authorizeStmt, 3, SQL_C_SLONG, 
                             SQL_NUMERIC, 2, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCBindColumn(authorizeStmt,1)" );
    ret = ODBCBindColumn( atxn->authorizeStmt, 1, SQL_C_SBIGINT, 0 );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCBindColumn(authorizeStmt,2)" );
    ret = ODBCBindColumn( atxn->authorizeStmt, 2, SQL_C_CHAR, 16 );
    if (  ret != SUCCESS )
        goto error;

    LIST_LINK( atxn->txn->stmts, atxn->authorizeStmt );

    debugMessage( atxn->txn->ctxt, "DEBUG: EXITOK: initTxnAuthorize" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( atxn->txn->ctxt, 
                      errorMessage( ret, atxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    debugMessage( atxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( atxn->txn->ctxt->dbconn );

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( atxn->txn->ctxt->msgBuff, 
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( atxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

    if (  atxn->authorizeStmt != NULL  )
        ODBCFreeStmt( &(atxn->authorizeStmt) );

#if defined(DEBUG)
    sprintf( atxn->txn->ctxt->msgBuff, 
             "DEBUG: EXITERR: initTxnAuthorize: %d", ret );
    debugMessage( atxn->txn->ctxt, NULL );
#endif
    return ret;
} // initTxnAuthorize

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
                   )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (atxn == NULL) || (atxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    if (  (atxn->authorizeStmt == NULL) || (atxn->cursor != NULL)  )
        return ERR_STATE_INTERNAL;

    debugMessage( atxn->txn->ctxt, "DEBUG: ENTER: executeTxnAuthorize" );
#if defined(DEBUG)
    sprintf( atxn->txn->ctxt->msgBuff, 
             "DEBUG: accountID = %ld", atxn->accountID );
    debugMessage( atxn->txn->ctxt, NULL );
#endif

    clearRowData( atxn );

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_BIGINT(accountID)" );
    errStack = NULL;
    ret = ODBCSetParam_BIGINT( atxn->authorizeStmt, 1, atxn->accountID );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_CHAR(accountType)" );
    ret = ODBCSetParam_CHAR( atxn->authorizeStmt, 2, (char *)accountType );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCSetParam_INTEGER(accountStatus)" );
    ret = ODBCSetParam_INTEGER( atxn->authorizeStmt, 3, accountStatus );
    if (  ret != SUCCESS )
        goto error;

    debugMessage( atxn->txn->ctxt, 
                  "DEBUG: ODBCExecute(authorizeStmt)" );
    errStack = atxn->authorizeStmt->errors;
    ret = ODBCExecute( atxn->authorizeStmt );
    if (  ret != SUCCESS )
        goto error;
    atxn->cursor = atxn->authorizeStmt;

    debugMessage( atxn->txn->ctxt, "DEBUG: EXITOK: executeTxnAuthorize" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( atxn->txn->ctxt,
                      errorMessage( ret, atxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    clearRowData( atxn );
    debugMessage( atxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( atxn->txn->ctxt->dbconn );
    atxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( atxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( atxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( atxn->txn->ctxt->msgBuff, 
             "DEBUG: EXITERR: executeTxnAuthorize: %d", ret );
    debugMessage( atxn->txn->ctxt, NULL );
#endif
    return ret;
} // executeTxnAuthorize

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
                 )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (atxn == NULL) || (atxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( atxn->txn->ctxt, "DEBUG: ENTER: fetchTxnAuthorize" );
    if (  (atxn->authorizeStmt == NULL) || (atxn->cursor == NULL)  )
        return ERR_STATE_INTERNAL;

    debugMessage( atxn->txn->ctxt, "DEBUG: ODBCFetch(cursor)" );
    errStack = atxn->cursor->errors;
    ret = ODBCFetch( atxn->cursor );
    if (  ret == ERR_ODBC_NO_DATA  )
    {
        clearRowData( atxn );
        return ret;
    }
    if (  ret != SUCCESS )
        goto error;

    setRowData( atxn );

    debugMessage( atxn->txn->ctxt, "DEBUG: EXITOK: fetchTxnAuthorize" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( atxn->txn->ctxt,
                      errorMessage( ret, atxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    clearRowData( atxn );
    debugMessage( atxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( atxn->txn->ctxt->dbconn );
    atxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( atxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( atxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( atxn->txn->ctxt->msgBuff, 
             "DEBUG: EXITERR: fetchTxnAuthorize: %d", ret );
    debugMessage( atxn->txn->ctxt, NULL );
#endif
    return ret;
} // fetchTxnAuthorize

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
                 )
{
    int ret;
    int rtdelay = 0;
    odbcerrlist_t * errStack = NULL;

    if (  (atxn == NULL) || (atxn->txn == NULL)  )
        return ERR_PARAM_INTERNAL;

    debugMessage( atxn->txn->ctxt, "DEBUG: ENTER: closeTxnAuthorize" );
    if (  (atxn->authorizeStmt == NULL) || (atxn->cursor == NULL)  )
        return SUCCESS;

    debugMessage( atxn->txn->ctxt, "DEBUG: ODBCCloseCursor(cursor)" );
    clearRowData( atxn );
    errStack = atxn->cursor->errors;
    ret = ODBCCloseCursor( atxn->cursor );
    if (  ret != SUCCESS )
        goto error;
    atxn->cursor = NULL;

    if (  atxn->txn->ctxt->commitReadTxn  )
    {
        debugMessage( atxn->txn->ctxt, "DEBUG: ODBCCommit" );
        errStack = atxn->txn->ctxt->dbconn->errors;
        ret = ODBCCommit( atxn->txn->ctxt->dbconn );
        if (  ret != SUCCESS )
            goto error;
    }

    debugMessage( atxn->txn->ctxt, "DEBUG: EXITOK: closeTxnAuthorize" );
    return SUCCESS;

error:
    if (  isRetryable( ret, errStack, &rtdelay )  )
        ret =  ERR_ODBC_RETRYABLE;
    else
    if (  isFailover( ret, errStack, &rtdelay )  )
        ret = ERR_ODBC_FAILOVER;
    else
        displayError( atxn->txn->ctxt,
                      errorMessage( ret, atxn->txn->ctxt ),
                      (ret==ERR_ODBC_NORMAL)?errStack:NULL );
    clearRowData( atxn );
    debugMessage( atxn->txn->ctxt, "DEBUG: ODBCRollback" );
    ODBCRollback( atxn->txn->ctxt->dbconn );
    atxn->cursor = NULL;

    if (  rtdelay  )
    {
#if defined(DEBUG)
        sprintf( atxn->txn->ctxt->msgBuff,
                 "DEBUG: sleeping for %d ms", rtdelay );
        debugMessage( atxn->txn->ctxt, NULL );
#endif
        sleepMs( rtdelay );
    }

#if defined(DEBUG)
    sprintf( atxn->txn->ctxt->msgBuff, 
             "DEBUG: EXITERR: closeTxnAuthorize: %d", ret );
    debugMessage( atxn->txn->ctxt, NULL );
#endif
    return ret;
} // closeTxnAuthorize

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
                   )
{
    if (  atxn != NULL  )
    {
        debugMessage( atxn->txn->ctxt, "DEBUG: ENTER: cleanupTxnAuthorize" );
        if (  atxn->txn->stmts != NULL  )
        {
            if ( atxn->authorizeStmt != NULL  )
                LIST_UNLINK( atxn->txn->stmts, atxn->authorizeStmt );
        }
        ODBCFreeStmt( &(atxn->authorizeStmt) );
        debugMessage( atxn->txn->ctxt, "DEBUG: EXITOK: cleanupTxnAuthorize" );
        free( (void *)atxn );
    }
} // cleanupTxnAuthorize

