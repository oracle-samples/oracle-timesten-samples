/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

//-----------------------------------------------------------
// 
// plsqlJDBC.java
// 
// Call PLSQL procedure and function from JDBC
// 
//----------------------------------------------------------
import java.sql.*;
import java.io.*;
import java.text.*;
import java.lang.*;
import java.util.*;
import com.timesten.jdbc.*;

class plsqlJDBC
{

  // class constants
  static int JDBCPLSQL_ORACLE = 0;
  static int JDBCPLSQL_TIMESTEN = 1;

  //---------------------------------------------------
  // class variables
  // All set to a default value, the default values
  // can be overwritten using command line options
  //---------------------------------------------------

  // Used to turn the Jdbc tracing ON
  static public boolean trace = false;

  // JDBC URL string
  static public String url = null;

  // JDBC defaultDSN string
  static public String defaultDSN = "sampledb";

  // JDBC user string
  static public String username = null;

  // JDBC password string
  static public String password = null;

  // DBMS type
  static public int dbms = JDBCPLSQL_TIMESTEN;

  // Callable PL/SQL statement for plsql procedure callProc
  static public CallableStatement callProc = null;

  // Callable PL/SQL statement for plsql function callFunc
  static public CallableStatement callFunc = null;

  // Callable PL/SQL statement for plsql anonymous block
  static public CallableStatement anonBlock = null;

  // Callable PL/SQL statement for plsql procedure openRefCursor
  static public CallableStatement openRefCursor = null;

  // connection for the thread
  static public Connection connection;


  //--------------------------------------------------
  // class constants
  //--------------------------------------------------

  public static final String exec_plsql_func = 
    "begin " + 
       ":luckyEmp := sample_pkg.getEmpName(:empNo, :errCode, :errText); " + 
    "end;";

  public static final String exec_plsql_proc = 
    "begin " + 
       "emp_pkg.givePayRaise(:numEmps, :luckyEmp, :errCode, :errText); " + 
    "end;";

  public static final String exec_OpenSalesPeopleCursor = 
    "begin " + 
        "emp_pkg.OpenSalesPeopleCursor(:cmdRefCursor, :errCode, :errText); " + 
    "end;";

  public static final String exec_anon_block = 
    "begin " + 

       "SELECT job, hiredate, sal, deptno " + 
       "INTO :jobtype, :hired, :salary, :dept " + 
       "FROM emp " + 
       "WHERE ename = UPPER(:luckyEmp); " + 

       "SELECT count(*) " + 
       "INTO :worked_longer " + 
       "FROM emp " + 
       "WHERE hiredate < :hired; " + 

       "SELECT count(*) " + 
       "INTO :higher_sal " + 
       "FROM emp " + 
       "WHERE sal > :salary; " + 

       "SELECT count(*) " + 
       "INTO :total_in_dept " + 
       "FROM emp " + 
       "WHERE deptno = :dept; " + 

       ":errCode := 0; " + ":errText := 'OK'; " + 

       " EXCEPTION " + 
       "    WHEN OTHERS THEN " + 
       "       :errCode  := SQLCODE; " + 
       "       :errText  := SUBSTR(SQLERRM, 1, 200); " + 
    "end;";



  //--------------------------------------------------
  // Function: main
  //--------------------------------------------------

  public static void main(String arg[])
  {

    Connection conn = null;

    // parse arguments
    parseArgs(arg);

    // Turn the JDBC tracing on
    if (plsqlJDBC.trace)
      DriverManager.setLogWriter(new PrintWriter(System.out, true));

    // Load the JDBC driver
    try
    {
      if (dbms == JDBCPLSQL_ORACLE)
      {
        Class.forName("oracle.jdbc.OracleDriver");
      }
      else
      {
        Class.forName("com.timesten.jdbc.TimesTenDriver");
        Class.forName("com.timesten.jdbc.TimesTenClientDriver");
      }

    }
    catch (java.lang.ClassNotFoundException e)
    {
      System.err.println(e.getMessage());
    }

    System.out.println();
    System.out.println("Connecting to the database");


    //Prompt for the Username and Password
    AccessControl ac = new AccessControl();
    username = ac.getUsername();
    password = ac.getPassword();

    System.out.println("Connected");

    // Connect and prepare the PLSQL block
    initialize();

    // Execute the PLSQL
    execute();

    // De-allocate resources
    cleanup();
  }


  //--------------------------------------------------
  // usage()
  //--------------------------------------------------
  static private void usage()
  {

    System.err.print(
      "\n" + 
      "Usage: plsqlJDBC [<-url url_string>] " + 
      "[-dbms <dbms_name>]" + 
      "[-h] [-help] [?]\n\n" + 
      "  -h                     Prints this message and exits.\n" + 
      "  -help                  Same as -h.\n" + 
      "  -url <url_string>      Specifies JDBC url string of the database\n" + 
      "                         to connect to\n" + 
      "  -dbms   <dbms_name>    Use timesten/oracle. Timesten is default\n");
    System.exit(1);
  }

