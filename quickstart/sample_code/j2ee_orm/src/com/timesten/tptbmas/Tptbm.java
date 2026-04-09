/*
* Copyright (c) 1999, 2025, Oracle and/or its affiliates.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package com.timesten.tptbmas;

import jakarta.persistence.Column;
import jakarta.persistence.EmbeddedId;
import jakarta.persistence.Entity;
import jakarta.persistence.IdClass;
import jakarta.persistence.NamedQuery;
import jakarta.persistence.Table;


/* This is the POJO (Plain Old Java Object) class for TptbmAS. An instance of 
 * this class corresponds to a unique row in the TPTBM table. Annotations are 
 * used to map the fields of this class to the database columns of the TPTBM 
 * table.
 */


@Entity
@Table(name = "TPTBM")
@NamedQuery (name = "findAllTptbms", query = "SELECT OBJECT (A) FROM Tptbm A")

public class Tptbm implements java.io.Serializable
{
  
  /* Tptbm fields map to this TPTBM database table:

  CREATE TABLE TPTBM (
    VPN_ID INT NOT NULL,
    VPN_NB INT NOT NULL,
    DIRECTORY_NB CHAR (10) NOT NULL,
    LAST_CALLING_PARTY CHAR (10) NOT NULL,
    DESCR CHAR (100) NOT NULL,
    PRIMARY KEY (VPN_ID, VPN_NB));

  */
  
  
  // composite primary key
  private TptbmPKey key;
  
  private String directory_nb;
  private String last_calling_party;
  private String descr;
  
  
  // no-argument public constructor is required for EJB 3.0
  public Tptbm ()
  {return;}
  
  
  // public getter/setter methods
  
  @EmbeddedId 
  public TptbmPKey getKey ()
  {return key;}
  
  public void setKey (TptbmPKey value)
  {this.key = value;}
  
  @Column (name="DIRECTORY_NB")
  public String getDirectory_nb ()
  {return directory_nb;}
  
  public void setDirectory_nb (String value)
  {this.directory_nb = value;}
  
  @Column (name="LAST_CALLING_PARTY")
  public String getLast_calling_party ()
  {return last_calling_party;}
  
  public void setLast_calling_party (String value)
  {this.last_calling_party = value;}
  
  @Column (name="DESCR")
  public String getDescr ()
  {return descr;}
  
  public void setDescr (String value)
  {this.descr = value;}


  
  // identify this instance
  public String toString ()
  {

    return "Tptbm row: <" + getKey ().toString () + ">,<" + getDirectory_nb () +
      ">,<" + getLast_calling_party () + ">,<" + getDescr () + ">";
 
  }
}











