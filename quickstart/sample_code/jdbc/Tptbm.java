/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */

//-----------------------------------------------------------
// 
// Tptbm.java
// 
// Java version of Tptbm benchmark for measuring scalability 
// 
//----------------------------------------------------------
import java.sql.*;
import java.io.*;
import java.text.*;
import java.lang.*;
import java.util.*; 
import com.timesten.jdbc.*;

class Tptbm
{
    // Compilation control
    static public final boolean enableScaleout = false;

    // class constants
    static public final int TPTBM_NONE = -1;
    static public final int TPTBM_ORACLE = 0;
    static public final int TPTBM_TIMESTEN = 1;
    static public final String dfltUsername = "appuser";
    static public final int assumedTPS = 100000;
    static public final int minHashPages = 40;
    static public final int dfltXacts = 10000;
    static public final int dfltRamptime = 10;
    static public final int dbModeId = -1;
    static public final int dbModeNb = -1;
    static public final int modeClassic = 0;
    static public final int modeScaleout = 1;
    static public final int modeScaleoutLocal = 2;
    static public final int modeScaleoutRouting = 3;
    static public final String dbModeClassicS = "CLASSIC";
    static public final String dbModeScaleoutS = "SCALEOUT";

    //---------------------------------------------------
    // class variables
    // All set to a default value, the default values
    // can be overwritten using command line options
    //---------------------------------------------------

    // Number of threads to be run
    static public int numThreads = 1;

    // Seed for the random number generator
    static public int seed = 1;

    // Number of transactions each thread runs
    static public int numXacts = 0;

    // Test duration (measured)
    static public int testDuration = 0;

    // Ramp time in duration mode
    static public int rampTime = -1;

    // Percentage of read transactions
    static public int  reads = 80;

    // Percentage of insert transactions
    static public int  inserts = 0;

    // Percentage of insert transactions
    static public int updates = 0;

    // Percentage of delete transactions
    static public int deletes = 0;

    // key is used to determine how many 
    // records to populate in the database.
    // key**2 records are initially inserted.
    static public int  key = 100;

    // Min. SQL ops per transaction 
    static public int  min_xact = 1;

    // Max. SQL ops per transaction
    static public int  max_xact = 1;

    // 1 insert 3 selects 1 update / xact 
    static public int multiop = 0;

    // Used to turn the Jdbc tracing ON
    static public boolean trace = false;

    // Used to turn the thread tracing in 
    // this application ON
    static public boolean threadtrace= false;

    // Used to specify if database does 
    // not have to be populated
    static public boolean nobuild = false;
    static public boolean buildonly = false;

    // Should we commit readonly transactions?
    static public boolean commitReads = false;

    // Used to turn on PrefetchClose ON with client/server
    static public boolean prefetchClose = false;

    // Verbose mode (undocumented)
    static public boolean verbose = false;

    // JDBC URL string
    static public String url = null;
    static public String urlUID = null;
    static public String urlPWD = null;

    // JDBC defaultDSN string
    static public String defaultDSN = tt_version.sample_DSN;

    // JDBC user string
    static public String username = null;

    // JDBC password string
    static public String password = null;

    // DBMS type
    static public int dbms = TPTBM_NONE;

    // did the user specify the '-key' parameter
    static public boolean fkey = false;

    // Range Index
    public static boolean range = false;

    // PL/SQL
    public static boolean usePlsql = false;

    // Indicates if we are running in C/S mode
    public static boolean isCS = false;

   // Indicates if we are connected to a Scaleout database
   public static boolean isScaleout = false;

    // Run mode
    public static int runMode = modeClassic;

    // TimesTen Data source
    public static TimesTenDataSource ttds = null;

    // Total number of replica sets
    public static int numReplicaSets = 0;

    //--------------------------------------------------
    // More class constants
    //--------------------------------------------------

    public static final String selDbType =
        "SELECT paramvalue FROM v$configuration WHERE paramname = 'TTGrid'";

    public static final String selDbMode =
        "SELECT descr FROM VPN_USERS WHERE vpn_id = " + dbModeId + " AND vpn_nb = " + dbModeNb;

    public static final String selNumRs = 
        "select count(distinct(repset)) from v$distribution_current";

    public static final String selRsIds = 
        "select distinct(repset) rs from v$distribution_current order by rs";

    public static final String insertStmt = 
	"insert into vpn_users values (?,?,?,'0000000000',?)";

    public static final String deleteStmt = 
	"delete from vpn_users where vpn_id = ? and vpn_nb = ?";

    //remember to give pages parameter for this statement, 
    // its calculated based on key
    private static final String createStmtHash = 
	"CREATE TABLE vpn_users(" +
	"vpn_id             TT_INTEGER   NOT NULL," + 
	"vpn_nb             TT_INTEGER   NOT NULL," +
	"directory_nb       CHAR(10)  NOT NULL," +
	"last_calling_party CHAR(10)  NOT NULL," +
	"descr              CHAR(100) NOT NULL," +
	"PRIMARY KEY (vpn_id,vpn_nb)) unique hash on (vpn_id,vpn_nb) pages = ";

    private static final String createStmtRange = 
	"CREATE TABLE vpn_users(" +
	"vpn_id             TT_INTEGER   NOT NULL," + 
	"vpn_nb             TT_INTEGER   NOT NULL," +
	"directory_nb       CHAR(10)  NOT NULL," +
	"last_calling_party CHAR(10)  NOT NULL," +
	"descr              CHAR(100) NOT NULL," +
	"PRIMARY KEY (vpn_id,vpn_nb))";

    private static final String distClause = 
        " distribute by hash(vpn_id, vpn_nb)";

    public static final String exec_plsql_stmnt =
    "DECLARE " +
    "key_cnt number(10); " +
    "ins_id number(10); " +
    "ins_nb number(10); " +
    "multiop number(10); " +
    "out_last_calling_party char(10); " +
    "sel_directory_nb char(10); " +
    "sel_last_calling_party char(10); " +
    "sel_descr char(100); " +
    "ins_dict char(10); " +
    "ins_descr char(100); " +
    "rand_id number; " +
    "rand_nb number; " +
    "rand_last char(10); " +
    "begin " +
    "key_cnt := :kc; " +
    "ins_id := :id; " +
    "ins_nb := :nb; " +
    "multiop := :multiop; " +
    "ins_dict := '55' || ins_id || ins_nb; " +
    "ins_descr := '<place holder for description of VPN %d extension %d>' || ins_id || ins_nb; " +
    "insert into vpn_users values (ins_id, ins_nb, ins_dict, '0000000000', ins_descr); " +
    "for i in 1..3 loop " +
    "rand_id := trunc(dbms_random.value(0, key_cnt)); " +
    "rand_nb := trunc(dbms_random.value(0, key_cnt)); " +
    "select directory_nb, last_calling_party, descr " +
    "into sel_directory_nb, sel_last_calling_party, sel_descr " +
    "from vpn_users " +
    "where vpn_id = rand_id and vpn_nb = rand_nb " +
    "and rownum <= 1; " +
    "end loop; " +
    "rand_id := trunc(dbms_random.value(0, key_cnt)); " +
    "rand_nb := trunc(dbms_random.value(0, key_cnt)); " +
    "dbms_output.put_line('id: ' || rand_id || '  nb: ' || rand_nb); " +
    "out_last_calling_party := rand_id || 'X' || rand_nb; " +
    "update vpn_users set last_calling_party = out_last_calling_party " +
    "where vpn_id = rand_id and vpn_nb = rand_nb; " +
    "if multiop = 2 then " +
    "delete from vpn_users " +
    "where vpn_id = ins_id and vpn_nb = ins_nb; " +
    "end if; " +
    "commit; " +
    "end;";

    public static final String init_plsql_stmnt =
	"declare " +
	"seed binary_integer; " +
	"begin " +
	"dbms_random.initialize(seed); " +
	"dbms_random.seed(seed); " +
	"end;";

    //--------------------------------------------------
    // Function: main
    // Populates the database and instantiate a 
    // TptbmThreadController object
    //--------------------------------------------------
    
