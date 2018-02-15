/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.math.BigDecimal;
import java.sql.*;
import com.timesten.jdbc.*;

/**
 * This is a wrapper for the TimesTenPreparedStatement class. It is
 * wrappped so that we can associate it with our wrapper for
 * TimesTenConnection in order to ensure proper cleanup and state
 * synchronization.
 *
 * The second reason for the wrapper is to differentiate exceptions
 * from some functions to allow for easier handling of grid retries
 * and client failovers in higher level code.
 *
 * Not all methods from the standard TimesTenPreparedStatement class 
 * are implemented, only those needed by the application. Extra methods
 * can easily be added as required.
 */
public class GSDbStatement
{
    // Statement label
    private String label = null; // Label for this statement. Must be unique
                                 // within a connection.

    // Connection and database related variables
    private GSDbConnection dbConn = null; // parent connection
    private TimesTenPreparedStatement ttStmt = null; // real prepared statement
    private GSDbResultSet resultSet = null; // result set
    private GSDbResultSet pResultSet = null; // copy of result set
    private String sqlText = null; // SQL text of statement
    private int updateCount = -1; // update count

    // Other variables
    private String errMsg = null; // last error on this statement

    // Common warning handling code - converts warnings to errors
    //
    // Handling warnings is always a tricky area; do you ignore them?
    // Or report them but otherwise not handle them? Ultimately that is
    // a question for the developer. The approach taken here is to treat
    // warnings as errors.
    private void handleWarnings()
        throws SQLException
    {
        SQLWarning sqw = null;
        try { sqw = ttStmt.getWarnings(); }
            catch ( SQLException sqe ) { ; }
        if (  sqw != null  )
            throw (SQLException)sqw;
    } // handleWarnings

