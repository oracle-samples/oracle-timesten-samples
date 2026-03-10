/*
 * JMS/XLA syncJMS2.java sample code - Processing JMS/XLA updates using jakarta jms.
 *
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 * Note:  JMS provides two ways to read messages - one using 
 * synchronous calls to the receive() method (used here), 
 * and one using asynchronous calls to the onMessage method (used
 * in asyncJMS2.java).  TimesTen's JMS/XLA facility supports both approaches
 * However, the receive() method is generally faster. 
 * Please consider it for performance sensitive applications.
 */

// JMS imports
import jakarta.jms.JMSException;
import jakarta.jms.MapMessage;
import jakarta.jms.Session;
import jakarta.jms.Topic;
//import jakarta.jms.TopicConnection;
import jakarta.jms.TopicConnectionFactory;
import jakarta.jms.TopicSession;
import jakarta.jms.TopicSubscriber;
import javax.naming.Context;
import javax.naming.InitialContext;

// JDBC imports
import java.sql.DriverManager;
import java.sql.CallableStatement;
import java.sql.SQLException;
import java.sql.Connection;

// Other imports
import java.io.BufferedReader;
import java.io.IOException;
import java.util.Collections;
import java.util.Enumeration;
import java.util.StringTokenizer;
import java.util.Vector;
import java.util.*;


/**
 * Demo which shows how to use JMS/XLA to process updates.
 * It synchronously subscribes to updates for a table defined at runtime from the command line
 */
public class syncJMS2
{

  /** Subscriber */
  private Subscriber sub;

  /** table name */
  private static String tableName = "customer";

  /** JMS/XLA topic name */
  private static String topicName = "syncJMS2";

  /** XLA bookmark name */
  private static String bookmarkName = "bookmark";

  /** JDBC user name */
  public static String jdbcUsername = "appuser";

  /** XLA user name */
  public static String xlaUsername = "xlauser";

  /** XLA password */
  public static String xlaPassword = null;

  /** TimesTen dataserver driver name */
  private static final String TTDRIVER = "com.timesten.jdbc.TimesTenDriver";

  /** prefix for JDBC connection URL */
  private static final String TTPREFIX = "jdbc:timesten:direct:";

  /** Set to shut things down */
  private boolean shuttingDown = false;

  /** Set when finished shutting things down */
  private boolean shutdownDone = false;

  /** Standard input */
  private static BufferedReader stdinReader;

  /** timeout on receive calls (milliseconds) */
  private static final int TIMEOUT = 200;

  private int deleteCnt = 0;
  private boolean ntBatchDemo = false;


  /**
   * Main routine.
   * @param args Command line arguments.
   */
  public static void main(final String[] args)
  {

    // Specify the topic "syncJMS2" to get the connection parameters from
    // the syncJMS2 demo entry in jmsxla.xml.
    final syncJMS2 demo = new syncJMS2();

    // parse command line arguments
    parseArgs(args);

    log("topic = " + topicName + ", bookmark = " + bookmarkName + ", schema = " + jdbcUsername + ", table = " + tableName);

    int status = 1; // exit code - 0 for success, 1 for error

    // Setup the XLA Bookmark and subscription
    demo.init();

    try
    {

      // Add a shutdown hook to clean things up on interrupts.
      // Works for Windows console apps but not for Windows services:
      //  -  http://forum.java.sun.com/thread.jspa?threadID=5124003&tstart=0
      Runtime.getRuntime().addShutdownHook(new Thread("Shutdown thread")
      {
        public final void run()
        {

          demo.shutdown(); synchronized(demo)
          {
            while (!demo.isDone())
            {
              try
              {
                demo.wait(); 
              }
              catch (InterruptedException e)
              {
                e.printStackTrace(); 
              }
            } // while
          } // synchronized(demo)
        } // run
      } );

      // start the demo
      try
      {
        demo.run(demo.jdbcUsername); 
      }
      catch (Throwable e)
      {
        System.err.println("Caught " + e); System.err.println("At:"); e.printStackTrace(System.err); 
      }

    }
    catch (Throwable e)
    {
      log("main caught exception: " + e); status = 1; 
    }

    log("Exiting"); System.exit(status); 
  } // main


