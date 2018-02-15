/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package com.timesten.tptbmas;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Enumeration;

import javax.ejb.EJB;
import javax.naming.*;



/* Implements an EJB 3.0 client using the TptbmSession remote interface. This 
 * client communicates with an application server that has deployed the 
 * TptbmSessionBean class.
 */



public class ASClient extends CommonClient
{
  
  // the JNDI lookup for the session
  static private String m_sessionLookup = "ejb/TptbmSession";

  // the client's interface to the server's session
  TptbmSession m_session = null;
  
  // the JNDI context
  InitialContext m_ctx = null;
  
  
  
  public ASClient (PrintWriter writer,
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
    return new ASClient (writer,
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
    ASClient client = null;
    
    // read and validate command line args
    if (!processArgs (args))
    {
      System.exit (1);
    }
    
    System.out.println ("Beginning ASClient...");
    
    // instantiate a new client with threadId = 0
    client = new ASClient (new PrintWriter (System.out, true),
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
    
    System.out.println ("End ASClient.");
  }
  
  
  // validate and assign command line arguments
  public static boolean processArgs (String[] args)
  {
    // locals
    boolean status = true;
    int argCount = args.length;
    int index;
    
    // check for the EJB session lookup name command line arg.
    for (index = 0; index < argCount; index ++)
    {

      if (args[index].equals ("-sessionLookup"))
      {
        // set the m_emName attribute
        if (index + 1 < argCount)
        {
          m_sessionLookup = args[index + 1];
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
      System.out.println ("Usage: com.timesten.tptbmas.ASClient " +
        "[-sessionLookup session_lookup] " +
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
  
  public void txnBegin ()
  throws Exception
  {
    // All transactions are managed by the container on the server
  }
  
  public void txnCommit ()
  throws Exception
  {
    // All transactions are managed by the container on the server
  }
  
  public void txnRollback ()
  throws Exception
  {
    // All transactions are managed by the container on the server
  }
  
  public void open ()
  throws Exception
  {

    // lookup the Tptbm session bean interface 
    try
    {
      m_ctx = new InitialContext();
      m_session = (TptbmSession) m_ctx.lookup(m_sessionLookup);
    }
    catch (NamingException ne)
    {
      logVerbose ("The client was unable to lookup \"" + m_sessionLookup + 
                  "\" on the server.");
      throw (Exception) ne;
    }
  }
  
   
  public void close ()
  throws Exception
  {
    if (m_ctx != null)
      m_ctx.close ();
  }
  

 
 
 
  public void read ()
  throws Exception
  {
    TptbmPKey pkey = getRandomKey ();
    Tptbm tptbm;
    
    log ("Reading Tptbm " + pkey.toString () + "...");
    
    tptbm = m_session.findByPrimaryKey (pkey);
    
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
    
    m_session.persist (newTptbm);
    
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
    tptbm = m_session.findByPrimaryKey (pkey);
    
    // update the record 
    tptbm.setLast_calling_party (pkey.getVpn_id () + "x" + pkey.getVpn_nb ());
    
    // save it
    m_session.merge (tptbm);
    
    if (m_debug)
    {
      logVerbose (tptbm.toString ());
    }    
  }
  
  
  
  
  
  public void populate ()
  throws Exception
  {
    ArrayList<Tptbm> insertList = new ArrayList<Tptbm> (m_keyCount ^ 2);
    TptbmPKey pkey; 
    String directory_nb;
    String last_calling_party = "0000000000";
    String descr;
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
        

        // create the new instance
        Tptbm newTptbm = new Tptbm ();
        newTptbm.setKey (pkey);
        newTptbm.setDirectory_nb (directory_nb);
        newTptbm.setLast_calling_party (last_calling_party);
        newTptbm.setDescr (descr);
        
        insertList.add (newTptbm);
        
        if (m_debug)
        {
          logVerbose (newTptbm.toString ());
        }              
        
      }
    }
    
    // send the list to the server
    m_session.populate (insertList, m_maxEntitiesInCache);
    
    endTime = System.currentTimeMillis ();

    logVerbose ("TPTBM table populated with " + (m_keyCount * m_keyCount) +
      " rows in " + ((double)(endTime - startTime) / (double)1000) + " seconds.");
    
    
    return;
  }  
  
  
  public void deleteAll ()
  throws Exception
  {
    long startTime, endTime;
    int rowCount = 0;
    
    logVerbose ("Deleting all rows from TPTBM table...");
    startTime = System.currentTimeMillis ();
    
    rowCount = m_session.deleteAll (m_maxEntitiesInCache);
    
    endTime = System.currentTimeMillis ();
    logVerbose (rowCount + " rows in TPTBM table deleted in " +
      ((double)(endTime - startTime) / (double)1000) + " seconds.");

    return;
  }
   
 
}
