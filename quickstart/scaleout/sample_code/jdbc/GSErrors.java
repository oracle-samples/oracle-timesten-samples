/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.sql.*;

/**
 * All error messages produced by the GridSample application plus
 * a method to determine if an exception represents a database
 * fatal error.
 */
public class GSErrors
{
    // Error messages
    public static final String M_CONN_NOT_OPEN = "database connection not open";
    public static final String M_ALREADY_OPEN = "database connection already open";
    public static final String M_CONN_LOST = "database connection lost";
    public static final String M_PARAM_ERR = "parameter error";
    public static final String M_DUPL_LBL = "duplicate statement label";
    public static final String M_STATE_ERR = "state error";
    public static final String M_NOGRID_ERR = "not a grid database";
    public static final String M_DATA_ERR = "data error";
    public static final String M_NO_INPUT = "no input value";
    public static final String M_NO_RESULTS = "no rows found";
    public static final String M_ROWCOUNT_ERR = "too few/many rows modified";

    // TimesTen native errors indicating element invalidation
    private static final int dbFatalErrors[] = { 846, 994 };

    /**
     * Indicates if a SQLException represents an error that is fatal to
     * the connected database (element invalidation has occurred)
     */
    public static boolean isDbFatalError(
                                         SQLException ex
                                        )
    {
        boolean ret = false;

        if (  ex != null  )
        {
            int ec = ex.getErrorCode();
            for (int i = 0; i < dbFatalErrors.length; i++)
                if (  ec == dbFatalErrors[i]  )
                {
                    ret = true;
                    break;
                }
        }

        return ret;
    } // isDbFatalError

} // GSErrors
