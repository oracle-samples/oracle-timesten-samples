/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * TTJdbcExamples.java demonstrates how to connect, disconnect, and
 * run some basic features of TimesTen, through the TimesTen JDBC
 * Driver.
 *
 * Usage: TTJdbcExamples [-h] [-t] [-c] [-run num] [[-dsn] dsnname]
 *      -h[elp]     Print this usage message and exit
 *      -t[race]    Turn on JDBC driver debug tracing
 *      -c[lient]   Connect using client/server (default is direct connect)
 *      -run num    Example number to run (Default is run all)
 *      -dsn        Name of the data store to connect to (optional)
 *
 *      Example number values:
 *      1    Loads appropriate driver for the connection type
 *      2    Connects to and disconnect from specified data store
 *      3    Shows operations performed by a typical application
 *      4    Shows how to change and print a query plan
 *      5    Shows how to use the batch update facility
 *
 *          Example command lines:
 *      java TTJdbcExamples
 *      java TTJdbcExamples sampledb
 *      java TTJdbcExamples -run 2 sampledb
 *      java TTJdbcExamples -client -run 2 -dsn sampledbCS
 *
 */

import java.io.*;
import java.text.*;
import java.sql.*;
import java.lang.ClassNotFoundException;
import java.lang.Integer;

/**
 * <code>TTJdbcExamples</code> uses the TimesTen JDBC Driver to
 * demonstrate how to connect, disconnect, and exercise various
 * database functionalities within TimesTen.
 */
public class TTJdbcExamples
{
  /**
   * The name of the TimesTen JDBC Driver (direct connection)
   */
  private static final String DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";

  /**
   * The connection prefix of the TimesTen JDBC Driver (direct connection)
   */
  private static final String DIRECT_CONNECT_PREFIX = "jdbc:timesten:direct:";

  /**
   * The name of the TimesTen JDBC Driver (client connection)
   */
  private static final String CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";

  /**
   * The connection prefix of the TimesTen JDBC Driver (client connection)
   */
  private static final String CLIENT_CONNECT_PREFIX = "jdbc:timesten:client:";

  // Static variables - used by main

  /**
   * Flag indicating whether a "signal" has been received.
   * This flag is set by our shutdown hook thread.
   */
  static boolean hasReceivedSignal = false;

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

  // Instance attributes - used by instances of the TTJdbcExamples object.

  /**
   * Name of driver to load
   */
  private String myDriver;

  /**
   * Connection url
   */
  private String myUrl;

  /**
   * Connection Password
   */
//  private String myPassword = null;
  static public String myPassword = null;


  /**
   * Connection Username
   */
//  private String myUsername = null;
   static public String myUsername = null;

  /**
   * What example to run
   */
  private int runWhat;

  /**
   * Flag is true if the driver has been loaded
   */
  private boolean isDriverLoaded;

  /**
   * Flag to indicate if a database connection is live
   */

  private boolean isConnected = false;


  // Methods

    //--------------------------------------------------
    //
    // Function: getConsoleString
    //
    // Description: Read a String from the console and return it
    //
    //--------------------------------------------------

    static private String getConsoleString (String what) {

      String temp = null;

      try {

        InputStreamReader isr = new InputStreamReader( System.in );
        BufferedReader     br = new BufferedReader( isr );
        temp                  = br.readLine();

      } catch (IOException e)
      {
        System.out.println();
        System.out.println("Problem reading the " + what);
        System.out.println();
      }

      return temp;
    }

    //--------------------------------------------------
    //
    // Function: getUsername
    //
    // Description: Assign the username
    //
    //--------------------------------------------------

    static private void getUsername() {

          // Default username for APPUSER
//        System.out.println();
//        System.out.print("Username: ");
//        TTJdbcExamples.myUsername = getConsoleString("Username");
        TTJdbcExamples.myUsername = "appuser";
    }

    //--------------------------------------------------
    //
    // Function: getPassword
    //
    // Description: Assign the password
    //
    //--------------------------------------------------

    static private void getPassword() {

        System.out.println();
        TTJdbcExamples.myPassword = PasswordField.readPassword("Enter password for 'appuser': ");
    }

  /**
   * Check if a "signal" is pending - i.e. the shutdown hook
   * thread has been entered.
   */
  private static boolean IsSignalPending()
  {
    return (hasReceivedSignal == true);
  }

