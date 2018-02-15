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
import com.timesten.jdbc.TimesTenConnection;
class Tptbm
{

    // class constants
    static int TPTBM_ORACLE = 0;
    static int TPTBM_TIMESTEN = 1;
    static String dfltUsername = "appuser";

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
    static public int  numXacts = 10000;

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
    static public boolean commitReads = false;

    // Used to turn on PrefetchClose ON with client/server
    static public boolean prefetchClose = false;

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
    static public int dbms = TPTBM_TIMESTEN;

    // Range Index
    public static boolean range = false;

    // PL/SQL
    public static boolean usePlsql = false;

    //--------------------------------------------------
    // class constants
    //--------------------------------------------------

    public static final String insertStmt = 
	"insert into vpn_users values (?,?,?,'0000000000',?)";

    public static final String deleteStmt = 
	"delete from vpn_users where vpn_id = ? and vpn_nb = ?";

    //--------------------------------------------------
    // member constants
    //--------------------------------------------------

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
	"descr              CHAR(100) NOT NULL)";

    private static final String createAltDB = 
	"CREATE TABLE vpn_users(" +
	"vpn_id             number(10)   NOT NULL," + 
	"vpn_nb             number(10)   NOT NULL," +
	"directory_nb       CHAR(10)  NOT NULL," +
	"last_calling_party CHAR(10)  NOT NULL," +
	"descr              CHAR(100) NOT NULL," +
	"PRIMARY KEY (vpn_id,vpn_nb)) ";

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

    //-----------------------------
    // member variables
    //-----------------------------
    static private TptbmThread[] threads;
  

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
	long tps;
	
