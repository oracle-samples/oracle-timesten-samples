/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.sql.*;

/**
 *  This is an exception derived from SQLException. It is thrown by
 *  various components when a connection failover occurs. It also
 *  defines some additional methods.
 */

public class GSConnectionFailoverException
    extends SQLException
{
    // A SQLSTATE and native error combination that represents a
    // connection failover
    private static class FailoverState
    {
        String  sqlState = null;
        Integer nativeError = null;

        FailoverState(
                      String state,
                      int    err
                     )
        {
            sqlState = state;
            nativeError = new Integer(err);
        }
    }

    // List of failover states that indicate a failover
    private static final FailoverState csFailoverStates[] = 
        initFailoverStates();

    // The underlying SQLException
    private SQLException sqe = null;

    // The failover delay value (ms)
    private int failoverDelayVal = GSConstants.DFLT_FAILOVER_DELAY;

   /**
    * Static initializer function to set up the array of failover states
    */
    private static FailoverState[] initFailoverStates()
    {
        FailoverState fos[] = new FailoverState[1];
        fos[0] = new FailoverState(null,47137);
        return fos;
    } // initFailoverStates

    /**
     * Public static method to determine if a SQLException is a 
     * client failover exception
     */
    public static boolean isFailover(
                                     SQLException ex
                                    )
    {
        boolean ret = false;

        if (  ex != null  )
        {
            String sqlState = ex.getSQLState();
            int ec = ex.getErrorCode();
            for (int i = 0; i < csFailoverStates.length; i++)
                if (  
                      ( (csFailoverStates[i].sqlState == null) ||
                        (sqlState.equals(csFailoverStates[i].sqlState)) ) &&
                      ( (csFailoverStates[i].nativeError == null) ||
                        (csFailoverStates[i].nativeError.intValue() == ec) )
                   )
                {
                    ret = true;
                    break;
                }
        }

        return ret;
    } // isFailover

    /**
     * Constructor
     */
    GSConnectionFailoverException(
                                  SQLException s // original exception
                                 )
    {
        sqe = s;
    } // Constructor

    /**
     * Return the SQLSTATE
     */
    public String getSQLState()
    {
        String state = null;

        if (  sqe != null  )
            state = sqe.getSQLState();

        return state;
    } // getSQLState

    /**
     * Return the native error code
     */
    public int getErrorCode()
    {
        int ec = 0;

        if (  sqe != null  )
            ec = sqe.getErrorCode();

        return ec;
    } // getErrorCode

    /**
     * Getter the failover delay
     */
    public int failoverDelay()
    {
        return failoverDelayVal;
    } // failoverDelay

    /**
     * Getter for the underlying SQLException
     */
    public SQLException getSQLException()
    {
        return sqe;
    } // getSQLException

} // GSConnectionFailoverException
