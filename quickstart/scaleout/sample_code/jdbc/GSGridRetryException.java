/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.sql.*;

/**
 *  This is an exception derived from SQLException. It is thrown by
 *  various components when an exception is caught that represents an
 *  error that should be retried.
 */

public class GSGridRetryException
    extends SQLException
{
    // Class that represents a retryable error and associated info
    private static class RetryState
    {
        String  sqlState = null;
        int     errorCode = 0;
        int     retryDelay = 0;

        RetryState(
                   String  state,
                   int     nativeerr,
                   int     delay
                  )
        {
            sqlState = state;
            errorCode = nativeerr;
            retryDelay = delay;
        }
    }

    // List of retryable SQLSTATEs
    private static RetryState gridRetryStates[] =
        initRetryStates();

    // The underlying SQLException
    protected SQLException sqe = null;

    // What delay is needed before retrying (ms)?
    protected int retryDelayVal = 0;

    // initialises retry state array
    private static RetryState[] initRetryStates()
    {
        RetryState rts[] = new RetryState[3];

        // transient grid error
        rts[0] = new RetryState("TT005",0,GSConstants.DFLT_RETRY_DELAY);
        // deadlock error
        rts[1] = new RetryState(null,6002,GSConstants.DFLT_RETRY_DELAY);
        // lock timeout error
        rts[2] = new RetryState(null,6003,GSConstants.DFLT_RETRY_DELAY);

        return rts;
    } // initRetryStates

    // Checks if a combination of SQLSTATE and native error matches
    // an entry in the retryable list and if so returns its index
    // otherwise returns -1
    private static int retryIndex(
                                  String state,
                                  int    errcode
                                 )
    {
        for (int i = 0; i < gridRetryStates.length; i++)
            if (  ( (gridRetryStates[i].sqlState == null) ||  
                    gridRetryStates[i].sqlState.equals(state) ) &&
                  ( (gridRetryStates[i].errorCode == 0) ||
                    (errcode == gridRetryStates[i].errorCode) )  )
                return i;
        return -1;
    } // retryIndex

    /**
     * Public static method to determine if an exception represents
     * a grid retryable error
     */
    public static boolean isGridRetryable(
                                          SQLException ex
                                         )
    {
        boolean ret = false;

        if (  (ex != null) &&
              (retryIndex( ex.getSQLState(), ex.getErrorCode() ) >= 0)  )
            ret = true;

        return ret;
    } // isGridRetryable

    /**
     * Constructor
     */
    GSGridRetryException( SQLException s )
    {
        int ri;

        sqe = s;
        if (  sqe != null  )
        {
            ri = retryIndex( sqe.getSQLState(), sqe.getErrorCode() );
            if (  ri >= 0  )
                retryDelayVal = gridRetryStates[ri].retryDelay;
        }
    } // Constructor

    /**
     * Return the SQLSTATE
     */
    public String getSQLState()
    {
        if (  sqe != null  )
            return sqe.getSQLState();
        else
            return null;
    } // getSQLState

    /**
     * Return the native error
     */
    public int getErrorCode()
    {
        if (  sqe != null  )
            return sqe.getErrorCode();
        else
            return 0;
    } // getErrorCode

    /**
     * Return the underlying SQLException
     */
    public SQLException getSQLException()
    {
        return sqe;
    } // getSQLException

    /**
     * Returns the required retry delay
     */
    public int retryDelay()
    {
        return retryDelayVal;
    } // retryDelay

} //  GSGridRetryException