    public static void main(String arg[]) 
    {

	TptbmThreadController controller;
	Connection conn = null;
	long elapsedTime;
        long totalXacts = 0;
        long totalLocates = 0;
	long tps;
        int tno;
        boolean error = false;
	
	// parse arguments
	parseArgs(arg);

	// Turn the JDBC tracing on
	if(Tptbm.trace)
	    DriverManager.setLogWriter(new PrintWriter(System.out, true));
	
	// Load the JDBC driver
	try {
	    if(dbms == TPTBM_ORACLE) {
		Class.forName("oracle.jdbc.OracleDriver");
	    }
	    else {
                ttds = new TimesTenDataSource();
	    }
	} catch (java.lang.ClassNotFoundException e) {
	    System.err.println(e.getMessage());
            System.exit(2);
	}

        System.out.println();
        System.out.println("Connecting to the database ...");

        // Prompt for the Username and Password
        getUsername();
        getPassword();

	// Connect to the database
        try {
            if (  dbms == TPTBM_ORACLE  ) {
	        conn = DriverManager.getConnection(Tptbm.url, username, password);
            } else {
                ttds.setUrl(Tptbm.url);
                ttds.setUser(username);
                ttds.setPassword(password);
                conn = ttds.getConnection();

                // check database type
                isScaleout = getDbType(conn);
                if (  (runMode > modeClassic) && !isScaleout  ) {
                    System.err.println("\n'-scaleout' was specified but database is classic\n");
		    conn.close();
		    System.exit(1);
                }
            }

            System.out.println();
            System.out.println("Connected to database as " + username + "@" + Tptbm.url);

	    // Disable auto-commit mode
	    conn.setAutoCommit(false);

        } catch ( SQLException sqe ) {
	    printSQLException(sqe);
	    System.exit(2);
        }

	try {
	    // populate the database if nobuild option is not specified
            if ( ! nobuild )
	        error = !populate(conn);
	    if (buildonly || error) {
	        conn.close();
                if ( error )
	            System.exit(2);
                else
	            System.exit(0);
	    }

            if (  nobuild  ) {
                error = ! checkDbParams(conn);
                if ( error ) {
	            System.exit(2);
                }
            }

            if (  runMode == modeScaleoutRouting ) {
                Statement stmt = conn.createStatement();
                ResultSet res = stmt.executeQuery(selNumRs);
                if (  !res.next() ) {
                    System.err.println("Unable to retrieve grid topography");
                    System.exit(2);
                }
                numReplicaSets = res.getInt(1);
                res.close();
                stmt.close();
            }

	    controller = new TptbmThreadController(numThreads, numXacts, testDuration, rampTime);
            System.out.println();
	    System.out.print("Begining execution with " + numThreads + " thread(s): ");
            System.out.print(reads + "% read, " + updates + "% update, ");
            System.out.print(inserts + "% insert, " + deletes + "% delete");
            System.out.println();

	    error = ! controller.start();

	    if (!nobuild)
		conn.close();

            if (  ! error  ) {
	        // Calculate metrics and print
	        elapsedTime = controller.getElapsedTime();
                TptbmThread tth = null;
                for (tno = 0; tno < numThreads; tno++) {
                    tth = controller.getThread(tno);
                    totalXacts += tth.getTimedXacts();
                    totalLocates += tth.getLocCycles();
                }
	        tps = (long)(((double)totalXacts/(double)elapsedTime) * 1000.0);
    
	        System.out.println();
    
	        System.out.print("Elapsed Time:         ");
	        System.out.printf("%9d ms\n", elapsedTime);
    
	        System.out.print("Transactions Per Sec: ");
	        System.out.printf("%9d\n", tps);

                if (  verbose && (runMode == modeScaleoutLocal)  ) {
                    double avgLoc = (double)totalLocates / (double)totalXacts;
                    System.out.print("Average location cycles:  ");
                    System.out.printf("%.3f per operation\n", avgLoc);
                }
            }

            System.out.println("");

	} catch ( SQLException e ) {
	    printSQLException(e);
            System.exit(2);
	}

        System.exit(0);
    }

    //--------------------------------------------------
    // getDbType()
    //--------------------------------------------------
    static private boolean getDbType(Connection conn) {
        Statement stmt = null;
        ResultSet res = null;

        if (  conn != null  )
            try {
                stmt = conn.createStatement();
                res = stmt.executeQuery(selDbType);
                if (  res.next()  )
                    if (  res.getInt(1) == 1  )
                        return true;
            } catch ( Exception e ) {
                return false;
            } finally {
                if ( res != null )
                    try { res.close(); } catch ( Exception e ) { ; }
                if ( stmt != null )
                    try { stmt.close(); } catch ( Exception e ) { ; }
            }

        return false;
    } // getDbType

    //--------------------------------------------------
    // checkDbParams()
    //--------------------------------------------------
    static private boolean checkDbParams(Connection conn) {
        Statement stmt = null;
        ResultSet res = null;
        String dbp = null;
        String dbm = null;
        String dbk = null;

        if (  conn != null  )
            try {
                stmt = conn.createStatement();
                res = stmt.executeQuery(selDbMode);
                if (  res.next()  ) {
                    dbp = res.getString(1);
                    int i = dbp.indexOf('/');
                    if ( i >= 0 ) {
                        dbm = dbp.substring(0,i);
                        i += 1;
                        int i2 = dbp.indexOf(' ');
                        dbk = dbp.substring(i,i2);
                        i = Integer.parseInt(dbk);
                        if (  dbm.equals(dbModeClassicS) || dbm.equals(dbModeScaleoutS)  ) {
                           if (  ! Tptbm.fkey  ) {
                               key = i;
                               return true;
                           } else {
                               if ( key == i  )
                                   return true;
                               else
                                   System.err.println("Specified key value " + key + 
                                                      " differs from value used to create database ("+ dbk + ")");
                           }
                        }
                    }
                }
            } catch ( Exception e ) {
                System.err.println("Error occurred while trying to retrieve benchmark parameters from database.");
                return false;
            } finally {
                if ( res != null )
                    try { res.close(); } catch ( Exception e ) { ; }
                if ( stmt != null )
                    try { stmt.close(); } catch ( Exception e ) { ; }
            }

        System.err.println("Benchmark table was not properly populated (missing metadata).");
        return false;
    } // checkDbParams

