/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.util.*;
import java.math.BigDecimal;
import java.sql.*;
import com.timesten.jdbc.*;

/**
 * A class that represents a generic application transaction. This class is
 * never instantiated directly but is a template which is used to derive
 * actual transaction classes.
 */
public class GSAppTransaction
{
    // Connection and database related variables
    protected GSDbConnection dbConn = null; // database connection
    protected int rowCount = 0; // DML row count

    // Error handling
    protected String errMsg = null; // Last error for this transaction

    // Other things
    protected String myName = null; // transaction name
    protected GSGlobal ctxt = null; // associated context
    protected boolean returnsResultSet = false; // does not return a result set

    /**
     * Constructor
     */
    GSAppTransaction(
                     GSDbConnection dbcon,
                     GSGlobal cx
                    )
    {
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx,
                             "DEBUG: ENTER: GSAppTransaction: Constructor" );
        ctxt = cx;
        dbConn = dbcon;
        if (  ctxt != null  )
            ctxt.addTxn( this );
        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( cx,
                             "DEBUG: EXIT: GSAppTransaction: Constructor" );
    } // Constructor

    /**
     * Return the transaction's name
     */
    public String getTxnName()
    {
        return myName;
    } // getTxnName

    /**
     * Return the transaction's row count
     */
    public int getRowCount()
    {
        return rowCount;
    } // getRowCount

    /**
     * Return the last error for this transaction
     */
    public String getLastError()
    {
        return errMsg;
    } // getLastError

    /**
     * Is the result for this transaction a GSDbResultSet?
     */
    public boolean resultIsResultSet()
    {
        return returnsResultSet;
    } // resultIsResultSet

    /**
     * Do a 'next()' on a result set.
     */
    protected boolean next(
                           GSDbResultSet rs
                          )
        throws SQLException,
               GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: next" );
        errMsg = null;
        if ( rs == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                                 "DEBUG: EXITERR: GSAppTransaction: next" );
            return result;
        }

        try {
            result =  rs.next();
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSAppTransaction: next: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSAppTransaction: next: Failover" );
            throw cfe;
        }  catch (  SQLException sqe  ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: GSAppTransaction: next: Exception" );
            throw sqe;
        }

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                         "DEBUG: EXIT: GSAppTransaction: next: " + result );
        return result;
    } // next

    /**
     * Get an 'int' column value for a result set.
     */
    protected int getInt(
                         GSDbResultSet rs,
                         int           index
                        )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        int result = 0;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: getInt" );
        errMsg = null;
        if ( rs == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: EXITERR: GSAppTransaction: getInt" );
            return result;
        }

        try {
            result =  rs.getInt( index  );
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: getInt: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: getInt: Failover" );
            throw cfe;
        }  catch (  SQLException sqe  ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: getInt: Exception" );
        }

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                         "DEBUG: EXIT: GSAppTransaction: getInt: " + result );
        return result;
    } // getInt

    /**
     * Get a 'long' column value for a result set.
     */
    protected long getLong(
                           GSDbResultSet rs,
                           int           index
                          )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        long result = 0;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: getLong" );
        errMsg = null;
        if ( rs == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: getLong" );
            return result;
        }

        try {
            result =  rs.getLong( index  );
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: getLong: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: getLong: Failover" );
            throw cfe;
        }  catch (  SQLException sqe  ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: getLong: Exception" );
        }

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                     "DEBUG: EXIT: GSAppTransaction: getLong: " + result );
        return result;
    } // getLong

    /**
     * Get a 'double' column value for a result set.
     */
    protected double getDouble(
                               GSDbResultSet rs,
                               int           index
                              )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        double result = 0.0;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: getDouble" );
        errMsg = null;
        if ( rs == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: getDouble" );
            return result;
        }

        try {
            result =  rs.getDouble( index  );
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: getDouble: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: getDouble: Failover" );
            throw cfe;
        }  catch (  SQLException sqe  ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: getDouble: Exception" );
        }

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                     "DEBUG: EXIT: GSAppTransaction: getDouble: " + result );
        return result;
    } // getDouble

    /**
     * Get a 'String' column value for a result set.
     */
    protected String getString(
                               GSDbResultSet rs,
                               int           index
                              )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        String result = null;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: getString" );
        errMsg = null;
        if ( rs == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: getString" );
            return result;
        }

        try {
            result =  rs.getString( index  );
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: getString: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: getString: Failover" );
            throw cfe;
        }  catch (  SQLException sqe  ) {
            errMsg = sqe.getSQLState() + ":"  +
                     sqe.getErrorCode() + ":" +
                     sqe.getMessage();
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: getString: Exception" );
        }

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                     "DEBUG: EXIT: GSAppTransaction: getString: " + result );
        return result;
    } // getString

    /**
     * Set an 'int' input parameter for a statement.
     */
    protected boolean setInt(
                             GSDbStatement stmt,
                             int           index,
                             int           param
                            )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: setInt" );
        if ( stmt == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: setInt" );
            return false;
        }

        try {
            result =  stmt.setInt( index, param );
            if (  ! result  )
                errMsg = stmt.getLastError();
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: setInt: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: setInt: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                                 "DEBUG: EXITOK: GSAppTransaction: setInt" );
            else
                GSUtil.debugMessage( ctxt,
                                 "DEBUG: EXITERR: GSAppTransaction: setInt" );
        }
        return result;
    } // setInt

    /**
     * Set a 'long' input parameter for a statement.
     */
    protected  boolean setLong(
                               GSDbStatement stmt,
                               int           index,
                               long          param
                              )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: setLong" );
        if ( stmt == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: setLong" );
            return false;
        }

        try {
            result =  stmt.setLong( index, param );
            if (  ! result  )
                errMsg = stmt.getLastError();
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: setLong: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: GSAppTransaction: setLong: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITOK: GSAppTransaction: setLong" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: setLong" );
        }
        return result;
    } // setLong

    /**
     * Set a 'String' input parameter for a statement.
     */
    protected boolean setString(
                                GSDbStatement stmt,
                                int           index,
                                String        param
                               )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: setString" );
        if ( stmt == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: setString" );
            return false;
        }

        try {
            result =  stmt.setString( index, param );
            if (  ! result  )
                errMsg = stmt.getLastError();
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: setString: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: setString: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITOK: GSAppTransaction: setString" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: setString" );
        }
        return result;
    } // setString

    /**
     * Set a 'double' input parameter for a statement.
     */
    protected boolean setDouble(
                                GSDbStatement stmt,
                                int           index,
                                double        param
                               )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: setDouble" );
        if ( stmt == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: setDouble" );
            return false;
        }

        try {
            result =  stmt.setDouble( index, param );
            if (  ! result  )
                errMsg = stmt.getLastError();
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: setDouble: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                       "DEBUG: GSAppTransaction: setDouble: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITOK: GSAppTransaction: setDouble" );
            else
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: setDouble" );
        }
        return result;
    } // setDouble

    /**
     * Set a 'BigDecimal' input parameter for a statement.
     */
    protected boolean setBigDecimal(
                                    GSDbStatement stmt,
                                    int           index,
                                    BigDecimal    param
                                )
        throws GSConnectionFailoverException,
               GSGridRetryException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: setBigDecimal" );
        if ( stmt == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                         "DEBUG: EXITERR: GSAppTransaction: setBigDecimal" );
            return false;
        }

        try {
            result =  stmt.setBigDecimal( index, param );
            if (  ! result  )
                errMsg = stmt.getLastError();
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                     "DEBUG: GSAppTransaction: setBigDecimal: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                     "DEBUG: GSAppTransaction: setBigDecimal: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                           "DEBUG: EXITOK: GSAppTransaction: setBigDecimal" );
            else
                GSUtil.debugMessage( ctxt,
                           "DEBUG: EXITERR: GSAppTransaction: setBigDecimal" );
        }
        return result;
    } // setBigDecimal

    /**
     * Execute a statement.
     */
    protected boolean executeStmt(
                                  GSDbStatement stmt
                                 )
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: executeStmt" );
        if ( stmt == null  )
        {
            errMsg = GSErrors.M_STATE_ERR;
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                             "DEBUG: EXITERR: GSAppTransaction: executeStmt" );
            return false;
        }

        try {
            // execute
            result = stmt.execute();
            if (  ! result  )
                errMsg = stmt.getLastError();
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                     "DEBUG: GSAppTransaction: executeStmt: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                     "DEBUG: GSAppTransaction: executeStmt: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                           "DEBUG: EXITOK: GSAppTransaction: executeStmt" );
            else
                GSUtil.debugMessage( ctxt,
                           "DEBUG: EXITERR: GSAppTransaction: executeStmt" );
        }
        return result;
    } // executeStmt

    /**
     *  Commit the transaction.
     */
    protected boolean commitTxn()
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        boolean result = false;

        if (  GSConstants.debugEnabled  )
            GSUtil.debugMessage( ctxt,
                             "DEBUG: ENTER: GSAppTransaction: commitTxn" );
        try {
            result = dbConn.commitEx();
            if (  ! result  )
                errMsg = dbConn.getLastError();
        } catch (  GSGridRetryException gre  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                     "DEBUG: GSAppTransaction: commitTxn: Transient error" );
            throw gre;
        } catch (  GSConnectionFailoverException cfe  ) {
            if (  GSConstants.debugEnabled  )
                GSUtil.debugMessage( ctxt,
                     "DEBUG: GSAppTransaction: commitTxn: Failover" );
            throw cfe;
        }

        if (  GSConstants.debugEnabled  )
        {
            if (  result  )
                GSUtil.debugMessage( ctxt,
                           "DEBUG: EXITOK: GSAppTransaction: commitTxn" );
            else
                GSUtil.debugMessage( ctxt,
                           "DEBUG: EXITERR: GSAppTransaction: commitTxn" );
        }
        return result;
    } // commitTxn

    /**
     * Cleanup the transaction and release all resources
     *
     * Usually will be overidden
     */
    public void cleanup()
    {
    } // cleanup
   
    /**
     * Initialize this transaction; prepare statements, allocate
     * resources etc.
     *
     * Usually will be overidden
     */
    public boolean init()
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        return false;
    } // init

    /**
     * Execute the transaction, returning a boolean
     *
     * Usually will be overridden
     */
    public boolean execute()
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        return false;
    } // Execute

    /**
     * Execute the transaction, returning a GSDbResultSet
     *
     * Usually will be overridden
     */
    public GSDbResultSet executeRS()
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        return null;
    } // ExecuteRS

    /**
     * Close any open result set.
     *
     * Usually will be overridden
     */
    public boolean close()
        throws GSGridRetryException, 
               GSConnectionFailoverException
    {
        return false;
    } // close

} // GSAppTransaction
