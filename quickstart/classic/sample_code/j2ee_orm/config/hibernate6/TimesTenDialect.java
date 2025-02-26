/*
* Copyright (c) 2025, Oracle and/or its affiliates.
*
* Licensed under the Universal Permissive License v 1.0 as shown
* at http://oss.oracle.com/licenses/upl
*/
package org.hibernate.dialect;

import java.sql.Types;
import java.util.Date;

import org.hibernate.dialect.Dialect;
import org.hibernate.dialect.function.CommonFunctionFactory;
import org.hibernate.dialect.function.StandardSQLFunction;
import org.hibernate.dialect.function.CurrentFunction;
import org.hibernate.dialect.temptable.TemporaryTableKind;
import org.hibernate.dialect.sequence.SequenceSupport;
import org.hibernate.dialect.pagination.LimitHandler;
import org.hibernate.boot.model.FunctionContributions;
import org.hibernate.boot.model.TypeContributions;
import org.hibernate.engine.jdbc.dialect.spi.DialectResolutionInfo;
import org.hibernate.engine.spi.SessionFactoryImplementor;
import org.hibernate.type.descriptor.sql.spi.DdlTypeRegistry;
import org.hibernate.type.descriptor.sql.internal.DdlTypeImpl;
import org.hibernate.type.spi.TypeConfiguration;
import org.hibernate.type.SqlTypes;
import org.hibernate.type.BasicType;
import org.hibernate.type.BasicTypeRegistry;
import org.hibernate.type.StandardBasicTypes;
import org.hibernate.query.sqm.produce.function.StandardFunctionArgumentTypeResolvers;
import org.hibernate.query.sqm.produce.function.FunctionParameterType;
import org.hibernate.sql.ast.SqlAstTranslator;
import org.hibernate.sql.ast.SqlAstTranslatorFactory;
import org.hibernate.sql.ast.spi.StandardSqlAstTranslatorFactory;
import org.hibernate.sql.exec.spi.JdbcOperation;
import org.hibernate.sql.ast.tree.Statement;
import org.hibernate.service.ServiceRegistry;

import org.hibernate.dialect.sequence.TimesTenSequenceSupport;
import org.hibernate.dialect.pagination.TimesTenLimitHandler;

/**
* A SQL dialect for TimesTen with Hibernate 6
*
* This dialect was tested with TimesTen 22.1 databases
* configured for Oracle type mode (TypeMode=0).
*
* Changes from previous versions:
*
* 1. Support for Hibernate 6
* 2. Added support for uid, SESSION_USER, SYSTEM_USER, and CURRENT_USER SQL functions
*
* Known unsupported features:
* 
* 1. CLOB/BLOB SQL functions.
* 2. Subqueries that use addition. EG: 
*      select (sysdate + numtodsinterval(1,'Hour')) from...
* 3. timestampadd and timestampdiff functions.
*
*/
public class TimesTenDialect extends Dialect {
	private static final DatabaseVersion MINIMUM_VERSION = DatabaseVersion.make( 22, 1);

  public TimesTenDialect() {
    super( MINIMUM_VERSION );
  }

  public TimesTenDialect(DialectResolutionInfo info) {
    super( info );
  }

  @Override
  protected DatabaseVersion getMinimumSupportedVersion() {
    return MINIMUM_VERSION;
  }

  @Override
  protected void registerColumnTypes(TypeContributions typeContributions, ServiceRegistry serviceRegistry) {
    super.registerColumnTypes( typeContributions, serviceRegistry );
    final DdlTypeRegistry ddlTypeRegistry = typeContributions.getTypeConfiguration().getDdlTypeRegistry();

    if ( getVersion().isSameOrAfter( 25 ) ) {
      ddlTypeRegistry.addDescriptor( new DdlTypeImpl( SqlTypes.JSON, "json", this ) );
    }

  }

