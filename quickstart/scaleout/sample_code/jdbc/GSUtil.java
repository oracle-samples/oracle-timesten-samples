/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

import java.math.BigDecimal;
import java.math.RoundingMode;
import java.util.concurrent.ThreadLocalRandom;
import java.sql.*;

/**
 *  Utility functions used in the GridSample sample program.
 */
public class GSUtil
{
    /**
     * Sleep for 'n' milliseconds
     */
    public static void sleep(
                             long n
                            )
    {
        try {
            Thread.sleep( n );
        } catch ( Exception e ) { ; }
    } // sleep

    /**
     * get the current date and time as a fixed length timestamp
     * (for use in messages)
     */
    public static String getTimestamp()
    {
        java.sql.Timestamp ct =
            new java.sql.Timestamp(System.currentTimeMillis());
        String sct = ct.toString();
        int l = sct.length();
        if (  l == 22  )
            sct = sct + "0";
        else
        if (  l == 21  )
            sct = sct + "00";

        return sct;
    } // getTimestamp

    /**
     * Get a random long within a specified (positive) range
     */
    public static long getRandomLong(
                                     long min,
                                     long max
                                    )
    {
        long rval = 0;

        if (  (min >= max) || (max < 1) || (min < 0)  )
            rval = 0;
        else
            rval = ThreadLocalRandom.current().nextLong(max-min) + min;

        return rval;
    } // getRandomLong

    /**
     * Get a random double within a specified (positive) range
     */
    public static double getRandomDouble(
                                         double min,
                                         double max
                                        )
    {
        double rval = 0;

        if (  (min >= max) || (max <= 0.0) || (min <= 0.0)  )
            rval = 0.0;
        else
            rval = ThreadLocalRandom.current().nextDouble(max-min) + min;

        return rval;
    } // getRandomDouble

    /**
     * Choose a random transaction with a distribution governed by the
     * global parameters (in 'ctxt')
     */
    public static int chooseTxnType(
                                    GSGlobal ctxt
                                   )
    {
        int rnd = ThreadLocalRandom.current().nextInt( 100 );

        if (  rnd < (ctxt.pctAuthorize + ctxt.pctQuery + 
                     ctxt.pctCharge + ctxt.pctTopUp)  )
        {
            if (  rnd < (ctxt.pctAuthorize + ctxt.pctQuery + ctxt.pctCharge)  )
            {
                if (  rnd < (ctxt.pctAuthorize + ctxt.pctQuery)  )
                {
                    if (  rnd < ctxt.pctAuthorize  )
                        return GSConstants.TXN_AUTHORIZE;
                    else
                        return GSConstants.TXN_QUERY;
                }
                else
                    return GSConstants.TXN_CHARGE;
            }
            else
                return GSConstants.TXN_TOPUP;
        }
        else
            return GSConstants.TXN_PURGE;
    } // chooseTxnType

    /**
     * Get a random currency amount in the specified range (limited)
     */
    public static BigDecimal getRandomAmount(
                                             double min,
                                             double max
                                            )
    {
        double rdval = getRandomDouble( min, max );
        if (  rdval == 0.0  )
            return null;

        BigDecimal bdval = new BigDecimal( rdval );
        bdval = bdval.setScale( 2, RoundingMode.HALF_UP );

        return bdval;
    } // getRandomAmount

    /**
     * Choose a random customer number within the range that is present 
     * in the database
     */
    public static long getRandomCustomer(
                                         GSGlobal ctxt
                                        )
    {
         return getRandomLong( ctxt.minCustID, ctxt.maxCustID );
    } // getRandomCustomer

    /**
     * Choose a random account number within the range that is present 
     * in the database
     */
    public static long getRandomAccount(
                                         GSGlobal ctxt
                                        )
    {
         return getRandomLong( ctxt.minAccountID, ctxt.maxAccountID );
    } // getRandomCustomer

    /**
     * Print an error message with a timestamp
     */
    public static void printError(
                                  String emsg
                                 )
    {
        System.err.println( GSUtil.getTimestamp() + ": " + emsg );
    } // printError

    /**
     * Print a message with a timestamp
     */
    public static void printMessage(
                                    String msg
                                   )
    {
        System.out.println( GSUtil.getTimestamp() + ": " + msg );
    } // printMessage

    /**
     * Print a message with a timestamp to the log (if enabled)
     */
    public static void logMessage(
                                  GSGlobal ctxt,
                                  String msg
                                 )
    {
        if (  ctxt.logOut != null  )
            ctxt.logOut.println( GSUtil.getTimestamp() + ": " + msg );
    } // logMessage

    /**
     * Print a message with a timestamp to the log if it is enabled
     * and we are in debug mode
     */
    public static void debugMessage(
                                    GSGlobal ctxt,
                                    String msg
                                   )
    {
        if (  GSConstants.debugEnabled &&
              ctxt.debugMode && 
              (ctxt.logOut != null)  )
            ctxt.logOut.println( GSUtil.getTimestamp() + ": " + msg );
    } // logMessage

    /**
     * Print a message with a timestamp and optionally log it
     */
    public static void printMessage(
                                    GSGlobal ctxt,
                                    String msg,
                                    boolean log
                                   )
    {
        System.out.println( GSUtil.getTimestamp() + ": " + msg );
        if (  log  )
            logMessage( ctxt, msg );
    } // printMessage

    /**
     * Print an error message with a timestamp and optionally log it
     */
    public static void printError(
                                  GSGlobal ctxt,
                                  String msg,
                                  boolean log
                                 )
    {
        System.err.println( GSUtil.getTimestamp() + ": " + msg );
        if (  log  )
            logMessage( ctxt, msg );
    } // printError

} // GSUtil
