/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package com.timesten.tptbmas;

import java.util.Iterator;
import java.io.PrintWriter;
import javax.persistence.*;


/* Implements an EJB 3.0 client using the EntityManager persistence API. This is
 * a J2SE application configuration that does not involve a separate application
 * server process.
 */


public class JPAClient extends CommonClient
{

  // interfaces for EJB 3.0 J2SE persistence
  private static EntityManagerFactory m_emf = null;  
  private EntityManager m_emgr = null;

  // the name of the EntityManager configuration specified
  // in the persistence.xml file
  private static String m_emName = "TptbmHibernate";
    
  // keep global track of the number of JPAClient threads that
  // are using the EntityManagerFactory
  private static int m_factoryUseCount = 0;
  
  
  public JPAClient (PrintWriter writer, 
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
    return new JPAClient (writer,
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
    JPAClient client = null;
        
    // read and validate command line args
    if (!processArgs (args))
    {
      System.exit (1);
    }

    System.out.println ("Beginning JPAClient...");
    
    // instantiate a new client with threadId = 0
    client = new JPAClient (new PrintWriter (System.out, true), 
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
    
    System.out.println ("End JPAClient.");
  }
  
  
  // validate and assign command line arguments
  public static boolean processArgs (String[] args)
  {
    // locals
    boolean status = true;
    int argCount = args.length;
    int index;
    
    // check for the entity manager name command line arg.
    for (index = 0; index < argCount; index ++)
    {

      if (args[index].equals ("-emName"))
      {
        // set the m_emName attribute
        if (index + 1 < argCount)
        {
          m_emName = args[index + 1];
          index ++;
          status = true;
        }
        else
        {
          status = false;
        }
      }      
    }

    // read the common client command line args
    if (status)
    {
      status = CommonClient.processArgs (args);
    }
    
    if (!status)
    {
      System.out.println ("Usage: com.timesten.tptbmas.JPAClient " +
        "[-emName entity_manager_name] " +
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
    // open a single EntityManagerFactory to be shared among all threads
    if (m_emf == null)
    {
      m_emf = Persistence.createEntityManagerFactory (m_emName);
    }
    
    m_factoryUseCount ++;
    
    // open an exclusive EntityManager for this thread
    m_emgr = m_emf.createEntityManager ();
     
     return;
  }
  
  public synchronized void close ()
  throws Exception
  {
  
    // close this thread's EntityManager
    m_emgr.close ();
    m_emgr = null;
  
    // if there are no other threads using this EntityManagerFactory then
    // close it
    m_factoryUseCount --;
        
    if (m_factoryUseCount == 0)
    {
      m_emf.close ();
      m_emf = null;
    }   
  }
  
  public void txnBegin ()
  throws Exception
  {
    m_emgr.getTransaction ().begin ();     
  }
  
  
  public void txnCommit ()
  throws Exception
  {
    m_emgr.getTransaction ().commit ();
    m_emgr.clear ();    
  }
  
  public void txnRollback ()
  throws Exception
  {
    m_emgr.getTransaction ().rollback ();
    m_emgr.clear ();    
  } 

  
  public void read ()
  throws Exception
  {
    TptbmPKey pkey = getRandomKey ();
    Tptbm tptbm;
    
    log ("Reading Tptbm " + pkey.toString () + "...");
    
    tptbm = m_emgr.find (Tptbm.class, pkey);
    
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
    
    m_emgr.persist (newTptbm);
    
    if (m_debug)
    {
      logVerbose (newTptbm.toString ());
    }    
  }
  
  
  public void update ()
  throws Exception
  {
    Tptbm tptbm = null;
    TptbmPKey pkey = getRandomKey ();
    
    log ("Updating Tptbm " + pkey.toString () + "...");
    
    // lookup the record 
    tptbm = m_emgr.find (Tptbm.class, pkey);
    
    // update the record 
    tptbm.setLast_calling_party (pkey.getVpn_id () + "x" + pkey.getVpn_nb ());
    
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
        
        m_emgr.persist (newTptbm);        
        cacheCount ++;
        
        if (m_debug)
        {
          logVerbose (newTptbm.toString ());
        }              
                
        if (cacheCount % m_maxEntitiesInCache == 0)
        {
          m_emgr.flush ();
        }
        
      }
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
    Iterator tptbms;
    Tptbm tptbm;    
    
    logVerbose ("Deleting all rows from TPTBM table...");
    startTime = System.currentTimeMillis ();
    
    tptbms = m_emgr.createNamedQuery ("findAllTptbms").
      getResultList ().iterator ();
    

    while (tptbms.hasNext ())
    {
      tptbm = (Tptbm) tptbms.next ();
          
      if (m_debug)
      {
        logVerbose (tptbm.toString ());
      }      
      
      if (!m_emgr.contains (tptbm))
      {
        tptbm = m_emgr.merge (tptbm);
      }
      
      m_emgr.remove (tptbm);
      cacheCount ++;
      
      if (cacheCount % m_maxEntitiesInCache == 0)
      {
        m_emgr.flush();
      } 
      
    }
    

    endTime = System.currentTimeMillis ();
    
    logVerbose (cacheCount + " rows in TPTBM table deleted in " +
      ((double)(endTime - startTime) / (double)1000) + " seconds.");
 
    
    return;
  }
  
 
}