  @Override
  protected String columnType(int sqlTypeCode) {
    switch ( sqlTypeCode ) {
      case SqlTypes.BIT:
      case SqlTypes.TINYINT:
        return "TT_TINYINT";
      case SqlTypes.SMALLINT:
        return "TT_SMALLINT";
      case SqlTypes.INTEGER:
        return "TT_INTEGER";
      case SqlTypes.BIGINT:
        return "TT_BIGINT";

      case SqlTypes.CHAR:
        return "CHAR(1)";
      case SqlTypes.VARCHAR:
      case SqlTypes.LONGVARCHAR:
        return "VARCHAR2($l)";

      case SqlTypes.BINARY:
        return "BINARY($1)";
      case SqlTypes.VARBINARY:
      case SqlTypes.LONGVARBINARY:
        return "VARBINARY($1)";

      case SqlTypes.NUMERIC:
      case SqlTypes.DECIMAL:
        return "NUMBER($p,$s)";
      case SqlTypes.FLOAT:
        return "BINARY_FLOAT";
      case SqlTypes.DOUBLE:
        return "BINARY_DOUBLE";

      case SqlTypes.DATE:
        return "TT_DATE";
      case SqlTypes.TIME:
        return "TT_TIME";
      case SqlTypes.TIMESTAMP:
        return "TIMESTAMP";

      case SqlTypes.BLOB:
        return "BLOB";
      case SqlTypes.CLOB:
        return "CLOB";
      case SqlTypes.NCLOB:
        return "NCLOB";

      default:
        return super.columnType( sqlTypeCode ); 
    }
  }

