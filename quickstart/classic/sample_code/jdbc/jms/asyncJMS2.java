/*
 * JMS/XLA asyncJMS2.java sample code - Processing JMS/XLA updates using jakarta jms.
 *
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * Note:  JMS provides two ways to read messages - one using 
 * asynchronous callbacks to the onMessage method (used here), 
 * and one using synchronous calls to the receive() method (used
 * in syncJMS2.java).  TimesTen's JMS/XLA 
 * facility supports both approaches.  However, the receive() method
 * is generally faster.  Please consider it for performance
 * sensitive applications.

 */

import java.io.PrintStream;
import java.util.Enumeration;
import java.util.*;
import java.io.IOException;

// JMS imports
import jakarta.jms.JMSException;
import jakarta.jms.MapMessage;
import jakarta.jms.Message;
import jakarta.jms.MessageListener;
import jakarta.jms.Session;
import jakarta.jms.Topic;
import jakarta.jms.TopicConnection;
import jakarta.jms.TopicConnectionFactory;
import jakarta.jms.TopicSession;
import jakarta.jms.TopicSubscriber;
import javax.naming.Context;
import javax.naming.InitialContext;

// TimesTen XLA imports
import com.timesten.dataserver.jmsxla.XlaConstants;

// JDBC imports
import java.sql.DriverManager;
import java.sql.CallableStatement;
import java.sql.SQLException;
import java.sql.Connection;


/**
 * Demo which shows how to use JMS/XLA to process updates.
 */
public class asyncJMS2
{

  /** Standard error stream */
  private static PrintStream errStream = System.err;

  /** Standard output stream */
  private static PrintStream outStream = System.out;

  private TopicConnection connection;
  private TopicSession session;
  private String bookmark;
  private int deleteCnt = 0;
  private boolean ntBatchDemo = false;

  /** TimesTen dataserver driver name */
  private static final String TTDRIVER = "com.timesten.jdbc.TimesTenDriver";

  /** prefix for JDBC connection URL */
  private static final String TTPREFIX = "jdbc:timesten:direct:";

  /** JMS/XLA topic name */
  private static String topicName = "asyncJMS2";

  /** JDBC user name */
  public static String jdbcUsername = "appuser";

  /** XLA user name */
  public static String xlaUsername = "xlauser";

  /** XLA password */
  public static String xlaPassword = null;