  /**
   * Calls IsSignalPending() and throws an exception if true.
   */
  private static boolean CheckIfStopIsRequested()
    throws InterruptedException
  {
    if (IsSignalPending() == true) {
      throw new InterruptedException("We are being requested to stop!");
    }
    return false;
  }


  /**
   * Prints uage information to System.err stream
   */
  public static void usage()
  {
    System.err.println(
      "\nUsage: TTJdbcExamples [-h] [-t] [-c] [-run num] [[-dsn] dsnname]\n" +
      "    -h[elp]\tPrint this usage message and exit\n" +
      "    -t[race]\tTurn on JDBC driver debug tracing\n" +
      "    -c[lient]\tConnect using client/server (default is direct connect)\n" +
      "    -run num\tExample number to run (Default is run all)\n" +
      "    -dsn\tName of the database to connect to (optional)\n" +
      "\n    Example number values:\n" +
      "    1    Loads appropriate driver for the connection type\n" +
      "    2    Connects to and disconnect from specified data store\n" +
      "    3    Shows operations performed by a typical application\n" +
      "    4    Shows how to change and print a query plan\n" +
      "    5    Shows how to use the batch update facility\n" +
      "\n\tExample command lines:\n" +
      "    java TTJdbcExamples\n"+
      "    java TTJdbcExamples " + tt_version.sample_DSN + "\n"+
      "    java TTJdbcExamples -run 2 " + tt_version.sample_DSN + "\n" +
      "    java TTJdbcExamples -client -run 2 -dsn " + 
                tt_version.sample_DSN_CS );
  }

  /**
   * Start of program.
   *
   * Parses the arguments.
   * Instantiates the TTJdbcExamples object.
   * Runs the examples.
   */
  public static void main(String args[])
  {
    String dsnname = null;
    boolean isCSConn = false;
    boolean doTrace = false;
    int whatToRun = 0;
    int argInd = 0;

    // Iterate over all of the arguments
    for (argInd = 0; argInd < args.length; argInd++) {

      // Break out if argument does not start with '-'
      if (args[argInd].charAt(0) != '-') {
        break;
      }

      if (args[argInd].equalsIgnoreCase("-h") ||
          args[argInd].equalsIgnoreCase("-help")) {
        usage();
        System.exit(0);

      } else if (args[argInd].equalsIgnoreCase("-c") ||
                 args[argInd].equalsIgnoreCase("-client")) {
        isCSConn = true;

      } else if (args[argInd].equalsIgnoreCase("-t") ||
                 args[argInd].equalsIgnoreCase("-trace")) {
        doTrace = true;

      } else if (args[argInd].equals("-r") ||
                 args[argInd].equals("-run")) {
        argInd++;
        if (argInd >= args.length) {
          System.err.println("Error: -run option requires an argument");
          usage();
          System.exit(1);
        }

        try {
          whatToRun = Integer.parseInt(args[argInd]);
        } catch (NumberFormatException ex) {
          System.err.println("Error: -run option requires a number.");
          usage();
          System.exit(1);
        }

        if (whatToRun < 0 || whatToRun > 6) {
          System.err.println("Error: -run option requires a number between 1 and 6");
          usage();
          System.exit(1);
        }

      } else if (args[argInd].equalsIgnoreCase("-dsn")) {
        argInd++;
        if (argInd >= args.length) {
          System.err.println("Error: -dsn option requires an argument");
          usage();
          System.exit(1);
        }

        dsnname = args[argInd];

      } else {
        System.err.println("Error: unknown argument " + args[argInd]);
        usage();
        System.exit(1);
      }
    }

    // If there are any left over arguments, treat it as a dsnname;
    if (argInd < args.length) {
      if (dsnname != null) {
        System.err.println("Error: extra argument. Parsing stopped at " + args[argInd]);
        usage();
        System.exit(1);
      }
      dsnname = args[argInd];
      argInd++;
    }

    // If dsnname is not set at this point, then default it.
    if (dsnname == null || dsnname.length() == 0) {

      // Default the DSN
      dsnname = tt_version.sample_DSN;
    }

    // If there are any left over arguments, it is an error at this point.
    if (argInd < args.length) {
      System.err.println("Error: too many arguments. Parsing stopped at " + args[argInd]);
      usage();
      System.exit(1);
    }

    // Enable tracing in TimesTen JDBC driver for debugging
    if (doTrace) {
      DriverManager.setLogWriter(new PrintWriter(System.out, true));
    }

    System.out.println();
    System.out.println("Connecting to the database ...");

    // Prompt for the Username and Password
    getUsername();
    getPassword();

    // Construct the example-running object
    TTJdbcExamples examples;
    if (isCSConn) {
      examples = new TTJdbcExamples(CLIENT_DRIVER,
                                    CLIENT_CONNECT_PREFIX + dsnname,
                                    whatToRun);
    } else {
      examples = new TTJdbcExamples(DIRECT_DRIVER,
                                    DIRECT_CONNECT_PREFIX + dsnname,
                                    whatToRun);
    }

    // Use shutdown hooks to cleanly exit the application in
    // the event of a JVM shutdown notification
    stopMonitor = new Object();

    Runtime.getRuntime().addShutdownHook(new Thread("Shutdown thread") {
      public void run()
      {
        synchronized (stopMonitor) {
          if (shouldWait) {
            // System.out.println("Shutdown thread Waiting");
            hasReceivedSignal = true;
            try {
              // Wait for OK to proceed
              stopMonitor.wait();  // Might want have time limit.
            } catch (InterruptedException e) {
              e.printStackTrace();
            }
            // System.out.println("Leaving Shutdown thread");
          }
        }
      }
    });

    synchronized (stopMonitor) {
      shouldWait = true;
    }

    // Run the examples
    int exitCode = examples.runExamples();

    synchronized (stopMonitor) {
      shouldWait = false;
      stopMonitor.notify();
    }

    System.exit(exitCode);
  }

