/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates how an application can use TimesTen as a fast online feature
 *   store for real-time personalization support. The application keeps the
 *   latest feature values close to the service that needs them, so it can
 *   fetch the current state with very low latency, refresh stale data, and
 *   store a JSON audit trail for downstream analysis.
 *
 *   NOTE: The sample uses simulated feature updates and does not call an AI
 *   model, perform vector search, or run in-database model inference.
 */

package jdbc.demo;

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
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
 * Provides a JDBC-based demonstration of an application-managed online
 * feature store backed by TimesTen.
 */
public class FeatureStore
{
  private static final String PROGRAM_NAME = "FeatureStore";
  private static final String DEFAULT_USERNAME = "appuser";

  private static final String TIMESTEN_DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";
  private static final String TIMESTEN_CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";

  private static final String DIRECT_URL_PREFIX = "jdbc:timesten:direct:";
  private static final String CLIENT_URL_PREFIX = "jdbc:timesten:client:";

  private static final String TABLE_NAME = "user_features";
  private static final int FEATURE_TTL_MINUTES = 60;

  private static final String CREATE_TABLE_SQL =
    "CREATE TABLE " + TABLE_NAME + " ("
    + "feature_key    VARCHAR2(64)   NOT NULL PRIMARY KEY, "
    + "tenant_id      VARCHAR2(30)   NOT NULL, "
    + "user_id        VARCHAR2(30)   NOT NULL, "
    + "feature_name   VARCHAR2(60)   NOT NULL, "
    + "feature_value  JSON           NOT NULL, "
    + "freshness_ts   TIMESTAMP      NOT NULL, "
    + "model_version  VARCHAR2(40)   NOT NULL, "
    + "audit_payload  JSON, "
    + "created_at     TIMESTAMP      NOT NULL, "
    + "updated_at     TIMESTAMP      NOT NULL, "
    + "expires_at     TIMESTAMP      NOT NULL)";

  private static final String CREATE_TENANT_USER_INDEX_SQL =
    "CREATE INDEX idx_user_features_tenant_user "
    + "ON " + TABLE_NAME + " (tenant_id, user_id)";

  private static final String CREATE_FRESHNESS_INDEX_SQL =
    "CREATE INDEX idx_user_features_freshness "
    + "ON " + TABLE_NAME + " (expires_at)";

  private static final String INSERT_FEATURE_SQL =
    "INSERT INTO " + TABLE_NAME + " ("
    + "feature_key, tenant_id, user_id, feature_name, feature_value, freshness_ts, "
    + "model_version, audit_payload, created_at, updated_at, expires_at) "
    + "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  private static final String DELETE_FEATURE_SQL =
    "DELETE FROM " + TABLE_NAME + " WHERE feature_key = ?";

  private static final String SELECT_ACTIVE_FEATURES_SQL =
    "SELECT feature_name, "
    + "       JSON_SERIALIZE(feature_value RETURNING VARCHAR2(4000)), "
    + "       TO_CHAR(freshness_ts, 'YYYY-MM-DD HH24:MI:SS'), "
    + "       model_version, "
    + "       JSON_SERIALIZE(audit_payload RETURNING VARCHAR2(4000)) "
    + "FROM " + TABLE_NAME + " "
    + "WHERE tenant_id = ? "
    + "  AND user_id = ? "
    + "  AND expires_at > ? "
    + "ORDER BY feature_name";

  private static final String SELECT_FEATURE_SUMMARY_SQL =
    "SELECT tenant_id, "
    + "       user_id, "
    + "       COUNT(*), "
    + "       SUM(CASE "
    + "             WHEN JSON_VALUE(feature_value, '$.valueType' RETURNING VARCHAR2(20)) = 'numeric' "
    + "             THEN JSON_VALUE(feature_value, '$.value' RETURNING TT_INT) "
    + "             ELSE 0 "
    + "           END) "
    + "FROM " + TABLE_NAME + " "
    + "WHERE expires_at > ? "
    + "GROUP BY tenant_id, user_id "
    + "ORDER BY tenant_id, user_id";

  private static final String DELETE_EXPIRED_SQL =
    "DELETE FROM " + TABLE_NAME + " WHERE expires_at <= ?";

  private static final String DROP_TABLE_SQL = "DROP TABLE " + TABLE_NAME;

  private static final FeatureUpdate[] FEATURE_UPDATES = new FeatureUpdate[] {
    new FeatureUpdate("retail_app", "user_001", "cart_value",
                      "{\"valueType\":\"numeric\",\"value\":128,\"source\":\"checkout-events\",\"freshness\":\"seconds\"}",
                      "feature-agg-v1",
                      "{\"variant\":\"priority-shipping\",\"reason\":\"high_cart_value\",\"confidence\":0.92}"),
    new FeatureUpdate("retail_app", "user_001", "preferred_channel",
                      "{\"valueType\":\"string\",\"value\":\"mobile\",\"source\":\"profile-service\",\"freshness\":\"minutes\"}",
                      "feature-agg-v1",
                      "{\"variant\":\"mobile-first\",\"reason\":\"recent_mobile_usage\",\"confidence\":0.88}"),
    new FeatureUpdate("field_service", "user_204", "device_risk",
                      "{\"valueType\":\"numeric\",\"value\":73,\"source\":\"device-telemetry\",\"freshness\":\"seconds\"}",
                      "feature-agg-v2",
                      "{\"variant\":\"proactive-support\",\"reason\":\"elevated_risk_score\",\"confidence\":0.81}")
  };

