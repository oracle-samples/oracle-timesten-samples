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
 * GSOdbc.c - ODBC functionality
 *
 * TimesTen 18.1 introduced support for ODBC 3.5 but the default is still
 * the older ODBC 2.5 supported by previous versions of TimesTen. This
 * module illustrates how an application can support both API versions
 * based on the ODBCVER macro. It also shows a comparison of various ODBC
 * functions at both API levels so may be useful as a guide to migrating
 * existing TimsTen ODBC apps using ODBC 2.5 to the newer ODBC 3.5 API.
 *
 ****************************************************************************/

#include "GridSample.h"


/****************************************************************************
 *
 * Internal functions
 *
 ****************************************************************************/

/*
 * Check if an ODBC C data type is supported by this program for values
 * returned as part of a result set. If it is then validate the passed in
 * buffer length and return the size of buffer (in bytes) that needs to be
 * allocated to hold the returned column value.
 *
 * INPUTS:
 *     clientType   - ODBC C data type specifier
 *     bufferLength - the application proposed buffer length
 *
 * OUTPUTS:
 *     bindSize     - the number of bytes required to be allocated to store
 *                    the returned column value.
 *
 * RETURNS:
 *     True if success, false on error.
 */
static boolean
odbcValidColType(
                 SQLSMALLINT clientType,
                 SQLLEN      bufferLength,
                 int       * bindSize
                )
{
    boolean ret = false;

    if (  bindSize == NULL  )
        return false;
    *bindSize = 0;

    switch (  clientType  )
    {
        case SQL_C_CHAR:
            if (  bufferLength > 0  )
            {
                ret = true;
                *bindSize = (4 * bufferLength) + 1; // UTF-8
            }
            break;
        case SQL_C_UTINYINT:
            if (  bufferLength == 0  )
            {
                ret = true;
                *bindSize = sizeof(char);
            }
            break;
        case SQL_C_SSHORT:
            if (  bufferLength == 0  )
            {
                ret = true;
                *bindSize = sizeof(short);
            }
            break;
        case SQL_C_SLONG:
            if (  bufferLength == 0  )
            {
                ret = true;
                *bindSize = sizeof(int);
            }
            break;
        case SQL_C_SBIGINT:
            if (  bufferLength == 0  )
            {
                ret = true;
                *bindSize = sizeof(long);
            }
            break;
        case SQL_C_FLOAT:
            if (  bufferLength == 0  )
            {
                ret = true;
                *bindSize = sizeof(float);
            }
            break;
        case SQL_C_DOUBLE:
            if (  bufferLength == 0  )
            {
                ret = true;
                *bindSize = sizeof(double);
            }
            break;
        default:
            ret = false;
            break;
    }

    return ret;
} // odbcValidColType

/*
 * Check if the combination of an ODBC C data type and an ODBC database data
 * type is supported by this program for values provided as input parameters.
 * If it is then compute and return the size of buffer that must be allocated
 * to hold the value.
 *
 * INPUTS:
 *     clientType    - ODBC C data type specifier
 *     dbType        - ODBC database data type specifier
 *     ColumnSize    - ODBC column size value
 *     DecimalDigits - ODBC decimal digits value
 *
 * OUTPUTS:
 *     buffSize     - the number of bytes required to be allocated to store
 *                    the parameter value.
 *
 * RETURNS:
 *     True if success, false on error.
 */
