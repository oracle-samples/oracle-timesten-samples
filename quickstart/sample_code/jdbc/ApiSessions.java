/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates how an application can keep active API session state in
 *   TimesTen. Fast access to short-lived operational data helps services route
 *   requests, update activity counters, and retire sessions that are no longer
 *   active.
 *
 *   The sample uses an 'api_sessions' table with session identifiers, user
 *   names, service names, regions, request counters, session status, and a
 *   last-seen timestamp. It demonstrates basic SQL operations that are common
 *   in low-latency application state management:
 *     - Creates the 'api_sessions' table
 *     - Populates the table (based on 'NUM_RECORDS')
 *     - Performs a number of SELECTS (based on 'READ_PERCENTAGE')
 *     - Updates request counters and last-seen timestamps for active sessions
 *       (based on 'UPDATE_PERCENTAGE')
 *     - Deletes a number of session records (based on 'UPDATE_PERCENTAGE')
 *     - Drops the table
 */

package jdbc.demo;

import java.io.PrintWriter;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Timestamp;
import java.util.ArrayList;
import java.util.List;

/**
 * Provides a JDBC-based demonstration of an application-managed API session
 * state cache backed by TimesTen.
 */
public class ApiSessions
{
  private static final String PROGRAM_NAME = "ApiSessions";
  private static final String DEFAULT_USERNAME = "appuser";

  private static final String TIMESTEN_DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";
  private static final String TIMESTEN_CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";

  private static final String DIRECT_URL_PREFIX = "jdbc:timesten:direct:";
  private static final String CLIENT_URL_PREFIX = "jdbc:timesten:client:";

  private static final String TABLE_NAME = "api_sessions";

  private static final int NUM_RECORDS = 100;
  private static final int READ_PERCENTAGE = 80;
  private static final int UPDATE_PERCENTAGE = 20;

  private static final String CREATE_TABLE_SQL =
    "CREATE TABLE " + TABLE_NAME + " ("
    + "session_id     TT_INT        NOT NULL PRIMARY KEY, "
    + "user_name      VARCHAR2(30)  NOT NULL, "
    + "service_name   VARCHAR2(40)  NOT NULL, "
    + "region         VARCHAR2(20)  NOT NULL, "
    + "request_count  TT_INT        NOT NULL, "
    + "session_status VARCHAR2(12)  NOT NULL, "
    + "last_seen      TIMESTAMP     NOT NULL)";

  private static final String INSERT_SQL =
    "INSERT INTO " + TABLE_NAME
    + " VALUES (:1,:2,:3,:4,:5,:6,:7)";

  private static final String SELECT_SQL =
    "SELECT user_name, service_name, region, request_count, session_status "
    + "FROM " + TABLE_NAME + " "
    + "WHERE session_id = :1";

  private static final String UPDATE_SQL =
    "UPDATE " + TABLE_NAME + " "
    + "SET request_count = request_count + 1, "
    + "    session_status = :1, "
    + "    last_seen = :2 "
    + "WHERE session_id = :3";

  private static final String DELETE_SQL =
    "DELETE FROM " + TABLE_NAME + " WHERE session_id = :1";

  private static final String DROP_SQL =
    "DROP TABLE " + TABLE_NAME;

  private static final String[] SERVICES = new String[] {
    "orders", "payments", "search", "support"
  };

  private static final String[] REGIONS = new String[] {
    "us-east", "us-west", "eu-central", "ap-south"
  };

  private final IOLibrary ioLibrary;

  public static void main(String[] args)
  {
    int exitCode = new ApiSessions().run(args);
    if (exitCode != 0)
    {
      System.exit(exitCode);
    }
  }

  public ApiSessions()
  {
    ioLibrary = new IOLibrary(System.err);
  }

  private int run(String[] args)
  {
    String usage = buildUsage();
    if (!parseOptions(args, usage))
    {
      return 1;
    }

    if (ioLibrary.opt_doTrace)
    {
      DriverManager.setLogWriter(new PrintWriter(System.out, true));
    }

    if (!loadDrivers())
    {
      return 1;
    }

    AccessControl accessControl = new AccessControl();
    String username = resolveUsername(accessControl);
    String password = accessControl.getPassword(username);
    String url = buildJdbcUrl();

    System.out.println();
    System.out.println("Connecting using URL: " + url);

    Connection connection = null;
    try
    {
      connection = DriverManager.getConnection(url, username, password);
      connection.setAutoCommit(true);

      System.out.println("Connected");
      runDemo(connection);
      System.out.println("Completed API sessions sample operations");
      return 0;
    }
    catch (SQLException e)
    {
      printSQLException(e);
      return 1;
    }
    finally
    {
      dropTable(connection, false);

      if (connection != null)
      {
        try
        {
          connection.close();
        }
        catch (SQLException e)
        {
          printSQLException(e);
        }
      }
    }
  }

  private boolean parseOptions(String[] args, String usage)
  {
    return ioLibrary.parseOpts(args, usage);
  }

  private String buildUsage()
  {
    return ioLibrary.getUsageString(PROGRAM_NAME);
  }

  private boolean loadDrivers()
  {
    try
    {
      Class.forName(TIMESTEN_DIRECT_DRIVER);
      if (ioLibrary.opt_doClient)
      {
        Class.forName(TIMESTEN_CLIENT_DRIVER);
      }
      return true;
    }
    catch (ClassNotFoundException e)
    {
      String drivers = ioLibrary.opt_doClient
                       ? TIMESTEN_DIRECT_DRIVER + ", " + TIMESTEN_CLIENT_DRIVER
                       : TIMESTEN_DIRECT_DRIVER;
      System.err.println("Unable to load TimesTen JDBC driver(s): " + drivers + ". " + e.getMessage());
      return false;
    }
  }