    //--------------------------------------------------
    // usage()
    //--------------------------------------------------
    static private void usage(boolean full) {
	
        if ( full )
	System.err.println(
             "\nThis program implements a multi-user throughput benchmark.\n" );

	System.err.println(
	     "Usage:\n\n" +

	     "  Tptbm {-h | -help}\n\n" +

             "  Tptbm [-url <url_string>] [-threads <num_threads>] [-dbms <dbms_name>]\n" +
	     "        [-multiop | [-read <read%>] [-insert <insert%>] [-delete <delete%>]]\n" +
	     "        [{-xact <xacts> | -sec <seconds> [-ramp <rseconds>]}]\n" +
	     "        [-min <min_xacts>] [-max <max_xacts>] [-seed <seed>]\n" +
             //"        [-csCommit] [-commitReads] [-range] [-key <keys>] [-plsql]\n" +
             "        [-commitReads] [-range] [-key <keys>] [-plsql]\n" +
	     "        [-trace] [-nobuild | -build]" );
        if ( enableScaleout )
	    System.err.println(
             "        [-scaleout [local | routing]]");
	System.err.println("");
           

        if ( full ) {
	System.err.println(
	     "  -h                     Prints this message and exits.\n\n" + 

	     "  -help                  Same as -h.\n\n" +

	     "  -url <url_string>      Specifies JDBC url string of the database\n" +
	     "                         to connect to.\n\n" +

	     "  -threads <num_threads> Specifies the number of concurrent \n" +
	     "                         threads. The default is 1.\n\n" +

	     "  -dbms <dbms_name>      Use 'timesten' or 'oracle'. 'timesten' is\n" +
             "                         the default.\n\n" +

	     "  -multiop               1 insert, 3 selects, 1 update / transaction.\n\n" +

	     "  -read <read%>          Specifies the percentage of read operations.\n" +
	     "                         The default is 80.\n\n" +

	     "  -insert <insert%>      Specifies the percentage of insert operations.\n" +
	     "                         The default is 0. Can't be used with '-sec' or\n" +
	     "                         '-nobuild'.\n\n" +

	     "  -delete <delete%>      Specifies the percentage of delete operations.\n" +
	     "                         The default is 0. Can't be used with '-sec' or\n" +
	     "                         '-nobuild'.\n\n" +

             "                         The percentage of updates is 100 minus the read,\n" +
             "                         insert and delete percentages.\n\n" +

	     "  -xact <xacts>          Specifies the number of transactions that each\n" +
	     "                         thread should execute. The default is " + dfltXacts + ".\n" +
	     "                         Mutually exclusive with '-sec'.\n\n" +

             "  -sec <seconds>         Specifies that <seconds> is the test measurement\n" +
             "                         duration. The default is to run in transaction\n" +
             "                         mode (-xact). Mutually exclusive with '-xact'.\n\n" +

             "  -ramp <rseconds>       Specifies that <rseconds> is the ramp up & down\n" +
             "                         time in duration mode (-sec). Default is 10.\n" +
	     "                         Mutually exclusive with '-xact'.\n\n" +

             "  -min <min_xacts>       Minimum operations per transaction. Default is 1.\n\n" +

             "  -max <max_xacts>       Maximum operations per transaction. Default is 1.\n\n" +

             "                         The number of operations in each transaction are\n" +
             "                         randomly chosen between min_xacts and max_xacts.\n\n" +

             "  -seed <seed>           Specifies the seed for the random \n" +
             "                         number generator.\n\n" +

	     //"  -csCommit              Turn on prefetchClose for client/server. Not\n" +
	     //"                         allowed for direct mode connections or when\n" +
	     //"                         usign an Oracle database.\n\n" +

	     "  -commitReads           By default readonly transactions are not committed,\n" +
	     "                         use this option to force them to be committed.\n\n" +

	     "  -range                 Use a range rather than hash index for the primary key.\n" +
	     "                         This is the default when running against an Oracle\n" +
	     "                         database. Cannot be used with '-nobuild'.\n\n" +

	     "  -key <keys>            Specifies the number of records (squared) to initially\n" +
	     "                         populate in the data store. The default for keys is 100.\n" +
	     "                         Cannot be used with '-nobuild'.\n\n" +

	     "  -plsql                 Use PL/SQL for -multiop to reduce round trips.\n\n" +

	     "  -trace                 Turns JDBC tracing on.\n\n" +

	     "  -nobuild               Do not build the table; assumes table exists and is\n" +
	     "                         already populated. Cannot be used with '-insert' or\n" +
             "                         '-delete'.\n" );

         if ( ! enableScaleout  )
             System.err.println(
	     "  -build                 Builds table and exits. Only the '-key' and '-range'\n" +
	     "                         parameters are relevant.\n");
         else
             System.err.println(
	     "  -build                 Builds table and exits. Only the '-key', '-range' and\n" +
	     "                         '-scaleout' parameters are relevant.\n\n" +

	     "  -scaleout              Run in Scaleout mode. Creates the table with a hash\n" +
             "                         distribution and adapts runtime behaviour for Scaleout.\n" +
             "                         If not specified then the default is to run in Classic mode.\n" +
             "                         You can run in Classic mode aginst a database built in\n" +
             "                         Scaleout mode but not vice-versa.\n\n" +

	     "      local              Use the routing API to constrain all data access to be to\n" +
             "                         rows in the locally connected database element; each thread\n" +
             "                         generates keys that it knows refer to rows in the element\n" +
             "                         that it is connected to. Only relevant at run-time. Cannot\n" +
             "                         be used with '-plsql'.\n\n" +

	     "      routing            Use the routing API to optimize data access. Each\n" +
             "                         thread maintains a connection to every database\n" +
             "                         element and uses the routing API to direct operations\n" +
             "                         to an element that it knows contains the target row.\n" +
             "                         Only relevant at run time. Cannot be used with '-multiop',\n" +
             "                         with '-min' or '-max' > 1 or with '-plsql'. Can only be\n" +
             "                         used in client-server mode.\n\n" +

             "For the most accurate results, use duration mode (-sec) with a measurement\n" +
             "time of at least several minutes and a ramp time of at least 30 seconds.\n"
	     );
        } // full
	System.exit(1);
    }

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
        if (  urlUID == null  )
            username = dfltUsername;
        else
            username = urlUID;
    }

    //--------------------------------------------------
    //
    // Function: getPassword
    //
    // Description: Assign the password
    //
    //--------------------------------------------------

    static private void getPassword() {
        if (  urlPWD != null  )
            password = urlPWD;
        else
        {
            String prompt = "Enter password for '" + username + "': ";
            password = PasswordField.readPassword(prompt);
        }
    }

    //--------------------------------------------------
    //
    // Function: populate
    //
    // Description: Connects to the datastore, 
    // inserts key**2 records, and disconnects
    //
    //--------------------------------------------------

    static private boolean populate(Connection conn) {

	Statement stmt;
	PreparedStatement pstmt;
	int outerIndex, innerIndex;
	int cc = 0;
	Timer clock = new Timer();
	int pages = ((key*key)/256)+1;
        String createStmt = null;
        String dist = "";
        String dbm = null;

      System.out.println();
	System.out.println("Populating benchmark database ...");
	try {

	    if (pages < minHashPages) pages = minHashPages;

	    // if (multiop == 1) {
	    if (inserts > 0) {
		long addpgs;
		int avgops = min_xact + ((max_xact - min_xact + 1)/2);
                long nx = numXacts;
                if (  numXacts <= 0  )
                    nx = testDuration * assumedTPS;
	        addpgs = ((numThreads * nx * inserts * avgops) / (256 * 100)) + 1;
		pages += addpgs;
	    }

	    // Create a Statement object
	    stmt = conn.createStatement();

	    // Drop and re-create the table
	    try {
	        stmt.executeUpdate("drop table vpn_users");
	    } catch ( SQLException ex ) { ; }

            if (  runMode > modeClassic  )
                dist = distClause;
	    if( range )
                createStmt = createStmtRange + dist;
            else
                createStmt = createStmtHash + pages + dist;

	    stmt.executeUpdate(createStmt);

	    // Prepare the insert statement
	    pstmt = conn.prepareStatement(insertStmt);

            // Store the database mode and key value
            if (  runMode == modeClassic  )
                dbm = dbModeClassicS;
            else
                dbm = dbModeScaleoutS;
            pstmt.setInt(1,dbModeId);
            pstmt.setInt(2,dbModeNb);
            pstmt.setString(3," ");
            pstmt.setString(4,dbm + "/" + key);
	    pstmt.executeUpdate();

	    // commit 
	    conn.commit();

	    clock.start();

	    // Insert key**2 records
	    for(outerIndex = 0; outerIndex < key; outerIndex++) 
	    {
		for ( innerIndex = 0; innerIndex < key; innerIndex++)
		{
		    pstmt.setInt(1, outerIndex);
		    pstmt.setInt(2, innerIndex);
		    pstmt.setString(3, "55"+(innerIndex%10000)+(outerIndex%10000));
		    pstmt.setString(4, "<place holder for description of VPN " +
				    outerIndex + " extension " + innerIndex);

		    pstmt.executeUpdate();
		    // commit every 256 rows
		    cc++;
		    if ( (cc % 256) == 0) 
		    {
		        cc = 0;
		        conn.commit();
		    }
		}
	    }

	    // commit 
	    conn.commit();

	    clock.stop();

	    stmt.close();
	    pstmt.close();
	   
	    long ms = clock.getTimeInMs();

	    if (ms > 0) {
		long tps = (long) ((double)(Tptbm.key*Tptbm.key)/ms * 1000);
		System.out.println ("\nDatabase populated with " + 
				    Tptbm.key*Tptbm.key + 
				    " rows in " +
				    ms + " ms" + " (" +
				    tps + " TPS)\n");
	    }

	} catch ( SQLException e ) {
	    printSQLException(e);
            return false;
	}

        return true;
    }


    //--------------------------------------------------
    //
    // Function: parseArgs
    // 
    // Parses command line arguments
    //
    //--------------------------------------------------
    private static void parseArgs( String[] args) {
        boolean fread = false;
        boolean finsert = false;
        boolean fdelete = false;
        boolean fmin = false;
        boolean fmax = false;
        boolean fseed = false;
        boolean fthreads = false;
	int i=0;

	//	System.out.println("args.length: " + args.length);	
	while(i<args.length) {

	    // Command line help
	    if(args[i].equalsIgnoreCase("-h") || 
	       args[i].equalsIgnoreCase("-help")) {
		usage(true);
	    }

	    // JDBC url string
	    else if(args[i].equalsIgnoreCase("-url")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if ( url != null )
                    usage(false);
		url = args[i++];
	    }

	    // number of threads
	    else if(args[i].equalsIgnoreCase("-threads")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if ( fthreads )
                    usage(false);
                fthreads = true;
		numThreads = Integer.parseInt(args[i++]);
		if(numThreads <= 0) {
		    System.err.println(
			  "'-threads' requires a positive, non-zero, integer argument");
		    System.exit(1);
		}
	    }

	    // number of transactions for each thread
	    else if(args[i].equalsIgnoreCase("-xact") || args[i].equalsIgnoreCase("-xacts")) {
                if ( (numXacts > 0) || (testDuration > 0) || (rampTime >= 0) ) {
                    usage(false);
                }
		if(++i >= args.length) {
		    usage(false);
		}
		numXacts = Integer.parseInt(args[i++]);
		if(numXacts <= 0) {
		    System.err.println(
			  "'-xact' requires a positive, non-zero, integer argument");
		    System.exit(1);
		}
	    }

            // test duration
            else if(args[i].equalsIgnoreCase("-sec")) {
                if ( (numXacts > 0) || (testDuration > 0) ) {
                    usage(false);
                }
                if(++i >= args.length) {
                    usage(false);
                }
                testDuration = Integer.parseInt(args[i++]);
		if(testDuration <= 0) {
		    System.err.println(
			  "'-sec' requires a positive, non-zero, integer argument");
		    System.exit(1);
		}
            }
	    
            // ramp time
            else if(args[i].equalsIgnoreCase("-ramp")) {
                if ( (numXacts > 0) || (rampTime >= 0) ) {
                    usage(false);
                }
                if(++i >= args.length) {
                    usage(false);
                }
                rampTime = Integer.parseInt(args[i++]);
		if(rampTime < 0) {
		    System.err.println(
                          "'-ramp' requires a positive integer argument");
                    System.exit(1);
                }
            }
	    
            // seed for random number generator
            else if(args[i].equalsIgnoreCase("-seed")) {
                if(++i >= args.length) {
                    usage(false);
                }
                if ( fseed )
                    usage(false);
                fseed = true;
                seed = Integer.parseInt(args[i++]);
		if( seed <= 0 ) {
		    System.err.println(
                          "'-seed' requires a positive, non-zero, integer argument");
                    System.exit(1);
                }
            }

	    // Percentage of read transactions
	    else if(args[i].equalsIgnoreCase("-read") || args[i].equalsIgnoreCase("-reads")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if ( fread || (multiop > 0)  )
		    usage(false);
		reads = Integer.parseInt(args[i++]);
		if( (reads < 0) || (reads > 100) ) {
		    System.err.println(
                          "'-read' requires a positive integer argument <= 100");
                    System.exit(1);
                }
                fread = true;
	    }

	    // percentage of insert transactions
	    else if(args[i].equalsIgnoreCase("-insert") || args[i].equalsIgnoreCase("-inserts")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if ( finsert || (multiop > 0)  )
		    usage(false);
		inserts = Integer.parseInt(args[i++]);
		if( (inserts < 0) || (inserts > 100) ) {
		    System.err.println(
                          "'-insert' requires a positive integer argument <= 100");
                    System.exit(1);
                }
                finsert = true;
	    }

	    // percentage of delete transactions
	    else if(args[i].equalsIgnoreCase("-delete") || args[i].equalsIgnoreCase("-deletes")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if ( fdelete || (multiop > 0)  )
		    usage(false);
		deletes = Integer.parseInt(args[i++]);
		if( (deletes < 0) || (deletes > 100) ) {
		    System.err.println(
                          "'-delete' requires a positive integer argument <= 100");
                    System.exit(1);
                }
                fdelete = true;
	    }

	    // key to determine number of records to 
	    // insert initially
	    else if(args[i].equalsIgnoreCase("-key")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if ( fkey )
                    usage(false);
                if ( nobuild ) {
                    System.err.println("You cannot specify '-key' with '-nobuild'.");
                    System.exit(1);
                }
                fkey = true;
		key = Integer.parseInt(args[i++]);
		if(key <= 0) {
		    System.err.println(
			  "'-key' requires a positive, non-zero, integer argument");
                    System.exit(1);
                }
	    }

	    // minimum number of statements/transaction
	    else if(args[i].equalsIgnoreCase("-min")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if ( fmin )
                    usage(false);
                fmin = true;
		min_xact = Integer.parseInt(args[i++]);
		if(min_xact <= 0) {
		    System.err.println(
			  "'-min' requires a positive, non-zero, integer argument");
                    System.exit(1);
                }
	    }

	    // maximum number of statements/transaction
	    else if(args[i].equalsIgnoreCase("-max")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if ( fmax )
                    usage(false);
                fmax = true;
		max_xact = Integer.parseInt(args[i++]);
		if(max_xact <= 0) {
		    System.err.println(
                          "'-max' requires a positive, non-zero, integer argument");
                    System.exit(1);
                }
	    }

            // database selection
	    else if(args[i].equalsIgnoreCase("-dbms")) {
		if(++i >= args.length) {
		    usage(false);
		}
                if (  dbms > TPTBM_NONE  )
		    usage(false);
		if(args[i].equalsIgnoreCase("oracle"))
		    dbms = TPTBM_ORACLE;
                else
		if(args[i].equalsIgnoreCase("timesten"))
		    dbms = TPTBM_TIMESTEN;
                else
                    usage(false);
		i += 1;
	    }

            // scaleout and routing mode
	    else if(args[i].equalsIgnoreCase("-scaleout")) {
                if (  ! enableScaleout  )
                    usage(false);
                if (  runMode > modeClassic  )
                    usage(false);
                runMode = modeScaleout;
		if(++i < args.length) {
		    if(args[i].equalsIgnoreCase("local")) {
                        runMode = modeScaleoutLocal;
		        i += 1;
                    }
                    else
		    if(args[i].equalsIgnoreCase("routing")) {
                        runMode = modeScaleoutRouting;
		        i += 1;
                    }
                    else
		    if(!args[i].startsWith("-"))
                        usage(false);
	        }
	    }

	    // verbose (undocumented)
	    else if(args[i].equalsIgnoreCase("-verbose")) {
		verbose = true;
		i += 1;
	    }

	    // prefetchClose ON (undocumented)
	    else if(args[i].equalsIgnoreCase("-csCommit")) {
                if ( prefetchClose )
                    usage(false);
		prefetchClose = true;
		i += 1;
	    }

	    // multiop
	    else if(args[i].equalsIgnoreCase("-multiop")) {
                if ( fread || finsert || fdelete || (multiop > 0)  )
		    usage(false);
		multiop = 1;
		min_xact = 5;
		max_xact = 5;
		reads = 60;
		inserts = 20;
		i += 1;
	    }

	    // Range
	    else if(args[i].equalsIgnoreCase("-range")) {
                if ( range )
                    usage(false);
                if ( nobuild ) {
                    System.err.println("You cannot specify '-range' with '-nobuild'.");
                    System.exit(1);
                }
		range = true;
		i += 1;
	    }

	    // JDBC tracing
	    else if(args[i].equalsIgnoreCase("-trace")) {
                if ( trace )
                    usage(false);
		trace = true;
		i += 1;
	    }

	    // Thread tracing: to complement JDBC tracing
	    else if(args[i].equalsIgnoreCase("-threadtrace")) {
                if ( threadtrace )
                    usage(false);
		threadtrace= true;
		i += 1;
	    }

	    // Commit reads - to match behavior prior to 5.0
	    else if(args[i].equalsIgnoreCase("-commitReads")) {
                if ( commitReads )
                    usage(false);
		commitReads = true;
		i += 1;
	    }

	    // Do not build the database
	    else if(args[i].equalsIgnoreCase("-nobuild")) {
                if ( nobuild || buildonly )
                    usage(false);
                if ( fkey ) {
                    System.err.println("You cannot specify '-key' with '-nobuild'.");
                    System.exit(1);
                }
                if ( range ) {
                    System.err.println("You cannot specify '-range' with '-nobuild'.");
                    System.exit(1);
                }
		nobuild = true;
		i += 1;
	    }

	    // Build and exit
	    else if(args[i].equalsIgnoreCase("-build")) {
                if ( nobuild || buildonly )
                    usage(false);
		buildonly = true;
		i += 1;
	    }

	    // PL/SQL
	    else if(args[i].equalsIgnoreCase("-plsql")) {
                if ( usePlsql )
                    usage(false);
		usePlsql = true;
		i += 1;
	    }

            // Invalid argument
	    else {
		usage(false);
	    }
	}

        if (  dbms == TPTBM_NONE  )
            dbms = TPTBM_TIMESTEN;

        if (  usePlsql && (runMode > modeScaleout)  ) {
            if (  runMode == modeScaleoutLocal  )
	       System.err.println("'-plsql' cannot be combined with '-scaleout local'");
            else
	       System.err.println("'-plsql' cannot be combined with '-scaleout routing'");
	    System.exit(1);
        }

        if (  numXacts == 0  ) {
            if (  testDuration <= 0  )
	        numXacts = dfltXacts;
            else {
                if (  rampTime < 0  )
                    rampTime = dfltRamptime;
                if (  inserts > 0  ) {
	            System.err.println("'-insert' cannot be used in duration mode");
	            System.exit(1);
                }
                if (  deletes > 0  ) {
	            System.err.println("'-delete' cannot be used in duration mode");
	            System.exit(1);
                }
            }
        } else {
            rampTime = 0;
            testDuration = 0;
        }

	if ((reads + inserts + deletes) > 100){
	    System.err.println("Transaction mix should not exceed 100");
	    System.exit(1);
	}

	if ( min_xact > max_xact ) {
	    System.err.println("Value for -min must be <= value for -max");
	    System.exit(1);
	}

        // How many updates
        updates = 100 - reads - inserts - deletes;

	if(url == null) {
          // Default the URL
          url = "jdbc:timesten:" + defaultDSN;
	}
        else
        {
            String urlUC = url.toUpperCase();
            int indStart = urlUC.indexOf(";UID=");
            int indEnd = -1;
            if (  indStart >= 0  )
            {
                indStart += 5;
                indEnd = urlUC.indexOf(';',indStart);
                if (  indEnd >= 0  )
                {
                    urlUID = url.substring(indStart,indEnd);
                    url = url.substring(0,indStart-5) + url.substring(indEnd);
                }
                else
                {
                    if (  indStart < url.length()  )
                        urlUID = url.substring(indStart);
                    else
                        urlUID = "";
                    url = url.substring(0,indStart-5);
                }
            }
            urlUC = url.toUpperCase();
            indStart = urlUC.indexOf(";PWD=");
            indEnd = -1;
            if (  indStart >= 0  )
            {
                indStart += 5;
                indEnd = urlUC.indexOf(';',indStart);
                if (  indEnd >= 0  )
                {
                    urlPWD = url.substring(indStart,indEnd);
                    url = url.substring(0,indStart-5) + url.substring(indEnd);
                }
                else
                {
                    if (  indStart < url.length()  )
                        urlPWD = url.substring(indStart);
                    else
                        urlPWD = "";
                    url = url.substring(0,indStart-5);
                }
            }
        }

        if (  url.startsWith( "jdbc:timesten:client:" )  )
            isCS = true;

        if (  prefetchClose && ! isCS  ) {
              System.err.println("'-csCommit' can only be used with client/server connections.");
              System.exit(1);
        }

        if (  dbms == TPTBM_ORACLE  ) {
            range = true;
            if (  (dbms == TPTBM_ORACLE) && (runMode > modeClassic)  ) {
                  System.err.println("'-scaleout' can only be used with TimesTen.");
                  System.exit(1);
            }
        }

        if (  !isCS && (runMode == modeScaleoutRouting)  )   {
              System.err.println("'-scaleout routing' can only be used with client/server connections.");
              System.exit(1);
        }

        if (multiop == 0) {
            int nx = numXacts;
            if (  nx <= 0  )
                nx = assumedTPS * testDuration;

            if(numThreads * ( nx/100 * (float)inserts) > (key * key)) {
              System.err.println("Inserts as part of transaction mix exceed number initially populated." );
              System.exit(1);
            }
            if(numThreads * ( nx/100 * (float)deletes) > (key * key)) {
              System.err.println("Deletes as part of transaction mix exceed number initially populated." );
              System.exit(1);
            }
        } else
        if (  runMode > modeScaleout  ) {
              System.err.println("'-scaleout local' or '-scaleout routing' cannnot be used with '-multiop'.");
              System.exit(1);
        }
 
        if (  (runMode > modeScaleoutLocal) && ((min_xact != 1) || (max_xact != 1))  ) {
              System.err.println("'-scaleout routing' cannnot be used with '-min' or '-max' > 1.");
              System.exit(1);
        }

	if (usePlsql) {
	    if (multiop < 1) {
		System.err.println("PL/SQL only allowed with multiop\n");
		usage(false);
	    }
	}

    }

    public static void printSQLException ( SQLException ex )
    {
	for ( ; ex != null; ex = ex.getNextException() )
	{
	    System.out.println(ex.getMessage()); 
	    System.out.println("Vendor Error Code: " + ex.getErrorCode());
	    System.out.println("SQL State: " + ex.getSQLState());
	    ex.printStackTrace();
	}
    }
}