static boolean
odbcTypeCompatible(
                   SQLSMALLINT clientType,
                   SQLSMALLINT dbType,
                   SQLULEN     ColumnSize,
                   SQLSMALLINT DecimalDigits,
                   int       * buffSize
                  )
{
    boolean ret = false;

    if (  buffSize == NULL  )
        return false;
    *buffSize = 0;

    switch (  dbType  )
    {
        case SQL_CHAR:
        case SQL_VARCHAR:
            if (  (ColumnSize == 0) || (DecimalDigits != 0)  )
                ret = false;
            else
                switch (  clientType  )
                {
                    case SQL_C_CHAR:
                        ret = true;
                        *buffSize = (ColumnSize * 4) + 1; // UTF-8
                        break;
                    case SQL_C_STINYINT:
                    case SQL_C_UTINYINT:
                        if (  ColumnSize < 4  )
                            ret = false;
                        else
                        {
                            ret = true;
                            *buffSize = sizeof(char);
                        }
                        break;
                    case SQL_C_SSHORT:
                    case SQL_C_USHORT:
                        if (  ColumnSize < 6  )
                            ret = false;
                        else
                        {
                            ret = true;
                            *buffSize = sizeof(short);
                        }
                    case SQL_C_SLONG:
                    case SQL_C_ULONG:
                        if (  ColumnSize < 11  )
                            ret = false;
                        else
                        {
                            ret = true;
                            *buffSize = sizeof(int);
                        }
                    case SQL_C_SBIGINT:
                    case SQL_C_UBIGINT:
                        if (  ColumnSize < 21  )
                            ret = false;
                        else
                        {
                            ret = true;
                            *buffSize = sizeof(long);
                        }
                    case SQL_C_FLOAT:
                        if (  ColumnSize < 29  )
                            ret = false;
                        else
                        {
                            ret = true;
                            *buffSize = sizeof(float);
                        }
                    case SQL_C_DOUBLE:
                        if (  ColumnSize < 58  )
                            ret = false;
                        else
                        {
                            ret = true;
                            *buffSize = sizeof(double);
                        }
                        break;
                    default:
                        ret = false;
                        break;
                }
            break;
        case SQL_TINYINT:
            if (  (ColumnSize != 0) || (DecimalDigits != 0)  )
                ret = false;
            else
                switch (  clientType  )
                {
                    case SQL_C_CHAR:
                        *buffSize = 3;
                        ret = true;
                        break;
                    case SQL_C_UTINYINT:
                        *buffSize = sizeof(char);
                        ret = true;
                        break;
                    default:
                        ret = false;
                        break;
                }
            break;
        case SQL_SMALLINT:
            if (  (ColumnSize != 0) || (DecimalDigits != 0)  )
                ret = false;
            else
                switch (  clientType  )
                {
                    case SQL_C_CHAR:
                        *buffSize = 7;
                        ret = true;
                        break;
                    case SQL_C_STINYINT:
                    case SQL_C_UTINYINT:
                        *buffSize = sizeof(char);
                        ret = true;
                        break;
                    case SQL_C_SSHORT:
                        *buffSize = sizeof(short);
                        ret = true;
                        break;
                    default:
                        ret = false;
                        break;
                }
            break;
        case SQL_INTEGER:
            if (  (ColumnSize != 0) || (DecimalDigits != 0)  )
                ret = false;
            else
                switch (  clientType  )
                {
                    case SQL_C_CHAR:
                        *buffSize = 12;
                        ret = true;
                        break;
                    case SQL_C_STINYINT:
                    case SQL_C_UTINYINT:
                        *buffSize = sizeof(char);
                        ret = true;
                        break;
                    case SQL_C_SSHORT:
                    case SQL_C_USHORT:
                        *buffSize = sizeof(short);
                        ret = true;
                        break;
                    case SQL_C_SLONG:
                        *buffSize = sizeof(int);
                        ret = true;
                        break;
                    default:
                        ret = false;
                        break;
                }
            break;
        case SQL_BIGINT:
            if (  (ColumnSize != 0) || (DecimalDigits != 0)  )
                ret = false;
            else
                switch (  clientType  )
                {
                    case SQL_C_CHAR:
                        *buffSize = 22;
                        ret = true;
                        break;
                    case SQL_C_STINYINT:
                    case SQL_C_UTINYINT:
                        *buffSize = sizeof(char);
                        ret = true;
                        break;
                    case SQL_C_SSHORT:
                    case SQL_C_USHORT:
                        *buffSize = sizeof(short);
                        ret = true;
                        break;
                    case SQL_C_SLONG:
                    case SQL_C_ULONG:
                        *buffSize = sizeof(int);
                        ret = true;
                        break;
                    case SQL_C_SBIGINT:
                        *buffSize = sizeof(long);
                        ret = true;
                        break;
                    default:
                        ret = false;
                        break;
                }
            break;
        case SQL_NUMERIC:
            if (  (ColumnSize == 0) || 
                  (DecimalDigits < 0) || 
                  (DecimalDigits > ColumnSize)  )
                ret = false;
            else
                switch (  clientType  )
                {
                    case SQL_C_CHAR:
                        *buffSize = ColumnSize + 3;
                        ret = true;
                        break;
                    case SQL_C_STINYINT:
                    case SQL_C_UTINYINT:
                        *buffSize = sizeof(char);
                        ret = true;
                        break;
                    case SQL_C_SSHORT:
                    case SQL_C_USHORT:
                        *buffSize = sizeof(short);
                        ret = true;
                        break;
                    case SQL_C_SLONG:
                    case SQL_C_ULONG:
                        *buffSize = sizeof(int);
                        ret = true;
                        break;
                    case SQL_C_SBIGINT:
                    case SQL_C_UBIGINT:
                        *buffSize = sizeof(long);
                        ret = true;
                        break;
                    case SQL_C_FLOAT:
                        *buffSize = sizeof(float);
                        ret = true;
                        break;
                    case SQL_C_DOUBLE:
                        *buffSize = sizeof(double);
                        ret = true;
                        break;
                    default:
                        ret = false;
                        break;
                }
            break;
        case SQL_REAL:
        case SQL_DOUBLE:
            if (  (ColumnSize == 0) || (DecimalDigits != 0)  )
                ret = false;
            else
                switch (  clientType  )
                {
                    case SQL_C_CHAR:
                        if (  dbType == SQL_DOUBLE  )
                            *buffSize = 58;
                        else
                            *buffSize = 29;
                        ret = true;
                        break;
                    case SQL_C_STINYINT:
                    case SQL_C_UTINYINT:
                        *buffSize = sizeof(char);
                        ret = true;
                        break;
                    case SQL_C_SSHORT:
                    case SQL_C_USHORT:
                        *buffSize = sizeof(short);
                        ret = true;
                        break;
                    case SQL_C_SLONG:
                    case SQL_C_ULONG:
                        *buffSize = sizeof(int);
                        ret = true;
                        break;
                    case SQL_C_SBIGINT:
                    case SQL_C_UBIGINT:
                        *buffSize = sizeof(long);
                        ret = true;
                        break;
                    case SQL_C_FLOAT:
                        *buffSize = sizeof(float);
                        ret = true;
                        break;
                    case SQL_C_DOUBLE:
                        *buffSize = sizeof(double);
                        ret = true;
                        break;
                    default:
                        ret = false;
                        break;
                }
            break;
        default:
            ret = false;
            break;
    }

    return ret;
} // odbcTypeCompatible

/*
 * Given a list of parameters, return the item whose parameter number matches
 * the provided one.
 *
 * INPUTS:
 *     pl  - parameter list
 *     pno - the desired parameter number
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the requested entry if present, NULL otherwise.
 */
static parambinding_t *
odbcGetParameter(
                 paramlist_t * pl,
                 int           pno
                )
{
    parambinding_t * p = NULL;

    if (  (pl == NULL) || (pno < 1)  )
        return NULL;

    p = pl->first;
    while (  p != NULL  )
    {
        if (  p->paramno == (SQLSMALLINT)pno  )
            return p;
        p = p->next;
    }

    return NULL;
} // odbcGetParameter

/*
 * Given a list of columns, return the item whose column number matches
 * the provided one.
 *
 * INPUTS:
 *     cl  - column list
 *     cno - the desired column number
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     A pointer to the requested entry if present, NULL otherwise.
 */
static colbinding_t *
odbcGetColumn(
              collist_t * cl,
              int         cno
             )
{
    colbinding_t * c = NULL;

    if (  (cl == NULL) || (cno < 1)  )
        return NULL;

    c = cl->first;
    while (  c != NULL  )
    {
        if (  c->colno == (SQLSMALLINT)cno  )
            return c;
        c = c->next;
    }

    return NULL;
} // odbcGetColumn

/*
 * Free a parameter structure.
 *
 * INPUTS:
 *     param - a pointer to a pointer to the structure
 *     defer - true; do not free the top level structure
 *             false; free the top level structure
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
static void
odbcFreeParam(
              parambinding_t ** param,
              boolean           defer
             )
{
    if (  (param == NULL) || (*param == NULL)  )
        return; // okay to free an already freed parameter

    (*param)->paramno = 0;
    (*param)->stmt = NULL;
    if (  (*param)->ParameterValuePtr != NULL  )
    {
        free( (void *)(*param)->ParameterValuePtr );
        (*param)->ParameterValuePtr = NULL;
    }
    if (  ! defer  )
    {
        free( (void *)(*param) );
        (*param) = NULL;
    }
} // odbcFreeParam

/*
 * Free a column structure.
 *
 * INPUTS:
 *     col   - a pointer to a pointer to the structure
 *     defer - true; do not free the top level structure
 *             false; free the top level structure
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
static void
odbcFreeCol(
            colbinding_t ** col,
            boolean         defer
           )
{
    if (  (col == NULL) || (*col == NULL)  )
        return; // okay to free an already freed column

    (*col)->colno = 0;
    (*col)->stmt = NULL;
    if (  (*col)->TargetValuePtr != NULL  )
    {
        free( (void *)(*col)->TargetValuePtr );
        (*col)->TargetValuePtr = NULL;
    }
    if (  ! defer  )
    {
        free( (void *)(*col) );
        (*col) = NULL;
    }
} // odbcFreeCol

/*
 * Allocate and initialize a parameter structure.
 *
 * INPUTS:
 *     stmt          - pointer to the statement structure with which the
 *                     parameter is associated
 *     paramno       - the parameter number (> 0)
 *     clientType    - the ODBC C data type for the parameter
 *     dbType        - the ODBC database data type for the parameter
 *     ColumnSize    - the ODBC ColumnSize value for the parameter
 *     DecimalDigits - the ODBC DecimalDigits value for the parameter
 *
 * OUTPUTS:
 *     error        - a pointer to an integer to receive the error code
 *
 * RETURNS:
 *     Pointer to the allocated and initialized structure, or NULL if an
 *     error occurred.
 */