  /**
   * prints all attributes of a SQLException object to
   * System.err and all chained exceptions.
   */
  public static void reportSQLException(SQLException ex)
  {
    int errCount = 0;

    while (ex != null) {
      System.err.println ("\n\t----- SQL Error -----");

      System.err.println ("\tError Message: " + ex.getMessage ());
      if (errCount == 0) {
        ex.printStackTrace ();
      }

      System.err.println ("\tSQL State: " + ex.getSQLState ());
      System.err.println ("\tNative Error Code: " + ex.getErrorCode ());
      System.err.println ("");

      ex = ex.getNextException ();
      errCount ++;
    }
  }

  /**
   * prints all attributes of a SQLWarning object to
   * System.err and all chained warnings
   */
  public static void reportSQLWarning(SQLWarning wn)
  {
    int warnCount = 0;

    while (wn != null) {
      System.err.println ("\n\t----- SQL Warning -----");

      // Is this a regular SQLWarning object or a DataTruncation object?
      if (wn instanceof DataTruncation) {
        DataTruncation trn = (DataTruncation) wn;
        System.err.println ("\tData Truncation Warning: " + trn.getMessage ());
        if (warnCount == 0) {
          trn.printStackTrace ();
        }

        System.err.println ("\tSQL State: " + trn.getSQLState ());
        System.err.println ("\tNative Error Code: " + trn.getErrorCode ());

        System.err.println ("\n\tgetIndex (): " + trn.getIndex ());
        System.err.println ("\tgetParameter (): " + trn.getParameter ());
        System.err.println ("\tgetRead (): " + trn.getRead ());
        System.err.println ("\tgetDataSize (): " + trn.getDataSize ());
        System.err.println ("\tgetTransferSize (): " + trn.getTransferSize ());
      } else {
        System.err.println ("\tWarning Message: " + wn.getMessage ());
        if (warnCount == 0) {
          wn.printStackTrace ();
        }

        System.err.println ("\tSQL State: " + wn.getSQLState ());
        System.err.println ("\tNative Error Code: " + wn.getErrorCode ());
      }

      System.err.println ("");

      wn = wn.getNextWarning ();
      warnCount++;
    }
  }

  /**
   * Constructs a TTJdbcExamples object and initialize myDriver and myUrl
   */
  public TTJdbcExamples(String driver, String url, int runWhat)
  {
    this.myDriver  = driver;
    System.out.println();
    System.out.println("TTJdbcExamples.constructor: using DRIVER: " + myDriver);
    this.myUrl = url;
    System.out.println("TTJdbcExamples.constructor: using URL:    " + myUrl);
    System.out.println("TTJdbcExamples.constructor: using USER:   " + myUsername);
    System.out.println();

    this.runWhat = runWhat;

    this.isDriverLoaded = false;
  }