  //--------------------------------------------------
  //
  // Function: getConsoleString
  //
  // Description: Read a String from the console and return it
  //
  //--------------------------------------------------

  static private String getConsoleString(String what)
  {

    String temp = null;

    try
    {

      InputStreamReader isr = new InputStreamReader(System.in);
      BufferedReader br = new BufferedReader(isr);
      temp = br.readLine();

    }
    catch (IOException e)
    {
      System.out.println();
      System.out.println("Problem reading the " + what);
      System.out.println();
    }

    return temp;
  }


  //--------------------------------------------------
  //
  // Function: parseArgs
  // 
  // Parses command line arguments
  //
  //--------------------------------------------------
  private static void parseArgs(String[] args)
  {

    int i = 0;

    //	System.out.println("args.length: " + args.length);	
    while (i < args.length)
    {

      // Command line help
      if (args[i].equalsIgnoreCase("-h") || args[i].equalsIgnoreCase("-help"))
      {
        usage();
      }

      // JDBC url string
      else if (args[i].equalsIgnoreCase("-url"))
      {
        if (i > args.length)
        {
          usage();
        }

        url = args[i + 1];
        i += 2;
      }

      else if (args[i].equalsIgnoreCase("-dbms"))
      {
        if (i > args.length)
        {
          usage();
        }
        if (args[i + 1].equalsIgnoreCase("oracle"))
          dbms = JDBCPLSQL_ORACLE;
        i += 2;
      }

      // JDBC tracing
      else if (args[i].equalsIgnoreCase("-trace"))
      {
        if (i > args.length)
        {
          usage();
        }
        trace = true;
        i += 1;
      }

      else
      {
        usage();
      }
    }


    if (url == null)
    {

      // Default the URL
      url = "jdbc:timesten:" + defaultDSN;
    }

  }


  public static void printSQLException(SQLException ex)
  {
    for (; ex != null; ex = ex.getNextException())
    {
      System.out.println(ex.getMessage());
      System.out.println("Vendor Error Code: " + ex.getErrorCode());
      System.out.println("SQL State: " + ex.getSQLState());
      ex.printStackTrace();
    }
  }




