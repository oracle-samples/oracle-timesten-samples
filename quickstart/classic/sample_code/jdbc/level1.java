/*
 * level1.java
 *
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Do some simple operations on a TimesTen data store
 * using the TimesTen JDBC driver (JDK 1.4 and newer)
 * 1. Connect to the data store using the DriverManager interface
 * 2. Insert some rows read from a data file (located at datfiles/input1.dat)
 * 3. Select some rows from the CUSTOMER table
 * 4. Disconnect from the data store
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

public class level1
{
  // Private static final defines

  /**
   * Name of TimesTen driver
   */
  private static final String timestenDriver = "com.timesten.jdbc.TimesTenDriver";

  /**
   * Prefix URL to pass to DriverManager.getConnection() for
   * TimesTen Direct Connection
   */
  private static final String directURLPrefix = "jdbc:timesten:direct:";

  /**
   * Prefix of URL to pass to DriverManager.getConnection() for
   * TimesTen client Connection
   */
  private static final String clientURLPrefix = "jdbc:timesten:client:";

  /**
   * Name of input file containing data to insert.
   */
  private static final String INPUTFILE = "datfiles/input1.dat";

  /**
   * Canned SQL statements
   */
  /* Insert into customer table */
  private static final String INSERTSTMT = "insert into customer values (?,?,?,?)";

  /* TimeTen internal cmd to update statistics on the customer table */
  private static final String UPDSTATSTMT = "{CALL ttOptUpdateStats('customer',1)}";

  /* Select from customer table */
  private static final String SELECTSTMT = "select cust_num, region, name, address from customer";

  // Private static variables

  /**
   * Standard output and error streams
   */
  private static PrintStream errStream = System.err;
  private static PrintStream outStream = System.out;


  // private static boolean opt_PrintVersion = false;

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


  // Instance variables for the instantiated level1 object

  /**
   * JDBC connection URL
   */
  private String URL = "";

  /**
   * main: Start of appliation.
   *
   * 1. Parse the arguments.
   * 2. Instantiate and run the level1 example.
   */
  public static void main(String[] args)
  {
    int retcode = 1;      // Return code: Assume failure

      // Parse options
	  IOLibrary myLib = new IOLibrary(errStream);
	  //String className = this.getClass().getName();
	  String className = "level1";
	  String usageString = myLib.getUsageString(className);
	  if (myLib.parseOpts(args, usageString) == false) {
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

    // Construct the level1 object
    level1 lvl1 = new level1();

    try {
      // Tell the shutdown hook to wait for a clean exit
      synchronized (stopMonitor) {
        shouldWait = true;
      }

      // Run the level1 example.
      retcode = lvl1.runexample(myLib.opt_doTrace, myLib.opt_doClient, myLib.opt_connstr);

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
   * Constructor for class level1
   */
  public level1()
  {
  }

  /**
   * Run the level1 example
   */
  public int runexample(boolean doTrace, boolean doClient, String connStr)
  {

    int retcode = 1;    // Return code: Assume failure
    Connection con = null;

    // Turn on tracing if requested.
    // Do this before loading the TimesTen Driver.
    if (doTrace) {
      outStream.println("\nEnabling JDBC Tracing");
      DriverManager.setLogWriter(new PrintWriter(outStream, true));
    }

    // Load the TimesTen JDBC driver
    // This is one way to load the TimesTen JDBC driver - using
    // The DriverManager interface.  The other way is to use the
    // DataSource interface [see level2.java].
    try {
      outStream.println("\nLoading Driver " + timestenDriver);
      Class.forName(timestenDriver);
      if (doClient) {
        outStream.println("\nUsing client connection");
        URL = clientURLPrefix + connStr;
      } else {
        outStream.println("\nUsing direct connection");
        URL = directURLPrefix + connStr;
      }
    } catch (ClassNotFoundException ex) {
      ex.printStackTrace();
      return retcode;
    }



    try {
		
		//Prompt for the Username and Password
		AccessControl ac = new AccessControl();
		String username = ac.getUsername();
		String password = ac.getPassword();

		// Connect here to prevent disconnect/unload by populate 
		
		try 
		{
			    con = DriverManager.getConnection(URL, username, password);
			    System.out.println();
				//System.out.println("Connected to database as " + username + "@" + defaultDSN);
			    System.out.println("Connected to database as " + username + "@" + connStr);

			    // Initialize the database
			   
			    InitializeDatabase idb = new InitializeDatabase();
			    idb.initialize(con);
				
		}
		catch (SQLException sqle) {sqle.printStackTrace();}
       


      // Report any SQLWarnings on the connection
      reportSQLWarnings(con.getWarnings());
      CheckIfStopIsRequested();

      // Explicitly turn off auto-commit
      con.setAutoCommit(false);
      reportSQLWarnings(con.getWarnings());

      // Prepare all Statements ahead of time
      PreparedStatement pIns = con.prepareStatement(INSERTSTMT);
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
        pIns.setInt(1, Integer.parseInt(fields[0]));    // cust_num
        pIns.setString(2, fields[1]);                   // region
        pIns.setString(3, fields[2]);                   // name
        pIns.setString(4, fields[3]);                   // addresss
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

      // Update statistics
      outStream.println("\nUpdating statistics");
      Statement stmt = con.createStatement();
      stmt.execute(UPDSTATSTMT);
      // Commit the statistics update
      con.commit();
      // Close the statement - we're done with it
      stmt.close();

      // Report any SQLWarnings on the connection
      reportSQLWarnings(con.getWarnings());
      CheckIfStopIsRequested();

      // Select out some rows
      outStream.println("\nExecuting prpared SELECT statement");
      ResultSet rs = pSel.executeQuery();

      outStream.println("Fetching result set...");
      while (rs.next()) {
        outStream.println("\n  Customer number: " + rs.getInt(1));
        outStream.println("  Region: " + rs.getString(2));
        outStream.println("  Name: " + rs.getString(3));
        outStream.println("  Address: " + rs.getString(4));
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
        // This error returns SQLState S0002 "Base table not found" and
        // native Error 2206 TT_ERR_TABLE_DOES_NOT_EXIST.  Since these two
        // errors return equivalent information, we match against SQLState
        // here for demonstration purposes.
        errStream.println("\nError:  The table customer does not exist.\n\tPlease run ttIsql -f input0.dat DSN  to initialize the database.");

      } else if (ex.getErrorCode() == 907) {
        // This error returns SQLState 23000 "constraint violation" and
        // native Error 907 TT_ERR_KEY_EXISTS.  Since there can be many
        // types of constraint violations that return the same SQLState, we
        // match against the more specific native code.
        errStream.println("\nError:  Attempting to insert a row with a duplicate primary key.\n\tPlease rerun ttIsql -f input0.dat DSN to reinitialize the database.");
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
