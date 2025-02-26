/*
* Copyright (c) 2025, Oracle and/or its affiliates.
*
* Licensed under the Universal Permissive License v 1.0 as shown
* at http://oss.oracle.com/licenses/upl
*/
package org.hibernate.dialect.pagination;

import org.hibernate.query.spi.Limit;
import org.hibernate.dialect.pagination.LimitHandler;
import org.hibernate.dialect.pagination.AbstractLimitHandler;

public class TimesTenLimitHandler extends AbstractLimitHandler {
  public static final TimesTenLimitHandler INSTANCE = new TimesTenLimitHandler();

  public TimesTenLimitHandler(){
  }

  @Override
  public boolean supportsLimit() {
    return true;
  }

  @Override
  public boolean supportsOffset() {
    return false;
  }
  
  @Override
  public boolean supportsLimitOffset() { 
    return true;
  }

  @Override
  public boolean supportsVariableLimit() {
    // a limit string using literals instead of parameters is
    // required to translate from Hibernate's 0 based row numbers
    // to TimesTen 1 based row numbers
    return false;
  }

  @Override
  // TimesTen is 1 based
  public int convertToFirstRowValue(int zeroBasedFirstResult) {
    return zeroBasedFirstResult + 1;
  }

  @Override
  public boolean useMaxForLimit() {
    return true;
  }

  @Override
  public boolean bindLimitParametersFirst() {
    return true;
  }

  protected String limitClause(boolean hasFirstRow) {
    return hasFirstRow ? " rows ? to ?" : " first ?";
  }

}
