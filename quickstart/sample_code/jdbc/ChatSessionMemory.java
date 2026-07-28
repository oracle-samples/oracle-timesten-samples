/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates how an application can use TimesTen as the primary
 *   store for short-lived AI chat/session memory. The application stores
 *   recent messages, tool-call metadata, safety labels, and citations in JSON
 *   so it can restore context quickly between turns.
 *
 *   NOTE: Responses are simulated; this sample does not call an AI model,
 *   perform vector search, or run in-database model inference.
 *
 *   The sample performs the following steps:
 *     - Creates a 'chat_sessions' table
 *     - Creates indexes for tenant/user and expiration lookups
 *     - Seeds one expired chat session
 *     - Starts and resumes sample chat sessions
 *     - Stores recent messages and metadata in JSON
 *     - Queries JSON fields to summarize active sessions
 *     - Deletes expired chat sessions
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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Provides a JDBC-based demonstration of an application-managed AI chat
 * session memory cache backed by TimesTen.
 */
public class ChatSessionMemory
{
  private static final String PROGRAM_NAME = "ChatSessionMemory";
  private static final String DEFAULT_USERNAME = "appuser";

  private static final String TIMESTEN_DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";
  private static final String TIMESTEN_CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";

  private static final String DIRECT_URL_PREFIX = "jdbc:timesten:direct:";
  private static final String CLIENT_URL_PREFIX = "jdbc:timesten:client:";

  private static final String TABLE_NAME = "chat_sessions";
  private static final int SESSION_TTL_MINUTES = 45;

  private static final String CREATE_TABLE_SQL =
    "CREATE TABLE " + TABLE_NAME + " ("
    + "session_key        VARCHAR2(64)   NOT NULL PRIMARY KEY, "
    + "tenant_id          VARCHAR2(30)   NOT NULL, "
    + "user_id            VARCHAR2(30)   NOT NULL, "
    + "conversation_topic VARCHAR2(80)   NOT NULL, "
    + "session_state      JSON, "
    + "created_at         TIMESTAMP      NOT NULL, "
    + "last_updated_at    TIMESTAMP      NOT NULL, "
    + "expires_at         TIMESTAMP      NOT NULL)";

  private static final String CREATE_TENANT_USER_INDEX_SQL =
    "CREATE INDEX idx_chat_sessions_tenant_user "
    + "ON " + TABLE_NAME + " (tenant_id, user_id)";

  private static final String CREATE_EXPIRES_INDEX_SQL =
    "CREATE INDEX idx_chat_sessions_expires "
    + "ON " + TABLE_NAME + " (expires_at)";

  private static final String INSERT_SESSION_SQL =
    "INSERT INTO " + TABLE_NAME + " ("
    + "session_key, tenant_id, user_id, conversation_topic, session_state, "
    + "created_at, last_updated_at, expires_at) "
    + "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

  private static final String UPDATE_SESSION_SQL =
    "UPDATE " + TABLE_NAME + " "
    + "SET session_state = ?, last_updated_at = ?, expires_at = ? "
    + "WHERE session_key = ?";

  private static final String SELECT_ACTIVE_SUMMARY_SQL =
    "SELECT tenant_id, "
    + "       user_id, "
    + "       conversation_topic, "
    + "       JSON_VALUE(session_state, '$.turn_count' RETURNING TT_INT), "
    + "       JSON_VALUE(session_state, '$.model.name' RETURNING VARCHAR2(60)), "
    + "       JSON_VALUE(session_state, '$.safety.status' RETURNING VARCHAR2(20)) "
    + "FROM " + TABLE_NAME + " "
    + "WHERE expires_at > ? "
    + "ORDER BY tenant_id, user_id, conversation_topic";

  private static final String SELECT_ACTIVE_STATE_SQL =
    "SELECT session_key, tenant_id, user_id, conversation_topic, "
    + "       JSON_SERIALIZE(session_state RETURNING VARCHAR2(4000)), "
    + "       last_updated_at, expires_at "
    + "FROM " + TABLE_NAME + " "
    + "WHERE expires_at > ? "
    + "ORDER BY last_updated_at DESC";

  private static final String DELETE_EXPIRED_SQL =
    "DELETE FROM " + TABLE_NAME + " WHERE expires_at <= ?";

  private static final String DROP_TABLE_SQL = "DROP TABLE " + TABLE_NAME;

