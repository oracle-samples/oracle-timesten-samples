/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.sql.*;
import com.timesten.jdbc.*;

/**
 * This is a wrapper for the regular JDBC ResultSet class. It is
 * wrappped so that we can  associate it with our wrapper for 
 * TimesTenPreparedStatement in order to ensure proper cleanup and 
 * state synchronization.
 *
 * The second reason for the wrapper is to differentiate exceptions
 * from some functions to allow for easier handling of grid retries
 * and client failovers in higher level code.
 *
 * Not all methods from the standard ResultSet class are implemented,
 * only those needed by the application. Extra methods can easily be
 * added as required.
 */
public class GSDbResultSet
{
    // Connection and database related variables
    private GSDbConnection dbConn = null; // parent connection
    private GSDbStatement dbStmt = null;  // parent statement
    private ResultSet dbResultSet = null;  // the real result set

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
        try { sqw = dbResultSet.getWarnings(); }
            catch ( SQLException sqe ) { ; }
        if (  sqw != null  )
            throw (SQLException)sqw;
    } // handleWarnings

    // common exception handling code
    private void handleException(
                                 SQLException sqe
                                )
        throws GSGridRetryException,
               GSConnectionFailoverException,
               SQLException
    {
        if (  GSErrors.isDbFatalError( sqe )  )
        {
            dbConn.close(true);
            errMsg = GSErrors.M_CONN_LOST;
            throw sqe;
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
            throw sqe;
        }
    } // handleException

    /**
     * Constructor - INTERNAL USE ONLY
     */
    GSDbResultSet( 
                   GSDbStatement stmt,   // parent statement
                   ResultSet rs          // real result set
                 )
    {
        dbStmt = stmt;
        dbConn = stmt.getConnection();
        dbResultSet = rs;
    } // Constructor

    /**
     * Getter for last error
     */
    public String getLastError()
    {
        return errMsg;
    } // getLastError

    /**
     * Equivalent to ResultSet.next()
     */
    public boolean next()
        throws GSGridRetryException,
               GSConnectionFailoverException,
               SQLException
    {
        boolean ret = false;

        try {
            ret = dbResultSet.next();
            handleWarnings();
        } catch (  SQLException sqe  ) {
            handleException( sqe );
        }

        return ret;
    } // next

    /**
     * Equivalent to ResultSet.getString()
     */
    public String getString(
                            int columnIndex
                           )
        throws GSGridRetryException,
               GSConnectionFailoverException,
               SQLException
    {
        String ret = null;

        try {
            ret = dbResultSet.getString(columnIndex);
            handleWarnings();
        } catch ( SQLException sqe ) {
            handleException( sqe );
        }

        return ret;
    } // geString

    /**
     * Equivalent to ResultSet.getInt()
     */
    public int getInt(
                       int columnIndex
                      )
        throws GSGridRetryException,
               GSConnectionFailoverException,
               SQLException
    {
        int ret = 0;

        try {
            ret = dbResultSet.getInt(columnIndex);
            handleWarnings();
        } catch ( SQLException sqe ) {
            handleException( sqe );
        }

        return ret;
    } // getInt

    /**
     * Equivalent to ResultSet.getLong()
     */
    public long getLong(
                        int columnIndex
                       )
        throws GSGridRetryException,
               GSConnectionFailoverException,
               SQLException
    {
        long ret = 0;

        try {
            ret = dbResultSet.getLong(columnIndex);
            handleWarnings();
        } catch ( SQLException sqe ) {
            handleException( sqe );
        }

        return ret;
    } // getLong

    /**
     * Equivalent to ResultSet.getDouble()
     */
    public double getDouble(
                            int columnIndex
                           )
        throws GSGridRetryException,
               GSConnectionFailoverException,
               SQLException
    {
        double ret = 0.0;

        try {
            ret = dbResultSet.getDouble(columnIndex);
            handleWarnings();
        } catch ( SQLException sqe ) {
            handleException( sqe );
        }

        return ret;
    } // getDouble

    /**
     * Equivalent to ResultSet.close()
     */
    public void close()
    {
        if (  dbResultSet != null  )
        {
            try {
                dbResultSet.close();
            } catch ( SQLException sqe ) { ; }
            dbResultSet = null;
        }
    } // close

} //  GSDbResultSet