//--------------------------------------------------
// Class: TptbmConnection
//--------------------------------------------------

class TptbmConnection 
{
    public short rsId = 0;
    public Connection conn = null;
    public PreparedStatement selStmt = null;
    public PreparedStatement insStmt = null;
    public PreparedStatement updStmt = null;
    public PreparedStatement delStmt = null;
    public PreparedStatement initPlsqlStmt = null;
    public PreparedStatement execPlsqlStmt = null;

    TptbmConnection() { }
} // TptbmConnection

//--------------------------------------------------
// Class: TptbmRouting
//--------------------------------------------------

class TptbmRouting
{
    private int numRs = 0;
    private TimesTenDataSource ds = null;
    private TptbmConnection rsConn[] = null;

    TptbmRouting(int nRs) {
        numRs = nRs;
        rsConn = new TptbmConnection[numRs];
    }

    public boolean buildRouting( TimesTenDataSource ds, Connection conn, 
                                 boolean prefetchClose,
                                 String selectStmt, String insertStmt,
                                 String updateStmt, String deleteStmt )
        throws SQLException
    {
        int rsind = 0;
        short rsId = 0;
        this.ds = ds;
        Statement stmt = conn.createStatement();
        ResultSet res = stmt.executeQuery(Tptbm.selRsIds);
        TimesTenConnectionBuilder ttCb = ds.createTimesTenConnectionBuilder().user(Tptbm.username).password(Tptbm.password);
        while ( res.next() ) {
            rsId = res.getShort(1);
            if (  Tptbm.verbose  )
                if (  rsId != (rsind + 1)  ) 
                    System.err.println("Warning: routing error: build: " + rsId + " should be " + (rsind+1));
            rsConn[rsind] = new TptbmConnection();
            rsConn[rsind].rsId = rsId;
            rsConn[rsind].conn = ttCb.replicaSetID(rsId).build();
            rsConn[rsind].selStmt = rsConn[rsind].conn.prepareStatement(selectStmt);
            rsConn[rsind].insStmt = rsConn[rsind].conn.prepareStatement(insertStmt);
            rsConn[rsind].updStmt = rsConn[rsind].conn.prepareStatement(updateStmt);
            rsConn[rsind].delStmt = rsConn[rsind].conn.prepareStatement(deleteStmt);
            if (  prefetchClose )
                ((TimesTenConnection)rsConn[rsind].conn).setTtPrefetchClose(true);
	    rsConn[rsind].conn.setAutoCommit(false);
            rsind += 1;
        }
        res.close(); res = null;
        stmt.close(); stmt = null;
        for (rsind = 0; rsind < numRs; rsind++) 
            rsConn[rsind].conn.commit();
        if (  rsind == 0  )
            return false;
        return true;
    }

