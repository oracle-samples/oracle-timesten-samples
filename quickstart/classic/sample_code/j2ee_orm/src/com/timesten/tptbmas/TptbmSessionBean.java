/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package com.timesten.tptbmas;

import java.util.Collection;
import java.util.Iterator;
import javax.ejb.Stateless;
import javax.persistence.EntityManager;
import javax.persistence.PersistenceContext;
import javax.persistence.PersistenceContextType;

/* TptbmSessionBean is a stateless EJB 3.0 session bean that runs in the
 * application server's container. It implements the TptbmSession interface and
 * manages instances of the Tptbm class via the EntityManager API.
 */


@Stateless (mappedName="ejb/TptbmSession")
public class TptbmSessionBean implements TptbmSession
{
  
  // interface for EJB 3.0 persistence
  @PersistenceContext (type=PersistenceContextType.TRANSACTION)
  private EntityManager m_emgr = null;


  
  public TptbmSessionBean()
  {}
  
  
  public void persist (Tptbm tptbm)
  throws Exception
  {
    m_emgr.persist (tptbm);
  }
  
  public void merge (Tptbm tptbm)
  throws Exception
  {
    m_emgr.merge (tptbm);
  }
  
  public void remove (Tptbm tptbm)
  throws Exception
  {
    m_emgr.remove (m_emgr.merge (tptbm));
  }

  public Tptbm findByPrimaryKey (TptbmPKey primaryKey)
  throws Exception
  {
    // lookup the record 
    return m_emgr.find (Tptbm.class, primaryKey);
  }

  // insert all instances into the TPTBM table  
  public void populate (Collection<Tptbm> tptbms, int flushCount)
  throws Exception
  {
    int insertCount = 0;
    Iterator insertList = tptbms.iterator ();
    
    while (insertList.hasNext ())
    {
      m_emgr.persist (insertList.next ());  
      insertCount ++;
      
      if (insertCount % flushCount == 0)
      {
        m_emgr.flush ();
      }
    }

  }
  
  // delete all rows in the TPTBM table and return the row count
  public int deleteAll (int flushCount)
  throws Exception
  {
    int deleteCount = 0;
    Iterator tptbms;
    Tptbm tptbm;    
    
    
    tptbms = m_emgr.createNamedQuery ("findAllTptbms").
      getResultList ().iterator ();
    

    while (tptbms.hasNext ())
    {
      tptbm = (Tptbm) tptbms.next ();
       
      if (!m_emgr.contains (tptbm))
      {
        tptbm = m_emgr.merge (tptbm);
      }
      
      m_emgr.remove (tptbm);
      deleteCount ++;
      
      
      if (deleteCount % flushCount == 0)
      {
        m_emgr.flush ();
      }

    }
    
    return deleteCount;
  }
  
}
