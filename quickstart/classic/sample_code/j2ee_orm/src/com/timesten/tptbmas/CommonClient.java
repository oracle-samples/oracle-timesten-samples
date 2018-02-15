/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package com.timesten.tptbmas;


import java.util.Random;
import java.io.PrintWriter;

/* CommonClient is the base abstract class from which all concrete client 
 * classes inherit. CommonClient implements the high level benchmark setup and 
 * execution logic. CommonClient also defines the abstract methods which are 
 * implemented by derived classes using a specific persistence API.
 */


public abstract class CommonClient extends Thread
{
  
  // external test parameter defaults, these values can be overidden
  // on the client command line:
  static protected boolean m_verbose = true;
  static protected boolean m_debug = false;  
  static protected int m_keyCount = 100;
  static protected int m_numXacts= 1000;
  static protected int m_numThreads = 4;
  static protected int m_percentReads = 20;
  static protected int m_percentInserts = 40;
  static protected long m_seed = 0;
    
  // internal test parameters:
  protected int m_threadId = 0;
  protected int m_threadSleepMilli = 100;
  protected Random m_randNum = null;
  protected PrintWriter m_stdOut = null;
  protected int m_maxEntitiesInCache = 1024; 
  
  // internal global counters updated by all threads
  static protected int m_readOpCount = 0;
  static protected int m_insertOpCount = 0;
  static protected int m_updateOpCount = 0;
  
  
  // abstract methods implemented in derived classes
  public abstract void open () throws Exception;
  public abstract void close () throws Exception;
  public abstract void txnBegin () throws Exception;
  public abstract void txnCommit () throws Exception;
  public abstract void txnRollback () throws Exception;
  public abstract void read () throws Exception;
  public abstract void insert () throws Exception;
  public abstract void update () throws Exception;
  public abstract void deleteAll () throws Exception;
  public abstract void populate () throws Exception;  
  
  public abstract CommonClient createNewInstance (PrintWriter writer, 
    int keyCount,
    int numXacts, 
    int numThreads, 
    int threadId,
    int percentReads, 
    int percentInserts,
    boolean verbose);
  
  
  
  // constructor
  public CommonClient (PrintWriter writer, 
    int keyCount,
    int numXacts, 
    int numThreads, 
    int threadId,
    int percentReads, 
    int percentInserts,
    boolean verbose)
  {
    // set external test parameters
    m_stdOut = writer;    
    m_keyCount = keyCount;
    m_numXacts = numXacts;
    m_numThreads = numThreads;
    m_threadId = threadId;
    m_percentReads = percentReads;
    m_percentInserts = percentInserts;    
    m_verbose = verbose;
    
    // init. the random number generator
    m_randNum = new Random ();
    
    // if the seed is 0 then use the system time as the seed
    if (m_seed == 0)
    {
      m_seed = System.currentTimeMillis ();
    }

    m_randNum.setSeed (m_seed);

    return;
  }
  
  

  // called by base classes to prepare and execute the benchmark
  public void runMain ()
  {
        
    try
    {
      // delete any rows and then populate the TPTBM table for the benchmark
      // run
      setupTptbm ();
       
      // run the benchmark   
      runTptbm ();     
    }
    catch (Exception ex)
    {
      ex.printStackTrace ();
    }
    
  }
  
  