  /**
   * Runs the examples
   */
  public int runExamples()
  {
    int retCode = 0;

    try {
      switch(runWhat) {
      case 1:
        // If this method runs successfully that
        // means system is able to find the driver
        // classes and libraries
        loadDriver();
        break;

      case 2:
        // If this method runs successfully then TimesTen
        // is running and the DSN is configured correctly.
        connectAndDisconnect();
        break;

      case 3:
        // Demonstrates basic database operations
        elementaryOperations();
        break;

      case 4:
        // Demonstrates how to change and print a query plan
        changeQueryPlan();
        break;

      case 5:
        // Demonstrates how to use the batch update facility
        batchExecution();
        break;

      default:
        // run all the examples by default
        loadDriver();
        if (IsSignalPending() == true) break;
        connectAndDisconnect();
        if (IsSignalPending() == true) break;
        elementaryOperations();
        if (IsSignalPending() == true) break;
        changeQueryPlan();
        if (IsSignalPending() == true) break;
        batchExecution();
      }
      System.out.println("Done.");

    } catch (ClassNotFoundException ex) {
      retCode=2;

    } catch (SQLException ex) {
      retCode=3;
    }

    // Warning: if there is still a connection to the data store
    // at this point, then the data store will be invalidated
    // when this program exits.
    if (isConnected == true) {
      System.out.println("Warning: exiting without disconnecting from data store!");
    }

    return retCode;
  }

  /**
   * Demonstrates how to load a TimesTen JDBC Driver
   */
  public void loadDriver()
  {
    if (!isDriverLoaded) {
      System.out.println("TTJdbcExamples.loadDriver()");
      try {
        // Load TimesTen JDBC driver using Class.forName()
        Class.forName(this.myDriver);
        isDriverLoaded = true;
      } catch (ClassNotFoundException ex) {
        ex.printStackTrace();
      }
    }
  }


  /**
   * Demonstrates how to connect to and disconnect from TimesTen
   */
  public void connectAndDisconnect()
  {
    System.out.println("TTJdbcExamples.connectAndDisconnect()");

    // Load TimesTen JDBC driver if necessary
    loadDriver();

    Connection con = null;

    try {
      // Open a connection
      System.out.println("Open a connection.");
      isConnected = true;
      con = DriverManager.getConnection(myUrl, myUsername, myPassword);

      // Report any SQLWarnings on the connection
      reportSQLWarning(con.getWarnings());
      CheckIfStopIsRequested();

      // Fall through to con.close() in the finally clause

    } catch (SQLException ex) {
      reportSQLException(ex);
      // Fall through to con.close() in the finally clause

    } catch (InterruptedException ex) {
      System.err.println(ex.getMessage());
      // Fall through to con.close() in the finally clause

    } finally {
      try {
        if (con != null && con.isClosed() == false) {
          // Close the connection
          System.out.println("Close the connection.");
          con.close();
        }
        isConnected = false;
      }  catch (SQLException ex) {
        reportSQLException(ex);
      }
    }
  }