  private String resolveUsername(AccessControl accessControl)
  {
    String username = accessControl.getUsername();
    if (username == null || username.isEmpty())
    {
      return DEFAULT_USERNAME;
    }
    return username;
  }

  private String buildJdbcUrl()
  {
    String prefix = ioLibrary.opt_doClient ? CLIENT_URL_PREFIX : DIRECT_URL_PREFIX;
    return prefix + ioLibrary.opt_connstr;
  }

  private void runDemo(Connection connection) throws SQLException
  {
    dropTable(connection, false);
    createTable(connection);
    populateTable(connection);
    performDml(connection, "select");
    performDml(connection, "update");
    performDml(connection, "delete");
  }

  private void createTable(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(CREATE_TABLE_SQL))
    {
      statement.execute();
    }

    try (PreparedStatement statement = connection.prepareStatement("SELECT COUNT(*) FROM " + TABLE_NAME);
         ResultSet resultSet = statement.executeQuery())
    {
      if (resultSet.next() && resultSet.getInt(1) == 0)
      {
        System.out.println("Table " + TABLE_NAME + " has been created");
      }
    }
  }

  private void populateTable(Connection connection) throws SQLException
  {
    System.out.println("Populating table");

    int keyCnt = (int) Math.sqrt(NUM_RECORDS);
    if (keyCnt * keyCnt != NUM_RECORDS)
    {
      throw new IllegalStateException("NUM_RECORDS must have an exact square root");
    }

    try (PreparedStatement statement = connection.prepareStatement(INSERT_SQL))
    {
      for (int i = 0; i < keyCnt; i++)
      {
        for (int j = 0; j < keyCnt; j++)
        {
          int sessionId = (i * keyCnt) + j;
          String userName = "user_" + i + j;
          String serviceName = SERVICES[sessionId % SERVICES.length];
          String region = REGIONS[sessionId % REGIONS.length];
          int requestCount = sessionId % 25;
          String sessionStatus = "ACTIVE";
          Timestamp lastSeen = currentTimestamp();

          statement.setInt(1, sessionId);
          statement.setString(2, userName);
          statement.setString(3, serviceName);
          statement.setString(4, region);
          statement.setInt(5, requestCount);
          statement.setString(6, sessionStatus);
          statement.setTimestamp(7, lastSeen);
          statement.executeUpdate();
        }

        System.out.println("  Inserted " + ((i + 1) * keyCnt) + " rows");
      }
    }

    try (PreparedStatement statement = connection.prepareStatement("SELECT COUNT(*) FROM " + TABLE_NAME);
         ResultSet resultSet = statement.executeQuery())
    {
      if (resultSet.next() && resultSet.getInt(1) != (keyCnt * keyCnt))
      {
        System.out.println("Error populating table");
      }
    }
  }

  private void performDml(Connection connection, String operation) throws SQLException
  {
    System.out.println("Performing " + operation + "s");

    int numOperations;
    if ("select".equals(operation))
    {
      numOperations = (int) (NUM_RECORDS * (READ_PERCENTAGE / 100.0));
    }
    else if ("update".equals(operation) || "delete".equals(operation))
    {
      numOperations = (int) (NUM_RECORDS * (UPDATE_PERCENTAGE / 100.0));
    }
    else
    {
      throw new IllegalArgumentException("Unsupported operation: " + operation);
    }

    int operationsPerformed = 0;
    int keyCnt = (int) Math.sqrt(NUM_RECORDS);

    for (int i = 0; i < keyCnt; i++)
    {
      for (int j = 0; j < keyCnt; j++)
      {
        int sessionId = (i * keyCnt) + j;
        if ("select".equals(operation))
        {
          try (PreparedStatement statement = connection.prepareStatement(SELECT_SQL))
          {
            statement.setInt(1, sessionId);
            try (ResultSet resultSet = statement.executeQuery())
            {
              if (resultSet.next())
              {
                operationsPerformed++;
              }
            }
          }
        }
        else if ("update".equals(operation))
        {
          try (PreparedStatement statement = connection.prepareStatement(UPDATE_SQL))
          {
            statement.setString(1, "ACTIVE");
            statement.setTimestamp(2, currentTimestamp());
            statement.setInt(3, sessionId);
            operationsPerformed += statement.executeUpdate();
          }
        }
        else
        {
          try (PreparedStatement statement = connection.prepareStatement(DELETE_SQL))
          {
            statement.setInt(1, sessionId);
            operationsPerformed += statement.executeUpdate();
          }
        }

        if (operationsPerformed == numOperations)
        {
          System.out.println("  " + operation + "(ed) " + operationsPerformed + " rows");
          return;
        }
      }

      System.out.println("  " + operation + "(ed) " + ((i + 1) * keyCnt) + " rows");
    }
  }

  private void dropTable(Connection connection, boolean reportMissing)
  {
    if (connection == null)
    {
      return;
    }

    try (PreparedStatement statement = connection.prepareStatement(DROP_SQL))
    {
      statement.execute();
    }
    catch (SQLException e)
    {
      if (reportMissing)
      {
        System.out.println("Table " + TABLE_NAME + " not dropped: " + e.getMessage());
      }
    }
  }

  private Timestamp currentTimestamp()
  {
    return new Timestamp(System.currentTimeMillis());
  }

  private void printSQLException(SQLException exception)
  {
    SQLException current = exception;
    while (current != null)
    {
      System.err.println("SQLException: " + current.getMessage());
      System.err.println("SQLState: " + current.getSQLState());
      System.err.println("Vendor error: " + current.getErrorCode());
      current = current.getNextException();
      if (current != null)
      {
        System.err.println("---");
      }
    }
  }
}