    // common exception handling code
    private void handleException(
                                 SQLException sqe
                                )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        if (  GSErrors.isDbFatalError( sqe )  )
        {
            dbConn.close(true);
            errMsg = GSErrors.M_CONN_LOST;
        }
        else
        if (  GSConnectionFailoverException.isFailover( sqe )  )
        {
            dbConn.rollback();
            throw new GSConnectionFailoverException( sqe );
        }
        else
        if (  GSGridRetryException.isGridRetryable( sqe )  )
        {
            dbConn.rollback();
            throw new GSGridRetryException( sqe );
        }
        else
        {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
        }
    } // handleException

    /**
     * Constructor
     */
    GSDbStatement( String lbl,                    // statement label
                   GSDbConnection conn,           // parent connection
                   String sql                     // SQL statement
                 )
    {
        label = lbl;
        dbConn = conn;
        sqlText = sql;
    } // Constructor

    /**
     * Getter for last error
     */
    public String getLastError()
    {
        return errMsg;
    } // getLastError

    /**
     * Getter for statement's label
     */
    public String getLabel()
    {
        return label;
    } // getLabel

    /**
     * Getter for the SQL statement
     */
    public String getSQL()
    {
        return sqlText;
    } // getSQL

    /**
     * Getter for associated Connection
     */
    public GSDbConnection getConnection()
    {
        return dbConn;
    } // getConnection

    /**
     * Does the statement currently have a ResultSet associated with it
     */
    public boolean hasResultSet()
    {
        return ( pResultSet != null );
    }

    /**
     * Equivalent to TimesTenPreparedStatement.setInt()
     */
    public boolean setInt(
                          int index,
                          int param
                         )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  ttStmt != null  )
            try {
                ttStmt.setInt( index, param );
                handleWarnings();
                result = true;
            } catch ( SQLException sqe ) {
                handleException( sqe );
            }
        else
            errMsg = GSErrors.M_STATE_ERR;

        return result;
    } // setInt

    /**
     * Equivalent to TimesTenPreparedStatement.setLong()
     */
    public boolean setLong(
                           int  index,
                           long param
                          )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  ttStmt != null  )
            try {
                ttStmt.setLong( index, param );
                handleWarnings();
                result = true;
            } catch ( SQLException sqe ) {
                handleException( sqe );
            }
        else
            errMsg = GSErrors.M_STATE_ERR;

        return result;
    } // setLong

    /**
     * Equivalent to TimesTenPreparedStatement.setString()
     */
    public boolean setString(
                             int    index,
                             String param
                            )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  ttStmt != null  )
            try {
                ttStmt.setString( index, param );
                handleWarnings();
                result = true;
            } catch ( SQLException sqe ) {
                handleException( sqe );
            }
        else
            errMsg = GSErrors.M_STATE_ERR;

        return result;
    } // setString

    /**
     * Equivalent to TimesTenPreparedStatement.setDouble()
     */
    public boolean setDouble(
                             int    index,
                             double param
                            )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  ttStmt != null  )
            try {
                ttStmt.setDouble( index, param );
                handleWarnings();
                result = true;
            } catch ( SQLException sqe ) {
                handleException( sqe );
            }
        else
            errMsg = GSErrors.M_STATE_ERR;

        return result;
    } // setDouble

    /**
     * Equivalent to TimesTenPreparedStatement.setBigDecimal()
     */
    public boolean setBigDecimal(
                                 int    index,
                                 BigDecimal param
                                )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  ttStmt != null  )
            try {
                ttStmt.setBigDecimal( index, param );
                handleWarnings();
                result = true;
            } catch ( SQLException sqe ) {
                handleException( sqe );
            }
        else
            errMsg = GSErrors.M_STATE_ERR;

        return result;
    } // setBigDecimal

    /**
     * Equivalent to TimesTenPreparedStatement.getUpdateCount()
     */
    public int getUpdateCount()
    {
        int ret = updateCount;

        updateCount = -1;
        pResultSet = null;

        return ret;
    } // getUpdateCount

    /**
     * Equivalent to TimesTenPreparedStatement.getResultSet()
     */
    public GSDbResultSet getResultSet()
    {
        GSDbResultSet ret = pResultSet;

        updateCount = -1;
        pResultSet = null;

        return ret;
    } // getResultSet

    /**
     * Equivalent to TimesTenPreparedStatement.execute()
     */
    public boolean execute()
        throws GSGridRetryException, GSConnectionFailoverException
    {
        boolean ret = false;

        // Are we in a state where we can 'execute'?
        if (  ttStmt == null  )
        {
            // No, we are not
            errMsg = GSErrors.M_STATE_ERR;
            return false;
        }

        pResultSet = null;
        updateCount = -1;
        // Try and execute
        try {
            ret = ttStmt.execute();
            handleWarnings();
            if (  ret  )
            { // we have a result set
                ResultSet rs = ttStmt.getResultSet();
                handleWarnings();
                if (  rs != null  )
                {
                    resultSet = new GSDbResultSet( this, rs );
                    pResultSet = resultSet;
                }
            }
            else // it's an update count
                updateCount = ttStmt.getUpdateCount();
            ret = true;
        } catch (  SQLException sqe  ) {
            ret = false;
            handleException( sqe );
        }

        return ret;
    } // execute

    /**
     * Equivalent to TimesTenPreparedStatement.close()
     */
    public void close()
    {
        if (  dbConn != null  )
            dbConn.closeStmt( this );
    } // close

    /**
     * Close statement's result set (if any) - INTERNAL USE ONLY
     */
    public void closeResult()
    {
        if (  resultSet != null  )
        {
            resultSet.close();
            resultSet = null;
            pResultSet = null;
            updateCount = -1;
        }
    } // closeResult

    /**
     * Close this statement - INTERNAL USE ONLY
     */
    public void realclose()
    {
        errMsg = null;
        closeResult();
        if (  ttStmt != null  )
        {
            // Close the statement ignoring any errors since there is nothing
            // that can be done practically to handle them other than to log
            // them somewhere. We do set the errMsg field so if a caller
            // is really interested they can determine if there was an error
            // by calling getLastError on this object
            try {
                ttStmt.close();
                handleWarnings();
            } catch ( SQLException sqe ) {
                errMsg = sqe.getSQLState() + ":" +
                         sqe.getErrorCode() + ":" +
                         sqe.getMessage();
            }
            ttStmt = null;
        }
    } // realclose

    /**
     * Prepare, or re-prepare, this statement -- INTERNAL USE ONLY
     */
    public boolean prepare()
        throws GSGridRetryException, GSConnectionFailoverException
    {
        boolean ret = false;
        TimesTenConnection conn = null;

        errMsg = null;
        if (  dbConn == null  )
        {
            errMsg = GSErrors.M_CONN_NOT_OPEN;
            return ret;
        }
        conn = dbConn.getConnection();
        if (  conn == null  )
        {
            errMsg = GSErrors.M_CONN_NOT_OPEN;
            return ret;
        }

        if (  ttStmt != null  )
            close();

        try {
            ttStmt =
                (TimesTenPreparedStatement)conn.prepareStatement( sqlText );
            handleWarnings();
            ret = true;
        } catch ( SQLException sqe ) {
            handleException( sqe );
        }

        return ret;
    } // prepare

} //  GSDbStatement