  private final IOLibrary ioLibrary;

  private static final class FeatureUpdate
  {
    private final String tenantId;
    private final String userId;
    private final String featureName;
    private final String featureValueJson;
    private final String modelVersion;
    private final String decisionJson;

    FeatureUpdate(String tenantId, String userId, String featureName,
                  String featureValueJson, String modelVersion, String decisionJson)
    {
      this.tenantId = tenantId;
      this.userId = userId;
      this.featureName = featureName;
      this.featureValueJson = featureValueJson;
      this.modelVersion = modelVersion;
      this.decisionJson = decisionJson;
    }
  }

  private static final class FeatureRecord
  {
    private final String featureKey;
    private final String tenantId;
    private final String userId;
    private final String featureName;
    private final String featureValueJson;
    private final String modelVersion;
    private final String auditPayloadJson;

    FeatureRecord(String featureKey, String tenantId, String userId, String featureName,
                  String featureValueJson, String modelVersion, String auditPayloadJson)
    {
      this.featureKey = featureKey;
      this.tenantId = tenantId;
      this.userId = userId;
      this.featureName = featureName;
      this.featureValueJson = featureValueJson;
      this.modelVersion = modelVersion;
      this.auditPayloadJson = auditPayloadJson;
    }
  }

  public FeatureStore()
  {
    ioLibrary = new IOLibrary(System.err);
  }

  public static void main(String[] args)
  {
    int exitCode = new FeatureStore().run(args);
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
      System.out.println("Completed feature store sample operations");
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
    createSchema(connection);
    seedStaleFeature(connection);

    for (FeatureUpdate feature : FEATURE_UPDATES)
    {
      upsertFeature(connection, feature);
    }

    printFeatureSummary(connection);
    printFeatureSet(connection, "retail_app", "user_001");
    printFeatureSet(connection, "field_service", "user_204");
    deleteExpiredFeatures(connection);
    dropTable(connection, true);
  }

  private void createSchema(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(CREATE_TABLE_SQL))
    {
      statement.execute();
    }
    System.out.println("Table " + TABLE_NAME + " created");

    try (PreparedStatement statement = connection.prepareStatement(CREATE_TENANT_USER_INDEX_SQL))
    {
      statement.execute();
    }
    System.out.println("Index IDX_USER_FEATURES_TENANT_USER created");

