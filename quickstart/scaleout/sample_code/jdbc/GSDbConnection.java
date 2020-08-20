/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.util.*;
import java.sql.*;
import com.timesten.jdbc.*;

 /**
 * This is a wrapper for the TimesTenConnection class. It is
 * wrappped so that we can  associate it with our wrapper for
 * TimesTenPreparedStatement in order to ensure proper cleanup and 
 * state synchronization.
 *
 * The second reason for the wrapper is to differentiate exceptions
 * from some functions to allow for easier handling of grid retries
 * and client failovers in higher level code.
 *
 * Not all methods from the standard TimesTenConnection class
 * are implemented, only those needed by the application. Extra methods
 * can easily be added as required.
 */
public class GSDbConnection
{
    // Constants
    private static final String gridQuery = 
                                "call ttConfiguration('TTGridEnable')";
    private static final String elementIdQuery = 
                                "SELECT elementid# FROM DUAL";

    // Connection and database related variables
    private boolean direct = true;  // connection is direct mode
    private boolean grid = false;   // db is a grid database
    private String DSN = null;      // DSN for the connection
    private String user = null;     // username for the connection
    private String password = null; // password for the connection
    private String connName = null; // connection name
    private String URL = null;      // JDBC URL for the connection
    private int gridElementId = 0;  // element ID of connected grid element
    private TimesTenDataSource ttDS = null;   // TT datasource
    private TimesTenConnection ttConn = null; // TT connection
    private GSDbStatement eidStmt = null; // element id query statement

    // Error handling
    private String errMsg = null; // Last error for this connection

    // Map of GSDbStatement objects associated with this connection
    private HashMap<String, GSDbStatement> dbStmts = null;

    // common warning handling code for connection
    private void handleWarningsConn()
        throws SQLException
    {
        SQLWarning sqw = null;
        try { sqw = ttConn.getWarnings(); }
            catch ( SQLException sqe ) { ; }
        if (  sqw != null  )
            throw (SQLException)sqw;
    } // handleWarningsConn

    /**
     * Constructor
     */
    GSDbConnection( String pDSN,      // target DSN
                    boolean pCS,      // Client-server mode?
                    String pUser,     // DB user name
                    String pPassword, // DB password
                    String pCname     // Connection name
                  )
    {
        DSN = pDSN;
        direct = ! pCS;
        user = pUser;
        password = pPassword;
        connName = pCname;
        ttDS = new TimesTenDataSource();
        dbStmts = new HashMap<String,GSDbStatement>();
    } // Constructor

    /**
     * Assemble the JDBC connectionURL
     */
    private String createURL()
    {
        String url = null;

        if (  DSN != null  )
        {
            if (  direct  )
                url = GSConstants.ttURLPrefix +
                      GSConstants.ttDirect +
                      "DSN=" +
                      DSN;
            else
                url = GSConstants.ttURLPrefix +
                      GSConstants.ttClient +
                      "DSN=" +
                      DSN;
        }
        if (  connName != null  )
            url = url + ";" + GSConstants.ttConnName + "=" + connName;

        return url;
    } // createURL

    /**
     * Update grid info (currently only connected element id) for connection.
     * Throw SQLException as required.
     */
    private boolean updateGridInfoInt()
        throws SQLException
    {
        GSDbResultSet rs = null;
        boolean ret = false;

        errMsg = null;

        try {
            if (  ! grid  )
            {
                errMsg = GSErrors.M_NOGRID_ERR;
                return false;
            }
    
            if (  ! eidStmt.execute()  )
            {
                errMsg = eidStmt.getLastError();
            }
            else
            {
                rs = eidStmt.getResultSet();
                if (  rs == null  )
                {
                    errMsg = GSErrors.M_NO_RESULTS;
                }
                else
                {
                    rs.next();
                    gridElementId = rs.getInt(1);
                    ret = true;
                }
            }
        } finally {
            if (  rs != null  )
            {
                rs.close();
                rs = null;
            }
        }

        return ret;
    } // updateGridInfoInt


    /**
     * Update grid info (currently only connected element id) for connection
     */
    public boolean updateGridInfo()
    {
        try {
            return updateGridInfoInt();
        }
        catch ( SQLException sqe ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
        }
        
        return false;
    } // updateGridInfo