  private static final ChatTurn[] SAMPLE_TURNS = new ChatTurn[] {
    new ChatTurn("retail_app", "user_001", "order-status", "support-summary-v1",
                 "Where is order 45012?",
                 "The order is currently in transit and should arrive tomorrow.",
                 "lookup_order_status", "shipment-feed",
                 "{\"responseStyle\":\"concise\",\"locale\":\"en-US\"}"),
    new ChatTurn("retail_app", "user_001", "order-status", "support-summary-v1",
                 "Can you repeat the earlier status in one sentence?",
                 "The order is still in transit and is expected to arrive tomorrow.",
                 "lookup_order_status", "shipment-feed",
                 "{\"responseStyle\":\"concise\",\"locale\":\"en-US\"}"),
    new ChatTurn("field_service", "user_204", "router-troubleshooting", "technician-assist-v1",
                 "I need the previous troubleshooting steps for the packet-loss issue.",
                 "The router is still showing intermittent packet loss, so continue with the cable and firmware checks.",
                 "lookup_device_notes", "field-notes",
                 "{\"responseStyle\":\"detailed\",\"locale\":\"en-US\"}")
  };

  private final IOLibrary ioLibrary;
  private String userOverride;
  private String passwordOverride;
  private final Map<String, SessionContext> sessionContexts = new HashMap<String, SessionContext>();

  private static final class ChatTurn
  {
    private final String tenantId;
    private final String userId;
    private final String topic;
    private final String modelName;
    private final String userMessage;
    private final String responseHint;
    private final String toolName;
    private final String citationRef;
    private final String preferencesJson;

    ChatTurn(String tenantId, String userId, String topic, String modelName,
             String userMessage, String responseHint, String toolName,
             String citationRef, String preferencesJson)
    {
      this.tenantId = tenantId;
      this.userId = userId;
      this.topic = topic;
      this.modelName = modelName;
      this.userMessage = userMessage;
      this.responseHint = responseHint;
      this.toolName = toolName;
      this.citationRef = citationRef;
      this.preferencesJson = preferencesJson;
    }
  }

  private static final class ChatMessage
  {
    private final String role;
    private final String content;
    private final Timestamp timestamp;
    private final String extraJson;

    ChatMessage(String role, String content, Timestamp timestamp, String extraJson)
    {
      this.role = role;
      this.content = content;
      this.timestamp = timestamp;
      this.extraJson = extraJson;
    }

    String toJson()
    {
      StringBuilder builder = new StringBuilder();
      builder.append('{');
      builder.append("\"role\":\"").append(escapeJson(role)).append("\",");
      builder.append("\"content\":\"").append(escapeJson(content)).append("\",");
      builder.append("\"timestamp\":\"").append(timestamp.toString()).append("\"");
      if (extraJson != null && !extraJson.isEmpty())
      {
        builder.append(',');
        builder.append(extraJson);
      }
      builder.append('}');
      return builder.toString();
    }
  }

  private static final class SessionContext
  {
    private final String sessionKey;
    private String tenantId;
    private String userId;
    private String topic;
    private String modelName;
    private String preferencesJson;
    private int turnCount;
    private final List<ChatMessage> messages = new ArrayList<ChatMessage>();

    SessionContext(String sessionKey, ChatTurn turn, String assistantText, Timestamp now)
    {
      this.sessionKey = sessionKey;
      this.tenantId = turn.tenantId;
      this.userId = turn.userId;
      this.topic = turn.topic;
      this.modelName = turn.modelName;
      this.preferencesJson = turn.preferencesJson;
      this.turnCount = 1;

      addSystemMessage(now);
      addUserMessage(turn.userMessage, now);
      addAssistantMessage(turn.toolName, turn.citationRef, assistantText, now);
    }

    void appendTurn(ChatTurn turn, String assistantText, Timestamp now)
    {
      addUserMessage(turn.userMessage, now);
      addAssistantMessage(turn.toolName, turn.citationRef, assistantText, now);
      this.tenantId = turn.tenantId;
      this.userId = turn.userId;
      this.topic = turn.topic;
      this.modelName = turn.modelName;
      this.preferencesJson = turn.preferencesJson;
      this.turnCount++;
    }

    String getLastUserMessage()
    {
      for (int i = messages.size() - 1; i >= 0; i--)
      {
        if ("user".equals(messages.get(i).role))
        {
          return messages.get(i).content;
        }
      }
      return "";
    }

