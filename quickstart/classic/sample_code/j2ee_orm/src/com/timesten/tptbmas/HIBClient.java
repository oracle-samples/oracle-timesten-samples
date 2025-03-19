/*
* Copyright (c) 1999, 2025, Oracle and/or its affiliates.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package com.timesten.tptbmas;


import java.util.Iterator;
import java.io.PrintWriter;
import org.hibernate.*; 
import org.hibernate.cfg.Configuration;


/* Implements a Hibernate client that uses the Hibernate native Session 
 * interface for persistence.
 */

public class HIBClient extends CommonClient 
{


  // native interfaces for Hibernate persistence
  private static SessionFactory m_sessionFactory = null;
  private Session m_session = null;
  private Transaction m_txn = null;
  
  // the configuration file for the Hibernate SessionFactory
  private String m_hibernateCfg = "META-INF/hibernate.cfg.xml";
  
  // keep track of the number of HIBClients using the global SessionFactory
  private static int m_factoryUseCount = 0;

  
    
  public HIBClient (PrintWriter writer, 
    int keyCount,
    int numXacts, 
    int numThreads, 
    int threadId,
    int percentReads, 
    int percentInserts,
    boolean verbose)
  {
    super (writer,
      keyCount,
      numXacts, 
      numThreads, 
      threadId,
      percentReads, 
      percentInserts,
      verbose);

    return;
  }
  
  public CommonClient createNewInstance (PrintWriter writer, 
    int keyCount,
    int numXacts, 
    int numThreads, 
    int threadId,
    int percentReads, 
    int percentInserts,
    boolean verbose)
  {
    return new HIBClient (writer,
      keyCount,
      numXacts, 
      numThreads, 
      threadId,
      percentReads, 
      percentInserts,
      verbose);
  }

  
  // Runs the benchmark from the command line.
  public static void main (String[] args)
  {
    HIBClient client = null;
        
    // read and validate command line args
    if (!processArgs (args))
    {
      System.exit (1);
    }

    System.out.println ("Beginning HIBClient...");
    
    // instantiate a new client with threadId = 0
    client = new HIBClient (new PrintWriter (System.out, true), 
      m_keyCount, 
      m_numXacts,
      m_numThreads, 
      0,
      m_percentReads, 
      m_percentInserts, 
      m_verbose);
      
    
    try
    {
      client.runMain ();
    }
    catch (Exception ex)
    {
      ex.printStackTrace ();
    }
    
    System.out.println ("End HIBClient.");
  }
  
  // validate and assign command line arguments
  public static boolean processArgs (String[] args)
  {
    // locals
    boolean status = true;

    // read the common client command line args
    if (status)
    {
      status = CommonClient.processArgs (args);
    }
    
    if (!status)
    {
      System.out.println ("Usage: com.timesten.tptbmas.HIBClient " +
        "[-keyCount num_keys**2] " +
        "[-numXacts num_xacts_per_thread] " +
        "[-numThreads num_threads] " +
        "[-percentReads percent_reads] " +
        "[-percentInserts percent_inserts] " +
        "[-seed random_seed] " +
        "[-debug] " +
        "[-quiet] ");
    }

    
    return status;
  }   
  
  public synchronized void open ()
  throws Exception
  {   
    // open a single Hibernate session factory to be shared among all threads
    if (m_sessionFactory == null)
    {
      m_sessionFactory = 
        new Configuration ().configure (m_hibernateCfg).buildSessionFactory ();     
    }
    
    m_factoryUseCount ++;
    
    
    // open a dedicated Session for this thread
    m_session = m_sessionFactory.openSession();
  }
  
  public synchronized void close ()
  throws Exception
  {     
    // close this thread's Session
    m_session.close ();
    m_session = null;
    
    
    // if no other threads are using the factory then close it
    m_factoryUseCount --;    
    
    if (m_factoryUseCount == 0)
    {
      m_sessionFactory.close ();
      m_sessionFactory = null;
    }
  }
  
  public void txnBegin ()
  throws Exception
  {
    // begin a new transaction associated with the Hibernate session
    m_txn = m_session.beginTransaction ();
  }
   
  public void txnCommit ()
  throws Exception
  {
    // commit the current transaction associated with the Hibernate Session
    // (note that this operation automatically flushes the Session)
    m_txn.commit ();    
    m_txn = null;
    
    // clear the session level cache
    m_session.clear ();
  }
  