  /**
   * Demonstrates creating a table, creating an index, preparing
   * and end executing an insert, and a selecting from the table.
   */
  public void elementaryOperations()
  {
    int[] intValues = { 23125, 1125 };
    String[] strValues = { "John Smith", "Sam Jones" };

    System.out.println("TTJdbcExamples.elementaryOperations()");

    // Load TimesTen JDBC driver if necessary
    loadDriver();

    Connection con = null;

    try {
      // Open a connection
      System.out.println("Open a connection.");
      isConnected = true;
      con = DriverManager.getConnection(myUrl, myUsername, myPassword);

      // Report any SQLWarnings from the last call
      reportSQLWarning(con.getWarnings());
      CheckIfStopIsRequested();

      // Create a statement object
      System.out.println("Create a statement object.");
      Statement stmt = con.createStatement();

      // Create a table
      System.out.println("Creating table NameID.");
      stmt.executeUpdate
        ("CREATE TABLE NameID(id INTEGER, name VARCHAR(50))");

      // Report any SQLWarnings on the connection
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      // Create an index
      System.out.println("Creating index IxID.");
      stmt.executeUpdate("CREATE INDEX IxID ON NameID (id)");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      // Prepare a statement
      System.out.println("Preparing an insert statement.");
      PreparedStatement pstmt = con.prepareStatement
        ( "INSERT INTO NameID VALUES (?, ?)" );

      // Insert data using prepared statements
      System.out.println("Inserting data");
      for(int i=0; i<2; i++) {
        pstmt.setInt(1, intValues[i]);
        pstmt.setString(2, strValues[i]);
        pstmt.executeUpdate();

        // Report any SQLWarnings from the last call
        reportSQLWarning(pstmt.getWarnings());
        CheckIfStopIsRequested();
      }

      // Close the prepared statement
      System.out.println("Close the prepared statement.");
      pstmt.close();

      // Excute the select query
      System.out.println("Executing query");
      ResultSet rs = stmt.executeQuery("SELECT * FROM NameID");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      int numCols = rs.getMetaData().getColumnCount();

      // Fetch data
      System.out.println("Fetching data");
      System.out.println("Data:");
      while (rs.next()) {
        // Report any SQLWarnings from the last call
        reportSQLWarning(rs.getWarnings());
        CheckIfStopIsRequested();

        for (int i = 1; i <= numCols; i++) {
          System.out.print("\t"+rs.getObject(i));

          // Report any SQLWarnings from the last call
          reportSQLWarning(rs.getWarnings());
          CheckIfStopIsRequested();
        }
        System.out.println();
      }

      // Close the result set
      System.out.println("Close the result set.");
      rs.close();

      // Delete data
      System.out.println("Deleting data.");
      stmt.executeUpdate
        ("DELETE FROM NameID WHERE name LIKE 'S%' ");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      // Drop index
      System.out.println("Dropping index IxID.");
      stmt.executeUpdate("DROP INDEX IxID ");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();


      //Drop table
      System.out.println("Dropping table NameID.");
      stmt.executeUpdate("DROP TABLE NameID");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      // Close the statement object
      System.out.println("Close the statement.");
      stmt.close();

      // Fall through to con.close() in the finally clause

    } catch (SQLException ex) {
      reportSQLException(ex);
      // Fall through to con.close() in the finally clause

    } catch (InterruptedException ex) {
      System.err.println(ex.getMessage());
      // Fall through to con.close() in the finally clause

    } finally {
      try {
        if (con != null && con.isClosed() == false) {
          // Close the connection
          System.out.println("Close the connection.");
          con.close();
        }
        isConnected = false;
      } catch (SQLException ex) {
        reportSQLException(ex);
      }
    }
  }