    public void cleanup() 
    {
        int rsind = 0;
        for (rsind = 0; rsind < numRs; rsind++) {
            try { rsConn[rsind].conn.commit(); } catch ( Exception e ) { ; }
            try { rsConn[rsind].selStmt.close(); } catch ( Exception e ) { ; }
            try { rsConn[rsind].insStmt.close(); } catch ( Exception e ) { ; }
            try { rsConn[rsind].updStmt.close(); } catch ( Exception e ) { ; }
            try { rsConn[rsind].delStmt.close(); } catch ( Exception e ) { ; }
            if (  rsConn[rsind].initPlsqlStmt != null  ) {
                try { rsConn[rsind].initPlsqlStmt.close(); } catch ( Exception e ) { ; }
                try { rsConn[rsind].execPlsqlStmt.close(); } catch ( Exception e ) { ; }
            }
            try { rsConn[rsind].conn.close(); } catch ( Exception e ) { ; }
        }
    }

    public TptbmConnection getConnection( int id, int nb )
        throws SQLException
    {
        TimesTenDistributionKey dk = null;
        int rs;
        int rsind;

        dk = ds.createTimesTenDistributionKeyBuilder()
               .subkey(id, Types.INTEGER)
               .subkey(nb, Types.INTEGER)
                       .build();

        rs = (int)dk.getReplicaSetID();
        rsind = rs - 1;
        dk = null;
        // for (rsind = 0; rsind < numRs; rsind++) {
        //     if (  rs == rsConn[rsind].rsId  ) 
        //        return rsConn[rsind];
        // }
        if (  Tptbm.verbose  )
            if (  rsConn[rsind].rsId != rs  )
                System.err.println("Warning: routing error: route: " + rsConn[rsind].rsId + " should be " + rs);

        return rsConn[rsind];
    }
} // TptbmRouting

//--------------------------------------------------
// Class: TptbmError
//--------------------------------------------------

class TptbmError extends Exception
{
} // TptbmError

//--------------------------------------------------
// Class: TptbmThread
//--------------------------------------------------

class TptbmThread extends Thread
{

    //--------------------------------------------------
    // Class constants
    //--------------------------------------------------
    static private String selectStmt = 
	"select directory_nb, last_calling_party," +
	"descr from vpn_users where vpn_id = ? and vpn_nb= ?";
    static private String updateStmt = 
	"update vpn_users set last_calling_party" +
	"= ? where vpn_id = ? and vpn_nb = ?";
    static private String getRsIdStmt = 
        "select replicasetid# from dual";