    String toJson()
    {
      StringBuilder builder = new StringBuilder();
      builder.append('{');
      builder.append("\"tenantId\":\"").append(escapeJson(tenantId)).append("\",");
      builder.append("\"userId\":\"").append(escapeJson(userId)).append("\",");
      builder.append("\"topic\":\"").append(escapeJson(topic)).append("\",");
      builder.append("\"model\":{");
      builder.append("\"name\":\"").append(escapeJson(modelName)).append("\",");
      builder.append("\"provider\":\"simulated\"");
      builder.append("},");
      builder.append("\"preferences\":").append(preferencesJson).append(',');
      builder.append("\"safety\":{\"status\":\"allowed\"},");
      builder.append("\"turn_count\":").append(turnCount).append(',');
      builder.append("\"messages\":[");
      for (int i = 0; i < messages.size(); i++)
      {
        if (i > 0)
        {
          builder.append(',');
        }
        builder.append(messages.get(i).toJson());
      }
      builder.append(']');
      builder.append('}');
      return builder.toString();
    }

    private void addSystemMessage(Timestamp now)
    {
      messages.add(new ChatMessage(
          "system",
          "Keep recent chat context, citations, and safety metadata available for low-latency retrieval.",
          now,
          null));
    }

    private void addUserMessage(String content, Timestamp now)
    {
      messages.add(new ChatMessage("user", content, now, null));
    }

    private void addAssistantMessage(String toolName, String citationRef, String content, Timestamp now)
    {
      String extraJson =
        "\"tool_calls\":[{\"name\":\"" + escapeJson(toolName) + "\",\"status\":\"simulated\"}],"
        + "\"citations\":[{\"source\":\"" + escapeJson(citationRef) + "\",\"status\":\"simulated\"}]";
      messages.add(new ChatMessage("assistant", content, now, extraJson));
    }
  }

  private static final class ActiveSessionRow
  {
    private final String sessionKey;
    private final String tenantId;
    private final String userId;
    private final String topic;
    private final String sessionJson;
    private final Timestamp lastUpdatedAt;
    private final Timestamp expiresAt;

    ActiveSessionRow(String sessionKey, String tenantId, String userId, String topic,
                     String sessionJson, Timestamp lastUpdatedAt, Timestamp expiresAt)
    {
      this.sessionKey = sessionKey;
      this.tenantId = tenantId;
      this.userId = userId;
      this.topic = topic;
      this.sessionJson = sessionJson;
      this.lastUpdatedAt = lastUpdatedAt;
      this.expiresAt = expiresAt;
    }
  }

  public ChatSessionMemory()
  {
    ioLibrary = new IOLibrary(System.err);
  }

  public static void main(String[] args)
  {
    int exitCode = new ChatSessionMemory().run(args);
    if (exitCode != 0)
    {
      System.exit(exitCode);
    }
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
    String username = (userOverride != null) ? userOverride : resolveUsername(accessControl);
    String password = (passwordOverride != null)
                      ? passwordOverride
                      : accessControl.getPassword(username);
    String url = buildJdbcUrl();

    System.out.println("=== Chat session memory demo ===");
    System.out.println();
    System.out.println("Connecting using URL: " + url);

    Connection connection = null;
    try
    {
      connection = DriverManager.getConnection(url, username, password);
      connection.setAutoCommit(true);

      System.out.println("✓ Connected");
      runDemo(connection);
      System.out.println("✓ Completed chat session memory sample operations");
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
    List<String> forwardedArgs = new ArrayList<String>();

    for (int i = 0; i < args.length; i++)
    {
      String arg = args[i];

      if ("-u".equals(arg) || "-user".equals(arg) || "-username".equals(arg))
      {
        if (i + 1 >= args.length)
        {
          System.err.println("Missing value for " + arg);
          System.err.println(usage);
          return false;
        }
        userOverride = args[++i];
      }
      else if ("-p".equals(arg) || "-password".equals(arg) || "-pwd".equals(arg))
      {
        if (i + 1 >= args.length)
        {
          System.err.println("Missing value for " + arg);
          System.err.println(usage);
          return false;
        }
        passwordOverride = args[++i];
      }
      else
      {
        forwardedArgs.add(arg);
      }
    }

    String[] ioArgs = forwardedArgs.toArray(new String[forwardedArgs.size()]);
    return ioLibrary.parseOpts(ioArgs, usage);
  }

  private String buildUsage()
  {
    String baseUsage = ioLibrary.getUsageString(PROGRAM_NAME);
    String userOption = "\n  -u, -user <name>       supply username non-interactively";
    String pwdOption  = "\n  -p, -password <pw>     supply password non-interactively";
    return baseUsage + userOption + pwdOption;
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
    createSchema(connection);
    seedExpiredSession(connection);

    for (ChatTurn turn : SAMPLE_TURNS)
    {
      processTurn(connection, turn);
    }

    printActiveSummary(connection);
    deleteExpiredSessions(connection);
    dropTable(connection, true);
  }

  private void createSchema(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(CREATE_TABLE_SQL))
    {
      statement.execute();
    }
    System.out.println("✓ Table " + TABLE_NAME + " created");

    try (PreparedStatement statement = connection.prepareStatement(CREATE_TENANT_USER_INDEX_SQL))
    {
      statement.execute();
    }
    System.out.println("✓ Index IDX_CHAT_SESSIONS_TENANT_USER created");

    try (PreparedStatement statement = connection.prepareStatement(CREATE_EXPIRES_INDEX_SQL))
    {
      statement.execute();
    }
    System.out.println("✓ Index IDX_CHAT_SESSIONS_EXPIRES created");
  }

