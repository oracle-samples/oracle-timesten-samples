/*
 * level2.java
 *
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * This program performs simple operations on a TimesTen database
 * using the TimesTen JDBC driver (JDK 1.4 and newer).
 * 1. Connect to the database using the TimesTenDataSource interface
 * 2. Insert some rows read from a data file (located at datfiles/input1.dat)
 * 3. Delete some rows from the PRODUCT tablew
 * 4. Update some rows in the PRODUCT table
 * 5. Select some rows from the PRODUCT table
 * 6. Disconnect from the database
 *
 */

import java.math.BigInteger;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.sql.*;
import javax.sql.*;
import com.timesten.jdbc.TimesTenDataSource;

public class level2
{
  // Private static final defines

  /**
   * Usage string
   */
  private static final String usageString = "\n" +
    "Usage:  level2  [-t] [-d|-c] connstr\n" +
    "        level2  -h | -help \n\n" +
    "  -h | -help    print usage and exit\n" +
    "  -d            connect using direct driver (default)\n" +
    "  -c            connect using client driver\n" +
    "  -t            enable JDBC tracing\n" +
	"  connstr      connection string, e.g. \"DSN=" + 
           tt_version.sample_DSN + "\"\n"+
	"  Usage examples:\n"+
	"	  java level2\n"+
	"	  java level2 -c\n"+
	"	  java level2 mydsn_name\n"+
	"Program defaults if parameters not specified:\n"+
	"connstr=\"DSN=" + tt_version.sample_DSN + "\", -d for direct-linked";


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
  private static final String INPUTFILE = "datfiles/input2.dat";

  /**
   * Canned SQL statements
   */
  /* Insert into product table */
  private static final String INSERTSTMT = "insert into product values (?,?,?,?,?,?,?)";

  /* TimeTen internal cmd to update statistics on the product table */
  private static final String UPDSTATSTMT = "{CALL ttOptUpdateStats('product',1)}";

  /* Delete rows that have the duplicate product names */
  private static final String DELETESTMT = "delete from product p1 where exists (select name from product p2 where p1.name = p2.name and p1.prod_num > p2.prod_num)";

  /* Business is booming - raise all prices by 10% */
  private static final String UPDATESTMT = "update product set price = (price * 1.10)";

  /* Select from product table */
  private static final String SELECTSTMT = "select prod_num, name, price, ship_weight, description, picture, notes from product";

  // Private static variables

  /**
   * Standard output and error streams
   */
  private static PrintStream errStream = System.err;
  private static PrintStream outStream = System.out;


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


  // Instance variables for the instantiated level2 object

  /**
   * JDBC connection URL
   */
  private String URL = "";

  /**
   * main: Start of appliation.
   *
   * 1. Parse the arguments.
   * 2. Instantiate and run the level2 example.
   */
  public static void main(String[] args)
  {
    int retcode = 1;      // Return code: Assume failure


	  // Parse options
	  IOLibrary myLib = new IOLibrary(errStream);
	  //String className = this.getClass().getName();
	  String className = "level2";
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

    // Construct the level2 object
    level2 lvl2 = new level2();

    try {
      // Tell the shutdown hook to wait for a clean exit
      synchronized (stopMonitor) {
        shouldWait = true;
      }

      // Run the level2 example.
      retcode = lvl2.runexample(myLib.opt_doTrace, myLib.opt_doClient, myLib.opt_connstr);

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
   * Constructor for class level2
   */
  public level2()
  {
  }

  /**
   * Run the level2 example.
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

    try
	{

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
      PreparedStatement pIns = con.prepareStatement(INSERTSTMT);
      PreparedStatement pDel = con.prepareStatement(DELETESTMT);
      PreparedStatement pUpd = con.prepareStatement(UPDATESTMT);
      PreparedStatement pSel = con.prepareStatement(SELECTSTMT);
      // Prepare is a transaction; must commit to release locks
      con.commit();

      outStream.println();

      // Insert some rows, read from file
      BufferedReader in = new BufferedReader(new FileReader(INPUTFILE));
      String line;
      while ((line = in.readLine()) != null) {
        outStream.println("Inserting row: " + line);
        String[] fields = line.split(",");
        pIns.setInt(1, Integer.parseInt(fields[0]));    // prod_num
        pIns.setString(2, fields[1]);                   // name
        pIns.setString(3, fields[2]);                   // price
        pIns.setFloat(4, Float.parseFloat(fields[3]));  // ship_weight
        pIns.setString(5, fields[4]);                   // description
        BigInteger bi = new BigInteger(fields[5],2);
        pIns.setBytes(6, bi.toByteArray());             // picture
        pIns.setString(7, fields[6]);                   // notes
        pIns.executeUpdate();
      }
      // Commit the insert
      con.commit();
      in.close();
      // Close the prepared statement - we're done with it
      pIns.close();
      // Report any SQLWarnings on the connection
      reportSQLWarnings(con.getWarnings());
      CheckIfStopIsRequested();

      // Delete some rows
      outStream.println("\nExecuting prepared DELETE statement");
      int numRows = pDel.executeUpdate();
      outStream.println("Number of rows deleted: " + numRows);
      // Commit the delete
      con.commit();
      // Close the prepared statement - we're done with it
      pDel.close();
      // Report any SQLWarnings on the connection
      reportSQLWarnings(con.getWarnings());
      CheckIfStopIsRequested();

      // Update statistics
      outStream.println("\nUpdating statistics");
      Statement stmt = con.createStatement();
      stmt.execute(UPDSTATSTMT);
      // Commit the statistics update
      con.commit();
      // Report any SQLWarnings on the connection
      reportSQLWarnings(con.getWarnings());
      CheckIfStopIsRequested();

      // Update some rows
      outStream.println("\nExecuting prepared UPDATE statement");
      numRows = pUpd.executeUpdate();
      outStream.println("Number of rows updated: " + numRows);
      // Commit the update
      con.commit();
      // Close the prepared statement - we're done with it
      pUpd.close();
      // Report any SQLWarnings on the connection
      reportSQLWarnings(con.getWarnings());
      CheckIfStopIsRequested();

      // Select out some rows
      outStream.println("\nExecuting prpared SELECT statement");
      ResultSet rs = pSel.executeQuery();

      outStream.println("Fetching result set...");
      while (rs.next()) {
        outStream.println("\n  Product number: " + rs.getInt(1));
        outStream.println("  Name: " + rs.getString(2));
        outStream.println("  Price: " + rs.getBigDecimal(3));
        outStream.println("  Weight: " + rs.getFloat(4));
        outStream.println("  Description: " + rs.getString(5));
        outStream.println("  Picture: " + rs.getString(6));
        outStream.println("  Notes: " + rs.getString(7));
      }

      // Close the result set.
      rs.close();
      // Close the select statement - we're done with it
      pSel.close();

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
        errStream.println("\nError: The table product does not exist.\n\tPlease run ttIsql -f input0.dat to initialize the database.");
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
              outStream.println("\nEncountered error.  Rolling back transactions");
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


}


/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