	// New version w/ wait()/notify() calls.

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
		Class.forName("com.timesten.jdbc.TimesTenDriver");
		Class.forName("com.timesten.jdbc.TimesTenClientDriver");
	    }

	} catch (java.lang.ClassNotFoundException e) {
	    System.err.println(e.getMessage());
	}

        System.out.println();
        System.out.println("Connecting to the database ...\n");

        // Prompt for the Username and Password
        getUsername();
        getPassword();

	// Connect here to prevent disconnect/unload by populate 
	try {

	    if (!nobuild) {
		if(Tptbm.dbms == Tptbm.TPTBM_ORACLE) {
		    conn = DriverManager.getConnection(Tptbm.url, username, password);
		}
		else {
		    conn = DriverManager.getConnection(Tptbm.url, username, password);
		}

                System.out.println();
                System.out.println("Connected to database as " + username + "@" + Tptbm.url);

		// Disable auto-commit mode
		conn.setAutoCommit(false);

		// populate the database if nobuild option is not specified
		populate (conn);

		if (buildonly) {
		    conn.close();
		    System.exit(0);
		}
	    }

	    controller = new TptbmThreadController(numThreads);
            System.out.println();
	    System.out.print("Begining execution with " + numThreads + " thread(s): ");
            System.out.print(reads + "% read, " + updates + "% update, ");
            System.out.print(inserts + "% insert, " + deletes + "% delete");
            System.out.println();

	    controller.start();
	    elapsedTime = controller.getElapsedTime();
	    tps = (long)(numThreads * ((double)numXacts/elapsedTime) * 1000);

	    if (!nobuild)
		conn.close();


	    // Print the output
	    System.out.println();

	    System.out.print("Elapsed Time:         ");
	    System.out.printf("%9d ms\n", elapsedTime);

	    System.out.print("Transactions Per Sec: ");
	    System.out.printf("%9d\n", tps);

	} catch ( SQLException e ) {
	    printSQLException(e);
	}
    }


    //--------------------------------------------------
    // usage()
    //--------------------------------------------------
    static private void usage() {
	
	System.err.println
	    (
	     "\n" +
	     "Usage: Tptbm [-h | -help]\n" +
             "         [<-url url_string>] [-threads <num_threads>] [-reads <read_%>]\n" +
	     "         [-inserts <insert_%>] [-deletes <delete_%>] [-xacts <num_xacts>]\n" +
	     "         [-min <min_xacts>] [-max <max_xacts>] [-seed <seed>] [-multiop]\n" +
             "         [-dbms <dbms_name>] [-CSCommit] [-commitReads] [-range] [-key <keys>]\n" +
	     "         [-plsql] [-trace] [-nobuild | -build]\n\n" +
	     "  -h                     Prints this message and exits.\n" + 
	     "  -help                  Same as -h.\n" +
	     "  -url <url_string>      Specifies JDBC url string of the database\n" +
	     "                         to connect to.\n" +
	     "  -threads <num_threads> Specifies the number of concurrent \n" +
	     "                         threads. The default is 4.\n" +
	     "  -reads   <read_%>      Specifies the percentage of read-only\n" +
	     "                         transactions. The default is 80.\n" +
	     "  -inserts <insert_%>    Specifies the percentage of insert\n" +
	     "                         transactions. The default is 0.\n" +
	     "  -deletes <delete_%>    Specifies the percentage of delete\n" +
	     "                         transactions. The default is 0.\n" +
	     "                         The default value for keys is 100.\n" +
	     "  -xacts   <xacts>       Specifies the number of transactions\n" +
	     "                         that each thread should run.\n" + 
	     "                         The default is 10000.\n" +
             "  -min   <min_xacts>     Minimum operations per transaction.\n" +
             "                         Default is 1.\n" +
             "  -max   <max_xacts>     Maximum operations per transaction.\n" +
             "                         Default is 1.\n" +
             "                         Operations in a transaction randomly chosen\n" +
             "                         between min and max.\n" +
             "  -seed    <seed>        Specifies the seed for the random \n" +
             "                         number generator.\n" +
	     "  -multiop               1 insert, 3 selects, 1 update / transaction.\n" +
	     "  -dbms   <dbms_name>    Use timesten or oracle. timesten is the default.\n" +
	     "  -CSCommit              Turn on prefetchClose for client/server.\n" +
	     "  -commitReads           By default reads are not committed.\n" +
	     "  -range                 Range rather than hash index.\n" +
	     "  -key     <keys>        Specifies the number of records (squared)\n" +
	     "                         to initially populate in the data store.\n" +
	     "  -plsql                 Use PL/SQL for -multiop.\n" +
	     "  -trace                 Turns JDBC tracing on.\n" +
	     "  -nobuild               Do not build the database.\n" +
	     "  -build                 Builds database and exits.\n"
	     );
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

    static private void populate (Connection conn) {

	Statement stmt;
	PreparedStatement pstmt;
	int outerIndex, innerIndex;
	int cc = 0;
	Timer clock = new Timer();
	int pages = (key*key)/256;

      System.out.println();
	System.out.println("Populating benchmark database ...");
	try {

	    if (pages < 40) pages = 40;
	    if (multiop == 1) {
	        /* size for expected number of inserts, 20% of -xact option */
		int moppgs = (numXacts * numThreads * 2)/(256*10);
		if (moppgs > pages)
		    pages = moppgs;
	    }

	    // Create a Statement object
	    stmt = conn.createStatement();

	    // Execute the create table statement
	    if(Tptbm.dbms == Tptbm.TPTBM_ORACLE) {
		try {
		    stmt.executeUpdate("drop table vpn_users");
		} catch ( SQLException ex )
		{
		    // do nothing
		}
		stmt.executeUpdate(createAltDB);
	    }
	    else {
              try {
           		    stmt.executeUpdate("drop table vpn_users");
		  } catch ( SQLException ex )
		  {
		    // do nothing
		  }

             if (range) {
		stmt.executeUpdate(createStmtRange);
		stmt.executeUpdate("create unique index vpn_idx " +
				   "on vpn_users(vpn_id,vpn_nb)");
	     } 
	     else
		stmt.executeUpdate(createStmtHash + pages);
	    }

	    // Prepare the insert statement
	    pstmt = conn.prepareStatement(insertStmt);

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
		    pstmt.setString(3, "55"+innerIndex+outerIndex);
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
		System.out.println ("Database populated with " + 
				    Tptbm.key*Tptbm.key + 
				    " rows in " +
				    ms + " ms" + " (" +
				    tps + " TPS)");
	    }

	} catch ( SQLException e ) {
	    printSQLException(e);
	}
    }


    //--------------------------------------------------
    //
    // Function: parseArgs
    // 
    // Parses command line arguments
    //
    //--------------------------------------------------
    private static void parseArgs( String[] args) {
	int i=0;
	//	System.out.println("args.length: " + args.length);	
	while(i<args.length) {

	    // Command line help
	    if(args[i].equalsIgnoreCase("-h") || 
	       args[i].equalsIgnoreCase("-help")) {
		usage();
	    }

	    // JDBC url string
	    else if(args[i].equalsIgnoreCase("-url")) {
		if(i > args.length) {
		    usage();
		}
		
		url = args[i+1];
		i += 2;
	    }

	    // number of threads
	    else if(args[i].equalsIgnoreCase("-threads")) {
		if(i > args.length) {
		    usage();
		}
		numThreads = Integer.parseInt(args[i+1]);
		i+=2;
	    }

	    // number of transactions for each thread
	    else if(args[i].equalsIgnoreCase("-xacts")) {
		if(i > args.length) {
		    usage();
		}
		numXacts = Integer.parseInt(args[i+1]);
			i+=2;
	    }
	    
            // seed for random number generator
            else if(args[i].equalsIgnoreCase("-seed")) {
                if(i > args.length) {
                    usage();
                }
                seed = Integer.parseInt(args[i+1]);
                i+=2;
            }

	    // Percentage of read transactions
	    else if(args[i].equalsIgnoreCase("-reads")) {
		if(i > args.length) {
		    usage();
		}
		reads = Integer.parseInt(args[i+1]);
		i+=2;
	    }

	    // percentage of insert transactions
	    else if(args[i].equalsIgnoreCase("-inserts")) {
		if(i > args.length) {
		    usage();
		}
		inserts = Integer.parseInt(args[i+1]);
		i+=2;
	    }

	    // percentage of delete transactions
	    else if(args[i].equalsIgnoreCase("-deletes")) {
		if(i > args.length) {
		    usage();
		}
		deletes = Integer.parseInt(args[i+1]);
		i+=2;
	    }

	    // key to determine number of records to 
	    // insert initially
	    else if(args[i].equalsIgnoreCase("-key")) {
		if(i > args.length) {
		    usage();
		}
		key = Integer.parseInt(args[i+1]);
		i+=2;
		if(key < 0)
		    System.err.println(
			  "key flag requires a positive integer argument");
	    }

	    // minimum number of statements/transaction
	    else if(args[i].equalsIgnoreCase("-min")) {
		if(i > args.length) {
		    usage();
		}
		min_xact = Integer.parseInt(args[i+1]);
		i+=2;
		if(key < 0)
		    System.err.println(
			  "-min flag requires a positive integer argument");
	    }

	    // maximum number of statements/transaction
	    else if(args[i].equalsIgnoreCase("-max")) {
		if(i > args.length) {
		    usage();
		}
		max_xact = Integer.parseInt(args[i+1]);
		i+=2;
		if(key < 0)
		    System.err.println(
                          "-max flag requires a positive integer argument");
	    }

	    else if(args[i].equalsIgnoreCase("-dbms")) {
		if(i > args.length) {
		    usage();
		}
		if(args[i+1].equalsIgnoreCase("oracle"))
		   dbms = TPTBM_ORACLE;
		i += 2;
	    }

	    // prefetchClose ON
	    else if(args[i].equalsIgnoreCase("-CSCommit")) {
		if(i > args.length) {
		    usage();
		}
		prefetchClose = true;
		i += 1;
	    }


	    // multiop
	    else if(args[i].equalsIgnoreCase("-multiop")) {
		if(i > args.length) {
		    usage();
		}
		multiop = 1;
		min_xact = 5;
		max_xact = 5;
		reads = 60;
		inserts = 20;
		i += 1;
	    }

	    // Range
	    else if(args[i].equalsIgnoreCase("-range")) {
		if(i > args.length) {
		    usage();
		}
		range = true;
		i += 1;
	    }

	    // JDBC tracing
	    else if(args[i].equalsIgnoreCase("-trace")) {
		if(i > args.length) {
		    usage();
		}
		trace = true;
		i += 1;
	    }

	    // Thread tracing: to complement JDBC tracing
	    else if(args[i].equalsIgnoreCase("-threadtrace")) {
		if(i > args.length) {
		    usage();
		}
		threadtrace= true;
		i += 1;
	    }

	    // Commit reads - to match behavior prior to 5.0
	    else if(args[i].equalsIgnoreCase("-commitReads")) {
		if(i > args.length) {
		    usage();
		}
		commitReads = true;
		i += 1;
	    }
	    // Do not build the database
	    else if(args[i].equalsIgnoreCase("-nobuild")) {
		if(i > args.length) {
		    usage();
		}
		nobuild = true;
		i += 1;
	    }
	    // Build and exit
	    else if(args[i].equalsIgnoreCase("-build")) {
		if(i > args.length) {
		    usage();
		}
		buildonly = true;
		i += 1;
	    }
	    // PL/SQL
	    else if(args[i].equalsIgnoreCase("-plsql")) {
		if(i > args.length) {
		    usage();
		}
		usePlsql = true;
		i += 1;
	    }
	    else {
		usage();
	    }
	}

	if ((reads + inserts + deletes) > 100){
	    System.err.println();
	    System.err.println("Transaction mix should not exceed 100");
	    usage();
	}

      if ( (reads < 0) || (inserts < 0) || (deletes < 0)) {
	    System.err.println();
	    System.err.println("Percent of transaction types should not be negative");
	    usage();
      }

      // How many updates
      updates = 100 - reads - inserts - deletes;

	if (multiop == 0) {
	  if(numThreads * ( numXacts/100 * (float)inserts) > (key * key)) {
	    System.err.println("Inserts as part of transaction mix exceed\n" +
			       "number initially populated into data store." );
	    usage();
	  }

	  if(numThreads * ( numXacts/100 * (float)deletes) > (key * key)) {
	    System.err.println("Deletes as part of transaction mix exceed\n" +
			       "number initially populated into data store." );
	    usage();
	  }
	}

	if (nobuild && buildonly) {
	    System.err.println("Please specify either -nb OR -build");
	    usage();
	}
    
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

	if (usePlsql) {
	    if (multiop < 1) {
		System.err.println("PL/SQL only works with multiop\n");
		usage();
	    }
	    System.err.println("PL/SQL enabled\n");
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
    

    //--------------------------------------------------
    // Member variables
    //--------------------------------------------------

    // connection for the thread
    private Connection connection;

    // Prepared select statement for read xacts
    private PreparedStatement prepSelStmt;

    // Prepared Insert statement for insert xacts
    private PreparedStatement prepInsStmt;

    // Prepared Delete statement for delete xacts
    private PreparedStatement prepDelStmt;

    // Prepared Update statement for update xacts
    private PreparedStatement prepUpdStmt;

    // Prepared PL/SQL Init statement for plsql xacts
    private PreparedStatement prepPlInitStmt;

    // Prepared PL/SQL statement for plsql xacts
    private PreparedStatement prepPlStmt;

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
    

    //--------------------------------------------------
    // Constructor
    //--------------------------------------------------
    public TptbmThread(int id) {
	if(Tptbm.threadtrace)
	    System.out.println("constructing thread: " + id);
	this.id = id;
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
	}
	finally {
	    // does not harm to set the ready again in case we get an exception 
	    // in the intialization and parent is still wating for ready flag.
	    setReady();
	    setDone(); 
	}
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
    

    //--------------------------------------------------
    // Method : execute 
    // It executes specified number of transaction for 
    // the thread. Based on the transaction mix specified
    // it executes a read/insert/update
    //--------------------------------------------------
    private void execute() throws SQLException {

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
	    for (int i = 0; i < Tptbm.numXacts; i++) {
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
	    }
	} else { // ! usePlsql
	    for(int i=0; i < Tptbm.numXacts; i++)	{

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
		    
		        if (Tptbm.commitReads && jj == 0) {
		    	    connection.commit();
		        }
		    }
		    
		    // Execute update transaction
		    else if ( path == 0 ) {
    
		        // pick random values for key from the 
                        // range 0 -> key-1
		        id_int = (int)((Tptbm.key-1)*rand.nextFloat());
		        nb_int = (int)((Tptbm.key-1)*rand.nextFloat());
		        prepUpdStmt.setString(1, id_int + "x" + nb_int);
		        prepUpdStmt.setInt(2, id_int);
		        prepUpdStmt.setInt(3, nb_int);
		        if(Tptbm.threadtrace)
			    System.out.println("Thread " + id + 
					       " is excuting update"  );
		        prepUpdStmt.executeUpdate();
		        if (jj == 0) {
			    connection.commit();
		        }
		    }
    
		    // Execute delete transaction
		    else if (path == 3) {
		        id_int = delete_start;
		        nb_int = delete_present++;
		        prepDelStmt.setInt(1, id_int);
		        prepDelStmt.setInt(2, nb_int);
		        if(Tptbm.threadtrace)
			    System.out.println("Thread " + id + 
					       " is excuting delete"  );
		        if (delete_present == Tptbm.key) {
			    delete_present = 0;
			    delete_start += Tptbm.numThreads;
		        }
		    
		        prepDelStmt.executeUpdate();
		        if (jj == 0) {
			    connection.commit();
		        }
		    }
    
		    // Execute insert transaction
		    else {
		        id_int = insert_start;
		        nb_int = insert_present++;
		        prepInsStmt.setInt(1, id_int);
		        prepInsStmt.setInt(2, nb_int);
		        prepInsStmt.setString(3, "55"+id_int+nb_int);
		        prepInsStmt.setString(4, 
                                           "<place holder for description " +
					   "of VPN " + id_int + " extension " + 
					   nb_int + ">");
		        if(Tptbm.threadtrace)
			    System.out.println("Thread " + id + 
					       " is excuting insert"  );
		        if (insert_present == Tptbm.key) {
			    insert_present = 0;
			    insert_start += Tptbm.numThreads;
		        }
		    
		        prepInsStmt.executeUpdate();
		        if (jj == 0) {
			    connection.commit();
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

	//attributes.setProperty("overwrite", "0");
	    //attributes.setProperty("temporary", "0");
	    //attributes.setProperty("exclaccess", "0");

	// Connect to the data store
	if(Tptbm.threadtrace)	
	    System.out.println("Thread " + id + " is connecting"  );

	connection = DriverManager.getConnection(Tptbm.url, 
                                                 Tptbm.username, 
                                                 Tptbm.password);
	
	// set autocommit off
	connection.setAutoCommit(false);

        if (Tptbm.prefetchClose) {
	  ((TimesTenConnection)connection).setTtPrefetchClose(true);
	}
	connection.commit();
	if(Tptbm.threadtrace)
	    System.out.println("Thread " + id + " is preparing statements"  );
	prepSelStmt = connection.prepareStatement(selectStmt);
	prepInsStmt = connection.prepareStatement(Tptbm.insertStmt);
	prepDelStmt = connection.prepareStatement(Tptbm.deleteStmt);
	prepUpdStmt = connection.prepareStatement(updateStmt);
	if (Tptbm.usePlsql) {
	   prepPlInitStmt = connection.prepareStatement(Tptbm.init_plsql_stmnt);
	   prepPlStmt = connection.prepareStatement(Tptbm.exec_plsql_stmnt);
	}

	connection.commit();	

	insert_start = Tptbm.key + id;
	delete_start = id;
	
    }

    //--------------------------------------------------
    // method: cleanup
    // closes all the prepared statements and the connection
    //--------------------------------------------------

    public void cleanup() {
	try {
	    // commit outstanding activity to avoid function sequence error
	    connection.commit();
	    if(Tptbm.threadtrace)
		System.out.println("Thread " + id + " is closing statements"  );
	    prepSelStmt.close();
	    prepInsStmt.close();
	    prepUpdStmt.close();
	    if (Tptbm.usePlsql) {
		prepPlInitStmt.close();
		prepPlStmt.close();
	    }
	    connection.close();
	}
	catch ( SQLException e ) {
	    Tptbm.printSQLException(e);
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

    
    Timer clock = new Timer();

    // Constructor
    public TptbmThreadController(int numThreads) {
	threads = new TptbmThread[numThreads];
	this.numThreads = numThreads;
    }

    //--------------------------------------------------
    // Method: start
    //--------------------------------------------------
    public synchronized void start() {

	if(Tptbm.threadtrace)
	    System.out.println("Create threads");

	// Instantiate the threads
	for(int i=0; i <numThreads; i++) {
	    threads[i] = new TptbmThread(i);
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
	    while(!threads[i].readyYet() );
	}


	// Start the clock
	clock.start();

	if(Tptbm.threadtrace)
	    System.out.println(
                  "Signal all the threads to go ahead and start execution");

	// Start all the threads
	for ( int i=0; i < numThreads; i++ )
	    threads[i].setGo();	

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
	clock.stop();	

	// Cleanup all the threads
	for( int i=0; i < numThreads; i++ ) {
	    threads[i].cleanup();
	}

    }
    

    //--------------------------------------------------
    // Method: getElapsedTime
    //--------------------------------------------------
    public long getElapsedTime() {
	return clock.getTimeInMs();
    }

}


//--------------------------------------------------
// Class: Timer
//--------------------------------------------------
class Timer {

    private long startTime = -1;
    private long endTime = -1;
    
    public void start() {
	startTime = System.currentTimeMillis();
    }
    public void stop() {
	endTime = System.currentTimeMillis();
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