  // validate and assign common command line arguments that are common to all
  // versions of the TptbmAS client
  public static boolean processArgs (String[] args)
  {
    // locals
    boolean status = true;
    int argCount = args.length;
    int index;
    
    
    for (index = 0; index < argCount; index ++)
    {
      
      if (args[index].equals ("-keyCount"))
      {
        // set the m_keyCount attribute
        if (index + 1 < argCount)
        {
          m_keyCount = (Integer.valueOf (args[index + 1])).intValue ();
          index ++;
          status = true;
        }
        else
        {
          status = false;
        }
      }
      else if (args[index].equals ("-numXacts"))
      {
        // set them_numXacts attribute
        if (index + 1 < argCount)
        {
          m_numXacts = (Integer.valueOf (args[index + 1])).intValue ();
          index ++;
          status = true;
        }
        else
        {
          status = false;
        }
      }
      else if (args[index].equals ("-numThreads"))
      {
        // set the m_numThreads attribute
        if (index + 1 < argCount)
        {
          m_numThreads = (Integer.valueOf (args[index + 1])).intValue ();
          index ++;
          status = true;
        }
        else
        {
          status = false;
        }
      }
      else if (args[index].equals ("-quiet"))
      {
        m_verbose = false;
      }
      else if (args[index].equals ("-debug"))
      {
        m_debug = true;
      }      
      else if (args[index].equals ("-percentReads"))
      {
        // set the m_percentReads attribute
        if (index + 1 < argCount)
        {
          m_percentReads = (Integer.valueOf (args[index + 1])).intValue ();
          index ++;
          status = true;
        }
        else
        {
          status = false;
        }
      }
      else if (args[index].equals ("-percentInserts"))
      {
        // set the m_percentInserts attribute
        if (index + 1 < argCount)
        {
          m_percentInserts = (Integer.valueOf (args[index + 1])).intValue ();
          index ++;
          status = true;
        }
        else
        {
          status = false;
        }
      } 
      else if (args[index].equals ("-seed"))
      {
        // set the m_seed attribute
        if (index + 1 < argCount)
        {
          m_seed = (Long.valueOf (args[index + 1])).longValue ();
          index ++;
          status = true;
        }
        else
        {
          status = false;
        }
      }      
    }
    
    // make sure the key count makes sense
    if (m_keyCount <= 0)
    {
      status = false;
      System.out.println ("The keycount must be greater than 0.");
    }
    
    // make sure the transaction count makes sense
    if (m_numXacts < 0)
    {
      status = false;
      System.out.println ("The number of transactions cannot be less than 0.");
    }

    // make sure the thread count makes sense
    if (m_numThreads < 0)
    {
      status = false;
      System.out.println ("The number of threads cannot be less than 0.");
    }    
    
    // make sure the transaction mix makes sense
    if ((m_percentReads + m_percentInserts) > 100)
    {
      status = false;
      System.out.println ("The percentage of inserts plus the percentage of " +
        "reads exceeds 100 percent.");
    }
    
    // make sure the transaction mix makes sense
    if (m_percentReads < 0 || m_percentInserts < 0)
    {
      status = false;
      System.out.println ("The percentage of inserts and/or the percentage of " +
        "reads cannot be less than 0.");
    }
    
    
    
    return status;
  }
  

  
  