    /**
     * Update grid info (currently only connected element id) for connection.
     * Throws retry or failover exceptions if required.
     */
    public boolean updateGridInfoEx()
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        try {
            return updateGridInfoInt();
        }
        catch ( SQLException sqe ) {
            if (  GSGridRetryException.isGridRetryable( sqe )  )
            {
                rollback();
                throw new GSGridRetryException( sqe );
            }
            else
            if (  GSConnectionFailoverException.isFailover( sqe )  )
            {
                rollback();
                throw new GSConnectionFailoverException( sqe );
            }
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
        }

        return false;
    } // updateGridInfoEx

    /**
     * Getter for the last error on the connection
     */
    public String getLastError()
    {
        return errMsg;
    } // getLastError

    /**
     * Is this a connection to a TimesTen Scaleout database?
     */
    public boolean isGrid()
    {
        errMsg = null;
        return grid;
    } // isGrid

    /**
     * Returns the connected grid element ID (or 0 if not a grid database)
     */
    public int getGridElementID()
    {
        errMsg = null;
        if (  grid  )
            return gridElementId;
        else
            return 0;
    }

    /**
     * Returns the underlying connection - INTERNAL USE ONLY
     */
    public TimesTenConnection getConnection()
    {
        errMsg = null;
        return ttConn;
    } // getConnection

    /**
     * Connect to the database.
     *
     * NOTE: Does NOT handle grid retries or connection failovers. if
     *       one of those occurs then this method will return false.
     */
    public boolean connect()
    {
        TimesTenStatement ttstmt = null;

        errMsg = null;
        // Check if the connection is already open
        if (  ttConn != null  )
        {
            errMsg = GSErrors.M_ALREADY_OPEN;
            return false;
        }

        // Build URL, if required
        if (  URL == null  )
            URL = createURL();
        if (  URL == null  )
        {
            errMsg = GSErrors.M_PARAM_ERR;
            return false;
        }

        // Set the URL on the DataSource
        ttDS.setUrl( URL );

        // Check and set the username and password based on the
        // connection mode
        if (  direct  )
        {
            // set user on DataSource, if required
            if (  user != null  )
                ttDS.setUser( user );
            // user without password is okay but not vice versa
            if (  password != null  )
            {
                if (  user != null  )
                    ttDS.setPassword( password );
                else
                {
                    errMsg = GSErrors.M_PARAM_ERR;
                    return false;
                }
            }
        }
        else // Client-server
        {
            // C/S needs both username and password
            if (  ( user == null ) || ( password == null )  )
            {
                errMsg = GSErrors.M_PARAM_ERR;
                return false;
            }
            ttDS.setUser( user );
            ttDS.setPassword( password );
        }

        // Open the connection and disable autocommit
        try {
            ttConn = (TimesTenConnection)ttDS.getConnection();
            handleWarningsConn();
            ttConn.setAutoCommit(false);
            handleWarningsConn();
        } catch ( SQLException sqe ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            ttConn = null;
            return false;
        }

        // now check if we are connected to a grid database
        ResultSet rs = null;
        boolean result = false;
        SQLWarning sqw = null;
        try {
            ttstmt = (TimesTenStatement)ttConn.createStatement();
            handleWarningsConn();
            rs = ttstmt.executeQuery(gridQuery);
            try { sqw = ttstmt.getWarnings(); }
                catch ( SQLException sqe ) { ; }
            if (  sqw != null  )
                throw (SQLException)sqw;
            result = rs.next();
            try { sqw = rs.getWarnings(); }
                catch ( SQLException sqe ) { ; }
            if (  sqw != null  )
                throw (SQLException)sqw;
            if (  result  )
            {
                String s = rs.getString(2);
                try { sqw = rs.getWarnings(); }
                    catch ( SQLException sqe ) { ; }
                if (  sqw != null  )
                    throw (SQLException)sqw;
                if (  (s != null) && s.equals("1")  )
                    grid = true;
            }
            rs.close();
            try { sqw = rs.getWarnings(); }
                catch ( SQLException sqe ) { ; }
            if (  sqw != null  )
                throw (SQLException)sqw;
            rs = null;
            ttstmt.close();
            try { sqw = ttstmt.getWarnings(); }
                catch ( SQLException sqe ) { ; }
            if (  sqw != null  )
                throw (SQLException)sqw;
            ttstmt = null;
        } catch ( SQLException sqe ) {
            // don't try to handle any errors during connection open
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            if (  rs != null  )
                try { rs.close(); } catch ( SQLException sqe1 ) { ; }
            rs = null;
            if (  ttstmt != null  )
                try { ttstmt.close(); } catch ( SQLException sqe1 ) { ; }
            ttstmt = null;
            try { ttConn.close(); } catch ( SQLException sqe1 ) { ; }
            ttConn = null;
            return false;
        }

        // if grid, prepare element id query
        if (  grid  )
        {
            try {
                eidStmt = prepare( "eidStmt", elementIdQuery );
            } catch ( SQLException sqe ) {
                // don't try to handle grid retries or failovers during 
                // connection open
                errMsg = sqe.getSQLState() + ":"  +
                         sqe.getErrorCode() + ":" +
                         sqe.getMessage();
                eidStmt = null;
                try {
                    ttConn.close();
                } catch ( SQLException sqe1 ) { ; }
                ttConn = null;
                return false;
            }
    
            // and update grid info
            if (  ! updateGridInfo()  )
            {
                if (  eidStmt != null  )
                    eidStmt.close();
                eidStmt = null;
                try {
                    ttConn.close();
                } catch ( SQLException sqe1 ) { ; }
                ttConn = null;
                return false;
            }
        }

        return true;
    } // connect

    /**
     * Commit; throw an exception on non fatal (to the database) errors
     */
    public boolean commitEx()
        throws GSConnectionFailoverException, 
               GSGridRetryException
    {
        boolean ret = false;

        errMsg = null;
        if (  ttConn == null  )
        {
            errMsg = GSErrors.M_CONN_NOT_OPEN;
            return ret;
        }

        // Close all result sets on this connection
        if (  dbStmts != null  )
        {
            if (  ! dbStmts.isEmpty()  )
            {
                Object keys[] = dbStmts.keySet().toArray();
                for (int i = 0; i < keys.length; i++)
                    dbStmts.get((String)keys[i]).closeResult();
            }
        }

        // now commit
        try {
            ttConn.commit();
            handleWarningsConn();
            ret = true;
        } catch (SQLException sqe) {
            if (  GSConnectionFailoverException.isFailover( sqe )  )
            {
                rollback();
                throw new GSConnectionFailoverException( sqe );
            }
            else
            if (  GSGridRetryException.isGridRetryable( sqe )  )
            {
                rollback();
                throw new GSGridRetryException( sqe );
            }
            else
            if (  GSErrors.isDbFatalError( sqe )  )
            {
                rollback();
                close(true);
                errMsg = GSErrors.M_CONN_LOST;
            }
            else
                errMsg = sqe.getSQLState() + ":"  +
                         sqe.getErrorCode() + ":" +
                         sqe.getMessage();
        }

        return ret;
    } // commitEx

    /**
     * Commit; don't throw any exceptions.
     */
    public boolean commit()
    {
        boolean ret = false;

        errMsg = null;
        try {
            ret = commitEx();
        } catch (SQLException sqe) { ; }

        return ret;
    } // commit

    /**
     * Rollback; don't throw any exceptions.
     */
    public boolean rollback()
    {
        boolean ret = false;

        errMsg = null;
        if (  ttConn == null  )
        {
            errMsg = GSErrors.M_CONN_NOT_OPEN;
            return ret;
        }

        // Close all result sets on this connection
        if (  dbStmts != null  )
        {
            if (  ! dbStmts.isEmpty()  )
            {
                Object keys[] = dbStmts.keySet().toArray();
                for (int i = 0; i < keys.length; i++)
                    dbStmts.get((String)keys[i]).closeResult();
            }
        }

        try {
            ttConn.rollback();
            handleWarningsConn();
            ret = true;
        } catch (SQLException sqe) {
            if (  GSErrors.isDbFatalError( sqe )  )
            {
                errMsg = GSErrors.M_CONN_LOST;
                close(true);
            }
            else
                errMsg = sqe.getSQLState() + ":"  +
                         sqe.getErrorCode() + ":" +
                         sqe.getMessage();
        }

        return ret;
    } // rollback

    /**
     * Close the connection, with optional rollback
     */
    public boolean close(
                         boolean rollback
                        )
    {
        boolean ret = false;

        errMsg = null;
        if (  ttConn == null  )
            return true; // okay to close an already closed connection

        if (  rollback  )
            rollback();

        // Close and remove all statements (and result sets) associated 
        // with the connection
        if (  dbStmts != null  )
        {
            Object keys[] = dbStmts.keySet().toArray();
            for (int i = 0; i < keys.length; i++)
                dbStmts.get((String)keys[i]).close();
            dbStmts.clear();
        }

        // if grid, close the element ID query
        if (  eidStmt != null  )
        {
            eidStmt.close();
            eidStmt = null;
        }

        // close the connection
        try {
            ttConn.close();
            handleWarningsConn();
            ttConn = null;
            ret = true;
        } catch (SQLException sqe) {
            if (  GSConnectionFailoverException.isFailover( sqe )  )
            {
                // connection invalid due to failover
                rollback();
                ttConn = null;
                ret = true;
            }
            else
                // some other error
                ttConn = null;
                errMsg = sqe.getSQLState() + ":"  +
                         sqe.getErrorCode() + ":" +
                         sqe.getMessage();
        }

        return ret;
    } // close

    /**
     * Prepare a statement and assign it a label for fast retrieval
     */
    public GSDbStatement prepare(
                                 String lbl,
                                 String sql
                                )
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        GSDbStatement tmp = null;
        GSDbStatement res = null;

        errMsg = null;
        if (  (lbl == null) || (sql == null)  )
        {
            errMsg = GSErrors.M_PARAM_ERR;
            return res; // error!
        }

        if (  ttConn == null  )
        {
            errMsg = GSErrors.M_CONN_NOT_OPEN;
            return res; // error!
        }

        // check for duplicate label
        tmp = dbStmts.get( lbl );
        if (  tmp != null  )
            errMsg = GSErrors.M_DUPL_LBL;
        else
        {
            tmp = new GSDbStatement( lbl, this, sql );
            try {
                if (  ! tmp.prepare()  )
                    errMsg = tmp.getLastError();
                else
                {
                    dbStmts.put( lbl, tmp );
                    res = tmp;
                }
            } catch (  GSGridRetryException gre  ) {
                tmp.realclose();
                throw gre;
            } catch (  GSConnectionFailoverException cfe  ) {
                tmp.realclose();
                throw cfe;
            }
        }
                               
        return res;
    } // prepare

    /**
     * Close an existing prepared statement and remove it from the list for 
     * this connection - INTERNAL USE ONLY
     */
    public boolean closeStmt(
                             GSDbStatement stmt
                            )
    {
        GSDbStatement istmt = null;

        errMsg = null;
        if (  stmt == null  )
        {
            errMsg = GSErrors.M_PARAM_ERR;
            return false;
        }
        istmt = dbStmts.get( stmt.getLabel() );
        stmt.realclose();
        if (  istmt == stmt  )
            dbStmts.remove( stmt.getLabel() );

        return true;
    } // closeStmt

    /**
     * Reprepare all statements on this connection (used after failover)
     */
    public boolean reprepareAll()
    {
        boolean ret = true;

        errMsg = null;
        if (  dbStmts != null  )
        {
            if (  ! dbStmts.isEmpty()  )
            {
                Object keys[] = dbStmts.keySet().toArray();
                int i = 0;
                while (  ret && ( i < keys.length )  )
                {
                    GSDbStatement stmt = dbStmts.get((String)keys[i]);
                    stmt.realclose();
                    try {
                        if (  ! stmt.prepare()  )
                        {
                            errMsg = stmt.getLastError();
                            ret = false;
                        }
                    } catch (  SQLException sqe  ) {
                        errMsg = sqe.getSQLState() + ":"  +
                                 sqe.getErrorCode() + ":" +
                                 sqe.getMessage();
                        ret = false;
                    }
                    i++;
                }
            }
        }

        return ret;
    } // reprepareAll

} //  GSDbConnection