  // ******************************************************************
  // Method:
  //    optimizerExample
  //
  // Description:
  //    shows how to influence the query optimizer
  //
  // ******************************************************************
  public void changeQueryPlan()
    throws ClassNotFoundException, SQLException
  {
    boolean inXact = false;
    Connection con = null;
    Statement stmt = null;
    PreparedStatement pStmt1 = null;
    PreparedStatement pStmt2 = null;
    CallableStatement cStmt = null;
    ResultSet rs = null;
    ResultSet rsPlan = null;
    int numCols = 0;

    System.out.println("\nChange and Print a Query Plan");
    System.out.println(  "-----------------------------");

    // Load driver
    loadDriver();

    try {
      // Connect to a TimesTen DSN
      System.out.println("Open a connection.");
      isConnected = true;
      con = DriverManager.getConnection(myUrl, myUsername, myPassword);


      // Report any SQLWarnings from the last call
      reportSQLWarning(con.getWarnings());
      CheckIfStopIsRequested();

      con.setAutoCommit(false);
      inXact = true;

      // create tables
      stmt = con.createStatement();

      System.out.println("Create tables.");
      stmt.executeUpdate("CREATE TABLE Tbl1(key INT)");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      stmt.executeUpdate("CREATE TABLE Tbl2(key INT)");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      // create indexes
      System.out.println("Create indexes.");
      stmt.executeUpdate("CREATE UNIQUE INDEX ixTbl1 ON Tbl1(key)");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      stmt.executeUpdate("CREATE UNIQUE INDEX ixTbl2 ON Tbl2(key)");

      // Report any SQLWarnings from the last call
      reportSQLWarning(stmt.getWarnings());
      CheckIfStopIsRequested();

      System.out.println("Prepare insert statements.");
      pStmt1=con.prepareStatement("INSERT INTO Tbl1 VALUES (?)");
      pStmt2=con.prepareStatement("INSERT INTO Tbl2 VALUES (?)");

      // load data
      System.out.println("Loading the data.");
      for (int i=0; i<1000; i++) {
        pStmt1.setInt(1, i);
        pStmt1.executeUpdate();

        // Report any SQLWarnings from the last call
        reportSQLWarning(pStmt1.getWarnings());
        CheckIfStopIsRequested();

        pStmt2.setInt(1, i);
        pStmt2.executeUpdate();

        // Report any SQLWarnings from the last call
        reportSQLWarning(pStmt2.getWarnings());
        CheckIfStopIsRequested();
      }

      // prepare a call to disbale merge join
      System.out.println("Turn off MergeJoin.");
      cStmt = con.prepareCall("{CALL ttOptSetFlag('MergeJoin', 0)}");
      cStmt.execute();

      // Report any SQLWarnings from the last call
      reportSQLWarning(cStmt.getWarnings());
      CheckIfStopIsRequested();

      // select from the table
      System.out.println("Prepare select from plan.");
      pStmt2=con.prepareStatement("SELECT * FROM sys.plan");

      System.out.println("Turn on plan generator.");
      cStmt = con.prepareCall("{ CALL ttOptSetFlag('GenPlan', 1)}");
      cStmt.execute();

      System.out.println("Prepare the join query.");
      pStmt1=con.prepareStatement("SELECT * FROM Tbl1, Tbl2 "+
                                  "WHERE Tbl1.key = Tbl2.key");

      System.out.println("Execute plan query.");
      rsPlan = pStmt2.executeQuery();

      // Report any SQLWarnings from the last call
      reportSQLWarning(pStmt2.getWarnings());
      CheckIfStopIsRequested();

      rs = pStmt1.executeQuery();

      // Report any SQLWarnings from the last call
      reportSQLWarning(pStmt1.getWarnings());
      CheckIfStopIsRequested();

      numCols=rsPlan.getMetaData().getColumnCount();

      System.out.println("\nThe query optimizer plan " +
                         "for the join query:");
      while(rsPlan.next()) {

        // Report any SQLWarnings from the last call
        reportSQLWarning(rsPlan.getWarnings());
        CheckIfStopIsRequested();

        for (int i = 1; i <= numCols; i++) {
          System.out.print(rsPlan.getObject(i)+"\t");

          // Report any SQLWarnings from the last call
          reportSQLWarning(rsPlan.getWarnings());
          CheckIfStopIsRequested();
        }
        System.out.println();
      }
      System.out.println();

      // fetch the next row from the result set
      int rowCt=0;
      while(rs.next()) {
        rowCt++;
        CheckIfStopIsRequested();
      }
      System.out.println("Number of rows from select query = "+rowCt);

      con.commit();
      inXact = false;

      System.out.println("Close objects.");
      rsPlan.close();
      rs.close();
      pStmt1.close();
      pStmt2.close();
      cStmt.close();
      CheckIfStopIsRequested();

      inXact = true;
      stmt.executeUpdate("DROP TABLE Tbl1");
      stmt.executeUpdate("DROP TABLE Tbl2");
      con.commit();
      inXact = false;

      stmt.close();

    } catch (SQLException ex) {
      reportSQLException(ex);
    } catch (InterruptedException ex) {
      System.err.println(ex.getMessage());
      // Fall through to con.close() below
    } finally {
      try {
        if (con != null && con.isClosed() == false) {
          if (inXact) {
            System.out.println ("\nRoll back uncommitted transaction.");
            con.rollback();
            inXact = false;
          }
          System.out.println("Close the connection.");
          con.close();
        }
        isConnected = false;
      } catch (SQLException ex) {
        reportSQLException(ex);
      }
    }
  }

