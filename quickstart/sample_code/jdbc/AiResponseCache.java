/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates how an application can use TimesTen as the authoritative
 *   store for active AI response cache state. The application computes a
 *   deterministic cache key for each AI request, checks TimesTen for a fresh
 *   cached response, simulates a model call on cache miss, and stores the
 *   response and operational metadata with an expiration time.
 *
 *   NOTE: Responses are simulated; this sample does not call an AI model,
 *   perform vector search, run in-database model inference, or demonstrate
 *   TimesTen Cache for Oracle Database.
 */

package jdbc.demo;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Timestamp;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;

/**
 * Provides a JDBC-based demonstration of an application-managed AI response
 * cache backed by TimesTen.
 */
public class AiResponseCache
{
  private static final String PROGRAM_NAME = "AiResponseCache";
  private static final String DEFAULT_USERNAME = "appuser";
  private static final String TIMESTEN_DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";
  private static final String TIMESTEN_CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";
  private static final String DIRECT_URL_PREFIX = "jdbc:timesten:direct:";
  private static final String CLIENT_URL_PREFIX = "jdbc:timesten:client:";
  private static final String TABLE_NAME = "ai_response_cache";
  private static final int CACHE_TTL_MINUTES = 30;

  private static final String CREATE_TABLE_SQL =
    "CREATE TABLE " + TABLE_NAME + " ("
    + "cache_key        VARCHAR2(64)   NOT NULL PRIMARY KEY, "
    + "tenant_id        VARCHAR2(30)   NOT NULL, "
    + "user_id          VARCHAR2(30)   NOT NULL, "
    + "model_name       VARCHAR2(60)   NOT NULL, "
    + "prompt_hash      VARCHAR2(64)   NOT NULL, "
    + "prompt_summary   VARCHAR2(500)  NOT NULL, "
    + "response_text    VARCHAR2(4000) NOT NULL, "
    + "metadata         JSON, "
    + "hit_count        TT_INT         NOT NULL, "
    + "created_at       TIMESTAMP      NOT NULL, "
    + "last_accessed_at TIMESTAMP      NOT NULL, "
    + "expires_at       TIMESTAMP      NOT NULL)";

  private static final String CREATE_TENANT_MODEL_INDEX_SQL =
    "CREATE INDEX idx_ai_cache_tenant_model "
    + "ON " + TABLE_NAME + " (tenant_id, model_name)";

  private static final String CREATE_EXPIRES_INDEX_SQL =
    "CREATE INDEX idx_ai_cache_expires "
    + "ON " + TABLE_NAME + " (expires_at)";

  private static final String INSERT_CACHE_ENTRY_SQL =
    "INSERT INTO " + TABLE_NAME + " ("
    + "cache_key, tenant_id, user_id, model_name, prompt_hash, prompt_summary, "
    + "response_text, metadata, hit_count, created_at, last_accessed_at, expires_at) "
    + "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  private static final String SELECT_FRESH_ENTRY_SQL =
    "SELECT response_text, "
    + "       JSON_SERIALIZE(metadata RETURNING VARCHAR2(4000)), "
    + "       JSON_VALUE(metadata, '$.safetyLabel' RETURNING VARCHAR2(30)), "
    + "       hit_count, expires_at "
    + "FROM " + TABLE_NAME + " "
    + "WHERE cache_key = ? AND expires_at > ?";

  private static final String UPDATE_CACHE_HIT_SQL =
    "UPDATE " + TABLE_NAME + " "
    + "SET hit_count = hit_count + 1, last_accessed_at = ? "
    + "WHERE cache_key = ?";

  private static final String DELETE_EXPIRED_SQL =
    "DELETE FROM " + TABLE_NAME + " WHERE expires_at <= ?";

  private static final String DROP_TABLE_SQL = "DROP TABLE " + TABLE_NAME;

  private static final AiRequest[] SAMPLE_REQUESTS = new AiRequest[] {
    new AiRequest("retail_app", "user_001", "support-summary-v1",
                  "Summarize order 45012 for a support agent.", 0.2),
    new AiRequest("retail_app", "user_001", "support-summary-v1",
                  "Summarize order 45012 for a support agent.", 0.2),
    new AiRequest("field_service", "user_204", "technician-assist-v1",
                  "Draft troubleshooting steps for a router with intermittent packet loss.", 0.1)
  };

