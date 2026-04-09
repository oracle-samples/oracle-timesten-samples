/*
* Copyright (c) 2025, Oracle and/or its affiliates.
*
* Licensed under the Universal Permissive License v 1.0 as shown
* at http://oss.oracle.com/licenses/upl
*/
package org.hibernate.dialect.sequence;

import org.hibernate.dialect.sequence.SequenceSupport;
import org.hibernate.MappingException;

public class TimesTenSequenceSupport implements SequenceSupport {

  public static final SequenceSupport INSTANCE = new TimesTenSequenceSupport();

  @Override
  public boolean supportsSequences() {
    return true;
  }

  @Override
  public boolean supportsPooledSequences() {
    return true;
  }

  @Override
  public String getSelectSequenceNextValString(String sequenceName) {
    return sequenceName + ".nextval";
  }

  @Override
  public String getSequenceNextValString(String sequenceName) {
    return "select " + sequenceName + ".nextval from sys.dual";
  }
  
  @Override
  public String getFromDual() {
    return " from sys.dual";
  }

  @Override
  public String getCreateSequenceString(String sequenceName) {
    return "create sequence " + sequenceName;
  }

  @Override
  public String getDropSequenceString(String sequenceName) {
    return "drop sequence " + sequenceName;
  }

}
