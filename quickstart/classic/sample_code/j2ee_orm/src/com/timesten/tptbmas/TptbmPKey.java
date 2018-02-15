/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package com.timesten.tptbmas;

import java.io.Serializable;
import javax.persistence.Column;
import javax.persistence.Embeddable;

/* Each instance of the Tptbm class contains an instance of the TptbmPKey class.
 * This class represents the unique ID for each instance of Tptbm. The TPTBM 
 * table has a composite primary key consisting of the VPN_NB and VPN_ID table 
 * columns. TptbmPKey contains two fields called vpn_id and vpn_nb that are 
 * mapped to these table columns through the use of annotations.
 */

@Embeddable
public final class TptbmPKey implements Serializable
{
  // VPN_ID & VPN_NB persistent fields 
  private int vpn_id;
  private int vpn_nb;
  
  

  public TptbmPKey ()
  {}
  
  // application constructor
  public TptbmPKey (int vpn_id, int vpn_nb)
  {
    this.vpn_id = vpn_id;
    this.vpn_nb = vpn_nb;
  }
  
  // getter/setter methods
  @Column (name="VPN_ID")
  public int getVpn_id ()
  {return vpn_id;}
  
  public void setVpn_id (int value)
  {this.vpn_id = value;}
  
  @Column (name="VPN_NB")
  public int getVpn_nb ()
  {return vpn_nb;}
  
  public void setVpn_nb (int value)
  {this.vpn_nb = value;}  
  
  
  // identify the key
  public String toString ()
  {
    return "Tptbm key: <" + getVpn_id () + ">,<" + getVpn_nb () + ">";
  }

  // return a unique identifier
  public int hashCode () 
  {
    return getVpn_id () ^ getVpn_nb ();
  }
  
  // test for equality with another TptbmPKey
  public boolean equals (Object other)
  {
    if (other == this)
      return true;
    
    if (other instanceof TptbmPKey)
    {
      TptbmPKey otherKey = (TptbmPKey) other;
      
      return otherKey.getVpn_id () == this.getVpn_id () &&
        otherKey.getVpn_nb () == this.getVpn_nb ();
    }
    
    return false;
  }
}