  /**
   * Log a message.
   * @param msg Message to log.
   */
  private static final void log(final String msg)
  {
    System.out.println(); System.out.println(msg); 
  } // log

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

    while (i < args.length)
    {

      // Command line help
      if (args[i].equalsIgnoreCase("-h") || args[i].equalsIgnoreCase("-help") || args[i].equalsIgnoreCase("-?"))
      {
        usage(); 
      }

      // JMS TOPIC
      else if (args[i].equalsIgnoreCase("-topic"))
      {
        if (i > args.length)
        {
          usage(); 
        }

        topicName = args[i + 1]; i += 2; 
      }

      // XLA Bookmark
      else if (args[i].equalsIgnoreCase("-bookmark"))
      {
        if (i > args.length)
        {
          usage(); 
        }

        bookmarkName = args[i + 1]; i += 2; 
      }

      // table's schema
      else if (args[i].equalsIgnoreCase("-schema"))
      {
        if (i > args.length)
        {
          usage(); 
        }

        jdbcUsername = args[i + 1]; i += 2; 
      }

      // User table
      else if (args[i].equalsIgnoreCase("-table"))
      {
        if (i > args.length)
        {
          usage(); 
        }

        tableName = args[i + 1]; i += 2; 
      }

      // XLA username
      else if (args[i].equalsIgnoreCase("-xlauser"))
      {
        if (i > args.length)
        {
          usage(); 
        }

        xlaUsername = args[i + 1]; i += 2; 
      }

      // XLA password
      else if (args[i].equalsIgnoreCase("-xlapassword"))
      {
        if (i > args.length)
        {
          usage(); 
        }

        xlaPassword = args[i + 1]; i += 2; 
      }

      else
      {
        usage(); 
      }
    } // while 

  } // parseArgs  


  //--------------------------------------------------
  // usage()
  //--------------------------------------------------
  static private void usage()
  {

    System.err.print("\n" + "Usage: \n\n" + "  syncJMS2 [-h] [-help] [-?]\n" + 
                     "  syncJMS2 [-topic <topicName>] [-bookmark <bookmarkName>]\n" + 
                     "          [-schema <schemaName>] [-table <tableName>]\n" + 
                     "          [-xlauser <username>] [-xlapassword <password>]\n\n" + 
                     "  -h                        Prints this message and exits.\n" + 
                     "  -help                     Same as -h.\n" + 
                     "  -?                        Same as -help.\n" + 
                     "  -topic <topicName>        The JMS Topic of interest.\n" + 
                     "                            Defaults to 'syncJMS2'.\n" + 
                     "  -bookmark <bookmarkName>  The XLA bookmark.\n" + 
                     "                            Defaults to 'bookmark'.\n" + 
                     "  -schema <schemaName>      The schema for the table of interest.\n" + 
                     "                            Defaults to 'appuser'.\n" + 
                     "  -table <tableName>        The table of interest.\n" + 
                     "                            Defaults to 'customer'.\n" + 
                     "  -xlauser <username>       The username of the user with the XLA privilege.\n" + 
                     "                            Default xlauser.\n" + 
                     "  -xlapassword <password>   The password of the user with the XLA privilege.\n\n"); 

    System.exit(1); 
  } // usage



  public void init()
  {

    Connection conn = null; CallableStatement cStmt = null; 

    // Get a JDBC connection to TimesTen to call some XLA built-ins
    try
    {

      //Prompt for the XLA Username and Password
      AccessControl ac = new AccessControl(); 

      if (xlaPassword == null)
      {
        xlaPassword = ac.getPassword("xlauser"); 
      }

      Class.forName(TTDRIVER); 
      conn = DriverManager.getConnection(TTPREFIX + tt_version.sample_DSN, xlaUsername, xlaPassword); 
      conn.setAutoCommit(false); 

    }
    catch (ClassNotFoundException cnfe)
    {
      System.err.println("Got " + cnfe + " (driver=" + TTDRIVER + ")"); System.exit( - 1); 
    }
    catch (Exception e)
    {
      System.err.println("Got " + e); System.err.println("At:"); e.printStackTrace(); System.exit( - 1); 
    }

    // Call the XLA build-ins
    try
    {

      final String bookmarkCreate = "call ttxlabookmarkcreate('" + bookmarkName + "')"; 
      final String schema_tablename = jdbcUsername + "." + tableName; 
      final String xlaSubscribe = "call ttxlasubscribe('" + schema_tablename + "', '" + bookmarkName + "')"; 

      // Create the XLA bookmark 
      cStmt = conn.prepareCall(bookmarkCreate); cStmt.execute(); 

      // Create the XLA subscription 
      cStmt = conn.prepareCall(xlaSubscribe); cStmt.execute(); 

    }
    catch (SQLException sqle)
    {
      //sqle.printStackTrace();
    }

    try
    {
      conn.commit(); conn.close(); 
    }
    catch (SQLException sqle)
    {
      //sqle.printStackTrace();
    }

    log("Initialized XLA environment"); 
  }


  public final void run(String schema)throws Throwable
  {

    final String schema_table = schema + "." + tableName; 

    try
    {
      Subscriber sub = new Subscriber(topicName, bookmarkName); 
      String connStr = sub.getConnStr(); 
      log("Using connection string '" + connStr + "'"); 

      log("Waiting for XLA updates on table " + schema_table + " ... "); 
      log("- Please use ttIsql or level1 to modify the " + schema_table + " table"); 

      while (!isShuttingDown())
      {
        // get JMS messages
        sub.get(TIMEOUT); 
      }
      log("shutting down ..."); 
      log("cleaning up"); 
      sub.close(); 
      setShutdownDone(true); 
    }
    catch (Throwable ex)
    {
      // don't get stuck waiting for the subscriber
      setShutdownDone(true); throw(ex); 
    }
  } // run


  /** shut down the subscriber */
  protected void shutdown()
  {
    setShuttingDown(true); 
  }

  /**
   * @return Returns the shuttingDown.
   */
  private synchronized boolean isShuttingDown()
  {
    return shuttingDown; 
  }


  /**
   * @return Returns the shutdownDone.
   */
  private synchronized boolean isShutdownDone()
  {
    return shutdownDone; 
  }

  /**
   * @param shutdownDone The shutdownDone to set.
   */
  private synchronized void setShutdownDone(boolean shutdownDone)
  {
    this.shutdownDone = shutdownDone; notifyAll(); 
  }


  /**
   * @param shuttingDown The shuttingDown to set.
   */
  private synchronized void setShuttingDown(boolean shuttingDown)
  {
    this.shuttingDown = shuttingDown; 
  }

  /** @return true if finished shutting down */
  public final boolean isDone()
  {
    return isShutdownDone(); 
  } // isDone


  /**
   * Class which makes a JMS/XLA subscription
   * and gets update messages.
   */
  private class Subscriber
  {

    /** topic name (looked up in jmsxla.xml file) */
    private String topicName; 

    /** XLA bookmark name */
    private String bookmarkName; 

    /** JMS connection */
    private jakarta.jms.TopicConnection connection; 

    /** JMS session */
    private TopicSession session; 

    /** JMS topic */
    private Topic topic; 

    /** JMS subscriber */
    private TopicSubscriber subscriber; 

    /** max line length to print */
    private static final int MAXLINE = 72; 

    /** how to find connection string in topic.toString output */
    private final String CONNSTR_TAG = "connectionString="; 

    /**
     * @param topicID topic name (looked up in jmsxla.xml file)
     * @param bookmark XLA bookmark name
     */
    public Subscriber(final String topicID, final String bookmark)
    {
      try
      {
        topicName = topicID; bookmarkName = bookmark; 

//jakarta jms

      Properties props = new Properties();

      props.setProperty(Context.INITIAL_CONTEXT_FACTORY,
      "com.timesten.dataserver.jakartajmsxla.SimpleInitialContextFactory");
        

        // get Connection
        Context messaging = new InitialContext(props); 
        TopicConnectionFactory connectionFactory = (TopicConnectionFactory)messaging.lookup("TopicConnectionFactory"); 
        connection = connectionFactory.createTopicConnection(); 
        connection.start(); 

        // get Session
        log("create session"); 
        session = connection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE); 

        // subscribe to topic
        log("create topic"); 
        topic = session.createTopic(topicName); 
        log("createDurableSubscriber"); 
        subscriber = session.createDurableSubscriber(topic, bookmarkName); 
      }
      catch (Exception e)
      {
        System.err.println("Sub: got " + e); 
        System.err.println("at:"); 
        e.printStackTrace(); 
        setShutdownDone(true); 
        System.exit( - 1); 
      }
    }

    /**
     * This method will close the subscriber.  You cannot subscribe/unsubscribe/delete a bookmark in use.
     * @throws JMSException
     */
    private void closeSubscriber()throws JMSException
    {
      subscriber.close(); subscriber = null; 
    }
    private void resumeSubscription()throws JMSException
    {
      subscriber = session.createDurableSubscriber(topic, bookmarkName); 
    }

    /**
     * Return the connection string for the Topic
     * @throws Exception if cannot find the connection string
     * @return connection string
     */
    protected final String getConnStr()throws Exception
    {
      String topicParams = topic.toString(); 
      StringTokenizer st = new StringTokenizer(topicParams, ":"); 

      while (st.hasMoreTokens())
      {
        String param = st.nextToken(); 
        if (param.startsWith(CONNSTR_TAG))
        {
          return param.substring(CONNSTR_TAG.length()); 
        }
      }
      throw new Exception("Can't find connection string in '" + topicParams + "'"); 
    } // getConnStr

    /**
     * Dump message content.
     * @param msg the message to dump
     * @throws JMSException on failures reading message values
     */
    private void dumpMessage(final MapMessage msg)throws JMSException
    {
      // list the names with values
      Vector < String > fields = new Vector < String > (); 
      Enumeration e = msg.getMapNames(); 

      while (e.hasMoreElements())
      {
        fields.add((String)e.nextElement()); 
      }
      Collections.sort(fields); 
      e = fields.elements(); 
      String out = ""; 

      while (e.hasMoreElements())
      {
        String name = (String)e.nextElement(); 
        String value; 

        try
        {
          if (name.equals("__CONTEXT"))
          {
            byte[] b = msg.getBytes(name); if (b == null)
            {
              value = "(null)"; 
            }
            else
            {
              value = new String(b); 
            }
          }
          else
          {
            value = msg.getString(name); 
          }
        }
        catch (Exception ex)
        {
          log("Exception when fetching " + name + ": " + ex); 
          ex.printStackTrace(); 
          value = "???"; 
        }

        String next = "  " + name + "=" + value; 
        if (out.length() + next.length() > MAXLINE)
        {
          System.out.println(out); out = ""; 
        }
        out = out + next; 
      }

      if (out.length() > 0)
      {
        System.out.println(out); 
      }
    } // dumpMessage

    /**
     * Keep receiving and dumping messages until timeout.
     * @param timeout wait this long in receive
     */
    public final void get(final long timeout)
    {
      try
      {
        MapMessage msg; 
        do
        {
          msg = (MapMessage)subscriber.receive(timeout); 
          if (msg != null)
          {
            System.out.println(); 
            System.out.println(">>> got a " + msg.getJMSType() + " message"); 
            dumpMessage(msg); 

            if (msg.getJMSType().equals("DELETE"))
              ++deleteCnt; 
          }
        }
        while (msg != null); 

        // ShutdownHook doesn't work on Windows
        if (ntBatchDemo && deleteCnt > 1)
        {
          shutdown(); 
        }
      }
      catch (JMSException e)
      {
        System.err.println("Subscriber.get: got " + e); 
        System.err.println("at:"); 
        e.printStackTrace(); 
        setShutdownDone(true); 
        System.exit( - 1); 
      }
    } // get

    /** close the JMS/XLA connection */
    public final void close()
    {
      try
      {
        log("Subscriber close"); 
        subscriber.close(); 
        session.unsubscribe(bookmarkName); 
        session.close(); 
        connection.stop(); 
        connection.close(); 
      }
      catch (JMSException e)
      {
        System.err.println("Subscriber.close: got " + e); 
        System.err.println("at:"); 
        e.printStackTrace(); 
        setShutdownDone(true); 
        System.exit( - 1); 
      }
    } // close
  } // Subscriber class

}