  public void txnRollback ()
  throws Exception
  {
    // rollback the current transaction associated with the Hibernate Session
    m_txn.rollback ();
    m_txn = null;
    
    // clear the session level cache
    m_session.clear ();    
  } 

  
  public void read ()
  throws Exception
  {
    Tptbm tptbm = null;
    TptbmPKey key = getRandomKey ();    
    
    log ("Reading Tptbm " + key.toString () + "...");
    
    // this method will return a Tptbm object from the TPTBM table
    // based on the primary key 
    tptbm = (Tptbm) m_session.get (Tptbm.class, key);
    
    if (m_debug)
    {
      logVerbose (tptbm.toString ());
    }

  }
  
  
  
  public void insert ()
  throws Exception
  {
    // create and persist a new Tptbm instance
    Tptbm newTptbm = new Tptbm ();
    TptbmPKey pkey = new TptbmPKey (m_keyCount + m_threadId, m_insertOpCount);
    
    String last_calling_party = "" + pkey.getVpn_id () + "x" + pkey.getVpn_nb ();
    String directory_nb = "55" + pkey.getVpn_id () + "" + pkey.getVpn_nb ();
    String descr = "<place holder for description of VPN " + pkey.getVpn_id () +
      " extension " + pkey.getVpn_nb () + ">";
    
    newTptbm.setKey (pkey);
    newTptbm.setDirectory_nb (directory_nb);
    newTptbm.setLast_calling_party (last_calling_party);
    newTptbm.setDescr (descr);
    
    log ("Inserting Tptbm " + pkey.toString () + "...");
    
    m_session.save (newTptbm);
    
    if (m_debug)
    {
      logVerbose (newTptbm.toString ());
    }
    
    return;
  }
  
  
  public void update ()
  throws Exception
  {
    Tptbm tptbm = null;
    TptbmPKey key = getRandomKey ();    

    log ("Updating Tptbm " + key.toString () + "...");
    
    // load the record ... 
    tptbm = (Tptbm) m_session.load (Tptbm.class, key);
    
    // update the record 
    tptbm.setLast_calling_party (key.getVpn_id () + "x" + key.getVpn_nb ());
    
    if (m_debug)
    {
      logVerbose (tptbm.toString ());
    }
  }
  
  
  
  
  public void populate ()
  throws Exception
  {
    TptbmPKey pkey; 
    String directory_nb;
    String last_calling_party = "0000000000";
    String descr;
    int cacheCount = 0;
    long startTime, endTime;
       
    logVerbose ("Populating TPTBM table ...");    
    startTime = System.currentTimeMillis ();
    

    // VPN_ID & VPN_NB are 1-based indexes    
    for (int id = 1; id <= m_keyCount; id ++)
    {
      for (int nb = 1; nb <= m_keyCount; nb ++)
      {        
        pkey = new TptbmPKey (id, nb);    
        directory_nb = "55" + id + "" + nb;
        descr = "<place holder for description of VPN " + id +
          " extension " + nb + ">";
        

        // create and persist the instance
        Tptbm newTptbm = new Tptbm ();
        newTptbm.setKey (pkey);
        newTptbm.setDirectory_nb (directory_nb);
        newTptbm.setLast_calling_party (last_calling_party);
        newTptbm.setDescr (descr);
        
        m_session.save (newTptbm);        
        cacheCount ++;
        
        if (m_debug)
        {
          logVerbose (newTptbm.toString ());
        }        
        
        if (cacheCount % m_maxEntitiesInCache == 0)
        {
          m_session.flush ();
        }
        
      }
      txnCommit();
      txnBegin();
    }
    
    
    endTime = System.currentTimeMillis ();

    
    logVerbose ("TPTBM table populated with " + (m_keyCount * m_keyCount) +
      " rows in " + ((double)(endTime - startTime) / (double)1000) + " seconds.");
    
    
    return;
  }
  
  
  public void deleteAll ()
  throws Exception
  {
    int cacheCount = 0;
    long startTime, endTime;
    Iterator tptbms = null;
    Tptbm tptbm;
    
    logVerbose ("Deleting all rows from TPTBM table...");
    startTime = System.currentTimeMillis ();
    
    tptbms = m_session.createQuery ("FROM Tptbm").list ().iterator ();

    
    while (tptbms.hasNext ())
    {
      tptbm = (Tptbm) tptbms.next ();
      
      if (m_debug)
      {
        logVerbose (tptbm.toString ());
      }
      
      m_session.delete (tptbm);
      cacheCount ++;
      
      if (cacheCount % m_maxEntitiesInCache == 0)
      {
        m_session.flush ();
        txnCommit();
        txnBegin();
      }      
    }
    

    endTime = System.currentTimeMillis ();
    
    logVerbose (cacheCount + " rows in TPTBM table deleted in " +
      ((double)(endTime - startTime) / (double)1000) + " seconds.");
 
    
    return;
  }
  
 
}
