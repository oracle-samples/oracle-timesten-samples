/*
 * level4.java
 *
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Do some simple operations on a TimesTen database
 * using the TimesTen JDBC driver (JDK 1.4 and newer)
 * 1. Using multiple threads each with a connection to the database
 * 2. Process all orders (one thread per order) in INPUTFILE3
 *    a. Inserts will be done into the ORDERS and ORDER_ITEM tables and
 *    b. Updates will be done to the INVENTORY table.
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

public class level4
{
  // Private static final defines

  /**
   * Maximum number of threads to create
   */
  private static final int NUM_THREADS = 2;
  

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

  /**
   * The ThreadParameter class is used to pass parameters into
   * and receive return values out of a thread.
   *
   * @see #ProcessOrderThread
   */
  class ThreadParameter
  {
    int returnValue;
    int tid;
    String orderLine;

    Connection con;

    PreparedStatement pInsOrd;
    PreparedStatement pInsItm;
    PreparedStatement pUpdInv;
    PreparedStatement pSelInv;
  }

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


  // Instance variables for the instantiated level4 object

  /**
   * JDBC connection URL
   */
  private String URL = "";

  /**
   * main: Start of appliation.
   *
   * 1. Parse the arguments.
   * 2. Instantiate and run the level4 example.
   */
  public static void main(String[] args)
  {
    int retcode = 1;      // Return code: Assume failure

	  // Parse options
	  IOLibrary myLib = new IOLibrary(errStream);
	  //String className = this.getClass().getName();
	  String className = "level4";
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

    // Construct the level4 object
    level4 lvl4 = new level4();

    try {
      // Tell the shutdown hook to wait for a clean exit
      synchronized (stopMonitor) {
        shouldWait = true;
      }

      // Run the level4 example.
      retcode = lvl4.runexample(myLib.opt_doTrace, myLib.opt_doClient, myLib.opt_connstr);

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
   * Constructor for class level4
   */
  public level4()
  {
  }

  /**
   * Run the level4 example.
   */
  public int runexample(boolean doTrace, boolean doClient, String connStr)
  {
    int retcode = 1;    // Return code: Assume failure

    // Construct the connection URL
    if (doClient) {
      outStream.println("\nUsing client connection");
      URL = clientURLPrefix + connStr;
    } else {
      outStream.println("\nUsing direct connection");
      URL = directURLPrefix + connStr;
    }

    // An array to hold the threads that will be created.
    ProcessOrderThread[] threads = new ProcessOrderThread[NUM_THREADS];

    // An array of thread parameter objects
    ThreadParameter[] threadParms = new ThreadParameter[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
      threadParms[i] = new ThreadParameter();
    }

    // Use the TimesTenDataSource object to connect to the datastore.
    // See level1.java for example of using the DriverManager interface.
    TimesTenDataSource ds = new TimesTenDataSource();

    try {

      // Turn on tracing if requested
      if (doTrace) {
        outStream.println("\nEnabling JDBC Tracing");
        ds.setLogWriter(new PrintWriter(outStream, true));
      }

      // Open all connections to TimesTen
      ds.setUrl(URL);

	  //Prompt for the Username and Password
	  AccessControl ac = new AccessControl();
	  String username = ac.getUsername();
	  String password = ac.getPassword();
	
	  Connection conInit;
	  conInit = ds.getConnection(username,password);
      InitializeDatabase idb = new InitializeDatabase();
	  idb.initialize(conInit);


      for (int i = 0; i < NUM_THREADS; i++) 
	  {
        outStream.println("\nOpening connection [" + i + "] to " + URL);
        threadParms[i].tid = i;
        threadParms[i].con = ds.getConnection(username,password);
        reportSQLWarnings(threadParms[i].con.getWarnings());

        CheckIfStopIsRequested();

        // Explicitly turn off auto-commit
        threadParms[i].con.setAutoCommit(false);

        // Prepare all Statements ahead of time
        threadParms[i].pInsOrd = threadParms[i].con.prepareStatement(INSERTORDERSTMT);
        threadParms[i].pInsItm = threadParms[i].con.prepareStatement(INSERTOITEMSTMT);
        threadParms[i].pUpdInv = threadParms[i].con.prepareStatement(UPDATEINVSTMT);
        threadParms[i].pSelInv = threadParms[i].con.prepareStatement(SELECTINVSTMT);
        // Prepare is a transaction; must commit to release locks
        threadParms[i].con.commit();
      }

      int threadCount = 0;

      // Process the orders
      // Open file and read the input line by line
      BufferedReader in = new BufferedReader(new FileReader(INPUTFILE));
      String line;
      while ((line = in.readLine()) != null) {
        // Skip comments
        if (line.equals("") || line.charAt(0) == '#') {
          continue;
        }

        /*
         * 1. Spawn as many simultaneous threads as we have connections
         * 2. Wait for them all to finish
         * 3. Repeat
         */

        // Spawn the thread
        threadParms[threadCount].orderLine = line;
        threads[threadCount] = new ProcessOrderThread(threadParms[threadCount]);
        threads[threadCount].start();
        threadCount++;

        CheckIfStopIsRequested();

        // Wait for all threads
        if (threadCount >= NUM_THREADS) {
          //          outStream.println("processOrders: Still processing file...");

          for (int i = 0; i < NUM_THREADS; i++) {
            threads[i].join();
            CheckIfStopIsRequested();

            // Check if we need to exit
            outStream.println("\nthread [" + i + "] returned: " +
                              threadParms[i].returnValue);
            threads[i] = null;
            if (threadParms[i].returnValue != 0) {
              return retcode; // fall into finally clause
            }
          }

          threadCount = 0;
        }

      } // end of while()

      // Close the file.
      in.close();

      // Close all prepared statements - we're done with them.
      for (int i = 0; i < NUM_THREADS; i++) {
        threadParms[i].pInsOrd.close();
        threadParms[i].pInsItm.close();
        threadParms[i].pUpdInv.close();
        threadParms[i].pSelInv.close();
        // Report any SQLWarnings on the connection
        reportSQLWarnings(threadParms[i].con.getWarnings());
      }

      CheckIfStopIsRequested();

      // Report any SQLWarnings on the connection
      reportSQLWarnings(threadParms[0].con.getWarnings());
      CheckIfStopIsRequested();

      retcode = 0;      // If we reached here - success.
      // Fall through to finally clause

    } catch (IOException ex) {
      ex.printStackTrace();
      // Fall through to finally clause

    } catch (SQLException ex) {
      if (ex.getSQLState().equalsIgnoreCase("S0002")) {
        errStream.println("\nError:  The table does not exist.\n\tPlease run ttIsql -f input0.dat to initialize the database.");
      } else if (ex.getErrorCode() == 907) {
        errStream.println("\nError:  Attempting to insert a row with a duplicate primary key.\n\tPlease rerun ttIsql -f input0.dat to reinitialize the database.");
      } else {
        reportSQLExceptions(ex);
      }
      // Fall through to finally clause

    } catch (InterruptedException ex) {
      errStream.println(ex.getMessage());

      // Wait for threads to finish if they were interrupted.
      try {
        for (int i = 0; i < NUM_THREADS; i++) {
          if (threads[i] != null) {
            threads[i].join();
            threads[i] = null;
            outStream.println("thread [" + i + "] returned: " +
                              threadParms[i].returnValue);
          }
        }
      } catch (InterruptedException ex1) {
      }
      // Fall through to finally clause

    } finally {
      try {
        for (int i = 0; i < NUM_THREADS; i++) {
          if (threadParms[i].con != null && !threadParms[i].con.isClosed()) {
            // Rollback any transactions in case of errors
            if (retcode != 0) {
              try {
                outStream.println("\nEncountered error on a connection.  Rolling back all transactions");
                threadParms[i].con.rollback();
              } catch (SQLException ex) {
                reportSQLExceptions(ex);
              }
            }

            outStream.println("\nClosing connection [" + i + "]");
            threadParms[i].con.close();
            threadParms[i].con = null;
          }
        }
      } catch (SQLException ex) {
        reportSQLExceptions(ex);
      }
    }

    return retcode;
  }

  /**
   * This class is a thread to process a single order.
   * Through the threadParm object, it returns
   * 0 on success, 1 on failure, 2 on abort signal received
   */
  class ProcessOrderThread extends Thread
  {
    private int tid;
    private Connection con;
    private ThreadParameter threadParm;
    private PreparedStatement pInsOrd;
    private PreparedStatement pInsItm;
    private PreparedStatement pUpdInv;
    private PreparedStatement pSelInv;

    /**
     * Constructs the <code>ProcessOrderThread</code>
     *
     * @param threadParm the connection to the data store and its
     * associated prepared statements.
     */
    ProcessOrderThread(ThreadParameter threadParm)
    {
      this.threadParm = threadParm;
      this.tid = threadParm.tid;
      this.con = threadParm.con;
      this.pInsOrd = threadParm.pInsOrd;
      this.pInsItm = threadParm.pInsItm;
      this.pUpdInv = threadParm.pUpdInv;
      this.pSelInv = threadParm.pSelInv;
    }

    /**
     * Processes a single order
     */
    public void run()
    {
      int retcode = 1;    // Return code: Assume failure
      try {
        outStream.println("\n[" + tid + "] Processing data: " + threadParm.orderLine);
        String[] fields = threadParm.orderLine.split(",");
        pInsOrd.setInt(1, Integer.parseInt(fields[0]));  // cust_num
        pInsOrd.setTimestamp(2, Timestamp.valueOf(fields[1])); // when_placed
        pInsOrd.setNull(3, java.sql.Types.TIMESTAMP);   // when_shipped
        pInsOrd.setNull(4, java.sql.Types.VARCHAR);     // notes
        pInsOrd.executeUpdate();
        reportSQLWarnings(pInsOrd.getWarnings());

        // Process all the products that were ordered on the single line.
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
            errStream.println("[" + tid + "] Error: No inventory for pdocut number " +
                              prodNum);
            skipToNextOrder = true;
            break; // Break out of inner for loop
          }
          sleep(1000);
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
            errStream.println("[" + tid + "] Warning: Inventory for product number " +
                              prodNum + " is " + invCount + " Count.  " +
                              "\n\tCannot fill order for " + itemCount);
            skipToNextOrder = true;
            break; // Break out of for loop
          }

        } // end of for()

        numOrders++;

        /*
         * Finished processing a single line.
         * Now, rollback or commit?
         */
        if (skipToNextOrder || numOrders == 0) {
          if (skipToNextOrder == false) {
            errStream.println("[" + tid + "] Warning: No orders for any products were given.");
          }
          retcode = 1;
          // Rollback the order line
          outStream.println("[" + tid + "] Rolling back last transaction due to inability to process order");
          con.rollback();

        } else {
          // Commit the order
          con.commit();
        }

        retcode = 0;

      } catch (SQLException ex) {
        if (ex.getSQLState().equalsIgnoreCase("S0002")) {
          errStream.println("\n[" + tid + "] Error:  The table customer does not exists.\n\tPlease run ttIsql -f input0.dat to initialize the database.");
        } else if (ex.getErrorCode() == 907) {
          errStream.println("\n[" + tid + "] Error:  Attempting to insert a row with a duplicate primary key.\n\tPlease rerun ttIsql -f input0.dat to reinitialize the database.");
        } else {
          reportSQLExceptions(ex);
        }
        // Fall through to exiting thread

      } catch (InterruptedException ex) {
        retcode = 2;
        errStream.println(ex.getMessage());
        // Fall through to exiting thread

      } finally {
        threadParm.returnValue = retcode;
      }
    } // end of run()

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
