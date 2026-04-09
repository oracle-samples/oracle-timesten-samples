/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package com.timesten.tptbmas;

import java.util.Collection;
import javax.ejb.Remote;


/* TptbmSession is the remote interface used by application server clients to 
 * manage instances of the Tptbm class. The TptbmSessionBean class implements
 * this interface.
 */


@Remote
public interface TptbmSession
{
  public void persist (Tptbm tptbm)
  throws Exception;
  
  public void merge (Tptbm tptbm)
  throws Exception;
  
  public void remove (Tptbm tptbm)
  throws Exception;
  
  public Tptbm findByPrimaryKey (TptbmPKey primaryKey)
  throws Exception;
  
  public void populate (Collection<Tptbm> tptbms, int flushCount)
  throws Exception;
  
  public int deleteAll (int flushCount)
  throws Exception;
}