static parambinding_t *
odbcAllocParam(
               int         * error,
               sqlstmt_t   * stmt,
               int           paramno,
               SQLSMALLINT   clientType,
               SQLSMALLINT   dbType,
               SQLULEN       ColumnSize,
               SQLSMALLINT   DecimalDigits
              )
{
    parambinding_t * p = NULL;
    int buffSize = 0;

    if (  error == NULL )
        return NULL;

    if (  (stmt == NULL) || (paramno < 1)  )
    {
        *error = ERR_PARAM_INTERNAL;
        return NULL;
    }
    if (  ! odbcTypeCompatible( clientType, dbType, 
                                ColumnSize, DecimalDigits, &buffSize )  )
    {
        *error = ERR_TYPE_MISMATCH_INTERNAL;
        return NULL;
    }

    p = (parambinding_t *)calloc(1, sizeof(parambinding_t) );
    if ( p == NULL  )
    {
        *error = ERR_NOMEM;
        return NULL;
    }
    p->ParameterValuePtr = (SQLPOINTER)calloc(1, buffSize);
    if ( p->ParameterValuePtr == NULL  )
    {
        *error = ERR_NOMEM;
        free( (void *)p );
        return NULL;
    }

    p->stmt = stmt;
    p->paramno = paramno;
    p->InputOutputType = SQL_PARAM_INPUT;
    p->ValueType = clientType;
    p->ParameterType = dbType;
    p->ColumnSize = ColumnSize;
    p->DecimalDigits = DecimalDigits;
    if (  clientType == SQL_C_CHAR  )
    {
        p->BufferLength = buffSize;
        p->StrLen_or_IndPtr = SQL_NTS;
    }
    else
    {
        p->BufferLength = 0;
        p->StrLen_or_IndPtr = 0;
    }

    return p;
} // odbcAllocParam

/*
 * Allocate and initialize a column structure.
 *
 * INPUTS:
 *     stmt          - pointer to the statement structure with which the
 *                     column is associated
 *     colno         - the column number (> 0)
 *     clientType    - the ODBC C data type for the parameter
 *     BufferLength  - the application's proposed buffer length for
 *                     the column
 *
 * OUTPUTS:
 *     error        - a pointer to an integer to receive the error code
 *
 * RETURNS:
 *     Pointer to the allocated and initialized structure, or NULL if an
 *     error occurred.
 */
static colbinding_t *
odbcAllocCol(
             int         * error,
             sqlstmt_t   * stmt,
             int           colno,
             SQLSMALLINT   clientType,
             SQLLEN        BufferLength
            )
{
    colbinding_t * c = NULL;
    int bindSize = 0;

    if (  error == NULL )
        return NULL;

    if (  (stmt == NULL) || (colno < 1)  )
    {
        *error = ERR_PARAM_INTERNAL;
        return NULL;
    }
    if (  !  odbcValidColType( clientType, BufferLength, &bindSize )  )
    {
        *error = ERR_TYPE_MISMATCH_INTERNAL;
        return NULL;
    }

    c = (colbinding_t *)calloc(1, sizeof(colbinding_t) );
    if ( c == NULL  )
    {
        *error = ERR_NOMEM;
        return NULL;
    }
    c->TargetValuePtr = (SQLPOINTER)calloc(1, bindSize);
    if ( c->TargetValuePtr == NULL  )
    {
        *error = ERR_NOMEM;
        free( (void *)c );
        return NULL;
    }

    c->stmt = stmt;
    c->colno = colno;
    c->TargetType = clientType;
    c->BufferLength = bindSize;
    c->StrLen_or_Ind = 0;

    return c;
} // odbcAllocCol

/*
 * Free and unlink all the entries in an ODBC error list.
 *
 * INPUTS:
 *     errlist       - pointer to a pointer to the list
 *
 * OUTPUTS: None
 *
 * RETURNS: Nothing
 */
static void
odbcFreeErrors(
               odbcerrlist_t ** errlist
              )
{
    odbcerr_t * err, * terr;

    if (  (errlist == NULL) ||
          (*errlist == NULL)  )
        return;

    err = (*errlist)->first;
    while (  err != NULL  )
    {
        terr = err;
        if (  err->error_msg != NULL  )
        {
            free( (void *)err->error_msg );
            err->error_msg = NULL;
        }
        err = err->next;
        free( (void *)terr );
    }
    (*errlist)->first = (*errlist)->last = NULL;
    (*errlist)->numentries = 0;
} // odbcFreeErrors

/*
 * Retrieve any errors and warnings on an ODBC handle and build an
 * ODBC error list containing them.
 *
 * INPUTS:
 *     handleType - the type of the ODBC handle
 *     handle     - the ODBC handle
 *
 * OUTPUTS:
 *     errlist       - pointer to a pointer to the list
 *
 * RETURNS: Nothing
 */
static int
odbcGetErrors(
              odbcerrlist_t ** errlist,
              SQLSMALLINT      handleType,
              SQLHANDLE        handle
             )
{
    SQLRETURN rc;
    SQLSMALLINT i = 1;
    SQLINTEGER native_error;
    SQLCHAR sqlstate[LEN_SQLSTATE+1];
    SQLCHAR msgbuff[1024];
    SQLSMALLINT msglen;
    odbcerr_t * err;
#if ODBCVER < 0x0350
    SQLHENV  hEnv = SQL_NULL_HENV;
    SQLHDBC  hDbc = SQL_NULL_HDBC;
    SQLHSTMT hStmt = SQL_NULL_HSTMT;
#endif


    if (  errlist == NULL  )
        return ERR_PARAM_INTERNAL;
    odbcFreeErrors( errlist );

    native_error = 0;
    sqlstate[0] = '\0';
#if ODBCVER >= 0x0350
    rc = SQLGetDiagRec( handleType, handle, i, sqlstate, &native_error,
                        msgbuff, sizeof(msgbuff), &msglen );
#else
    switch (  handleType  )
    {
        case SQL_HANDLE_ENV:
            hEnv = (SQLHENV)handle;
            break;
        case SQL_HANDLE_DBC:
            hDbc = (SQLHDBC)handle;
            break;
        case SQL_HANDLE_STMT:
            hStmt = (SQLHSTMT)handle;
            break;
        default:
            return ERR_PARAM_INTERNAL;
            break;
    }
    rc = SQLError( hEnv, hDbc, hStmt, sqlstate, &native_error,
                   msgbuff, sizeof(msgbuff), &msglen );
#endif
    if (  rc == SQL_INVALID_HANDLE  )
        return ERR_ODBC_NOINFO;

    while (  rc != SQL_NO_DATA  )
    {
        if (  rc != SQL_SUCCESS  )
            break;
        if (  *errlist == NULL  )
        {
            *errlist = (odbcerrlist_t *)calloc(1, sizeof(odbcerrlist_t));
            if (  *errlist == NULL  )
                return ERR_NOMEM;
        }
        err = (odbcerr_t *)calloc(1, sizeof(odbcerr_t));
        if (  err == NULL  ) // out of memory so
            break;           // return what we have
        err->error_msg = strdup( (char *)msgbuff );
        if (  err->error_msg == NULL  ) // out of memory so
        {
            free( (void *)err );
            break;                      // return what we have
        }
        err->native_error = native_error;
        strcpy( err->sqlstate, (char *)sqlstate );
        LIST_LINK( (*errlist), err);
        native_error = 0;
        sqlstate[0] = '\0';
#if ODBCVER >= 0x0350
        rc = SQLGetDiagRec( handleType, handle, ++i, sqlstate,
                            &native_error, msgbuff, sizeof(msgbuff),
                            &msglen );
#else
        i++;
        rc = SQLError( hEnv, hDbc, hStmt, sqlstate, &native_error,
                       msgbuff, sizeof(msgbuff), &msglen );
#endif
    }
    if (  i == 1  ) // failed to complete first item
    {
        odbcFreeErrors( errlist );
        return ERR_ODBC_NOINFO;
    }

    return ERR_ODBC_NORMAL;
} // odbcGetErrors