    //--------------------------------------------------
    // Member variables
    //--------------------------------------------------

    // Error flag
    public boolean error = false;

    // Must stop flag
    public boolean mustStop = false;

    // Number of transactions
    private int numXacts;

    // Run duration (measured)
    private int testDuration;

    // Ramp time
    private int rampTime;

    // Data source
    private TimesTenDataSource ttds = new TimesTenDataSource();

    // connection for the thread
    private TptbmConnection connection = new TptbmConnection();

    // Routing info
    private TptbmRouting routingInfo = null;

    // Replica set ID for my connection
    private int myRsId = 0;

    // Threads id used for thread tracing
    private int id;
    
    // Each thread insert in a perticular area of the table
    // so they do not run into each other
    private int insert_start;
    private int delete_start;

    // Pointer in the table for the current thread
    private int insert_present = 0;
    private int delete_present = 0;
    
    // Flags to control the thread execution 
    private boolean go = false; 
    private boolean ready = false;
    private boolean done = false;
    
    // Counters and indicators
    private long timedXacts = 0;
    private long totalXacts = 0;
    private long totalLocCycles = 0;
    private boolean timing = false;
    private boolean doneTiming = false;
    private boolean doneRamping = false;

    //--------------------------------------------------
    // Constructor
    //--------------------------------------------------
    public TptbmThread(int id, int numXacts, int testDuration, int rampTime) {
	if(Tptbm.threadtrace)
	    System.out.println("constructing thread: " + id);
	this.id = id;
	this.numXacts = numXacts;
	this.testDuration = testDuration;
	this.rampTime = rampTime;
    }

    //--------------------------------------------------
    // Method: run
    // Overwrites the run method in the Thread class
    // Called when start is called on the thread
    //--------------------------------------------------
    public void run() {
	if(Tptbm.threadtrace)
	    System.out.println("started running thread: " + id);
	
	try {

	    // Do all the initialization work here(e.g. connect)
	    initialize();
	
	    setReady();

	
	    // Wait here until parent indicates that threads can start execution
	    if(Tptbm.threadtrace)
		System.out.println(
                      "thread " + id + " is waiting for the parent's signal");
	    // Block and wait for go.
            goYet();
	    // start execution
	    execute();
	    
	} catch( SQLException e) {
	    Tptbm.printSQLException(e);
            error = true;
	}
	finally {
	    // does not harm to set the ready again in case we get an exception 
	    // in the intialization and parent is still wating for ready flag.
	    setReady();
	    setDone(); 
	}
    }
    
    // Called by the controller class
    // to start timing in the thread
    public synchronized void timingStart() {
        timedXacts = 0;
        timing = true;
        doneTiming = false;
        doneRamping = true;
    }
    
    // Called by the controller class
    // to stop timing in the thread
    public synchronized void timingStop() {
        timing = false;
        doneTiming = true;
    }

    // Called by the controller class
    // to start ramping in the thread
    public synchronized void rampStart() {
        doneRamping = false;
    }

    // Called by the controller class
    // to stop ramping in the thread
    public synchronized void rampStop() {
        doneRamping = true;
    }

    // Called by the controller class
    // to indicate that thread can start
    // executing
    public synchronized void setGo() {
	go = true;
        notify();
    }
    
    // Called by TptbmThread class
    // to check if the thread can start executing
    public synchronized boolean goYet() {
        try {
          while(!go) {
            wait();
          }      
        }
        catch (InterruptedException iex) {
          iex.printStackTrace();
        }
        return go;
    }   
    
    // Called by TptbmThread class
    // to indicate that the thread is ready to start executing
    public synchronized void setReady() {
        ready = true;
        notify();
    }

    // Called by the controller class
    // to check if the thread is ready
    // to execute yet i.e. thread is 
    // finished initialization
    public synchronized boolean readyYet() {
          try {
            while(!ready) {
              wait();
            }      
          }
          catch (InterruptedException iex) {
            iex.printStackTrace();
          }
	  return ready;
    }
    
    // Called by TptbmThread class
    // to indicate that the thread has finished executing
    public synchronized void setDone() {
        done = true;
        notify();
    }    

    //--------------------------------------------------
    // To check if the thread is done executing
    //--------------------------------------------------    
    public synchronized boolean doneYet() {
        try {
            while(!done) {
              wait();
            }      
        }
        catch (InterruptedException iex) {
            iex.printStackTrace();
        }
	return done;
    }

    public synchronized boolean runFinished() {
        if ( error || mustStop )
            return true;
        if (  numXacts > 0   ) { // transaction mode
            return (totalXacts >= numXacts);
        } else {                       // duration mode
            return (doneTiming && doneRamping);
        }
    }

    public long getTimedXacts() {
        if (  numXacts > 0   ) { // transaction mode
            return totalXacts;
        } else {                       // duration mode
            return timedXacts;
        }
    }
    
    public long getLocCycles() {
        return totalLocCycles;
    }
    
    //--------------------------------------------------
    // Method : keyIsLocal
    // Checks if a key value is located in the replica
    // set to which we are connected.
    //--------------------------------------------------
    private boolean keyIsLocal( int rsId, int id, int nb )
        throws SQLException
    {
        TimesTenDistributionKey dk = null;

        if (  rsId <= 0  )
            return true;

        dk = Tptbm.ttds.createTimesTenDistributionKeyBuilder()
                       .subkey(id, Types.INTEGER)
                       .subkey(nb, Types.INTEGER)
                       .build();

        if (  rsId == dk.getReplicaSetID()  )
            return true;

        return false;
    } // keyIsLocal