  private IOLibrary ioLibrary;
  private String userOverride;
  private String passwordOverride;

  private static final class AiRequest
  {
    private final String tenantId;
    private final String userId;
    private final String modelName;
    private final String prompt;
    private final double temperature;

    AiRequest(String tenantId, String userId, String modelName, String prompt, double temperature)
    {
      this.tenantId = tenantId;
      this.userId = userId;
      this.modelName = modelName;
      this.prompt = prompt;
      this.temperature = temperature;
    }
  }

  private static final class SimulatedResponse
  {
    private final String responseText;
    private final String metadataJson;

    SimulatedResponse(String responseText, String metadataJson)
    {
      this.responseText = responseText;
      this.metadataJson = metadataJson;
    }
  }

  private static final class CacheEntry
  {
    private final String responseText;
    private final String metadataJson;
    private final String safetyLabel;
    private final int hitCount;
    private final Timestamp expiresAt;

    CacheEntry(String responseText, String metadataJson, String safetyLabel,
               int hitCount, Timestamp expiresAt)
    {
      this.responseText = responseText;
      this.metadataJson = metadataJson;
      this.safetyLabel = safetyLabel;
      this.hitCount = hitCount;
      this.expiresAt = expiresAt;
    }
  }

  public static void main(String[] args)
  {
    int exitCode = new AiResponseCache().run(args);
    if (exitCode != 0)
    {
      System.exit(exitCode);
    }
  }

  private int run(String[] args)
  {
    ioLibrary = new IOLibrary(System.err);
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

    System.out.println();
    System.out.println("Connecting using URL: " + url);

    try (Connection connection = DriverManager.getConnection(url, username, password))
    {
      connection.setAutoCommit(true);
      System.out.println("Connected");
      runDemo(connection);
      System.out.println("Completed AI response cache sample operations");
    }
    catch (SQLException e)
    {
      printSQLException(e);
      return 1;
    }

    return 0;
  }

  private boolean parseOptions(String[] args, String usage)
  {
    List<String> forwardedArgs = new ArrayList<String>();

    for (int i = 0; i < args.length; i++)
    {
      String arg = args[i];

      if ("-user".equals(arg) || "-username".equals(arg))
      {
        if (i + 1 >= args.length)
        {
          System.err.println("Missing value for " + arg);
          System.err.println(usage);
          return false;
        }
        userOverride = args[++i];
      }
      else if ("-password".equals(arg) || "-pwd".equals(arg))
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
    String userOption = "\n  -user <name>  supply username non-interactively";
    String pwdOption  = "\n  -password <pw> supply password non-interactively";
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
    seedExpiredEntry(connection);

    for (AiRequest request : SAMPLE_REQUESTS)
    {
      processRequest(connection, request);
    }

    printCacheSummary(connection);
    deleteExpiredEntries(connection);
    dropTable(connection, true);
  }

  private void createSchema(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(CREATE_TABLE_SQL))
    {
      statement.execute();
      System.out.println("Table " + TABLE_NAME + " created");
    }

    try (PreparedStatement statement = connection.prepareStatement(CREATE_TENANT_MODEL_INDEX_SQL))
    {
      statement.execute();
      System.out.println("Index IDX_AI_CACHE_TENANT_MODEL created");
    }

    try (PreparedStatement statement = connection.prepareStatement(CREATE_EXPIRES_INDEX_SQL))
    {
      statement.execute();
      System.out.println("Index IDX_AI_CACHE_EXPIRES created");
    }
  }

  private void dropTable(Connection connection, boolean reportMissing)
  {
    try (PreparedStatement statement = connection.prepareStatement(DROP_TABLE_SQL))
    {
      statement.execute();
      System.out.println("Table " + TABLE_NAME + " dropped");
    }
    catch (SQLException e)
    {
      if (reportMissing)
      {
        System.out.println("Table " + TABLE_NAME + " not dropped: " + e.getMessage());
      }
    }
  }

  private void seedExpiredEntry(Connection connection) throws SQLException
  {
    AiRequest request = new AiRequest("retail_app", "user_099", "support-summary-v1",
                                      "Summarize an expired support request.", 0.2);
    Timestamp createdAt = timestampMinutesAgo(120);
    Timestamp expiresAt = timestampMinutesAgo(1);
    SimulatedResponse response = simulateModelResponse(request);

    insertCacheEntry(connection, request, response.responseText, response.metadataJson, createdAt, expiresAt);
    System.out.println("Seeded 1 expired cache entry");
  }