/*
 * Free all memory and resources for an ODBC statement structure.
 *
 * INPUTS:
 *     stmt - a pointer to a pointer to the statement structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
static int
odbcFreeStmt(
                sqlstmt_t ** stmt
            )
{
    parambinding_t * param;
    colbinding_t   * col;
    SQLRETURN rc;
    int       ret;

    if (  (stmt == NULL) || (*stmt == NULL)  )
        return SUCCESS; // okay to free an already freed statement

    // clear error list
    odbcFreeErrors( &((*stmt)->errors)  );

    // Free the ODBC statement resources
    if (  (*stmt)->hStmt != SQL_NULL_HANDLE  )
    {
#if ODBCVER >= 0x0350
        rc = SQLFreeHandle( SQL_HANDLE_STMT, (*stmt)->hStmt );
#else
        rc = SQLFreeStmt( (*stmt)->hStmt, SQL_DROP );
#endif
        if (  (rc == SQL_SUCCESS) || (rc == SQL_INVALID_HANDLE)  )
            (*stmt)->hStmt = SQL_NULL_HANDLE;
        else
            ret = odbcGetErrors( &((*stmt)->errors),
                                 SQL_HANDLE_STMT,
                                 (*stmt)->hStmt );
    }

    // Free non-ODBC resources

    // Free sqltext
    if (  (*stmt)->sqltext != NULL  )
    {
        free( (void *)(*stmt)->sqltext );
        (*stmt)->sqltext = NULL;
    }
    
    // Free any parameters associated with the statement
    if (  (*stmt)->params != NULL  )
    {
        while (  (*stmt)->params->first != NULL  )
        {
            param = (*stmt)->params->first;
            LIST_UNLINK( (*stmt)->params, param );
            odbcFreeParam( &param, false );
        }
        free( (void *)(*stmt)->params );
        (*stmt)->params = NULL;
    }

    // Free any columns associated with the statement
    if (  (*stmt)->cols != NULL  )
    {
        {
            col = (*stmt)->cols->first;
            LIST_UNLINK( (*stmt)->cols, (*stmt)->cols->first );
            odbcFreeCol( &col, false );
        }
        free( (void *)(*stmt)->cols );
        (*stmt)->cols = NULL;
    }

    // Free the error list
    if (  (*stmt)->errors != NULL  )
    {
        free( (void *)((*stmt)->errors) );
        (*stmt)->errors = NULL;
    }

    return SUCCESS;
} // odbcFreeStmt

/*
 * Free all memory and resources for an ODBC connection structure.
 *
 * INPUTS:
 *     conn - a pointer to a pointer to the connection structure
 *
 * OUTPUTS: None
 *
 * RETURNS:
 *     Success or an error code.
 */
static int
odbcFreeConn(
             dbconnection_t ** conn
            )
{
    SQLRETURN rc;
    int       ret;

    if (  (conn == NULL) || (*conn == NULL)  )
        return SUCCESS; // okay to free an already freed connection

    if (  (*conn)->connected  )
        return ERR_STATE_INTERNAL;

    // Free non ODBC memory
    if (  (*conn)->dsn != NULL  )
    {
        free( (void *)(*conn)->dsn );
        (*conn)->dsn = NULL;
    }
    if (  (*conn)->uid != NULL  )
    {
        free( (void *)(*conn)->uid );
        (*conn)->uid = NULL;
    }
    if (  (*conn)->pwd != NULL  )
    {
        free( (void *)(*conn)->pwd );
        (*conn)->pwd = NULL;
    }
    if (  (*conn)->cname != NULL  )
    {
        free( (void *)(*conn)->cname );
        (*conn)->cname = NULL;
    }

    // clear error list
    odbcFreeErrors( &((*conn)->errors)  );

    // Free any statements associated with the connection
    if (  (*conn)->stmts != NULL  )
    {
        while (  (*conn)->stmts->first != NULL  )
        {
            ret = odbcFreeStmt( &((*conn)->stmts->first) );
            if (  ret != SUCCESS  )
            {
                if (  ret == ERR_ODBC_NORMAL  )
                { // report errors against the object in the call
                    (*conn)->errors = (*conn)->stmts->first->errors;
                    (*conn)->stmts->first->errors = NULL; // prevent double free
                }
                return ret;
            }
            LIST_UNLINK( (*conn)->stmts, (*conn)->stmts->first );
        }
        free( (void *)((*conn)->stmts)  );
        (*conn)->stmts = NULL;
    }

    // Free the ODBC connection resources
    if (  (*conn)->hDbc != SQL_NULL_HANDLE  )
    { 
#if ODBCVER >= 0x0350
        rc = SQLFreeHandle( SQL_HANDLE_DBC, (*conn)->hDbc );
#else
        rc = SQLFreeConnect( (*conn)->hDbc );
#endif
        if (  (rc == SQL_SUCCESS) || (rc == SQL_INVALID_HANDLE)  )
            (*conn)->hDbc = SQL_NULL_HANDLE;
        else
            ret = odbcGetErrors( &((*conn)->errors),
                                 SQL_HANDLE_DBC,
                                 (*conn)->hDbc );
    }

    // Free the ODBC environment resources
    if (  (*conn)->hEnv != SQL_NULL_HANDLE  )
    {
#if ODBCVER >= 0x0350
        rc = SQLFreeHandle( SQL_HANDLE_ENV, (*conn)->hEnv );
#else
        rc = SQLFreeEnv( (*conn)->hEnv );
#endif
        if (  (rc == SQL_SUCCESS) || (rc == SQL_INVALID_HANDLE)  )
            (*conn)->hEnv = SQL_NULL_HANDLE;
        else
            ret = odbcGetErrors( &((*conn)->errors),
                                 SQL_HANDLE_ENV,
                                 (*conn)->hEnv );
    }

    // Free the error list
    if (  (*conn)->errors != NULL  )
    {
        free( (void *)((*conn)->errors) );
        (*conn)->errors = NULL;
    }

    return SUCCESS;
} // odbcFreeConn