    //--------------------------------------------------
    // Method : execute 
    // It executes transactions until it is finished.
    // Based on the transaction mix specified it
    // executes reads/inserts/updates/deletes.
    //--------------------------------------------------
    private void execute() throws SQLException {
        TptbmConnection tconn = connection;
        PreparedStatement prepSelStmt = connection.selStmt;
        PreparedStatement prepInsStmt = connection.insStmt;
        PreparedStatement prepDelStmt = connection.delStmt;
        PreparedStatement prepUpdStmt = connection.updStmt;
        PreparedStatement prepPlInitStmt = connection.initPlsqlStmt;
        PreparedStatement prepPlStmt = connection.execPlsqlStmt;
        Random rand = new Random(Tptbm.seed);
	int id_int;
	int nb_int;
	int path = 0;
        int ops_in_xact = 1;
        int jj;
	String last_calling_party = null;

	if(Tptbm.threadtrace)
	    System.out.println("Executing thread " + id );

	if (Tptbm.min_xact == Tptbm.max_xact)
	    ops_in_xact = Tptbm.min_xact;
	else
	    ops_in_xact = Tptbm.max_xact - Tptbm.min_xact + 1;

	if (Tptbm.multiop == 1)
	    ops_in_xact = 5;

	if (Tptbm.usePlsql) {
	    prepPlInitStmt.executeUpdate();
	    for (int i = 0; ! runFinished(); i++) {
		id_int = insert_start;
		nb_int = insert_present++;
		prepPlStmt.setInt(1, Tptbm.key);
		prepPlStmt.setInt(2, id_int);
		prepPlStmt.setInt(3, nb_int);
		prepPlStmt.setInt(4, Tptbm.multiop);
		if(Tptbm.threadtrace)
		    System.out.println("Thread " + id + 
				       " is excuting PL/SQL block");
		if (insert_present == Tptbm.key) {
		    insert_present = 0;
		    insert_start += Tptbm.numThreads;
		}
		prepPlStmt.executeUpdate();
                totalXacts += 1;
                if (  timing  )
                    timedXacts += 1;
	    }
	} else { // ! usePlsql
	    for(int i=0; ! runFinished(); i++)	{

	        jj = ops_in_xact;

	        while( jj > 0){
		    jj--;

		    // Determine what type of transaction to execute
		    if (Tptbm.multiop == 1) {
		        switch (jj) {
		        case 4: path = 2; break; //insert
		        case 3: 
		        case 2: 
		        case 1: path = 1; break; //read
		        case 0: path = 0; break; //update
		        }
		    }
		    else {
		      if(Tptbm.reads != 100) 
		      {
		        // pick a number between 0 & 99
		        int randnum = (int)(rand.nextFloat() * 100);
    
		        if(randnum < Tptbm.reads+Tptbm.inserts+Tptbm.deletes) {
		          if(randnum < Tptbm.reads+Tptbm.inserts) {
			    if(randnum < Tptbm.reads) {
			          path = 1; // reads
			    }
			    else {
			      path = 2; // insert
			    }
		          }
		          else {
			    path = 3; // delete
		          }
		        }
		        else {
			    path = 0; // update
		        }
		      }
		      else 
		        path = 1;  // all transactions are read
		    } // not multiop
    

		    // Execute read transaction
		    if( path == 1 ) {
    
		        // pick random values for key from the 
                        // range 0 -> key-1
		        id_int = (int)((Tptbm.key-1)*rand.nextFloat());
		        nb_int = (int)((Tptbm.key-1)*rand.nextFloat());
                        if (  timing || (numXacts > 0)  )
                            totalLocCycles++;
                        if (  Tptbm.runMode == Tptbm.modeScaleoutLocal  ) {
                            while ( ! keyIsLocal(myRsId, id_int, nb_int) ) {
		                id_int = (int)((Tptbm.key-1)*rand.nextFloat());
		                nb_int = (int)((Tptbm.key-1)*rand.nextFloat());
                                if (  timing || (numXacts > 0)  )
                                    totalLocCycles++;
                            }
                        } else
                        if (  Tptbm.runMode == Tptbm.modeScaleoutRouting ) {
                            tconn = routingInfo.getConnection(id_int, nb_int);
                            prepSelStmt = tconn.selStmt;
                            prepInsStmt = tconn.insStmt;
                            prepDelStmt = tconn.delStmt;
                            prepUpdStmt = tconn.updStmt;
                        }
    
		        prepSelStmt.setInt(1, id_int);
		        prepSelStmt.setInt(2, nb_int);
		        if(Tptbm.threadtrace)
			    System.out.println("Thread " + id + 
					       " is excuting select"  );
		        ResultSet rs = prepSelStmt.executeQuery();
		    
		        int rows=0;
		        while (rs.next())
		        {
			    rs.getString(1);
			    rs.getString(2);
			    rs.getString(3);
		        }
		        rs.close(); 
		    
		        if (jj == 0) {
		            if (Tptbm.commitReads)
		    	        tconn.conn.commit();
                            totalXacts += 1;
                            if (  timing  )
                                timedXacts += 1;
                        }
		    }
		    
		    // Execute update transaction
		    else if ( path == 0 ) {
    
		        // pick random values for key from the 
                        // range 0 -> key-1
		        id_int = (int)((Tptbm.key-1)*rand.nextFloat());
		        nb_int = (int)((Tptbm.key-1)*rand.nextFloat());
                        if (  timing || (numXacts > 0)  )
                            totalLocCycles++;
                        if (  Tptbm.runMode == Tptbm.modeScaleoutLocal  ) {
                            while ( ! keyIsLocal(myRsId, id_int, nb_int) ) {
		                id_int = (int)((Tptbm.key-1)*rand.nextFloat());
		                nb_int = (int)((Tptbm.key-1)*rand.nextFloat());
                                if (  timing || (numXacts > 0)  )
                                    totalLocCycles++;
                            }
                        } else
                        if (  Tptbm.runMode == Tptbm.modeScaleoutRouting ) {
                            tconn = routingInfo.getConnection(id_int, nb_int);
                            prepSelStmt = tconn.selStmt;
                            prepInsStmt = tconn.insStmt;
                            prepDelStmt = tconn.delStmt;
                            prepUpdStmt = tconn.updStmt;
                        }
		        prepUpdStmt.setString(1, id_int + "x" + nb_int);
		        prepUpdStmt.setInt(2, id_int);
		        prepUpdStmt.setInt(3, nb_int);
		        if(Tptbm.threadtrace)
			    System.out.println("Thread " + id + 
					       " is excuting update"  );
		        prepUpdStmt.executeUpdate();
		        if (jj == 0) {
			    tconn.conn.commit();
                            totalXacts += 1;
                            if (  timing  )
                                timedXacts += 1;
		        }
		    }
    
		    // Execute delete transaction
		    else if (path == 3) {
		        id_int = delete_start;
		        nb_int = delete_present++;
		        if (delete_present == Tptbm.key) {
			    delete_present = 0;
			    delete_start += Tptbm.numThreads;
		        }
                        if (  timing || (numXacts > 0)  )
                            totalLocCycles++;
                        if (  Tptbm.runMode == Tptbm.modeScaleoutLocal  ) {
                            while ( ! keyIsLocal(myRsId, id_int, nb_int) ) {
		                id_int = delete_start;
		                nb_int = delete_present++;
                                if (  timing || (numXacts > 0)  )
                                    totalLocCycles++;
		                if (delete_present == Tptbm.key) {
			            delete_present = 0;
			            delete_start += Tptbm.numThreads;
		                }
                            }
                        } else
                        if (  Tptbm.runMode == Tptbm.modeScaleoutRouting ) {
                            tconn = routingInfo.getConnection(id_int, nb_int);
                            prepSelStmt = tconn.selStmt;
                            prepInsStmt = tconn.insStmt;
                            prepDelStmt = tconn.delStmt;
                            prepUpdStmt = tconn.updStmt;
                        }
		        prepDelStmt.setInt(1, id_int);
		        prepDelStmt.setInt(2, nb_int);
		        if(Tptbm.threadtrace)
			    System.out.println("Thread " + id + 
					       " is excuting delete"  );
		    
		        prepDelStmt.executeUpdate();
		        if (jj == 0) {
			    tconn.conn.commit();
                            totalXacts += 1;
                            if (  timing  )
                                timedXacts += 1;
		        }
		    }
    
		    // Execute insert transaction
		    else {
		        id_int = insert_start;
		        nb_int = insert_present++;
		        if (insert_present == Tptbm.key) {
			    insert_present = 0;
			    insert_start += Tptbm.numThreads;
		        }
                        if (  timing || (numXacts > 0)  )
                            totalLocCycles++;
                        if (  Tptbm.runMode == Tptbm.modeScaleoutLocal  ) {
                            while ( ! keyIsLocal(myRsId, id_int, nb_int) ) {
		                id_int = insert_start;
		                nb_int = insert_present++;
                                if (  timing || (numXacts > 0)  )
                                    totalLocCycles++;
		                if (insert_present == Tptbm.key) {
			            insert_present = 0;
			            insert_start += Tptbm.numThreads;
		                }
                            }
                        } else
                        if (  Tptbm.runMode == Tptbm.modeScaleoutRouting ) {
                            tconn = routingInfo.getConnection(id_int, nb_int);
                            prepSelStmt = tconn.selStmt;
                            prepInsStmt = tconn.insStmt;
                            prepDelStmt = tconn.delStmt;
                            prepUpdStmt = tconn.updStmt;
                        }
		        prepInsStmt.setInt(1, id_int);
		        prepInsStmt.setInt(2, nb_int);
		        prepInsStmt.setString(3, "55"+(id_int%10000)+(nb_int%10000));
		        prepInsStmt.setString(4, 
                                           "<place holder for description " +
					   "of VPN " + id_int + " extension " + 
					   nb_int + ">");
		        if(Tptbm.threadtrace)
			    System.out.println("Thread " + id + 
					       " is excuting insert"  );
		    
		        prepInsStmt.executeUpdate();
		        if (jj == 0) {
			    tconn.conn.commit();
                            totalXacts += 1;
                            if (  timing  )
                                timedXacts += 1;
		        }
		    }
	        }
	    }
	} // !usePlsql
    }

    //--------------------------------------------------
    // Method: initialize
    // Gets the connection for the thread and prepares
    // all the statements for the thread so it is 
    // ready to execute
    //--------------------------------------------------
    private void initialize() throws SQLException {
	
	if(Tptbm.threadtrace)
	    System.out.println("Initializing thread " + id );

	// Connect to the data store
	if(Tptbm.threadtrace)	
	    System.out.println("Thread " + id + " is connecting"  );

        if (  Tptbm.dbms == Tptbm.TPTBM_ORACLE  ) {
            connection.conn = DriverManager.getConnection(Tptbm.url, Tptbm.username, Tptbm.password);
        } else {
            ttds.setUrl(Tptbm.url);
            ttds.setUser(Tptbm.username);
            ttds.setPassword(Tptbm.password);
            connection.conn = ttds.getConnection();
        }
	
	// set autocommit off
	connection.conn.setAutoCommit(false);

        if (Tptbm.prefetchClose) {
	  ((TimesTenConnection)connection.conn).setTtPrefetchClose(true);
	}

        // If scaleout, get connection's replica set id
        if (  Tptbm.runMode > Tptbm.modeClassic ) {
            Statement stmt = null;
            ResultSet rs = null;
            stmt = connection.conn.createStatement();
            rs = stmt.executeQuery(getRsIdStmt);
            if (  rs.next()  ) 
                myRsId = rs.getInt(1);
        }

	connection.conn.commit();
	if(Tptbm.threadtrace)
	    System.out.println("Thread " + id + " is preparing statements"  );
	connection.selStmt = connection.conn.prepareStatement(selectStmt);
	connection.insStmt = connection.conn.prepareStatement(Tptbm.insertStmt);
	connection.delStmt = connection.conn.prepareStatement(Tptbm.deleteStmt);
	connection.updStmt = connection.conn.prepareStatement(updateStmt);
	if (Tptbm.usePlsql) {
	   connection.initPlsqlStmt = connection.conn.prepareStatement(Tptbm.init_plsql_stmnt);
	   connection.execPlsqlStmt = connection.conn.prepareStatement(Tptbm.exec_plsql_stmnt);
	}

        if (  Tptbm.runMode == Tptbm.modeScaleoutRouting ) {
            routingInfo = new TptbmRouting(Tptbm.numReplicaSets);
            routingInfo.buildRouting(ttds,connection.conn, Tptbm.prefetchClose,
                                     selectStmt,Tptbm.insertStmt,
                                     updateStmt,Tptbm.deleteStmt);
        }

	connection.conn.commit();	

	insert_start = Tptbm.key + id;
	delete_start = id;
    }