  private void processRequest(Connection connection, AiRequest request) throws SQLException
  {
    String cacheKey = buildCacheKey(request);
    CacheEntry cachedEntry = findCachedResponse(connection, cacheKey);

    if (cachedEntry != null)
    {
      try (PreparedStatement update = connection.prepareStatement(UPDATE_CACHE_HIT_SQL))
      {
        update.setTimestamp(1, currentTimestamp());
        update.setString(2, cacheKey);
        update.executeUpdate();
      }

      System.out.println("CACHE HIT  tenant=" + request.tenantId +
                         " model=" + request.modelName +
                         " hits=" + (cachedEntry.hitCount + 1) +
                         " expires=" + cachedEntry.expiresAt);
      System.out.println("  Response: " + cachedEntry.responseText);
      System.out.println("  Safety label from metadata: " + cachedEntry.safetyLabel);
      return;
    }

    SimulatedResponse response = simulateModelResponse(request);
    Timestamp createdAt = currentTimestamp();
    Timestamp expiresAt = new Timestamp(createdAt.getTime() + (CACHE_TTL_MINUTES * 60L * 1000L));

    insertCacheEntry(connection, request, response.responseText, response.metadataJson, createdAt, expiresAt);
    System.out.println("CACHE MISS tenant=" + request.tenantId +
                       " model=" + request.modelName +
                       " stored_for_minutes=" + CACHE_TTL_MINUTES);
    System.out.println("  Response: " + response.responseText);
  }

  private void insertCacheEntry(Connection connection, AiRequest request, String responseText,
                                String metadataJson, Timestamp createdAt, Timestamp expiresAt)
                                throws SQLException
  {
    String cacheKey = buildCacheKey(request);

    try (PreparedStatement statement = connection.prepareStatement(INSERT_CACHE_ENTRY_SQL))
    {
      statement.setString(1, cacheKey);
      statement.setString(2, request.tenantId);
      statement.setString(3, request.userId);
      statement.setString(4, request.modelName);
      statement.setString(5, buildPromptHash(request.prompt));
      statement.setString(6, summarizePrompt(request.prompt));
      statement.setString(7, responseText);
      statement.setString(8, metadataJson);
      statement.setInt(9, 0);
      statement.setTimestamp(10, createdAt);
      statement.setTimestamp(11, createdAt);
      statement.setTimestamp(12, expiresAt);
      statement.executeUpdate();
    }
  }

