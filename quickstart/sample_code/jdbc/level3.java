/*
 * level3.java
 *
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Do some simple operations on a TimesTen database
 * using the TimesTen JDBC driver (JDK 1.4 and newer)
 * 1. Connect to the data store
 * 2. Process all orders in INPUTFILE3 by
 *    a. Inserting into the ORDERS and ORDER_ITEM tables and
 *    b. Updating the INVENTORY table.
 * 3. Disconnect from the data store
 *
 * Prerequisite:
 * Run the following command every time before running this demo:
 *    ttIsql -f input0.dat
 */

import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.sql.*;
import javax.sql.*;
import com.timesten.jdbc.TimesTenDataSource;

public class level3
{
  // Private static final defines


  /**
   * Prefix URL to pass to DataSource.getConnection() for
   * TimesTen Direct Connection
   */
  private static final String directURLPrefix = "jdbc:timesten:direct:";

  /**
   * Prefix of URL to pass to DataSource.getConnection() for
   * TimesTen client Connection
   */
  private static final String clientURLPrefix = "jdbc:timesten:client:";

  /**
   * Name of input file containing data to insert.
   */
  private static final String INPUTFILE = "datfiles/input3.dat";

  /**
   * Canned SQL statements
   */
  /* Insert a new order into the ORDERS table */
  private static final String INSERTORDERSTMT = "insert into orders values (orderID.nextval,?,?,?,?)";
  /* Insert the new order for products into the ORDER_ITEM table */
  private static final String INSERTOITEMSTMT = "insert into order_item values (orderID.currval,?,?)";
  /* Check inventory */
  private static final String SELECTINVSTMT = "select quantity from inventory where prod_num = ? for update";
  /* Update inventory */
  private static final String UPDATEINVSTMT = "update inventory set quantity = (quantity - ?) where prod_num = ?";


  // Private static variables

  /**
   * Standard output and error streams
   */
  private static PrintStream errStream = System.err;
  private static PrintStream outStream = System.out;

  /**
   * Variables for passing values from options parsing
   */
  private static String opt_connstr = "";

  private static boolean opt_doClient = false;

  private static boolean opt_doTrace = false;

  // package static variables

  /**
   * Flag indicating whether the shutdown hook thread should wait
   * and hold-up the JVM exit so that the running examples can
   * implement a clean exit first.
   * This flag is set only when the examples are being run.
   */
  static boolean shouldWait = false;

  /**
   * A synchronization object used by the shutdown hook thread to
   * wait for an orderly database disconnect before exiting.
   */
  static Object stopMonitor;

  /**
   * Flag indicating whether a "signal" has been received.
   * This flag is set by our shutdown hook thread.
   */
  static boolean hasReceivedSignal = false;


  // Instance variables for the instantiated level3 object

  /**
   * JDBC connection URL
   */
  private String URL = "";

  /**
   * main: Start of appliation.
   *
   * 1. Parse the arguments.
   * 2. Instantiate and run the level3 example.
   */
  public static void main(String[] args)
  {
    int retcode = 1;      // Return code: Assume failure

	  // Parse options
	  IOLibrary myLib = new IOLibrary(errStream);
	  //String className = this.getClass().getName();
	  String className = "level3";
	  String usageString = myLib.getUsageString(className);
	  if (myLib.parseOpts(args, usageString) == false)
	  {
		  System.exit(retcode);
	  }

    // Add a thread to the JVM shutdown hook to cleanly exit the
    // application in the event of a JVM shutdown
    stopMonitor = new Object();
    Runtime.getRuntime().addShutdownHook(new Thread("Shutdown thread")
    {
      public void run() {
        synchronized (stopMonitor) {
          if (shouldWait) {
            // outStream.println("Shutdown thread Waiting");
            hasReceivedSignal = true;
            try {
              // Wait for OK to proceed
              stopMonitor.wait();  // Might want have time limit.
            } catch (InterruptedException e) {
              e.printStackTrace();
            }
            // outStream.println("Leaving Shutdown thread");
          }
        }
      }
    });

    // Construct the level3 object
    level3 lvl3 = new level3();

    try {
      // Tell the shutdown hook to wait for a clean exit
      synchronized (stopMonitor) {
        shouldWait = true;
      }

      // Run the level3 example.
      retcode = lvl3.runexample(myLib.opt_doTrace, myLib.opt_doClient, myLib.opt_connstr);

    } finally {
      // Unblock the shutdown hook
      synchronized (stopMonitor) {
        shouldWait = false;
        stopMonitor.notify();
      }
    }
    // exit program
    System.exit(retcode);
  }

  /**
   * Constructor for class level3
   */
  public level3()
  {
  }