    //--------------------------------------------------
    // method: cleanup
    // closes all the prepared statements and the connection
    //--------------------------------------------------

    public void cleanup() {
	try {
	    connection.conn.commit();
	    if(Tptbm.threadtrace)
		System.out.println("Thread " + id + " is closing statements"  );
	    connection.selStmt.close();
	    connection.insStmt.close();
	    connection.updStmt.close();
	    connection.delStmt.close();
	    if (Tptbm.usePlsql) {
		connection.initPlsqlStmt.close();
		connection.execPlsqlStmt.close();
	    }
	    connection.conn.close();
            if (  Tptbm.runMode == Tptbm.modeScaleoutRouting )
                routingInfo.cleanup();
	}
	catch ( SQLException e ) {
	    Tptbm.printSQLException(e);
            error = true;
	}
    }
    
} // class TptbmThread


//--------------------------------------------------
// Class TptbmThreadController
// This module is responsible - 
// 1. creating the threads 
// 2. makeing sure that all the threads are 
//    ready to execute before they start to execute 
// 3. keeping program from exiting before all the 
//    threads are done
// 4. Timing the execution of threads
//--------------------------------------------------

class TptbmThreadController {

    // List of all the threads
    private TptbmThread[] threads; 

    // number of threads
    private int numThreads;

    // Number of transactions
    private int numXacts;
    
    // Run duration (measured)
    private int testDuration;
    
    // Ramp time
    private int rampTime;
    
    // Timer
    private Timer clock = new Timer();

    // Timer valus
    private long tstart = 0;
    private long tcurr = 0;
    private long tnext = 0;
    private long tstop = 0;

    // Constructor
    public TptbmThreadController(int numThreads, int numXacts, int testDuration, int rampTime) {
	threads = new TptbmThread[numThreads];
	this.numThreads = numThreads;
	this.numXacts = numXacts;
	this.testDuration = testDuration;
	this.rampTime = rampTime;
    }

    private boolean threadError() {
        for (int i = 0; i < numThreads; i++)
            if ( threads[i].error  )
                return true;
        return false;
    }

    private void stopThreads()
    {
        for (int i = 0; i < numThreads; i++)
            threads[i].mustStop = true;
    }
    
    private void msSleep(int ms) {
        try {
            Thread.sleep(ms);
        } catch ( InterruptedException ie ) { ; }
    }

    //--------------------------------------------------
    // Method: start
    //--------------------------------------------------
    public synchronized boolean start() {

        boolean error = false;

        try {

	    if(Tptbm.threadtrace)
	        System.out.println("Create threads");

	    // Instantiate the threads
	    for(int i=0; i <numThreads; i++) {
	        threads[i] = new TptbmThread(i,numXacts,testDuration,rampTime);
	    }

	    if(Tptbm.threadtrace)
	        System.out.println("Start all the threads");

	    // Start all the threads
	    for ( int i=0; i < numThreads; i++ )
	        threads[i].start();	

	    if(Tptbm.threadtrace)
	        System.out.println("Check if all the threads are ready");
        
	    // Let all the threads initialize before
	    // they start executing
	    for( int i=0; i < numThreads; i++ ) {     
            // Block and check the readiness
	        while(!threads[i].readyYet() && !threads[i].error);
	    }
            if (threadError()) {
                throw new TptbmError();
            }

	    // Start the clock
	    tstart = clock.start();
            if (  rampTime > 0  )
                tnext = tstart + (1000 * rampTime);
            else
            if (  testDuration > 0  )
                tnext = tstart + (1000 * testDuration);
    
	    if(Tptbm.threadtrace)
	        System.out.println(
                      "Signal all the threads to go ahead and start execution");
    
	    // Start all the threads
	    for ( int i=0; i < numThreads; i++ )
            {
                if (  testDuration > 0  ) { // duration mode
                    if (  rampTime > 0  ) 
                        threads[i].rampStart();
                    else
                        threads[i].timingStart();
                }
	        threads[i].setGo();	
            }
    
            // Wait for ramp up to finish, if required
            if (  rampTime > 0  )
            {
                tcurr = clock.getCurrent();
                while (  tcurr < tnext  ) {
                    msSleep(1);
                    if (threadError()) {
                        throw new TptbmError();
                    }
                    tcurr = clock.getCurrent();
                }
                System.out.println("Ramp-up done...measuring begins");
                // start timing
	        for ( int i=0; i < numThreads; i++ )
                {
                    threads[i].timingStart();
                    tstart = tcurr;
                }
                tnext = tcurr + (1000 * testDuration);
            }

            // Wait for timing to finish, if required
            if (  testDuration > 0  )
            {
                tcurr = clock.getCurrent();
                while (  tcurr < tnext  ) {
                    msSleep(1);
                    if (threadError()) {
                        throw new TptbmError();
                    }
                    tcurr = clock.getCurrent();
                }
                // stop timing, start ramping (if required)
                if (  rampTime > 0  )
                {
                    tnext = tcurr + (1000 * rampTime);
	            for ( int i=0; i < numThreads; i++ )
                        threads[i].rampStart();
                }
	        for ( int i=0; i < numThreads; i++ )
                    threads[i].timingStop();
                tstop = clock.stop();
            }

            // Wait for ramp down to finish, if required
            if (  rampTime > 0  )
            {
                tcurr = clock.getCurrent();
                while (  tcurr < tnext  ) {
                    msSleep(1);
                    if (threadError()) {
                        throw new TptbmError();
                    }
                    tcurr = clock.getCurrent();
                }
                // stop ramping
                for ( int i=0; i < numThreads; i++ )
                    threads[i].rampStop();
                System.out.println("Ramp-down done...terminating");
            }

	    if(Tptbm.threadtrace)
	        System.out.println(
                      "Wait for all the threads to finish before exit");

	    // Wait for all the threads to finish 
	    // before you exit
	    for( int i=0; i < numThreads; i++ ) {
	        int j = 0;
	        while(!threads[i].doneYet()) {
		    j++;
	        }
	    }

	    // Stop the clock
            if (  numXacts > 0  )
	        tstop = clock.stop();	
        } catch ( TptbmError te ) {
            stopThreads();
            error = true;
        }

	// Cleanup all the threads
	for( int i=0; i < numThreads; i++ ) {
	    threads[i].cleanup();
	}

        return ! error;
    }
    
    public TptbmThread getThread(int tno) {
        return threads[tno];
    }

    //--------------------------------------------------
    // Method: getElapsedTime
    //--------------------------------------------------
    public long getElapsedTime() {
	// return clock.getTimeInMs();
	return (tstop - tstart);
    }

}


//--------------------------------------------------
// Class: Timer
//--------------------------------------------------
class Timer {

    private long startTime = -1;
    private long endTime = -1;
    
    public long start() {
	startTime = System.currentTimeMillis();
        return startTime;
    }

    public long stop() {
	endTime = System.currentTimeMillis();
        return endTime;
    }

    public long getCurrent() {
        return System.currentTimeMillis();
    }

    public long getElapsed() {
        if ( startTime <= 0  )
            return 0;
        else
            return (System.currentTimeMillis() - startTime);
    }

    public long getTimeInMs() {

	if((startTime == -1) || (endTime == -1)) {
	    System.err.println("call start() and stop() before this method");
	    return -1;
	}
	else if((endTime == startTime)) {
	    System.err.println(
                  "the start time and end time are the same...returning 1ms");
	    return 1;
	}
	else
	    return (endTime - startTime);
	
    }
}