  // new client threads start execution here
  public void run ()
  {
    int path;
    
    // execute the workload
    for (int index = 0; index < m_numXacts; index ++)
    {
      // randomly choose the type of operation to perform: read, update or insert
      if (m_percentReads != 100)
      {
        // select a number from 0 to 99
        int randNum = m_randNum.nextInt (100);
        
        if (randNum < (m_percentReads + m_percentInserts))
        {
          if (randNum < m_percentReads)
          {
            // read path
            path = 1;
          }
          else
          {
            // insert path
            path = 2;
          }
        }
        else
        {
          // update path
          path = 0;
        }
      }
      else
      {
        // 100% read path
        path = 1;
      }
      
      
      
      try
      {
        
        txnBegin ();
        
        // execute the operation
        switch (path)
        {
          case 0:
            update ();
            m_updateOpCount ++;
            break;
            
          case 1:
            read ();
            m_readOpCount ++;
            break;
            
          case 2:
            insert ();
            m_insertOpCount ++;
            break;
        }
        
        txnCommit ();
        
      }
      catch (Exception ex)
      {
        logVerbose ("Thread " + m_threadId + " failed.");
        ex.printStackTrace ();
        return;
      }
    }
    
    return;
  }
  
  
  // setup procedure executed before the benchmark runs, this procedure deletes 
  // all rows that may be in the TPTBM table and then populates the table
  // with a number of rows based on the value of m_keyCount
  public void setupTptbm ()
  throws Exception
  {
    
    logVerbose ("");    
    logVerbose ("Setting up Tptbm benchmark ...");      
    
    open ();
    
    txnBegin ();
    deleteAll ();
    txnCommit ();
    
    txnBegin ();
    populate ();
    txnCommit ();
    
    close ();
  }
  
  
  // benchmark execution procedure
  public void runTptbm ()
  throws Exception
  {
    long startTime;
    long endTime;
    long elapsedTime;
    float tps;
    int path = 0;
    boolean stillAlive = true;
    CommonClient clients [] = new CommonClient [m_numThreads];
    
    logVerbose ("");    
    logVerbose ("Initializing Tptbm benchmark ...");        
        
    // initialize each thread
    for (int index = 0; index < m_numThreads; index ++)
    {
      CommonClient newThread = createNewInstance (m_stdOut,
        m_keyCount, 
        m_numXacts, 
        m_numThreads, 
        1 + index,
        m_percentReads, 
        m_percentInserts, 
        m_verbose);
      
      clients [index] = newThread;
      newThread.open ();
    }

    
    
    // get the system start time
    logVerbose ("Running benchmark ...");        
    startTime = System.currentTimeMillis ();
   
    // start the threads
    for (int index = 0; index < m_numThreads; index ++)
    {
      clients [index].start ();
    }    
    
    // wait for all threads to complete
    while (stillAlive)
    {
      stillAlive = false;
      
      for (int index = 0; index < m_numThreads; index ++)
      {
        if (clients [index].isAlive ())
        {
          stillAlive = true;
          this.sleep (m_threadSleepMilli);
          break;
        }
      }
    }
    
    // get the system end time
    endTime = System.currentTimeMillis ();
    logVerbose ("Benchmark run has completed.");      
    
    
    // clean up each thread
    for (int index = 0; index < m_numThreads; index ++)
    {
     clients [index].close ();
    }  
    
    
    
    
    // calculate tps
    elapsedTime = endTime - startTime;
    tps = (float) ((double)m_numThreads *
      ((double)m_numXacts / (double)elapsedTime) * (double)1000);
    
    
    
    // report the results
    logVerbose ("");
    
    logVerbose ("Threads:                      " + m_numThreads);
    logVerbose ("Transactions per thread:      " + m_numXacts);
    
    logVerbose ("");
    
    logVerbose ("% read-only transactions:     " + m_percentReads);
    logVerbose ("% insert transactions:        " + m_percentInserts);
    logVerbose ("% update transactions:        " + (100 - m_percentReads - m_percentInserts));
    
    logVerbose ("");    
    
    logVerbose ("Random seed:                  " + m_seed);
    logVerbose ("Total read-only transactions: " + m_readOpCount);
    logVerbose ("Total insert transactions:    " + m_insertOpCount);
    logVerbose ("Total update transactions:    " + m_updateOpCount);    
    
    logVerbose ("");    
    
    logVerbose ("Elapsed time (msec):          " + elapsedTime);
    logVerbose ("Transaction Rate (TPS):       " + tps);
    logVerbose ("Transaction Rate (TPM):       " + (tps * 60));
    
    logVerbose ("");    
    
    
    return;
  }
  

  // returns a random Tptbm key from the initial population of rows
  // in the TPTBM table
  public TptbmPKey getRandomKey ()
  {
    TptbmPKey key = new TptbmPKey ();
    
    // VPN_ID & VPN_NB are 1-based indexes
    key.setVpn_id (m_randNum.nextInt (m_keyCount) + 1);
    key.setVpn_nb (m_randNum.nextInt (m_keyCount) + 1);
    
    return key;
  }
  


  
  
  public void log (String mssg)
  {
    if (m_verbose)
    {
      m_stdOut.println ("(" + m_threadId + "): " + mssg);
    }
  }
  
  public void logVerbose (String mssg)
  {
    m_stdOut.println ("(" + m_threadId + "): " + mssg);
  }
    
}
