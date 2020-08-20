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
 *  GSErrors.c - error related data and functions
 *
 ****************************************************************************/

#include "GridSample.h"

/*
 * Error array. Used to get the error messge corresponding to an error code.
 */

static char const * const
errMsgs[] =
{
    ERRM_NO_ERROR,                // 0
    ERRM_HELP,                    // 1
    ERRM_PARAM_INTERNAL,          // 2
    ERRM_STATE_INTERNAL,          // 3
    ERRM_TYPE_MISMATCH_INTERNAL,  // 4
    ERRM_NOMEM,                   // 5
    ERRM_PARAM,                   // 6
    ERRM_SIGNAL,                  // 7
    ERRM_USER,                    // 8
    ERRM_DATA,                    // 9
    ERRM_ODBC_NO_DATA,            // 10
    ERRM_ODBC_NOINFO,             // 11
    ERRM_ODBC_NORMAL,             // 12
    ERRM_ODBC_RETRYABLE,          // 13
    ERRM_ODBC_FAILOVER            // 14
}; // errMsgs

// minimum valid error code
static int
minErr = 1;

// number of entries in the error message array
static int
maxErr = ( (sizeof( errMsgs ) / sizeof( char * )) - 1 );

/*
 * Retryable error array. Defines the ODBC errors that can be retried.
 */

static odbcerr_t 
retryableErrors[] =
{
    { NULL, NULL, TT_ERR_SQLSTATE_RETRY, NULL, 
                  TT_ERR_NATIVE_NONE, DFLT_RETRY_DELAY },
    { NULL, NULL, TT_ERR_SQLSTATE_NULL, NULL,
                  TT_ERR_NATIVE_DEADLOCK, DFLT_RETRY_DELAY },
    { NULL, NULL, TT_ERR_SQLSTATE_NULL, NULL,
                  TT_ERR_NATIVE_LOCKTIMEOUT, DFLT_RETRY_DELAY }
}; // retryableErrors

// number of entries in the retryableErrors array
static int
numRetryableErrors = sizeof(retryableErrors) / sizeof(odbcerr_t);

/*
 * C/S failover error array. Defines the ODBC errors that indicate a
 * connection failover event.
 */

static odbcerr_t
csFailoverErrors[] =
{
    { NULL, NULL, TT_ERR_SQLSTATE_NULL, NULL, 
                  TT_ERR_NATIVE_FAILOVER, DFLT_FAILOVER_DELAY },
    { NULL, NULL, TT_ERR_SQLSTATE_GENERAL, NULL, 
                  TT_ERR_NATIVE_NONE, DFLT_FAILOVER_DELAY }
}; // csFailoverErrors

// number of entries in the csFailoverErrors array
static int
numCsFailoverErrors = sizeof(csFailoverErrors) / sizeof(odbcerr_t);

/****************************************************************************
 *
 * Internal functions
 *
 ****************************************************************************/

/*
 * Check if an ODBC error represents a retryable error.
 *
 * INPUTS:
 *     err   - pointer to an odbcerr_t structure
 *
 * OUTPUTS:
 *     delay - pointer to an integer to receive the retry delay value if this 
 *             error is retryable (can be NULL if the value is not required)
 *
 * RETURNS:
 *     True if the error is a retryable error, false otherwise.
 */
static boolean
isRetryableError(
                 odbcerr_t const * err,
                 int             * delay
                )
{
    int i;

    if (  err != NULL  )
    {
        for (i = 0; i < numRetryableErrors; i++)
        {
            if (
                 (!retryableErrors[i].sqlstate[0] ||
                  (strcmp(retryableErrors[i].sqlstate,err->sqlstate)== 0))
                 &&
                 ((retryableErrors[i].native_error == TT_ERR_NATIVE_NONE) ||
                  (retryableErrors[i].native_error == err->native_error))
               )
            {
                if (  delay != NULL )
                    *delay = retryableErrors[i].retry_delay;
                return true;
            }
        }
    }
    return false;
} // isRetryableError

/*
 * Check if an ODBC error represents a failover error.
 *
 * INPUTS:
 *     err   - pointer to an odbcerr_t structure
 *
 * OUTPUTS:
 *     delay - pointer to an integer to receive the retry delay value if this
 *             error is a failover (can be NULL if the value is not required)
 *
 * RETURNS:
 *     True if the error is a failover error, false otherwise.
 */
static boolean
isCsFailoverError(
                  odbcerr_t const * err,
                  int             * delay
                 )
{
    int i;

    if (  err != NULL  )
    {
        for (i = 0; i < numCsFailoverErrors; i++)
        {
            if (
                 (!csFailoverErrors[i].sqlstate[0] ||
                  (strcmp(csFailoverErrors[i].sqlstate,err->sqlstate)== 0))
                 &&
                 ((csFailoverErrors[i].native_error == TT_ERR_NATIVE_NONE) ||
                 (csFailoverErrors[i].native_error == err->native_error))
               )
            {
                if (  delay != NULL )
                    *delay = csFailoverErrors[i].retry_delay;
                return true;
            }
        }
    }
    return false;
} // isCsFailover

/****************************************************************************
 *
 * Public functions
 *
 ****************************************************************************/

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
            )
{
    char const * emsg = ERRM_UNKNOWN;

    if (  (errorCode >= 0) && (errorCode <= maxErr)  )
    {
        if (  (errorCode == ERR_USER) && (ctxt != NULL)  )
            emsg = ctxt->msgBuff;
        else
            emsg = errMsgs[ errorCode ];
    }

    return emsg;
} // errorMessage

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
           )
{
    odbcerr_t * esp = NULL;

    if (  (err != ERR_ODBC_NORMAL) || (errstack == NULL)  )
        return false;

    esp = errstack->first;
    while (  esp != NULL  )
    {
        if (  isRetryableError( esp, delay )  )
            return true;
        esp = esp->next;
    }

    return false;
} // isRetryable

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
          )
{
    odbcerr_t * esp = NULL;

    if (  (err != ERR_ODBC_NORMAL) || (errstack == NULL)  )
        return false;

    esp = errstack->first;
    while (  esp != NULL  )
    {
        if (  isCsFailoverError( esp, delay )  )
            return true;
        esp = esp->next;
    }

    return false;
} // isFailover