	@Override
	public void initializeFunctionRegistry(FunctionContributions functionContributions) {
		super.initializeFunctionRegistry(functionContributions);

		final TypeConfiguration typeConfiguration = functionContributions.getTypeConfiguration();
		CommonFunctionFactory functionFactory     = new CommonFunctionFactory(functionContributions);
    final BasicTypeRegistry basicTypeRegistry = typeConfiguration.getBasicTypeRegistry();
    final BasicType<Date>   timestampType     = basicTypeRegistry.resolve( StandardBasicTypes.TIMESTAMP );
    final BasicType<Date>   dateType          = basicTypeRegistry.resolve( StandardBasicTypes.DATE );
    final BasicType<Date>   timeType          = basicTypeRegistry.resolve( StandardBasicTypes.TIME );
    final BasicType<String> stringType        = basicTypeRegistry.resolve( StandardBasicTypes.STRING );
    final BasicType<Long>   longType          = basicTypeRegistry.resolve( StandardBasicTypes.LONG );
    final BasicType<Integer>intType           = basicTypeRegistry.resolve( StandardBasicTypes.INTEGER );

    // String Functions
    functionContributions.getFunctionRegistry().register( 
      "lower", new StandardSQLFunction("lower") 
    );
    functionContributions.getFunctionRegistry().register( 
      "upper", new StandardSQLFunction("upper") 
    );
    functionContributions.getFunctionRegistry().register( 
      "rtrim", new StandardSQLFunction("rtrim") 
    );
    functionContributions.getFunctionRegistry().register( 
      "ltrim", new StandardSQLFunction("ltrim") 
    );
    functionContributions.getFunctionRegistry().register( 
      "length", new StandardSQLFunction("length", StandardBasicTypes.LONG)
    );
    functionFactory.concat_pipeOperator();
    functionContributions.getFunctionRegistry().register( 
      "to_char", new StandardSQLFunction("to_char", StandardBasicTypes.STRING)
    );
    functionContributions.getFunctionRegistry().register( 
      "chr", new StandardSQLFunction("chr", StandardBasicTypes.CHARACTER)
    );
    functionContributions.getFunctionRegistry().register( 
      "instr", new StandardSQLFunction("instr", StandardBasicTypes.INTEGER) 
    );
    functionContributions.getFunctionRegistry().register( 
      "instrb", new StandardSQLFunction("instrb", StandardBasicTypes.INTEGER)
    );
    functionContributions.getFunctionRegistry().register( 
      "lpad", new StandardSQLFunction("lpad", StandardBasicTypes.STRING)
    );
    functionContributions.getFunctionRegistry().register( 
      "rpad", new StandardSQLFunction("rpad", StandardBasicTypes.STRING)
    );
    functionContributions.getFunctionRegistry().register( 
      "substr", new StandardSQLFunction("substr", StandardBasicTypes.STRING)
    );
    functionContributions.getFunctionRegistry().register( 
      "substrb", new StandardSQLFunction("substrb", StandardBasicTypes.STRING) 
    );
    functionContributions.getFunctionRegistry().register( 
      "substring", new StandardSQLFunction( "substr", StandardBasicTypes.STRING )
    );
    functionFactory.locate();
    functionContributions.getFunctionRegistry().register( 
      "str", new StandardSQLFunction("to_char", StandardBasicTypes.STRING)
    );
    functionContributions.getFunctionRegistry().register( 
      "soundex", new StandardSQLFunction("soundex") 
    );
    functionContributions.getFunctionRegistry().register( 
      "replace", new StandardSQLFunction("replace", StandardBasicTypes.STRING)
    );

    // Date/Time Functions
    functionContributions.getFunctionRegistry().register( 
      "to_date", new StandardSQLFunction("to_date", StandardBasicTypes.TIMESTAMP)
    );
    functionContributions.getFunctionRegistry().register( 
      "sysdate", new CurrentFunction("sysdate", "sysdate", timestampType)
    );
    functionContributions.getFunctionRegistry().register( 
      "getdate", new StandardSQLFunction("getdate", StandardBasicTypes.TIMESTAMP)
    );

    functionContributions.getFunctionRegistry().register( 
      "current_date",      new CurrentFunction("sysdate", "sysdate", dateType) 
    );
    functionContributions.getFunctionRegistry().register( 
      "current_time",      new CurrentFunction("sysdate", "sysdate", timeType) 
    );
    functionContributions.getFunctionRegistry().register( 
      "current_timestamp", new CurrentFunction("sysdate", "sysdate", timestampType) 
    );
    functionContributions.getFunctionRegistry().register( 
      "to_timestamp", new StandardSQLFunction("to_timestamp", StandardBasicTypes.TIMESTAMP)
    );

    // Multi-param date dialect functions
    functionContributions.getFunctionRegistry().register( 
      "add_months",     new StandardSQLFunction("add_months", StandardBasicTypes.DATE)
    );
    functionContributions.getFunctionRegistry().register( 
      "months_between", new StandardSQLFunction("months_between", StandardBasicTypes.FLOAT)
    );

    // Math functions
    functionContributions.getFunctionRegistry().register( 
      "abs", new StandardSQLFunction("abs")
    );
     functionContributions.getFunctionRegistry().register( 
      "acos", new StandardSQLFunction("acos")
    );
    functionContributions.getFunctionRegistry().register( 
      "asin", new StandardSQLFunction("asin")
    );
    functionContributions.getFunctionRegistry().register( 
      "atan", new StandardSQLFunction("atan")
    );
     functionContributions.getFunctionRegistry().register( 
      "atan2", new StandardSQLFunction("atan2")
    );
    functionContributions.getFunctionRegistry().register( 
      "ceil", new StandardSQLFunction("ceil")
    );
    functionContributions.getFunctionRegistry().register( 
      "cos", new StandardSQLFunction("cos")
    );
    functionContributions.getFunctionRegistry().register( 
      "cosh", new StandardSQLFunction("cosh")
    );
    functionContributions.getFunctionRegistry().register( 
      "exp", new StandardSQLFunction("exp")
    );
    functionContributions.getFunctionRegistry().register( 
      "ln", new StandardSQLFunction("ln")
    );
    functionFactory.log();
    functionContributions.getFunctionRegistry().register( 
      "sin", new StandardSQLFunction("sin")
    );
    functionContributions.getFunctionRegistry().register( 
      "sign", new StandardSQLFunction("sign", StandardBasicTypes.INTEGER)
    );
    functionContributions.getFunctionRegistry().register( 
      "sinh", new StandardSQLFunction("sinh")
    );
    functionContributions.getFunctionRegistry().register( 
      "mod", new StandardSQLFunction("mod")
    );
    functionContributions.getFunctionRegistry().register( 
      "round", new StandardSQLFunction("round")
    );
    functionContributions.getFunctionRegistry().register( 
      "trunc", new StandardSQLFunction("trunc")
    );
    functionContributions.getFunctionRegistry().register( 
      "tan", new StandardSQLFunction("tan")
    );
    functionContributions.getFunctionRegistry().register( 
      "tanh", new StandardSQLFunction("tanh")
    );
    functionContributions.getFunctionRegistry().register( 
      "floor", new StandardSQLFunction("floor")
    );
    functionContributions.getFunctionRegistry().register( 
      "power", new StandardSQLFunction("power", StandardBasicTypes.FLOAT)
    );

    // Bitwise functions
    functionFactory.bitand();
    functionContributions.getFunctionRegistry().register( 
      "bitnot", new StandardSQLFunction("bitnot")
    );

		functionContributions.getFunctionRegistry()
		  .patternDescriptorBuilder( "bitor", "(?1+?2-bitand(?1,?2))")
			.setExactArgumentCount( 2 )
			.setArgumentTypeResolver( StandardFunctionArgumentTypeResolvers
        .ARGUMENT_OR_IMPLIED_RESULT_TYPE )
			.register();

		functionContributions.getFunctionRegistry()
		  .patternDescriptorBuilder( "bitxor", "(?1+?2-2*bitand(?1,?2))")
			.setExactArgumentCount( 2 )
			.setArgumentTypeResolver( StandardFunctionArgumentTypeResolvers
        .ARGUMENT_OR_IMPLIED_RESULT_TYPE )
			.register();

    // Misc. functions
    functionContributions.getFunctionRegistry().register( 
      "nvl", new StandardSQLFunction("nvl")
    );
    functionFactory.coalesce();
    functionContributions.getFunctionRegistry().register( 
      "user",  new CurrentFunction("user", "user", stringType)
    );
    functionContributions.getFunctionRegistry().register( 
      "rowid", new CurrentFunction("rowid", "rowid", stringType)
    );
    functionContributions.getFunctionRegistry().register( 
      "uid", new CurrentFunction("uid", "uid", intType)
    );
    functionContributions.getFunctionRegistry().register( 
      "rownum", new CurrentFunction("rownum", "rownum", longType)
    );
    functionContributions.getFunctionRegistry().register( 
      "vsize", new StandardSQLFunction("vsize")
    );
    functionContributions.getFunctionRegistry().register( 
      "SESSION_USER", new CurrentFunction("SESSION_USER","SESSION_USER", stringType)
    );
    functionContributions.getFunctionRegistry().register( 
      "SYSTEM_USER",  new CurrentFunction("SYSTEM_USER", "SYSTEM_USER",  stringType)
    );
    functionContributions.getFunctionRegistry().register( 
      "CURRENT_USER", new CurrentFunction("CURRENT_USER","CURRENT_USER", stringType)
    );
		
	}