  private void seedExpiredSession(Connection connection) throws SQLException
  {
    ChatTurn expiredTurn = new ChatTurn("retail_app", "user_099", "expired-order-status",
                                        "support-summary-v1",
                                        "What happened with the old order update?",
                                        "This is an expired session that will be cleaned up.",
                                        "lookup_order_status", "shipment-feed",
                                        "{\"responseStyle\":\"concise\",\"locale\":\"en-US\"}");
    Timestamp createdAt = new Timestamp(System.currentTimeMillis() - (2L * 60L * 60L * 1000L));
    Timestamp expiresAt = new Timestamp(System.currentTimeMillis() - (60L * 1000L));
    String assistantText = simulateAssistantReply(expiredTurn, null);
    SessionContext context = new SessionContext(buildSessionKey(expiredTurn), expiredTurn, assistantText, createdAt);
    context.appendTurn(expiredTurn, assistantText, createdAt);

    insertSession(connection, context, createdAt, expiresAt);
    System.out.println("✓ Seeded 1 expired chat session");
  }

  private void processTurn(Connection connection, ChatTurn turn) throws SQLException
  {
    String sessionKey = buildSessionKey(turn);
    Timestamp now = currentTimestamp();
    Timestamp expiresAt = addMinutes(now, SESSION_TTL_MINUTES);
    SessionContext context = sessionContexts.get(sessionKey);

    if (context == null)
    {
      String assistantText = simulateAssistantReply(turn, null);
      context = new SessionContext(sessionKey, turn, assistantText, now);
      insertSession(connection, context, now, expiresAt);
      sessionContexts.put(sessionKey, context);
      System.out.println(
          "→ Session start: tenant=" + turn.tenantId
          + " user=" + turn.userId
          + " topic=" + turn.topic
          + " turns=1");
      System.out.println("  Assistant: " + assistantText);
    }
    else
    {
      String assistantText = simulateAssistantReply(turn, context);
      context.appendTurn(turn, assistantText, now);
      updateSession(connection, context, now, expiresAt);
      sessionContexts.put(sessionKey, context);
      System.out.println(
          "→ Session resume: tenant=" + turn.tenantId
          + " user=" + turn.userId
          + " topic=" + turn.topic
          + " turns=" + context.turnCount);
      System.out.println("  Assistant: " + assistantText);
    }
  }