  private CacheEntry findCachedResponse(Connection connection, String cacheKey) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(SELECT_FRESH_ENTRY_SQL))
    {
      statement.setString(1, cacheKey);
      statement.setTimestamp(2, currentTimestamp());

      try (ResultSet resultSet = statement.executeQuery())
      {
        if (resultSet.next())
        {
          return new CacheEntry(resultSet.getString(1),
                                resultSet.getString(2),
                                resultSet.getString(3),
                                resultSet.getInt(4),
                                resultSet.getTimestamp(5));
        }
      }
    }

    return null;
  }

  private void printCacheSummary(Connection connection) throws SQLException
  {
    System.out.println("Cache summary by tenant and model:");
    String summarySql = "SELECT tenant_id, model_name, COUNT(*), SUM(hit_count) "
                      + "FROM " + TABLE_NAME + " "
                      + "GROUP BY tenant_id, model_name "
                      + "ORDER BY tenant_id, model_name";

    try (PreparedStatement statement = connection.prepareStatement(summarySql);
         ResultSet resultSet = statement.executeQuery())
    {
      while (resultSet.next())
      {
        String tenantId = resultSet.getString(1);
        String modelName = resultSet.getString(2);
        int entryCount = resultSet.getInt(3);
        int hitCount = resultSet.getInt(4);

        System.out.println("  tenant=" + padRight(tenantId, 14)
                           + " model=" + padRight(modelName, 22)
                           + " entries=" + entryCount
                           + " hits=" + hitCount);
      }
    }

    System.out.println("Metadata safety labels:");
    String metadataSql = "SELECT cache_key, "
                       + "JSON_VALUE(metadata, '$.safetyLabel' RETURNING VARCHAR2(30)) "
                       + "FROM " + TABLE_NAME + " "
                       + "ORDER BY created_at";

    try (PreparedStatement statement = connection.prepareStatement(metadataSql);
         ResultSet resultSet = statement.executeQuery())
    {
      while (resultSet.next())
      {
        String cacheKey = resultSet.getString(1);
        String safetyLabel = resultSet.getString(2);
        System.out.println("  cache_key=" + cacheKey.substring(0, 12) + "... safetyLabel=" + safetyLabel);
      }
    }
  }

  private void deleteExpiredEntries(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(DELETE_EXPIRED_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      int deleted = statement.executeUpdate();
      System.out.println("Deleted " + deleted + " expired cache entry");
    }
  }

  private SimulatedResponse simulateModelResponse(AiRequest request)
  {
    int promptTokens = Math.max(8, wordCount(request.prompt) + 4);
    String responseText = "Simulated response for " + request.tenantId
                        + " using " + request.modelName + ": "
                        + summarizePrompt(request.prompt);
    int responseTokens = Math.max(12, wordCount(responseText) + 6);
    int latencyMs = 40 + (request.prompt.length() % 35);
    StringBuilder metadataJson = new StringBuilder();
    metadataJson.append('{');
    metadataJson.append("\"temperature\":").append(request.temperature).append(',');
    metadataJson.append("\"promptTokens\":").append(promptTokens).append(',');
    metadataJson.append("\"responseTokens\":").append(responseTokens).append(',');
    metadataJson.append("\"latencyMs\":").append(latencyMs).append(',');
    metadataJson.append("\"safetyLabel\":\"allowed\",");
    metadataJson.append("\"simulatedModelCall\":true");
    metadataJson.append('}');

    return new SimulatedResponse(responseText, metadataJson.toString());
  }

  private String buildCacheKey(AiRequest request)
  {
    String keyText = request.tenantId + "|" + request.modelName + "|" + request.prompt + "|" + request.temperature;
    return sha256Hex(keyText);
  }

  private String buildPromptHash(String prompt)
  {
    return sha256Hex(prompt);
  }

  private String summarizePrompt(String prompt)
  {
    if (prompt.length() <= 80)
    {
      return prompt;
    }

    return prompt.substring(0, 77) + "...";
  }

  private int wordCount(String text)
  {
    String trimmed = text.trim();
    if (trimmed.isEmpty())
    {
      return 0;
    }

    return trimmed.split("\\s+").length;
  }

  private Timestamp currentTimestamp()
  {
    return new Timestamp(System.currentTimeMillis());
  }

  private Timestamp timestampMinutesAgo(int minutes)
  {
    return new Timestamp(System.currentTimeMillis() - (minutes * 60L * 1000L));
  }

  private String sha256Hex(String text)
  {
    try
    {
      MessageDigest digest = MessageDigest.getInstance("SHA-256");
      byte[] hash = digest.digest(text.getBytes(StandardCharsets.UTF_8));
      return bytesToHex(hash);
    }
    catch (NoSuchAlgorithmException e)
    {
      throw new IllegalStateException("SHA-256 is not available", e);
    }
  }

  private String bytesToHex(byte[] bytes)
  {
    char[] hexArray = "0123456789abcdef".toCharArray();
    char[] hexChars = new char[bytes.length * 2];

    for (int j = 0; j < bytes.length; j++)
    {
      int value = bytes[j] & 0xFF;
      hexChars[j * 2] = hexArray[value >>> 4];
      hexChars[j * 2 + 1] = hexArray[value & 0x0F];
    }

    return new String(hexChars);
  }

  private String padRight(String value, int width)
  {
    StringBuilder builder = new StringBuilder(value);
    while (builder.length() < width)
    {
      builder.append(' ');
    }
    return builder.toString();
  }

  private void printSQLException(SQLException exception)
  {
    SQLException current = exception;
    while (current != null)
    {
      System.err.println("SQLException: " + current.getMessage());
      System.err.println("SQLState: " + current.getSQLState());
      System.err.println("ErrorCode: " + current.getErrorCode());
      current = current.getNextException();
      if (current != null)
      {
        System.err.println("Next exception:");
      }
    }
  }
}
