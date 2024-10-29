/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 */
package org.hibernate.dialect;

import java.sql.Types;

import org.hibernate.Hibernate;
import org.hibernate.cfg.Environment;
import org.hibernate.dialect.function.NoArgSQLFunction;
import org.hibernate.dialect.function.NvlFunction;
import org.hibernate.dialect.function.SQLFunctionTemplate;
import org.hibernate.dialect.function.StandardSQLFunction;
import org.hibernate.dialect.function.VarArgsSQLFunction;
import org.hibernate.sql.ANSIJoinFragment;
import org.hibernate.sql.JoinFragment;
import org.hibernate.type.StandardBasicTypes;
import org.hibernate.dialect.Dialect;

/**
 * A SQL dialect for TimesTen version 11.2.2.x.x.
 *
 * This dialect is designed for use with TimesTen 11.2.2.x.x databases
 * configured for Oracle type mode (TypeMode=0).
 *
 * Changes from previous versions:
 *
 * 1. CLOB, NCLOB and BLOB data types are now supported by TimesTen.
 * 2. Added Oracle implementation of getBasicSelectClauseNullString() and
 *    getSelectClauseNullString().
 * 3. Added support for the ADD_MONTHS(), MONTHS_BETWEEN(), SOUNDEX() and
 *    REPLACE () SQL functions.
 *
 */
 
 