  // ******************************************************************
  // Method:
  //    batchExecution
  //
  // Description:
  //    shows how to use the batch update facility
  //
  // ******************************************************************
  public void batchExecution()
    throws ClassNotFoundException, SQLException
  {
    boolean inXact = false;
    Connection con = null;
    Statement stmt = null;
    PreparedStatement pstmt = null;
    ResultSet rs = null;
    String sql = null;
    int param = 0;
    int updateCount [] = null;


    System.out.println ("\nExecute Statement and PreparedStatement Batch Updates");
    System.out.println (  "-----------------------------------------------------");

    // Load driver
    loadDriver();

    try {
      // Connect to a TimesTen DSN
      System.out.println ("Open a connection.");
      isConnected = true;
      con = DriverManager.getConnection(myUrl, myUsername, myPassword);

      // Report any SQLWarnings from the last call
      reportSQLWarning (con.getWarnings ());
      CheckIfStopIsRequested();

      con.setAutoCommit (false);
      inXact = true;

      // execute a Statement batch
      System.out.println ("\nExecuting Statement batch...\n");

      stmt = con.createStatement ();

      sql = "CREATE TABLE BATCH (C1 INTEGER)";
      System.out.println ("Adding SQL: " + sql);
      stmt.addBatch (sql);

      sql = "INSERT INTO BATCH VALUES (1)";
      System.out.println ("Adding SQL: " + sql);
      stmt.addBatch (sql);

      sql = "INSERT INTO BATCH VALUES (2)";
      System.out.println ("Adding SQL: " + sql);
      stmt.addBatch (sql);

      sql = "INSERT INTO BATCH VALUES (3)";
      System.out.println ("Adding SQL: " + sql);
      stmt.addBatch (sql);

      sql = "INSERT INTO BATCH VALUES (4)";
      System.out.println ("Adding SQL: " + sql);
      stmt.addBatch (sql);

      sql = "UPDATE BATCH SET C1 = C1 + 1";
      System.out.println ("Adding SQL: " + sql);
      stmt.addBatch (sql);

      // execute the Statement batch
      updateCount = stmt.executeBatch ();
      con.commit ();
      inXact = false;
      CheckIfStopIsRequested();

      System.out.println ("\nStatement execution update count: ");

      for (int index = 0; index < updateCount.length; index ++) {
        if (updateCount [index] == -2) {
          System.out.println ("Statement " + index + " returned " +
                              "Statement.SUCCESS_NO_INFO.");
        } else if (updateCount [index] == -3) {
          System.out.println ("Statement " + index + " returned " +
                              "Statement.EXECUTE_FAILED.");
        } else {
          System.out.println ("Statement " + index + " affected " +
                              updateCount [index] + " row(s).");
        }
      }


      // execute a PreparedStatement batch
      System.out.println ("\nExecuting PreparedStatement batch...\n");

      sql = "DELETE FROM BATCH WHERE C1 = ?";
      System.out.println ("Preparing SQL: " + sql);

      inXact = true;
      pstmt = con.prepareStatement (sql);

      param = 2;
      System.out.println ("Adding int parameter: " + param);
      pstmt.setInt (1, param);
      pstmt.addBatch ();

      param = 3;
      System.out.println ("Adding int parameter: " + param);
      pstmt.setInt (1, param);
      pstmt.addBatch ();

      param = 4;
      System.out.println ("Adding int parameter: " + param);
      pstmt.setInt (1, param);
      pstmt.addBatch ();

      param = 5;
      System.out.println ("Adding int parameter: " + param);
      pstmt.setInt (1, param);
      pstmt.addBatch ();


      // execute the PreparedStatement batch
      updateCount = pstmt.executeBatch ();
      con.commit ();
      inXact = false;
      CheckIfStopIsRequested();

      System.out.println ("\nPreparedStatement execution update count: ");

      for (int index = 0; index < updateCount.length; index ++) {
        if (updateCount [index] == -2) {
          System.out.println ("Statement " + index + " returned " +
                              "Statement.SUCCESS_NO_INFO.");
        } else if (updateCount [index] == -3) {
          System.out.println ("Statement " + index + " returned " +
                              "Statement.EXECUTE_FAILED.");
        } else {
          System.out.println ("Statement " + index + " affected " +
                              updateCount [index] + " row(s).");
        }
      }

    } catch (BatchUpdateException ex) {
      reportSQLException (ex);
    } catch (SQLException ex) {
      reportSQLException (ex);
    } catch (InterruptedException ex) {
      System.err.println(ex.getMessage());
      // Fall through to con.close() below
    }

    try {
      if (con != null && con.isClosed() == false) {
        if (inXact) {
          System.out.println ("\nRoll back uncommitted transaction.");
          con.rollback();
          inXact = false;
        }
        System.out.println ("\nDrop table.");
        stmt = con.createStatement ();
        inXact = true;
        stmt.executeUpdate ("DROP TABLE BATCH");
        con.commit ();
        inXact = false;
      }
    } catch (SQLException ex) {
      reportSQLException (ex);
    } finally {
      try {
        if (con != null && con.isClosed() == false) {
          if (inXact) {
            System.out.println ("\nRoll back uncommitted transaction.");
            con.rollback();
            inXact = false;
          }
          System.out.println("Close the connection.");
          con.close();
        }
        isConnected = false;
      } catch (SQLException ex) {
        reportSQLException(ex);
      }
    }
  }

}