    try (PreparedStatement statement = connection.prepareStatement(CREATE_FRESHNESS_INDEX_SQL))
    {
      statement.execute();
    }
    System.out.println("Index IDX_USER_FEATURES_FRESHNESS created");
  }

  private void seedStaleFeature(Connection connection) throws SQLException
  {
    FeatureUpdate staleFeature = new FeatureUpdate("retail_app", "user_999", "cart_value",
        "{\"valueType\":\"numeric\",\"value\":12,\"source\":\"old-events\",\"freshness\":\"minutes\"}",
        "feature-agg-v0",
        "{\"variant\":\"standard-shipping\",\"reason\":\"legacy_state\",\"confidence\":0.5}");

    Timestamp freshnessTs = new Timestamp(System.currentTimeMillis() - (3L * 60L * 60L * 1000L));
    Timestamp expiresAt = new Timestamp(System.currentTimeMillis() - (60L * 1000L));
    FeatureRecord record = buildFeatureRecord(staleFeature, freshnessTs, expiresAt, "stale_seed");
    insertFeature(connection, record, freshnessTs, expiresAt);
    System.out.println("Seeded 1 stale feature row");
  }

  private void upsertFeature(Connection connection, FeatureUpdate feature) throws SQLException
  {
    Timestamp freshnessTs = currentTimestamp();
    Timestamp expiresAt = addMinutes(freshnessTs, FEATURE_TTL_MINUTES);
    FeatureRecord record = buildFeatureRecord(feature, freshnessTs, expiresAt, "fresh_feature_upsert");
    insertOrReplaceFeature(connection, record, freshnessTs, expiresAt);
    System.out.println(
        "FEATURE UPSERT tenant=" + feature.tenantId
        + " user=" + feature.userId
        + " feature=" + feature.featureName
        + " model=" + feature.modelVersion);
  }

  private FeatureRecord buildFeatureRecord(FeatureUpdate feature, Timestamp freshnessTs,
                                           Timestamp expiresAt, String reason)
  {
    String featureKey = buildFeatureKey(feature);
    String auditPayload = buildAuditPayload(feature, reason);
    return new FeatureRecord(featureKey, feature.tenantId, feature.userId,
                             feature.featureName, feature.featureValueJson,
                             feature.modelVersion, auditPayload);
  }

  private void insertOrReplaceFeature(Connection connection, FeatureRecord record,
                                      Timestamp freshnessTs, Timestamp expiresAt) throws SQLException
  {
    try (PreparedStatement delete = connection.prepareStatement(DELETE_FEATURE_SQL))
    {
      delete.setString(1, record.featureKey);
      delete.executeUpdate();
    }
    insertFeature(connection, record, freshnessTs, expiresAt);
  }

  private void insertFeature(Connection connection, FeatureRecord record,
                             Timestamp freshnessTs, Timestamp expiresAt) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(INSERT_FEATURE_SQL))
    {
      statement.setString(1, record.featureKey);
      statement.setString(2, record.tenantId);
      statement.setString(3, record.userId);
      statement.setString(4, record.featureName);
      statement.setString(5, record.featureValueJson);
      statement.setTimestamp(6, freshnessTs);
      statement.setString(7, record.modelVersion);
      statement.setString(8, record.auditPayloadJson);
      statement.setTimestamp(9, freshnessTs);
      statement.setTimestamp(10, freshnessTs);
      statement.setTimestamp(11, expiresAt);
      statement.executeUpdate();
    }
  }

  private String buildFeatureKey(FeatureUpdate feature)
  {
    return sha256Hex(feature.tenantId + "|" + feature.userId + "|" + feature.featureName);
  }

  private String buildAuditPayload(FeatureUpdate feature, String reason)
  {
    StringBuilder builder = new StringBuilder();
    builder.append('{');
    builder.append("\"tenantId\":\"").append(escapeJson(feature.tenantId)).append("\",");
    builder.append("\"userId\":\"").append(escapeJson(feature.userId)).append("\",");
    builder.append("\"featureName\":\"").append(escapeJson(feature.featureName)).append("\",");
    builder.append("\"modelVersion\":\"").append(escapeJson(feature.modelVersion)).append("\",");
    builder.append("\"decision\":").append(feature.decisionJson).append(',');
    builder.append("\"reason\":\"").append(escapeJson(reason)).append("\",");
    builder.append("\"updatedAt\":\"").append(currentTimestampText()).append("\",");
    builder.append("\"simulatedModelCall\":true");
    builder.append('}');
    return builder.toString();
  }

  private void printFeatureSummary(Connection connection) throws SQLException
  {
    System.out.println("Active feature groups:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_FEATURE_SUMMARY_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      try (ResultSet resultSet = statement.executeQuery())
      {
        while (resultSet.next())
        {
          String tenantId = resultSet.getString(1);
          String userId = resultSet.getString(2);
          int featureCount = resultSet.getInt(3);
          int numericSum = resultSet.getInt(4);
          System.out.println(
              "  tenant=" + padRight(tenantId, 14)
              + " user=" + padRight(userId, 10)
              + " features=" + featureCount
              + " numeric_sum=" + numericSum);
        }
      }
    }
  }

  private void printFeatureSet(Connection connection, String tenantId, String userId) throws SQLException
  {
    System.out.println("Current features for tenant=" + tenantId + " user=" + userId + ":");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_FEATURES_SQL))
    {
      statement.setString(1, tenantId);
      statement.setString(2, userId);
      statement.setTimestamp(3, currentTimestamp());
      try (ResultSet resultSet = statement.executeQuery())
      {
        while (resultSet.next())
        {
          String featureName = resultSet.getString(1);
          String featureValueJson = resultSet.getString(2);
          String freshnessTs = resultSet.getString(3);
          String modelVersion = resultSet.getString(4);
          String auditJson = resultSet.getString(5);
          System.out.println("  feature=" + featureName + " freshness=" + freshnessTs + " model=" + modelVersion);
          System.out.println("    value=" + featureValueJson);
          System.out.println("    audit=" + auditJson);
        }
      }
    }
  }

  private void deleteExpiredFeatures(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(DELETE_EXPIRED_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      int deleted = statement.executeUpdate();
      System.out.println("Deleted " + deleted + " expired feature row");
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
        System.out.println("Table " + TABLE_NAME + " not dropped: " + e.getMessage());
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

  private String currentTimestampText()
  {
    return currentTimestamp().toString();
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

  private String sha256Hex(String text)
  {
    try
    {
      MessageDigest digest = MessageDigest.getInstance("SHA-256");
      byte[] hash = digest.digest(text.getBytes(StandardCharsets.UTF_8));
      StringBuilder builder = new StringBuilder();
      for (byte value : hash)
      {
        builder.append(String.format("%02x", value & 0xff));
      }
      return builder.toString();
    }
    catch (NoSuchAlgorithmException e)
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