  private void insertSession(Connection connection, SessionContext context,
                             Timestamp createdAt, Timestamp expiresAt) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(INSERT_SESSION_SQL))
    {
      statement.setString(1, context.sessionKey);
      statement.setString(2, context.tenantId);
      statement.setString(3, context.userId);
      statement.setString(4, context.topic);
      statement.setString(5, context.toJson());
      statement.setTimestamp(6, createdAt);
      statement.setTimestamp(7, createdAt);
      statement.setTimestamp(8, expiresAt);
      statement.executeUpdate();
    }
  }

  private void updateSession(Connection connection, SessionContext context,
                             Timestamp updatedAt, Timestamp expiresAt) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(UPDATE_SESSION_SQL))
    {
      statement.setString(1, context.toJson());
      statement.setTimestamp(2, updatedAt);
      statement.setTimestamp(3, expiresAt);
      statement.setString(4, context.sessionKey);
      statement.executeUpdate();
    }
  }

  private void printActiveSummary(Connection connection) throws SQLException
  {
    System.out.println("⋯ Active sessions by tenant/user/topic:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_SUMMARY_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      try (ResultSet resultSet = statement.executeQuery())
      {
        while (resultSet.next())
        {
          String tenantId = resultSet.getString(1);
          String userId = resultSet.getString(2);
          String topic = resultSet.getString(3);
          int turnCount = resultSet.getInt(4);
          String modelName = resultSet.getString(5);
          String safetyStatus = resultSet.getString(6);
          System.out.println(
              "  tenant=" + padRight(tenantId, 14)
              + " user=" + padRight(userId, 10)
              + " topic=" + padRight(topic, 24)
              + " turns=" + turnCount
              + " model=" + padRight(modelName, 22)
              + " safety=" + safetyStatus);
        }
      }
    }

    System.out.println("⋯ Latest session memory snapshots:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_STATE_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      try (ResultSet resultSet = statement.executeQuery())
      {
        while (resultSet.next())
        {
          String sessionKey = resultSet.getString(1);
          String tenantId = resultSet.getString(2);
          String topic = resultSet.getString(4);
          String sessionJson = resultSet.getString(5);
          Timestamp lastUpdatedAt = resultSet.getTimestamp(6);
          Timestamp expiresAt = resultSet.getTimestamp(7);
          System.out.println(
              "  session=" + sessionKey.substring(0, 12)
              + "... tenant=" + tenantId
              + " topic=" + topic
              + " updated=" + lastUpdatedAt
              + " expires=" + expiresAt);
          System.out.println("    session_state_json=" + shorten(sessionJson, 110));
        }
      }
    }
  }

  private void deleteExpiredSessions(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(DELETE_EXPIRED_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      int deleted = statement.executeUpdate();
      System.out.println("✓ Deleted " + deleted + " expired chat session");
    }
  }

  private void dropTable(Connection connection, boolean reportMissing)
  {
    if (connection == null)
    {
      return;
    }

    try (PreparedStatement statement = connection.prepareStatement(DROP_TABLE_SQL))
    {
      statement.execute();
      System.out.println("Table " + TABLE_NAME + " dropped");
    }
    catch (SQLException e)
    {
      if (reportMissing)
      {
        System.out.println("⚠ Table " + TABLE_NAME + " not dropped: " + e.getMessage());
      }
    }
  }

  private Timestamp currentTimestamp()
  {
    return new Timestamp(System.currentTimeMillis());
  }

  private Timestamp addMinutes(Timestamp timestamp, int minutes)
  {
    return new Timestamp(timestamp.getTime() + (minutes * 60L * 1000L));
  }

  private String simulateAssistantReply(ChatTurn turn, SessionContext existingContext)
  {
    String memoryClause = "";
    if (existingContext != null)
    {
      String lastUserMessage = existingContext.getLastUserMessage();
      if (lastUserMessage != null && !lastUserMessage.isEmpty())
      {
        memoryClause = " I remember your earlier message: " + shorten(lastUserMessage, 55) + ".";
      }
    }

    return (
        "Simulated reply for " + turn.tenantId + " using " + turn.modelName + ":"
        + memoryClause + " " + turn.responseHint).trim();
  }

  private String buildSessionKey(ChatTurn turn)
  {
    String keyText = turn.tenantId + "|" + turn.userId + "|" + turn.topic;
    return sha256Hex(keyText);
  }

  private String padRight(String value, int width)
  {
    if (value == null)
    {
      value = "";
    }

    if (value.length() >= width)
    {
      return value;
    }

    StringBuilder builder = new StringBuilder(value);
    while (builder.length() < width)
    {
      builder.append(' ');
    }
    return builder.toString();
  }

  private String shorten(String text, int limit)
  {
    if (text == null)
    {
      return "";
    }
    if (text.length() <= limit)
    {
      return text;
    }
    return text.substring(0, limit - 3) + "...";
  }

  private String sha256Hex(String text)
  {
    try
    {
      java.security.MessageDigest digest = java.security.MessageDigest.getInstance("SHA-256");
      byte[] hash = digest.digest(text.getBytes(java.nio.charset.StandardCharsets.UTF_8));
      StringBuilder builder = new StringBuilder();
      for (byte value : hash)
      {
        builder.append(String.format("%02x", value & 0xff));
      }
      return builder.toString();
    }
    catch (java.security.NoSuchAlgorithmException e)
    {
      throw new IllegalStateException("SHA-256 not available", e);
    }
  }

  private static String escapeJson(String text)
  {
    if (text == null)
    {
      return "";
    }

    return text
        .replace("\\", "\\\\")
        .replace("\"", "\\\"")
        .replace("\b", "\\b")
        .replace("\f", "\\f")
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t");
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