public class TimesTenDialect1122
  extends Dialect
{

  public TimesTenDialect1122()
  {
    super();
    
    // type mappings
    registerColumnType(Types.BIT, "TT_TINYINT");
    registerColumnType(Types.SMALLINT, "TT_SMALLINT");
    registerColumnType(Types.TINYINT, "TT_TINYINT");
    registerColumnType(Types.INTEGER, "TT_INTEGER");
    registerColumnType(Types.BIGINT, "TT_BIGINT");
    
    registerColumnType(Types.CHAR, "CHAR(1)");
    registerColumnType(Types.VARCHAR, 4194304, "VARCHAR2($l)");
    registerColumnType(Types.LONGVARCHAR, 4194304, "VARCHAR2($l)");
    
    registerColumnType(Types.BINARY, 8300, "BINARY($l)" );
    registerColumnType(Types.VARBINARY, 4194304, "VARBINARY($l)");
    registerColumnType(Types.LONGVARBINARY, 4194304, "VARBINARY($l)");
    
    registerColumnType(Types.NUMERIC, "NUMBER($p,$s)"); 
    registerColumnType(Types.DECIMAL, "NUMBER($p,$s)" );   
    registerColumnType(Types.FLOAT, "BINARY_FLOAT");
    registerColumnType(Types.DOUBLE, "BINARY_DOUBLE");
    
    registerColumnType(Types.DATE, "TT_DATE");
    registerColumnType(Types.TIME, "TT_TIME");
    registerColumnType(Types.TIMESTAMP, "TIMESTAMP");

    registerColumnType(Types.BLOB, "BLOB");
    registerColumnType(Types.CLOB, "CLOB");
    registerColumnType(Types.NCLOB, "NCLOB");


    // properties
    getDefaultProperties().setProperty(
      Environment.USE_STREAMS_FOR_BINARY, "true");
                                       
    getDefaultProperties().setProperty(
      Environment.STATEMENT_BATCH_SIZE, DEFAULT_BATCH_SIZE);
    
    // string functions                                 
    registerFunction("lower", new StandardSQLFunction("lower"));
    registerFunction("upper", new StandardSQLFunction("upper"));
    registerFunction("rtrim", new StandardSQLFunction("rtrim"));
    registerFunction("ltrim", new StandardSQLFunction("ltrim") );
    registerFunction("length", 
      new StandardSQLFunction("length", StandardBasicTypes.LONG) );
    registerFunction( "concat", 
      new VarArgsSQLFunction( StandardBasicTypes.STRING, "(", "||", ")" ) );
    registerFunction("to_char", 
      new StandardSQLFunction("to_char", StandardBasicTypes.STRING));
    registerFunction("chr", 
      new StandardSQLFunction("chr", StandardBasicTypes.CHARACTER));
    registerFunction("instr", 
      new StandardSQLFunction("instr", StandardBasicTypes.INTEGER) );
    registerFunction("instrb", 
      new StandardSQLFunction("instrb", StandardBasicTypes.INTEGER) );
    registerFunction("lpad", 
      new StandardSQLFunction("lpad", StandardBasicTypes.STRING) );
    registerFunction( "rpad", 
      new StandardSQLFunction("rpad", StandardBasicTypes.STRING) );
    registerFunction("substr", 
      new StandardSQLFunction("substr", StandardBasicTypes.STRING) );
    registerFunction("substrb", 
      new StandardSQLFunction("substrb", StandardBasicTypes.STRING) );
    registerFunction("substring", 
      new StandardSQLFunction( "substr", StandardBasicTypes.STRING ) );
    registerFunction("locate", 
      new SQLFunctionTemplate( StandardBasicTypes.INTEGER, "instr(?2,?1)" ) );
    registerFunction( "str", 
      new StandardSQLFunction("to_char", StandardBasicTypes.STRING) );
    registerFunction( "soundex", 
      new StandardSQLFunction("soundex") );     
    registerFunction( "replace", 
      new StandardSQLFunction("replace", StandardBasicTypes.STRING) );

    // date/time functions  
    registerFunction("to_date", 
      new StandardSQLFunction("to_date", StandardBasicTypes.TIMESTAMP));
    registerFunction("sysdate", 
      new NoArgSQLFunction("sysdate", StandardBasicTypes.TIMESTAMP, false));
    registerFunction("getdate", 
      new NoArgSQLFunction("getdate", StandardBasicTypes.TIMESTAMP, false));

    registerFunction("current_date", 
      new NoArgSQLFunction("sysdate", StandardBasicTypes.DATE, false));
    registerFunction("current_time", 
      new NoArgSQLFunction("sysdate", StandardBasicTypes.TIME, false));
    registerFunction("current_timestamp", 
      new NoArgSQLFunction("sysdate", StandardBasicTypes.TIMESTAMP, false));

    // Multi-param date dialect functions...
    registerFunction( "add_months", 
      new StandardSQLFunction("add_months", StandardBasicTypes.DATE) );
    registerFunction( "months_between",
      new StandardSQLFunction("months_between", StandardBasicTypes.FLOAT) );
 

    // math functions
    registerFunction("abs", new StandardSQLFunction("abs") );
    registerFunction("sign", 
      new StandardSQLFunction("sign", StandardBasicTypes.INTEGER));
    registerFunction("mod", new StandardSQLFunction("mod"));
    registerFunction("round", new StandardSQLFunction("round"));
    registerFunction("trunc", new StandardSQLFunction("trunc"));
    registerFunction("ceil", new StandardSQLFunction("ceil"));
    registerFunction("floor", new StandardSQLFunction("floor"));
    registerFunction("power", 
      new StandardSQLFunction("power", StandardBasicTypes.FLOAT));
    
    // misc. functions
    registerFunction("nvl", new StandardSQLFunction("nvl"));
    registerFunction("coalesce", new NvlFunction());
    
    registerFunction( "user", 
      new NoArgSQLFunction("user", StandardBasicTypes.STRING, false) );
    
    registerFunction( "rowid", 
      new NoArgSQLFunction("rowid", StandardBasicTypes.LONG, false) );
    registerFunction( "rownum", 
      new NoArgSQLFunction("rownum", StandardBasicTypes.LONG, false) );
    
      
  }

  public boolean dropConstraints()
  {
    return true;
  }

  public boolean qualifyIndexName()
  {
    return false;
  }

  public boolean supportsUnique()
  {
    return true;
  }

  public boolean supportsUniqueConstraintInCreateAlterTable()
  {
    return false;
  }

  public String getAddColumnString()
  {
    return "add";
  }

  public boolean supportsSequences()
  {
    return true;
  }

  public boolean supportsPooledSequences() 
  {
    return true;
  }
  
  public String getSelectSequenceNextValString(String sequenceName)
  {
    return sequenceName + ".nextval";
  }

  public String getSequenceNextValString(String sequenceName)
  {
    return "select " + sequenceName + ".nextval from sys.dual";
  }

  public String getCreateSequenceString(String sequenceName)
  {
    return "create sequence " + sequenceName;
  }

  public String getDropSequenceString(String sequenceName)
  {
    return "drop sequence " + sequenceName;
  }

  public String getQuerySequencesString()
  {
    return "select name from sys.sequences";
  }

  public JoinFragment createOuterJoinFragment()
  {
    // this dialect previously used OracleJoinFragment which resulted in
    // various SQL errors - ANSIJoinFragment is more reliable
    return new ANSIJoinFragment();
  }



  public String getForUpdateString()
  {
    return " for update";
  }

  public boolean supportsForUpdateNowait()
  {
    return true;
  }

  public String getForUpdateNowaitString()
  {
    return " for update nowait";
  }


  public boolean supportsColumnCheck()
  {
    return false;
  }

  public boolean supportsTableCheck()
  {
    return false;
  }

  public boolean supportsLimitOffset()
  {
    return true;
  }

  public boolean supportsVariableLimit()
  {
    // a limit string using literals instead of parameters is 
    // required to translate from Hibernate's 0 based row numbers 
    // to TimesTen 1 based row numbers
    return false;
  }

  public boolean supportsLimit()
  {
    return true;
  }

  public boolean useMaxForLimit()
  {
    return true;
  }

  public boolean bindLimitParametersFirst() 
  {
    return true;
  }

  public String getLimitString (String query, int offset, int limit)
  {
    // translate from Hibernate's 0 based row numbers to TimesTen's
    // 1 based row numbers
    String pagingClause = " rows " + (offset + 1) + " to " + limit;

    // use the 'SELECT ROWS M to N' syntax
    return new StringBuffer(query.length() + pagingClause.length()).
      append(query).insert(6, pagingClause).toString();
  }


  public String getBasicSelectClauseNullString (int sqlType)
  {
    return super.getSelectClauseNullString (sqlType);
  }

  public String getSelectClauseNullString (int sqlType)
  {

    switch (sqlType)
    {

      case Types.VARCHAR:
      case Types.CHAR:
        return "to_char(null)";

      case Types.DATE:
      case Types.TIMESTAMP:
      case Types.TIME:
        return "to_date(null)";

      default:
        return "to_number(null)";
    }
  }


  public boolean supportsCurrentTimestampSelection()
  {
    return true;
  }

  public String getCurrentTimestampSelectString()
  {
    return "select sysdate from sys.dual";
  }

  public boolean isCurrentTimestampSelectStringCallable()
  {
    return false;
  }

  public boolean supportsTemporaryTables()
  {
    return true;
  }

  public String generateTemporaryTableName(String baseTableName)
  {
    String name = super.generateTemporaryTableName(baseTableName);
    return name.length() > 30? name.substring(1, 30): name;
  }

  public String getCreateTemporaryTableString()
  {
    return "create global temporary table";
  }

  public String getCreateTemporaryTablePostfix()
  {
    return "on commit delete rows";
  }

 
  public boolean supportsUnionAll() 
  {
    return true;
  }


  public boolean supportsEmptyInList()
  {
    return true;
  }

  public boolean supportsCircularCascadeDeleteConstraints()
  {
    return false;
  }

  
  
}