  //--------------------------------------------------
  // Method : execute 
  // Execute the PLSQL routine
  //--------------------------------------------------
  public static void execute()
  {

    String luckyEmp = null;
    String hired = null;
    String job = null;
    int empNo = 0;
    int dept = 0;
    double salary = 0.0;
    int numEmps = 0;
    int worked_longer = 0;
    int higher_sal = 0;
    int total_in_dept = 0;
    int errCode = 0;
    String errText = null;
    ResultSet cursor = null;

    try
    {

      // Setup the IN and OUT parameters for the procedure emp_pkg.givePayRaise
      numEmps = 10;
      errCode = 0;
      errText = "OK";
      callProc.setInt(1, numEmps); // Bind input param
      callProc.registerOutParameter(2, Types.VARCHAR); // the return value
      callProc.registerOutParameter(3, Types.INTEGER); // errCode
      callProc.registerOutParameter(4, Types.VARCHAR); // errText

      if (plsqlJDBC.trace)
      {
        System.out.println("excuting PL/SQL procedure emp_pkg.givePayRaise");
      }

      callProc.executeUpdate();

      luckyEmp = callProc.getString(2);

      System.out.println();
      System.out.println("The employee who got the 10% pay raise was " + luckyEmp);
      System.out.println();


      // Setup the IN and OUT parameters for the anonymous PLSQL block
      anonBlock.registerOutParameter(1, Types.VARCHAR); // the jobtype
      anonBlock.registerOutParameter(2, Types.VARCHAR); // the hire date
      anonBlock.registerOutParameter(3, Types.DOUBLE); // the salary
      anonBlock.registerOutParameter(4, Types.INTEGER); // the deptartment
      anonBlock.setString(5, luckyEmp); // Input empName
      anonBlock.registerOutParameter(6, Types.INTEGER); // worked longer
      anonBlock.registerOutParameter(7, Types.INTEGER); // Higer salary
      anonBlock.registerOutParameter(8, Types.INTEGER); // Number in department
      anonBlock.registerOutParameter(9, Types.INTEGER); // errCode
      anonBlock.registerOutParameter(10, Types.VARCHAR); // errText
      errCode = 0;
      errText = "OK";

      if (plsqlJDBC.trace)
      {
        System.out.println("executing PL/SQL anonymous block");
      }

      anonBlock.executeUpdate();

      errCode = anonBlock.getInt(9);

      if (0 == errCode)
      {

        job = anonBlock.getString(1);
        hired = anonBlock.getString(2);
        salary = anonBlock.getDouble(3);
        dept = anonBlock.getInt(4);
        worked_longer = anonBlock.getInt(6);
        higher_sal = anonBlock.getInt(7);
        total_in_dept = anonBlock.getInt(8);

        System.out.printf("Name:        %s\n", luckyEmp);
        System.out.printf("Job:         %s\n", job);
        System.out.printf("Hired:       %s\t(%2d people have served longer).\n", 
                          hired, worked_longer);

        System.out.printf("Salary:      $%.2f\t\t\t(%2d people have a higher salary).\n", 
                          salary, higher_sal);

        System.out.printf("Department:  %d\t\t\t\t(%2d people in the department).\n\n", 
                          dept, total_in_dept);

      }
      else
      {

        System.out.println("Problem with anonymous block");
        System.out.println("Error code : " + errCode);
        System.out.println("Error text : " + anonBlock.getString(10));
        System.out.println();
      }



      // Setup the IN and OUT parameters for the function sample_pkg.getEmpName
      empNo = 7839;
      callFunc.registerOutParameter(1, Types.VARCHAR); // the return value
      callFunc.setInt(2, empNo); // Bind 2nd parameter
      callFunc.registerOutParameter(3, Types.INTEGER); // errCode
      callFunc.registerOutParameter(4, Types.VARCHAR); // errText

      if (plsqlJDBC.trace)
      {
        System.out.println("excuting PL/SQL function sample_pkg.getEmpName");
      }

      callFunc.executeUpdate();
      System.out.println("Employee name for empid " + empNo + " is : " + callFunc.getString(1));
      System.out.println();


      // Setup the OUT parameters for the procedure emp_pkg.OpenSalesPeopleCursor
      openRefCursor.registerOutParameter(1, TimesTenTypes.CURSOR); // the return value
      openRefCursor.registerOutParameter(2, Types.INTEGER); // errCode
      openRefCursor.registerOutParameter(3, Types.VARCHAR); // errText

      if (plsqlJDBC.trace)
      {
        System.out.println("excuting PL/SQL procedure emp_pkg.OpenSalesPeopleCursor");
      }

      openRefCursor.executeUpdate();

      cursor = ((TimesTenCallableStatement)openRefCursor).getCursor(1);

      if (null != cursor)
      {

        /* Iterate over the ref cursors result set */
        System.out.println("The sales people are:");
        System.out.println();
        System.out.println("  EMPNO ENAME      SALARY");
        System.out.println("  ===== ========== ==========");

        while (cursor.next())
        {

          empNo = cursor.getInt(1);
          luckyEmp = cursor.getString(2);
          salary = cursor.getDouble(3);

          System.out.printf("  %5d %-10s %10.2f\n", empNo, luckyEmp, salary);
        }

        cursor.close();
        openRefCursor.close();

        System.out.println();
      }

    }
    catch (SQLException e)
    {
      printSQLException(e);
    }
  }

  //--------------------------------------------------
  // Method: initialize
  // Gets the connection for the thread and prepares
  // all the statements for the thread so it is 
  // ready to execute
  //--------------------------------------------------
  static private void initialize()
  {

    try
    {

      // Connect to the data store
      if (plsqlJDBC.trace)
        System.out.println("Connecting");

      if (plsqlJDBC.dbms == plsqlJDBC.JDBCPLSQL_ORACLE)
      {
        connection = DriverManager.getConnection(plsqlJDBC.url, plsqlJDBC.username, plsqlJDBC.password);
      }
      else
      {
        connection = DriverManager.getConnection(plsqlJDBC.url, plsqlJDBC.username, plsqlJDBC.password);
      }

      // set autocommit off
      connection.setAutoCommit(false);

      connection.commit();
      if (plsqlJDBC.trace)
        System.out.println("Preparing PLSQL calls");

      callFunc = connection.prepareCall(plsqlJDBC.exec_plsql_func);
      callProc = connection.prepareCall(plsqlJDBC.exec_plsql_proc);
      openRefCursor = connection.prepareCall(plsqlJDBC.exec_OpenSalesPeopleCursor);
      anonBlock = connection.prepareCall(plsqlJDBC.exec_anon_block);

      connection.commit();

    }
    catch (SQLException e)
    {
      printSQLException(e);
    }

  }

  //--------------------------------------------------
  // method: cleanup
  // closes all the prepared statements and the connection
  //--------------------------------------------------

  static public void cleanup()
  {
    try
    {
      // commit outstanding activity to avoid function sequence error
      connection.commit();

      if (plsqlJDBC.trace)
        System.out.println("Closing statements");

      callFunc.close();

      System.out.println("Disconnecting");
      connection.close();
      System.out.println("Disconnected");
      System.out.println();
    }
    catch (SQLException e)
    {
      plsqlJDBC.printSQLException(e);
    }
  }

} /* plsqlJDBC */