  /**
   * Run the level3 example.
   */
  public int runexample(boolean doTrace, boolean doClient, String connStr)
  {
    int retcode = 1;    // Return code: Assume failure
    Connection con = null;

    // Use the TimesTenDataSource object to connect to the datastore.
    // See level1.java for example of using the DriverManager interface.
    TimesTenDataSource ds = new TimesTenDataSource();

    // Construct the connection URL
    if (doClient) {
      outStream.println("\nUsing client connection");
      URL = clientURLPrefix + connStr;
    } else {
      outStream.println("\nUsing direct connection");
      URL = directURLPrefix + connStr;
    }

    try {

      // Turn on tracing if requested
      if (doTrace) {
        outStream.println("\nEnabling JDBC Tracing");
        ds.setLogWriter(new PrintWriter(outStream, true));
      }

      // Open a connection to TimesTen
      outStream.println("\nOpening connection " + URL);
      ds.setUrl(URL);

	  //Prompt for the Username and Password
	  AccessControl ac = new AccessControl();
	  String username = ac.getUsername();
	  String password = ac.getPassword();

	  con = ds.getConnection(username,password);

	  InitializeDatabase idb = new InitializeDatabase();
	  idb.initialize(con);

      // Report any SQLWarnings on the connection
      reportSQLWarnings(con.getWarnings());
      CheckIfStopIsRequested();

      // Explicitly turn off auto-commit
      con.setAutoCommit(false);
      reportSQLWarnings(con.getWarnings());

      // Prepare all Statements ahead of time
      PreparedStatement pInsOrd = con.prepareStatement(INSERTORDERSTMT);
      PreparedStatement pInsItm = con.prepareStatement(INSERTOITEMSTMT);
      PreparedStatement pUpdInv = con.prepareStatement(UPDATEINVSTMT);
      PreparedStatement pSelInv = con.prepareStatement(SELECTINVSTMT);
      // Prepare is a transaction; must commit to release locks
      con.commit();

      outStream.println();

      // Process the orders
      // Outer while loop: Read the input line by line
      BufferedReader in = new BufferedReader(new FileReader(INPUTFILE));
      String line;
      while ((line = in.readLine()) != null) {
        // Skip comments
        if (line.equals("") || line.charAt(0) == '#') {
          continue;
        }
        outStream.println("Processing data: " + line);
        String[] fields = line.split(",");
        pInsOrd.setInt(1, Integer.parseInt(fields[0]));  // cust_num
        pInsOrd.setTimestamp(2, Timestamp.valueOf(fields[1])); // when_placed
        pInsOrd.setNull(3, java.sql.Types.TIMESTAMP);   // when_shipped
        pInsOrd.setNull(4, java.sql.Types.VARCHAR);     // notes
        pInsOrd.executeUpdate();
        reportSQLWarnings(pInsOrd.getWarnings());

        // Inner for loop: process all the products that were ordered
        int numOrders = 0;
        boolean skipToNextOrder = false;
        for (int i = 2; i < fields.length; i += 2) {
          int prodNum = Integer.parseInt(fields[i]);
          int itemCount = Integer.parseInt(fields[i + 1]);

          // Get the current inventory count for this product
          pSelInv.setInt(1, prodNum);
          ResultSet rs = pSelInv.executeQuery();
          int invCount = 0;
          if (rs.next()) {
            invCount = rs.getInt(1);
          } else {
            errStream.println("Error: No inventory for pdocut number " +
                              prodNum);
            skipToNextOrder = true;
            break; // Break out of inner for loop
          }

          // Is there enough to fill the order?
          if (itemCount <= invCount) {

            // Yes - insert the row into the ORDER_ITEM table
            pInsItm.setInt(1, prodNum);                   // prod_num
            pInsItm.setInt(2, itemCount);                 // quantity
            pInsItm.executeUpdate();
            // Report any SQLWarnings on the connection
            reportSQLWarnings(pInsItm.getWarnings());
            CheckIfStopIsRequested();

            // Debit the inventory
            pUpdInv.setInt(1, prodNum);                   // quantity
            pUpdInv.setInt(2, itemCount);                 // prod_num
            pUpdInv.executeUpdate();
            // Report any SQLWarnings on the connection
            reportSQLWarnings(pInsItm.getWarnings());

          } else {
            errStream.println("Warning: Inventory for product number " +
                              prodNum + " is " + invCount + " Count.  " +
                              "\n\tCannot fill order for " + itemCount);
            skipToNextOrder = true;
            break; // Break out of inner for loop
          }

        } // end inner for()

        numOrders++;

        /*
         * Finished processing a single line.
         * Now, rollback or commit?
         */
        if (skipToNextOrder || numOrders == 0) {
          if (skipToNextOrder == false) {
            errStream.println("Warning: No orders for any products were given.");
          }
          retcode = 1;
          // Rollback the order line
          outStream.println("Rolling back last transaction due to inability to process order");
          con.rollback();

        } else {
          // Commit the order
          con.commit();
        }

      } // end outer while()

      // Commit the entire order process?
      //      con.commit();
      in.close();
      // Close the prepared statements - we're done with it
      pInsOrd.close();
      pInsItm.close();
      pUpdInv.close();
      pSelInv.close();
      // Report any SQLWarnings on the connection
      reportSQLWarnings(con.getWarnings());
      CheckIfStopIsRequested();

      retcode = 0;      // If we reached here - success.
      // Fall through to con.close() in the finally clause

    } catch (IOException ex) {
      ex.printStackTrace();
      // Fall through to con.close() in the finally clause

    } catch (SQLException ex) {
      if (ex.getSQLState().equalsIgnoreCase("S0002")) {
        errStream.println("\nError:  The table customer does not exist.\n\tPlease run ttIsql -f input0.dat to initialize the database.");
      } else if (ex.getErrorCode() == 907) {
        errStream.println("\nError:  Attempting to insert a row with a duplicate primary key.\n\tPlease rerun ttIsql -f input0.dat to reinitialize the database.");
      } else {
        reportSQLExceptions(ex);
      }
      // Fall through to con.close() in the finally clause

    } catch (InterruptedException ex) {
      errStream.println(ex.getMessage());
      // Fall through to con.close() in the finally clause

    } finally {
      try {
        if (con != null && !con.isClosed()) {
          // Rollback any transactions in case of errors
          if (retcode != 0) {
            try {
              outStream.println("\nEncountered error.  Rolling back transaction");
              con.rollback();
            } catch (SQLException ex) {
              reportSQLExceptions(ex);
            }
          }

          outStream.println("\nClosing the connection\n");
          con.close();
        }
      } catch (SQLException ex) {
        reportSQLExceptions(ex);
      }
    }

    return retcode;
  }