/****************************************************************************
 *
 * Public interface
 *
 ****************************************************************************/

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
              )
{
    odbcFreeErrors( errlist );
} // ODBCFreeErrors

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
             )
{
    return odbcGetErrors( errlist, handleType, handle );
} // ODBCGetErrors

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
            )
{
    int       ret = SUCCESS;

    if (  (stmt != NULL) && (*stmt != NULL)  )
    {
        if (  (*stmt)->conn != NULL  )
            LIST_UNLINK( (*stmt)->conn->stmts, (*stmt) );
        ret = odbcFreeStmt( stmt );
        if (  ret == SUCCESS  )
        {
            free( (void *)(*stmt) );
            *stmt = NULL;
        }
    }

    return ret;
} // ODBCFreeStmt

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
             )
{
    SQLRETURN rc;
    int       ret = SUCCESS;

    if (  (conn == NULL) || (stmt == NULL)  )
        return ERR_PARAM_INTERNAL;
    if (  *stmt != NULL  )
        return ERR_STATE_INTERNAL;

    *stmt = (sqlstmt_t *)calloc( 1, sizeof(sqlstmt_t) );
    if (  *stmt == NULL  )
        return ERR_NOMEM;
    (*stmt)->errors = (odbcerrlist_t *)calloc( 1, sizeof(odbcerrlist_t) );
    if (  (*stmt)->errors == NULL  )
    {
        free( (void *)(*stmt) );
        *stmt = NULL;
        return ERR_NOMEM;
    }

    (*stmt)->conn = conn;

    // Allocate a statement handle
#if ODBCVER >= 0x0350
    rc = SQLAllocHandle( SQL_HANDLE_STMT,
                         conn->hDbc,
                         &((*stmt)->hStmt) );
#else
    rc = SQLAllocStmt( conn->hDbc, &((*stmt)->hStmt) );
#endif
    if (  rc == SQL_SUCCESS )
    {
        LIST_LINK( conn->stmts, (*stmt) );
        return SUCCESS;
    }

    if (  rc == SQL_INVALID_HANDLE  ) // highly unlikely
    {
        (*stmt)->hStmt = SQL_NULL_HANDLE;
        ret = ERR_ODBC_NOINFO;
    }
    else
        ret = odbcGetErrors( &(conn->errors),
                             SQL_HANDLE_DBC,
                             conn->hDbc );

    return ret;
} // ODBCAllocStmt

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
            )
{
    int ret = SUCCESS;

    if (  (conn != NULL) && (*conn != NULL)  )
    {
        if (  (ret = odbcFreeConn( conn )) == SUCCESS  )
        {
            free( (void *)(*conn)  );
            *conn = NULL;
        }
    }

    return ret;
} // ODBCFreeConn

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
             )
{
    SQLRETURN rc;

    if (  conn == NULL  )
        return ERR_PARAM_INTERNAL;
    if (  *conn != NULL  )
        return ERR_STATE_INTERNAL;

    *conn = (dbconnection_t *)calloc( 1, sizeof(dbconnection_t) );
    if (  *conn == NULL  )
        return ERR_NOMEM;
    (*conn)->errors = (odbcerrlist_t *)calloc( 1, sizeof(odbcerrlist_t) );
    if (  (*conn)->errors == NULL  )
    {
        free( (void *)(*conn) );
        *conn = NULL;
        return ERR_NOMEM;
    }
    (*conn)->hEnv = (*conn)->hDbc = SQL_NULL_HANDLE;

    // Allocate an environment handle
#if ODBCVER >= 0x0350
    if (  SQLAllocHandle( SQL_HANDLE_ENV, 
                          SQL_NULL_HANDLE,
                          &((*conn)->hEnv) ) != SQL_SUCCESS  )
#else
    if (  SQLAllocEnv( &((*conn)->hEnv) ) != SQL_SUCCESS  )
#endif
    {
        free( (void *)((*conn)->errors) );
        free( (void *)(*conn) );
        *conn = NULL;
        return ERR_ODBC_NOINFO;
    }

#if ODBCVER >= 0x0350
    // Request ODBC 3.x behaviour
    rc = SQLSetEnvAttr( (*conn)->hEnv,
                        SQL_ATTR_ODBC_VERSION,
                        (SQLPOINTER)SQL_OV_ODBC3,
                        0 );
    if (  rc != SQL_SUCCESS  )
    {
        free( (void *)((*conn)->errors) );
        free( (void *)(*conn) );
        *conn = NULL;
        return ERR_ODBC_NOINFO;
    }
#endif

#if ODBCVER >= 0x0350
    // Allocate a connection handle
    rc = SQLAllocHandle( SQL_HANDLE_DBC, 
                         (*conn)->hEnv,
                         &((*conn)->hDbc) );
#else
    rc = SQLAllocConnect( (*conn)->hEnv, &((*conn)->hDbc) );
#endif
    if (  rc != SQL_SUCCESS )
    {
        if (  rc != SQL_INVALID_HANDLE  ) 
            SQLFreeHandle( SQL_HANDLE_ENV, (*conn)->hEnv );
        free( (void *)((*conn)->errors) );
        free( (void *)(*conn) );
        *conn = NULL;
        return ERR_ODBC_NOINFO;
    }

    return SUCCESS;
} // ODBCAllocConn

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
          )
{
    int ret = SUCCESS;
    SQLRETURN rc;

    if (  conn == NULL  )
        return ERR_PARAM_INTERNAL;

    odbcFreeErrors( &(conn->errors)  );

    if (  (conn->hEnv == SQL_NULL_HANDLE) ||
          (conn->hDbc == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

#if ODBCVER >= 0x0350
    rc = SQLEndTran( SQL_HANDLE_DBC, conn->hDbc, SQL_COMMIT );
#else
    rc = SQLTransact( SQL_NULL_HENV, conn->hDbc, SQL_COMMIT );
#endif
    if (  rc == SQL_SUCCESS  )
        return SUCCESS;

    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(conn->errors),
                             SQL_HANDLE_DBC,
                             conn->hDbc );

    return ret;
} // ODBCCommit

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
            )
{
    int ret = SUCCESS;
    SQLRETURN rc;

    if (  conn == NULL  )
        return ERR_PARAM_INTERNAL;

    odbcFreeErrors( &(conn->errors)  );

    if (  (conn->hEnv == SQL_NULL_HANDLE) ||
          (conn->hDbc == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

#if ODBCVER >= 0x0350
    rc = SQLEndTran( SQL_HANDLE_DBC, conn->hDbc, SQL_ROLLBACK );
#else
    rc = SQLTransact( SQL_NULL_HENV, conn->hDbc, SQL_ROLLBACK );
#endif
    if (  rc == SQL_SUCCESS  )
        return SUCCESS;

    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(conn->errors),
                             SQL_HANDLE_DBC,
                             conn->hDbc );

    return ret;;
} // ODBCRollback

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
              )
{
    int ret = SUCCESS;
    SQLRETURN rc;

    if (  conn == NULL  )
        return ERR_PARAM_INTERNAL;

    odbcFreeErrors( &(conn->errors)  );

    if (  ! conn->connected  )
        return SUCCESS;

    if (  (conn->hEnv == SQL_NULL_HANDLE) ||
          (conn->hDbc == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

    rc = SQLDisconnect( conn->hDbc );
    if (  rc == SQL_SUCCESS  )
    {
        conn->connected = false;
        return SUCCESS;
    }

    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(conn->errors),
                             SQL_HANDLE_DBC,
                             conn->hDbc );

    return ret;
} // ODBCDisconnect

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
           )
{
    char connstr[MAX_LEN_CONNSTR+1];
    int p = 0, ret = SUCCESS;
    SQLRETURN rc;
    SQLSMALLINT csLen;

    if (  (conn == NULL) || (dsn == NULL) || (*dsn == '\0')  )
        return ERR_PARAM_INTERNAL;
    if (  ((uid == NULL) || (*uid == '\0')) &&
          ((pwd != NULL) && (*pwd != '\0'))  )
        return ERR_PARAM;

    odbcFreeErrors( &(conn->errors)  );

    if (  conn->connected  ||
          (conn->hEnv == SQL_NULL_HANDLE) ||
          (conn->hDbc == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

    conn->uid = conn->pwd = conn->cname = NULL;
    conn->dsn = strdup(dsn);
    if (  conn->dsn == NULL  )
        return ERR_NOMEM;

    if (  (uid != NULL) && (*uid != '\0')  )
    {
        conn->uid = strdup(uid);
        if (  conn->uid == NULL  )
            ret = ERR_NOMEM;
    }

    if (  (ret == SUCCESS) && (pwd != NULL) && (*pwd != '\0')  )
    {
        conn->pwd = strdup(pwd);
        if (  conn->pwd == NULL  )
            ret = ERR_NOMEM;
    }

    if (  (ret == SUCCESS) && (cname != NULL) && (*cname != '\0')  )
    {
        conn->cname = strdup(cname);
        if (  conn->cname == NULL  )
            ret = ERR_NOMEM;
    }

    if (  ret == SUCCESS  )
    {
        // Build connection string
        p += sprintf( &connstr[p], "%s=%s", ATTR_DSN, dsn );
        if (  (uid != NULL) && (*uid != '\0')  )
            p += sprintf( &connstr[p], ";%s=%s", ATTR_UID, uid );
        if (  (pwd != NULL) && (*pwd != '\0')  )
            p += sprintf( &connstr[p], ";%s=%s", ATTR_PWD, pwd );
        if (  (cname != NULL) && (*cname != '\0')  )
            p += sprintf( &connstr[p], ";%s=%s", ATTR_CONN_NAME, cname );

        // Open connection
        rc = SQLDriverConnect( conn->hDbc,
                               NULL,
                               (SQLCHAR *)connstr,
                               strlen( connstr ),
                               NULL,
                               0,
                               &csLen,
                               SQL_DRIVER_NOPROMPT );
        if (  rc != SQL_SUCCESS  )
        {
            if (  rc == SQL_INVALID_HANDLE  )
                ret = ERR_ODBC_NOINFO;
            else
                ret = odbcGetErrors( &(conn->errors),
                                     SQL_HANDLE_DBC,
                                     conn->hDbc );
        }
        else
            conn->connected = true;
    
        if (  ret == SUCCESS  )
        {
            // Disable autocommit mode.
#if ODBCVER >= 0x0350
            rc = SQLSetConnectAttr( conn->hDbc,
                                   SQL_ATTR_AUTOCOMMIT,
                                   SQL_AUTOCOMMIT_OFF,
                                   0 );
#else
            rc = SQLSetConnectOption( conn->hDbc,
                                   SQL_AUTOCOMMIT,
                                   SQL_AUTOCOMMIT_OFF );
#endif
            if (  rc != SQL_SUCCESS  )
            {
                if (  rc == SQL_INVALID_HANDLE  )
                    ret = ERR_ODBC_NOINFO;
                else
                    ret = odbcGetErrors( &(conn->errors),
                                         SQL_HANDLE_DBC,
                                         conn->hDbc );
            }
        }
    }

    // if error occurred, clean up resources
    if (  ret != SUCCESS  )
    {
        if (  conn->dsn != NULL  )
        {
            free( (void *)conn->dsn );
            conn->dsn = NULL;
        }
        if (  conn->uid != NULL  )
        {
            free( (void *)conn->uid );
            conn->uid = NULL;
        }
        if (  conn->pwd != NULL  )
        {
            free( (void *)conn->pwd );
            conn->pwd = NULL;
        }
        if (  conn->cname != NULL  )
        {
            free( (void *)conn->cname );
            conn->cname = NULL;
        }
        if (  conn->connected  )
        {
            // try to disconnect
            rc = SQLDisconnect( conn->hDbc );
            if (  (rc == SQL_SUCCESS) || (rc == SQL_SUCCESS_WITH_INFO)  )
                conn->connected = false;
        }
    }
    
   return ret;
} // ODBCConnect

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
           )
{
    int ret = SUCCESS;
    char * sqlt = NULL;
    SQLRETURN rc;

    if (  (stmt == NULL) || (sqlstmt == NULL)  )
        return ERR_PARAM_INTERNAL;
    if (  (stmt->errors == NULL) || (stmt->hStmt == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

    sqlt = strdup( sqlstmt );
    if (  sqlt == NULL  )
        return ERR_NOMEM;

    odbcFreeErrors( &(stmt->errors)  );

    rc = SQLPrepare( stmt->hStmt, (SQLCHAR *)sqlstmt, SQL_NTS );
    if (  rc == SQL_SUCCESS  )
    {
        stmt->sqltext = sqlt;
        return SUCCESS;
    }

    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(stmt->errors),
                             SQL_HANDLE_STMT,
                             stmt->hStmt );

    return ret;
} // ODBCPrepare

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
                 )
{
    int ret = SUCCESS;
    SQLRETURN rc;
    parambinding_t * param = NULL;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    if (  odbcGetParameter( stmt->params, paramno ) != NULL  )
        return ERR_PARAM_INTERNAL;

    param = odbcAllocParam( &ret, stmt, paramno, clientType,
                            dbType, ColumnSize, DecimalDigits );
    if (  param == NULL  )
        return ret;

    if (  stmt->params == NULL  )
    {
        stmt->params = (paramlist_t *)calloc(1, sizeof(paramlist_t));
        if (  stmt->params == NULL  )
            return ERR_NOMEM;
    }

    rc = SQLBindParameter( stmt->hStmt,
                           param->paramno,
                           param->InputOutputType,
                           param->ValueType,
                           param->ParameterType,
                           param->ColumnSize,
                           param->DecimalDigits,
                           param->ParameterValuePtr,
                           param->BufferLength,
                           &(param->StrLen_or_IndPtr) );
    if (  rc == SQL_SUCCESS  )
    {
        LIST_LINK( stmt->params, param );
        return SUCCESS;
    }

    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(stmt->errors),
                             SQL_HANDLE_STMT,
                             stmt->hStmt );

    return ret;
} // ODBCBindParameter

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
                  )
{
    parambinding_t * param = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_UTINYINT:
            *val = (unsigned char)(*(unsigned char *)param->ParameterValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetParam_TINYINT

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
                  )
{
    parambinding_t * param = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_UTINYINT:
            *val = (short)(*(unsigned char *)param->ParameterValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (short)(*(short *)param->ParameterValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetParam_SMALLINT

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
                  )
{
    parambinding_t * param = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_UTINYINT:
            *val = (int)(*(unsigned char *)param->ParameterValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (int)(*(short *)param->ParameterValuePtr);
            break;
        case SQL_C_SLONG:
            *val = (int)(*(int *)param->ParameterValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetParam_INTEGER

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
                 )
{
    parambinding_t * param = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_UTINYINT:
            *val = (long)(*(unsigned char *)param->ParameterValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (long)(*(short *)param->ParameterValuePtr);
            break;
        case SQL_C_SLONG:
            *val = (long)(*(int *)param->ParameterValuePtr);
            break;
        case SQL_C_SBIGINT:
            *val = (long)(*(long *)param->ParameterValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetParam_BIGINT

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
                )
{
    parambinding_t * param = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_UTINYINT:
            *val = (float)(*(unsigned char *)param->ParameterValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (float)(*(short *)param->ParameterValuePtr);
            break;
        case SQL_C_SLONG:
            *val = (float)(*(int *)param->ParameterValuePtr);
            break;
        case SQL_C_SBIGINT:
            *val = (float)(*(long *)param->ParameterValuePtr);
            break;
        case SQL_C_FLOAT:
            *val = (float)(*(float *)param->ParameterValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetParam_FLOAT

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
                 )
{
    parambinding_t * param = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_UTINYINT:
            *val = (double)(*(unsigned char *)param->ParameterValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (double)(*(short *)param->ParameterValuePtr);
            break;
        case SQL_C_SLONG:
            *val = (double)(*(int *)param->ParameterValuePtr);
            break;
        case SQL_C_SBIGINT:
            *val = (double)(*(long *)param->ParameterValuePtr);
            break;
        case SQL_C_FLOAT:
            *val = (double)(*(float *)param->ParameterValuePtr);
            break;
        case SQL_C_DOUBLE:
            *val = (double)(*(double *)param->ParameterValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetParam_DOUBLE

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
               )
{
    parambinding_t * param = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_CHAR:
            *val = (char *)param->ParameterValuePtr;
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetParam_CHAR


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
                  )
{
    parambinding_t * param = NULL;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_UTINYINT:
            *((unsigned char *)param->ParameterValuePtr) = (unsigned char)val;
            break;
        case SQL_C_SSHORT:
            *((short *)param->ParameterValuePtr) = (short)val;
            break;
        case SQL_C_SLONG:
            *((int *)param->ParameterValuePtr) = (int)val;
            break;
        case SQL_C_SBIGINT:
            *((long *)param->ParameterValuePtr) = (long)val;
            break;
        case SQL_C_FLOAT:
            *((float *)param->ParameterValuePtr) = (float)val;
            break;
        case SQL_C_DOUBLE:
            *((double *)param->ParameterValuePtr) = (double)val;
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCSetParam_TINYINT

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
                  )
{
    parambinding_t * param = NULL;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_SSHORT:
            *((short *)param->ParameterValuePtr) = (short)val;
            break;
        case SQL_C_SLONG:
            *((int *)param->ParameterValuePtr) = (int)val;
            break;
        case SQL_C_SBIGINT:
            *((long *)param->ParameterValuePtr) = (long)val;
            break;
        case SQL_C_FLOAT:
            *((float *)param->ParameterValuePtr) = (float)val;
            break;
        case SQL_C_DOUBLE:
            *((double *)param->ParameterValuePtr) = (double)val;
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCSetParam_SMALLINT

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
                  )
{
    parambinding_t * param = NULL;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_SLONG:
            *((int *)param->ParameterValuePtr) = (int)val;
            break;
        case SQL_C_SBIGINT:
            *((long *)param->ParameterValuePtr) = (long)val;
            break;
        case SQL_C_FLOAT:
            *((float *)param->ParameterValuePtr) = (float)val;
            break;
        case SQL_C_DOUBLE:
            *((double *)param->ParameterValuePtr) = (double)val;
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCSetParam_INTEGER

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
                 )
{
    parambinding_t * param = NULL;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_SBIGINT:
            *((long *)param->ParameterValuePtr) = (long)val;
            break;
        case SQL_C_FLOAT:
            *((float *)param->ParameterValuePtr) = (float)val;
            break;
        case SQL_C_DOUBLE:
            *((double *)param->ParameterValuePtr) = (double)val;
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCSetParam_BIGINT

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
                )
{
    parambinding_t * param = NULL;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_FLOAT:
            *((float *)param->ParameterValuePtr) = (float)val;
            break;
        case SQL_C_DOUBLE:
            *((double *)param->ParameterValuePtr) = (double)val;
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCSetParam_FLOAT

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
                 )
{
    parambinding_t * param = NULL;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_DOUBLE:
            *((double *)param->ParameterValuePtr) = (double)val;
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCSetParam_DOUBLE

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
               )
{
    parambinding_t * param = NULL;
    int l;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    param = odbcGetParameter( stmt->params, paramno );
    if (  param == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  param->ValueType  )
    {
        case SQL_C_CHAR:
            l = strlen(val);
            if (  l >= param->BufferLength  )
                return ERR_PARAM_INTERNAL;
            strcpy( (char *)param->ParameterValuePtr, val );
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCSetParam_CHAR

/*
 * Bind an output column. Allocates the necessary value buffer.
 *
 * INPUTS:
 *     stmt          - a pointer to a statement structure
 *     colno         - the column number (> 0)
 *     clientType    - ODBC C data type for the parameter
 *     maxDataLength - the maximum size for the column value (excluding any
 *                     terminating null byte)
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
              )
{
    int ret = SUCCESS;
    SQLRETURN rc;
    colbinding_t * col = NULL;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    if (  odbcGetColumn( stmt->cols, colno ) != NULL  )
        return ERR_PARAM_INTERNAL;

    col = odbcAllocCol( &ret, stmt, colno, clientType, maxDataLength );
    if (  col == NULL  )
        return ret;

    if (  stmt->cols == NULL  )
    {
        stmt->cols = (collist_t *)calloc(1, sizeof(collist_t));
        if (  stmt->cols == NULL  )
            return ERR_NOMEM;
    }
    
    rc = SQLBindCol( stmt->hStmt,
                     col->colno,
                     col->TargetType,
                     col->TargetValuePtr,
                     col->BufferLength,
                     &(col->StrLen_or_Ind) );
    if (  rc == SQL_SUCCESS  )
    {
        LIST_LINK( stmt->cols, col );
        return SUCCESS;
    }

    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(stmt->errors),
                             SQL_HANDLE_STMT,
                             stmt->hStmt );

    return ret;
} // ODBCBindColumn

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
                  )
{
    colbinding_t * col = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    col = odbcGetColumn( stmt->cols, colno );
    if (  col == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  col->TargetType  )
    {
        case SQL_C_UTINYINT:
            *val = (unsigned char)(*(unsigned char *)col->TargetValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetCol_TINYINT

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
                  )
{
    colbinding_t * col = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    col = odbcGetColumn( stmt->cols, colno );
    if (  col == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  col->TargetType  )
    {
        case SQL_C_UTINYINT:
            *val = (short)(*(unsigned char *)col->TargetValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (short)(*(short *)col->TargetValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetCol_SMALLINT

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
                  )
{
    colbinding_t * col = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    col = odbcGetColumn( stmt->cols, colno );
    if (  col == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  col->TargetType  )
    {
        case SQL_C_UTINYINT:
            *val = (int)(*(unsigned char *)col->TargetValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (int)(*(short *)col->TargetValuePtr);
            break;
        case SQL_C_SLONG:
            *val = (int)(*(int *)col->TargetValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetCol_INTEGER

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
                 )
{
    colbinding_t * col = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    col = odbcGetColumn( stmt->cols, colno );
    if (  col == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  col->TargetType  )
    {
        case SQL_C_UTINYINT:
            *val = (long)(*(unsigned char *)col->TargetValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (long)(*(short *)col->TargetValuePtr);
            break;
        case SQL_C_SLONG:
            *val = (long)(*(int *)col->TargetValuePtr);
            break;
        case SQL_C_SBIGINT:
            *val = (long)(*(long *)col->TargetValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetCol_BIGINT

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
                )
{
    colbinding_t * col = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    col = odbcGetColumn( stmt->cols, colno );
    if (  col == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  col->TargetType  )
    {
        case SQL_C_UTINYINT:
            *val = (float)(*(unsigned char *)col->TargetValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (float)(*(short *)col->TargetValuePtr);
            break;
        case SQL_C_SLONG:
            *val = (float)(*(int *)col->TargetValuePtr);
            break;
        case SQL_C_SBIGINT:
            *val = (float)(*(long *)col->TargetValuePtr);
            break;
        case SQL_C_FLOAT:
            *val = (float)(*(float *)col->TargetValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetCol_FLOAT

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
                 )
{
    colbinding_t * col = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    col = odbcGetColumn( stmt->cols, colno );
    if (  col == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  col->TargetType  )
    {
        case SQL_C_UTINYINT:
            *val = (double)(*(unsigned char *)col->TargetValuePtr);
            break;
        case SQL_C_SSHORT:
            *val = (double)(*(short *)col->TargetValuePtr);
            break;
        case SQL_C_SLONG:
            *val = (double)(*(int *)col->TargetValuePtr);
            break;
        case SQL_C_SBIGINT:
            *val = (double)(*(long *)col->TargetValuePtr);
            break;
        case SQL_C_FLOAT:
            *val = (double)(*(float *)col->TargetValuePtr);
            break;
        case SQL_C_DOUBLE:
            *val = (double)(*(double *)col->TargetValuePtr);
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetCol_DOUBLE

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
               )
{
    colbinding_t * col = NULL;

    if (  (stmt == NULL) || (val == NULL)  )
        return ERR_PARAM_INTERNAL;
    col = odbcGetColumn( stmt->cols, colno );
    if (  col == NULL  )
        return ERR_PARAM_INTERNAL;

    switch (  col->TargetType  )
    {
        case SQL_C_CHAR:
            *val = (char *)col->TargetValuePtr;
            break;
        default:
            return ERR_TYPE_MISMATCH_INTERNAL;
            break;
    }

    return SUCCESS;
} // ODBCGetCol_CHAR

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
           )
{
    int ret = SUCCESS;
    SQLRETURN rc;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    if (  (stmt->errors == NULL) || (stmt->hStmt == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

    odbcFreeErrors( &(stmt->errors)  );

    rc = SQLExecute( stmt->hStmt );
    if (  rc == SQL_SUCCESS  )
        return SUCCESS;

    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(stmt->errors),
                             SQL_HANDLE_STMT,
                             stmt->hStmt );

    return ret;
} // ODBCExecute

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
         )
{
    int ret = SUCCESS;
    SQLRETURN rc;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    if (  (stmt->errors == NULL) || (stmt->hStmt == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

    odbcFreeErrors( &(stmt->errors)  );

    rc = SQLFetch( stmt->hStmt );
    if (  rc == SQL_SUCCESS  )
        return SUCCESS;

    if (  rc == SQL_NO_DATA  )
        ret = ERR_ODBC_NO_DATA;
    else
    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(stmt->errors),
                             SQL_HANDLE_STMT,
                             stmt->hStmt );

    return ret;
} // ODBCFetch

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
               )
{
    int ret = SUCCESS;
    SQLRETURN rc;

    if (  stmt == NULL  )
        return ERR_PARAM_INTERNAL;
    if (  (stmt->errors == NULL) || (stmt->hStmt == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

    odbcFreeErrors( &(stmt->errors)  );

#if ODBCVER >= 0x0350
    rc = SQLCloseCursor( stmt->hStmt );
#else
    rc = SQLFreeStmt( stmt->hStmt, SQL_CLOSE );
#endif
    if (  rc == SQL_SUCCESS  )
        return SUCCESS;

    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(stmt->errors),
                             SQL_HANDLE_STMT,
                             stmt->hStmt );

    return ret;
} // ODBCCloseCursor

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
            )
{
    int ret = SUCCESS;
    SQLRETURN rc;
    SQLLEN count = 0;

    if (  (stmt == NULL) || (rowcount == NULL  )  )
        return ERR_PARAM_INTERNAL;
    if (  (stmt->errors == NULL) || (stmt->hStmt == SQL_NULL_HANDLE)  )
        return ERR_STATE_INTERNAL;

    odbcFreeErrors( &(stmt->errors)  );

    rc = SQLRowCount( stmt->hStmt, &count );
    if (  rc == SQL_SUCCESS  )
    {
        *rowcount = (long)count;
        return SUCCESS;
    }

    *rowcount = -1;
    if (  rc == SQL_INVALID_HANDLE  )
        ret = ERR_ODBC_NOINFO;
    else
        ret = odbcGetErrors( &(stmt->errors),
                             SQL_HANDLE_STMT,
                             stmt->hStmt );

    return ret;
} // ODBCRowCount