  /**
   * Main routine.
   * @param args Command line arguments.
   */
  public static void main(final String[] args)
  {

    // Specify the topic "asyncJMS2" to get the connection parameters from
    // the asyncJMS2 demo entry in jmsxla.xml.
    final asyncJMS2 demo = new asyncJMS2();

    // parse command line arguments
    parseArgs(args);

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
            }
          } // synchronized(demo)
        }
      } );

      status = demo.subscribe(topicName, demo.jdbcUsername);
    }
    catch (JMSException e)
    {
      errStream.println("main caught JMS exception: " + e);
      if (e.getLinkedException() != null)
      {
        errStream.println("Linked exception: " + e.getLinkedException());
      }
      status = 1;
    }
    catch (InterruptedException e)
    {
      try
      {
        //unsubscribe (which will delete the XLA bookmark)
        demo.session.unsubscribe(demo.bookmark);
        //close the connection
        demo.connection.close();
      }
      catch (JMSException e2)
      {
        errStream.println("main caught JMS exception: " + e2);
      }
    }
    catch (Throwable e)
    {
      errStream.println("main caught exception: " + e);
      status = 1;
    }

    outStream.println("Exiting");
    System.exit(status);
  }

  //--------------------------------------------------
  // usage()
  //--------------------------------------------------
  static private void usage()
  {

    System.err.print("\n" + "Usage: \n\n" + "  ayncJMS2 [-h] [-help] [-?]\n" + 
                     "  asyncJMS2 [-topic <topicName>] [-schema <schemaName>]\n" + 
                     "           [-xlauser <username>] [-xlapassword <password>]\n\n" + 
                     "  -h                        Prints this message and exits.\n" + 
                     "  -help                     Same as -h.\n" + 
                     "  -?                        Same as -help.\n" + 
                     "  -topic <topicName>        The JMS Topic of interest.\n" + 
                     "                            Defaults to 'asyncJMS'.\n" + 
                     "  -schema <schemaName>      The schema for the CUSTOMER table.\n" + 
                     "                            Defaults to 'appuser'.\n" + 
                     "  -xlauser <username>       The username of the user with the XLA privilege.\n" + 
                     "                            Defaults to 'xlauser'.\n" + 
                     "  -xlapassword <password>   The password of the user with the XLA privilege.\n\n");
    System.exit(1);
  } // usage


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

        topicName = args[i + 1];
        i += 2;
      }

      // table's schema
      else if (args[i].equalsIgnoreCase("-schema"))
      {
        if (i > args.length)
        {
          usage();
        }

        jdbcUsername = args[i + 1];
        i += 2;
      }

      // XLA username
      else if (args[i].equalsIgnoreCase("-xlauser"))
      {
        if (i > args.length)
        {
          usage();
        }

        xlaUsername = args[i + 1];
        i += 2;
      }

      // XLA password
      else if (args[i].equalsIgnoreCase("-xlapassword"))
      {
        if (i > args.length)
        {
          usage();
        }

        xlaPassword = args[i + 1];
        i += 2;
      }

      else
      {
        usage();
      }
    } // while

  } // parseArgs



  public void init()
  {

    Connection conn = null;
    CallableStatement cStmt = null;

    // Get a JDBC connection to TimesTen to call some XLA built-ins
    try
    {

      outStream.println();

      //Prompt for the XLA Password
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
      System.err.println("Got " + cnfe + " (driver=" + TTDRIVER + ")");
      System.exit( - 1);
    }
    catch (Exception e)
    {
      System.err.println("Got " + e);
      System.err.println("At:");
      e.printStackTrace();
      System.exit( - 1);
    }

    // Call the XLA build-ins
    try
    {

      // Create the XLA bookmark 
      cStmt = conn.prepareCall("call ttxlabookmarkcreate('bookmark')");
      cStmt.execute();

      // Create the XLA subscription 
      cStmt = conn.prepareCall("call ttxlasubscribe('" + jdbcUsername + ".customer', 'bookmark')");
      cStmt.execute();

    }
    catch (SQLException sqle)
    {
      //sqle.printStackTrace();
    }

    try
    {
      conn.commit();
      conn.close();
    }
    catch (SQLException sqle)
    {
      //sqle.printStackTrace();
    }

    outStream.println("Initialized XLA environment");
  }


  /**
   * Subscribes and waits for messages.
   *
   * @param topic  the topic name (defined in the configuration file)
   * @throws Exception  JMS or database connection errors.
   * @return error status - 0 means success
   */
  public int subscribe(String topic, String schema)throws Throwable
  {

    try
    {
//jakarta jms

      Properties props = new Properties();

      props.setProperty(Context.INITIAL_CONTEXT_FACTORY,
      "com.timesten.dataserver.jakartajmsxla.SimpleInitialContextFactory");



      TopicConnectionFactory connectionFactory;

      Context messaging = new InitialContext(props);
      connectionFactory = (TopicConnectionFactory)messaging .lookup("TopicConnectionFactory");
      connection = connectionFactory.createTopicConnection();

      session = connection.createTopicSession(false, Session.AUTO_ACKNOWLEDGE);
      MyListener myListener = new MyListener(outStream);

      outStream.println("Creating consumer for topic " + topic);
      Topic xlaTopic = session.createTopic(topic);
      bookmark = "bookmark";
      TopicSubscriber subscriber = session.createDurableSubscriber(xlaTopic, bookmark);

      // After setMessageListener() has been called, myListener's onMessage
      // method will be called for each message received.
      subscriber.setMessageListener(myListener);

      outStream.println("Starting JMS XLA subscriber");
      connection.start();

      outStream.println();
      outStream.println("Waiting for XLA updates on table " + schema + ".customer ... ");

      outStream.println("- Please use ttIsql or level1 to modify the CUSTOMER table");

      while (!isShuttingDown())
      {
        try
        {
          Thread.sleep(1000);
        }
        catch (InterruptedException e)
        {
          // unsubscribe (which will delete the XLA bookmark)
          session.unsubscribe(bookmark);
          // close the connection
          connection.close();
          // ignore
        }
      }
      outStream.println("shutting down ...");

      // unsubscribe (which will delete the XLA bookmark)
      session.unsubscribe(bookmark);

      // close the connection
      connection.close();

      setSubscriberDone(true);
    }
    catch (Throwable ex)
    {

      // don't get stuck waiting for the subscriber
      setSubscriberDone(true);
      throw(ex);
    }

    return 0; // success
  }


  /** set to shut down the subscriber */
  private boolean shuttingDown = false;

  /** set when subscriber has finished cleaning up */
  private boolean subscriberDone = false;

  /** @return true when the subscriber has finished */
  protected boolean isDone()
  {
    return isSubscriberDone();
  }

  /** shut down the subscriber */
  protected void shutdown()
  {
    setShuttingDown(true);
  }



  /**
   * Class to receive update messages.
   */
  class MyListener implements MessageListener
  {

    /** Standard output stream */
    private PrintStream outStream;

    /**
     * @param outStream standard output stream
     */
    MyListener(PrintStream outStream)
    {
      this.outStream = outStream;
    }

    /** This method is called when an XLA event occurs.
     * @param message a MapMessage describing the XLA event.
     */
    public void onMessage(Message message)
    {
      MapMessage mapMessage = (MapMessage)message;
      String messageType = null;

      if (message == null)
      {
        errStream.println("MyListener: update message is null");
        return ;
      }

      try
      {
        outStream.println();
        outStream.println("onMessage: got a " + mapMessage.getJMSType() + " message");

        // Get the type of event (insert, update, delete, drop table, etc.).
        int type = mapMessage.getInt(XlaConstants.TYPE_FIELD);
        if (type == XlaConstants.INSERT)
        {
          outStream.println("A row was inserted.");
        }
        else if (type == XlaConstants.UPDATE)
        {
          outStream.println("A row was updated.");
        }
        else if (type == XlaConstants.DELETE)
        {
          outStream.println("A row was deleted.");
          // ShutdownHook doesn't work on Windows
          ++deleteCnt;
          if (ntBatchDemo && deleteCnt > 1)
          {
            shutdown();
          }
        }
        else
        {

          // Messages are also received for DDL events such as CREATE TABLE.
          // This program processes INSERT, UPDATE, and DELETE events,
          // and ignores the DDL events.
          return ;
        }

        outStream.println();
        outStream.print("Fields in message: ");
        printMapNames(mapMessage);
        outStream.println();

        if (type == XlaConstants.INSERT || type == XlaConstants.UPDATE || type == XlaConstants.DELETE)
        {

          // Get the column values from the message.
          int cust_num = mapMessage.getInt("cust_num");
          String region = mapMessage.getString("region");
          String name = mapMessage.getString("name");
          String address = mapMessage.getString("address");

          outStream.println("New Column Values:");
          outStream.println("cust_num=" + cust_num);
          outStream.println("region=" + region);
          outStream.println("name=" + name);
          outStream.println("address=" + address);
        }

        if (type == XlaConstants.UPDATE)
        {
          // for update messages, print the old column values
          outStream.println("Old Column Values:");
          // Get the old (previous) values from the message, if any.
          int old_cust_num;
          try
          {
            old_cust_num = mapMessage.getInt("_cust_num");
            outStream.println("old cust_num=" + old_cust_num);
          }
          catch (Exception e)
          {
            errStream.println("Exception in " + this.getClass().getName());
            outStream.println("No old cust_num");
            e.printStackTrace();
          }
          String old_region = mapMessage.getString("_region");
          outStream.println("old region=" + old_region);
          String old_name = mapMessage.getString("_name");
          outStream.println("old name=" + old_name);
          String old_address = mapMessage.getString("_address");
          outStream.println("old address=" + old_address);
        }
        outStream.println();
      }
      catch (JMSException e)
      {
        errStream.println("JMSException in " + this.getClass().getName());
        e.printStackTrace();
      }
      catch (Exception e)
      {
        errStream.println("Exception in " + this.getClass().getName());
        e.printStackTrace();
      }
    }
  }

  /**
   * Prints the names of all fields in the given MapMessage.
   * @param mapMessage the XLA message
   * @throws JMSException for errors retrieving field values
   */
  void printMapNames(MapMessage mapMessage)throws JMSException
  {
    Enumeration names = mapMessage.getMapNames();
    if (names == null)
    {
      outStream.print("<getMapNames returned null>");
      return ;
    }
    while (names.hasMoreElements())
    {
      outStream.print(names.nextElement());
      if (names.hasMoreElements())
      {
        outStream.print(";");
      }
    }
    outStream.println();
  }


  /**
   * @return Returns the shuttingDown.
   */
  private synchronized boolean isShuttingDown()
  {
    return shuttingDown;
  }


  /**
   * @param shuttingDown The shuttingDown to set.
   */
  private synchronized void setShuttingDown(boolean shuttingDown)
  {
    this.shuttingDown = shuttingDown;
  }


  /**
   * @return Returns the subscriberDone.
   */
  private synchronized boolean isSubscriberDone()
  {
    return subscriberDone;
  }


  /**
   * @param subscriberDone The subscriberDone to set.
   */
  private synchronized void setSubscriberDone(boolean subscriberDone)
  {
    this.subscriberDone = subscriberDone;
    notifyAll();
  }


}