  /**
   * Dump a chain of SQLException objects
   *
   * @param ex SQLException object
   *
   * @return A count of the exceptions processed
   */
  static int reportSQLExceptions(SQLException ex)
  {
    int errCount = 0;

    if (ex != null) {
      errStream.println("\n--- SQLException caught ---");
      ex.printStackTrace();

      while (ex != null) {
        errStream.println("SQL State:   " + ex.getSQLState());
        errStream.println("Message:     " + ex.getMessage());
        errStream.println("Vendor Code: " + ex.getErrorCode());
        errCount++;
        ex = ex.getNextException();
        errStream.println();
      }
    }

    return errCount;
  }

  /**
   * Dump a chain of SQLWarning objects
   *
   * @param wn SQLWarning object
   *
   * @return A count of the warnings processed
   */
  static int reportSQLWarnings(SQLWarning wn)
  {
    int warnCount = 0;

    while (wn != null) {
      errStream.println("\n--- SQL Warning ---");

      // is this a SQLWarning object or a DataTruncation object?
      errStream.println("SQL State:   " + wn.getSQLState());
      errStream.println("Message:     " + wn.getMessage());
      errStream.println("Vendor Code: " + wn.getErrorCode());

      if (wn instanceof DataTruncation) {
        DataTruncation trn = (DataTruncation) wn;
        errStream.println("Truncation error in column: " + trn.getIndex());
      }

      warnCount++;
      wn = wn.getNextWarning();
      errStream.println();
    }

    return warnCount;
  }

  /**
   * Check if a "signal" is pending - i.e. the shutdown hook
   * thread has been entered.
   * @return true if a "signal" is pending.
   */
  private static boolean IsSignalPending()
  {
    return (hasReceivedSignal == true);
  }

  /**
   * Calls IsSignalPending() and throws an exception if true.
   * @return false if a "signal" is not pending.
   * @exception InterruptedException if a signal is pending
   */
  private static boolean CheckIfStopIsRequested()
    throws InterruptedException
  {
    if (IsSignalPending() == true) {
      throw new InterruptedException("\nWarning: Stop Requested.  Aborting!");
    }
    return false;
  }


  /**
   * Parse Options from the command line
   *
   * @param args  String array of arguments to parse
   * @param usage Print this usage String on errors during argument parsing
   *
   * @return true on success, false otherwise
   */
  private static boolean parseOpts(String[] args, String usage)
  {
    // Parse Command line
    int i;
    boolean argsOk;
    String arg;
    for (i = 0, argsOk = true, arg = null;
         argsOk && (i < args.length) && ((arg = args[i]).startsWith("-"));
         i++) {

      if (arg.equals("-h") || arg.equals("-help")) {
        argsOk = false;

      } else if (arg.equals("-t")) {
        opt_doTrace = true;

      } else if (arg.equals("-c")) {
        opt_doClient = true;

      } else if (arg.equals("-d")) {
        opt_doClient = false;

      } else {
        errStream.println("Illegal option '" + arg + "'");
        argsOk = false;
      }

    } // end of for() loop for argument parsing

    // Check if argument parsing exited for error reasons
    if (!argsOk) {
      errStream.println(usage);
      return argsOk;
    }

    // Set the connection string
    if (i < args.length) {
      opt_connstr = arg;
      i++;
    }

    // Check for unparsed arguments
    if (i != args.length) {
      errStream.println("Extra arguments left on command line");
      errStream.println(usage);
      return false;
    }

	 // Check for null or unset connection string
	 if (opt_connstr.equals(""))  {
	   if (opt_doClient) {
		 opt_connstr = tt_version.sample_DSN_CS;
	   } else {
		 opt_connstr = tt_version.sample_DSN;
	   }
	 }
    return true;
  }

}


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
