/*
* Copyright (c) 2025, Oracle and/or its affiliates.
*
* Licensed under the Universal Permissive License v 1.0 as shown
* at http://oss.oracle.com/licenses/upl
*/
package org.hibernate.dialect;

import org.hibernate.sql.ast.spi.AbstractSqlAstTranslator;
import org.hibernate.sql.ast.Clause;
import org.hibernate.sql.ast.tree.Statement;
import org.hibernate.sql.ast.tree.select.SelectClause;
import org.hibernate.sql.ast.tree.select.QueryPart;
import org.hibernate.sql.ast.tree.select.QuerySpec;
import org.hibernate.sql.ast.tree.expression.Expression;
import org.hibernate.sql.exec.spi.JdbcOperation;
import org.hibernate.internal.util.collections.Stack;
import org.hibernate.engine.spi.SessionFactoryImplementor;


public class TimesTenSqlAstTranslator<T extends JdbcOperation> extends AbstractSqlAstTranslator<T> {
  
  public TimesTenSqlAstTranslator(SessionFactoryImplementor sessionFactory, Statement statement) {
    super(sessionFactory,statement);
  }

  @Override
  protected void visitSqlSelections(SelectClause selectClause) {
    // For TimesTen we need to add "ROWS m TO n" just after SELECT
    renderRowsToClause( (QuerySpec) getQueryPartStack().getCurrent() );
    super.visitSqlSelections( selectClause );
  }

  @Override
  protected void renderRowsToClause(Expression offsetClauseExpression, Expression fetchClauseExpression) {
    // offsetClauseExpression -> firstRow
    // fetchClauseExpression  -> maxRows
    final Stack<Clause> clauseStack = getClauseStack();

    if ( offsetClauseExpression == null && fetchClauseExpression != null ) {
      // We only have a maxRows/limit. We use 'SELECT FIRST n' syntax
      appendSql("first ");
      clauseStack.push( Clause.FETCH );
      try {
        renderFetchExpression( fetchClauseExpression );
      }
      finally {
        clauseStack.pop();
      }
    }
    else if ( offsetClauseExpression != null && fetchClauseExpression == null ) {
      throw new UnsupportedOperationException( 
        "Only passing setFirstResult(m) and not setMaxResults(n) to 'ROWS m TO n' clause not supported." 
      );
    }
    else if ( offsetClauseExpression != null && fetchClauseExpression != null ) {
      // We have offset and maxRows/limit. We use 'SELECT ROWS offset TO limit' syntax
      appendSql( "rows " );
      
      // Render offset parameter
      clauseStack.push( Clause.OFFSET );
      try {
        renderOffsetExpression( offsetClauseExpression );
      }
      finally {
        clauseStack.pop();
      }

      appendSql( " to " );

      // Render maxRows/limit parameter
      clauseStack.push( Clause.FETCH );
      try {
        // TimesTen includes both m and n rows of 'ROWS m to n'; 
        // We need to substract 1 row to fit maxRows
        renderFetchPlusOffsetExpressionAsLiteral( fetchClauseExpression, offsetClauseExpression, -1 );
      }
      finally {
        clauseStack.pop();
      }
    }

    appendSql( WHITESPACE );
  }

  @Override
  public void visitOffsetFetchClause(QueryPart queryPart) {
    // Do nothing: Don't append anything else at the end for TimesTen
    // as we already did this after SELECT
  }

}