  @Override
  public boolean qualifyIndexName() {
    return false;
  }

  @Override
  public String getAddColumnString() {
    return "add";
  }

  @Override 
  public String getNativeIdentifierGeneratorStrategy() {
      return "sequence";
  }

  @Override
  public SequenceSupport getSequenceSupport() {
    return TimesTenSequenceSupport.INSTANCE;
  }

  @Override
  public String getQuerySequencesString() {
    return "select name from sys.sequences";
  }

  @Override
  public String getForUpdateString() {
    return " for update";
  }
  
  @Override
  public boolean supportsNoWait() {
    return true;
  }

  @Override
  public String getForUpdateNowaitString() {
    return " for update nowait";
  }

  @Override
  public boolean supportsColumnCheck() {
    return false;
  }

  @Override
  public boolean supportsTableCheck() {
    return false;
  }

  @Override
  public LimitHandler getLimitHandler() {
    return TimesTenLimitHandler.INSTANCE;
  }

  @Override
  public SqlAstTranslatorFactory getSqlAstTranslatorFactory() {
    return new StandardSqlAstTranslatorFactory() {
      @Override
     protected <T extends JdbcOperation> SqlAstTranslator<T> buildTranslator(
          SessionFactoryImplementor sessionFactory, Statement statement) {
        return new TimesTenSqlAstTranslator<>( sessionFactory, statement );
      }
    };
  }

  @Override
  public String getSelectClauseNullString (int sqlType, TypeConfiguration typeConfiguration)
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

  @Override
  public boolean supportsCurrentTimestampSelection() {
    return true;
  }

  @Override
  public String getCurrentTimestampSelectString() {
    return "select sysdate from sys.dual";
  }

  @Override
  public boolean isCurrentTimestampSelectStringCallable() {
    return false;
  }

  @Override
  public boolean supportsCircularCascadeDeleteConstraints() {
    return false;
  }

  @Override
  public TemporaryTableKind getSupportedTemporaryTableKind() {
    return TemporaryTableKind.GLOBAL;
  }

  @Override
  public String getTemporaryTableCreateOptions() {
    return "on commit delete rows";
  }

  @Override                                                            
  protected void initDefaultProperties() {
    super.initDefaultProperties(); 
  }

  @Override
  public String currentDate() {
      return "sysdate";
  }

	@Override
	public String currentTime() {
	  return "sysdate";
	}

  @Override
  public int getMaxVarcharLength() {
    // 1 to 4,194,304 bytes according to TimesTen Doc
    return 4194304;
  }

  @Override
  public int getMaxVarbinaryLength() {
    // 1 to 4,194,304 bytes according to TimesTen Doc
    return 4194304;
  }

  @Override
  public boolean supportsBitType() {
    return false;
  }

  @Override
  public boolean isEmptyStringTreatedAsNull() {
    return true;
  }

  @Override
  public int getMaxAliasLength() {
    // This should be 30, but hibernate uses 10 spaces
    return 20;
  }

  @Override
  public int getMaxIdentifierLength() {
    return 30;
  }

  @Override
  public boolean canCreateSchema() {
    return false;
  }

  @Override
  public boolean supportsTupleDistinctCounts() {
    return false;
  }

  public String getDual() {
    return "dual"; 
  }

  public String getFromDualForSelectOnly() {
    return " from dual";
  }

}
