/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates how an application can use TimesTen as a fast store for
 *   real-time telecom call routing state. The application keeps hot routing
 *   decisions close to the service that needs them, applies deterministic
 *   routing rules, stores the resulting decision and metadata in JSON, and
 *   cleans up expired state.
 *
 *   The sample uses simulated routing rules. It does not call a telecom
 *   switch, perform network signaling, or depend on an external service.
 *
 *   The sample performs the following steps:
 *     - Creates a 'call_routing_state' table
 *     - Creates indexes for tenant/subscriber, call id, and expiration lookups
 *     - Seeds one expired routing record
 *     - Processes call routing requests
 *     - Shows a repeated request being replayed from the existing state
 *     - Rereads the stored decision if concurrent requests insert the same key
 *     - Stores request and decision metadata in JSON
 *     - Summarizes active routing decisions by tenant/subscriber/state
 *     - Deletes expired routing records
 *     - Drops the table
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
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.List;

/**
 * Provides a JDBC-based demonstration of a real-time telecom call routing
 * state store backed by TimesTen.
 */
public class TelecomCallRoutingState
{
  private static final String PROGRAM_NAME = "TelecomCallRoutingState";
  private static final String DEFAULT_USERNAME = "appuser";

  private static final String TIMESTEN_DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";
  private static final String TIMESTEN_CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";

  private static final String DIRECT_URL_PREFIX = "jdbc:timesten:direct:";
  private static final String CLIENT_URL_PREFIX = "jdbc:timesten:client:";

  private static final String TABLE_NAME = "call_routing_state";

  private static final String CREATE_TABLE_SQL =
    "CREATE TABLE " + TABLE_NAME + " ("
    + "routing_key        VARCHAR2(64)   NOT NULL PRIMARY KEY, "
    + "tenant_id          VARCHAR2(30)   NOT NULL, "
    + "subscriber_id      VARCHAR2(30)   NOT NULL, "
    + "call_id            VARCHAR2(40)   NOT NULL, "
    + "source_region      VARCHAR2(30)   NOT NULL, "
    + "target_region      VARCHAR2(30)   NOT NULL, "
    + "network_slice      VARCHAR2(30)   NOT NULL, "
    + "priority_class     VARCHAR2(20)   NOT NULL, "
    + "route_state        VARCHAR2(20)   NOT NULL, "
    + "route_reason       VARCHAR2(120)  NOT NULL, "
    + "request_payload    JSON, "
    + "decision_payload   JSON, "
    + "created_at         TIMESTAMP      NOT NULL, "
    + "updated_at         TIMESTAMP      NOT NULL, "
    + "expires_at         TIMESTAMP      NOT NULL)";

  private static final String CREATE_TENANT_SUBSCRIBER_INDEX_SQL =
    "CREATE INDEX idx_call_route_tenant_sub "
    + "ON " + TABLE_NAME + " (tenant_id, subscriber_id)";

  private static final String CREATE_CALL_ID_INDEX_SQL =
    "CREATE INDEX idx_call_route_call_id "
    + "ON " + TABLE_NAME + " (call_id)";

  private static final String CREATE_EXPIRES_INDEX_SQL =
    "CREATE INDEX idx_call_route_expires "
    + "ON " + TABLE_NAME + " (expires_at)";

  private static final String INSERT_ROUTING_SQL =
    "INSERT INTO " + TABLE_NAME + " ("
    + "routing_key, tenant_id, subscriber_id, call_id, source_region, "
    + "target_region, network_slice, priority_class, route_state, route_reason, "
    + "request_payload, decision_payload, created_at, updated_at, expires_at) "
    + "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  private static final String SELECT_EXISTING_ROUTING_SQL =
    "SELECT route_state, route_reason, TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3') "
    + "FROM " + TABLE_NAME + " "
    + "WHERE routing_key = ? "
    + "  AND expires_at > ?";

  private static final String DELETE_EXPIRED_ROUTING_FOR_KEY_SQL =
    "DELETE FROM " + TABLE_NAME + " "
    + "WHERE routing_key = ? "
    + "  AND expires_at <= ?";

  private static final String SELECT_ACTIVE_SUMMARY_SQL =
    "SELECT tenant_id, subscriber_id, route_state, COUNT(*) "
    + "FROM " + TABLE_NAME + " "
    + "WHERE expires_at > ? "
    + "GROUP BY tenant_id, subscriber_id, route_state "
    + "ORDER BY tenant_id, subscriber_id, route_state";

  private static final String SELECT_ACTIVE_DETAILS_SQL =
    "SELECT call_id, source_region, target_region, network_slice, priority_class, "
    + "       route_state, route_reason, TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3') "
    + "FROM " + TABLE_NAME + " "
    + "WHERE expires_at > ? "
    + "ORDER BY tenant_id, subscriber_id, call_id";

  private static final String DELETE_EXPIRED_SQL =
    "DELETE FROM " + TABLE_NAME + " WHERE expires_at <= ?";

  private static final String DROP_TABLE_SQL =
    "DROP TABLE " + TABLE_NAME;

  private static final CallRequest[] CALL_REQUESTS = new CallRequest[] {
    new CallRequest("north_mobile", "sub_1001", "call_1001",
                    "us-east", "us-east", "gold", "standard", true,
                    "edge-east-1", 20),
    new CallRequest("north_mobile", "sub_1001", "call_1001",
                    "us-east", "us-east", "gold", "standard", true,
                    "edge-east-1", 20),
    new CallRequest("north_mobile", "sub_2002", "call_2001",
                    "us-east", "eu-west", "silver", "standard", false,
                    "edge-eu-2", 10),
    new CallRequest("field_support", "sub_3003", "call_3001",
                    "us-west", "us-west", "platinum", "emergency", true,
                    "edge-west-9", 15),
    new CallRequest("field_support", "sub_4004", "call_4001",
                    "us-west", "us-west", "gold", "standard", true,
                    "edge-west-5", 12)
  };

  private static final CallRequest EXPIRED_ROUTING =
    new CallRequest("north_mobile", "sub_9999", "call_expired_0001",
                    "us-west", "us-west", "bronze", "standard", true,
                    "edge-west-1", 5);

  private final IOLibrary ioLibrary;
  private String userOverride;
  private String passwordOverride;

  private static final class CallRequest
  {
    private final String tenantId;
    private final String subscriberId;
    private final String callId;
    private final String sourceRegion;
    private final String targetRegion;
    private final String networkSlice;
    private final String priorityClass;
    private final boolean roamingAllowed;
    private final String preferredRoute;
    private final int holdMinutes;

    CallRequest(String tenantId, String subscriberId, String callId,
                String sourceRegion, String targetRegion, String networkSlice,
                String priorityClass, boolean roamingAllowed, String preferredRoute,
                int holdMinutes)
    {
      this.tenantId = tenantId;
      this.subscriberId = subscriberId;
      this.callId = callId;
      this.sourceRegion = sourceRegion;
      this.targetRegion = targetRegion;
      this.networkSlice = networkSlice;
      this.priorityClass = priorityClass;
      this.roamingAllowed = roamingAllowed;
      this.preferredRoute = preferredRoute;
      this.holdMinutes = holdMinutes;
    }
  }

  private static final class RoutingDecision
  {
    private final String state;
    private final String reason;
    private final int ttlMinutes;

    RoutingDecision(String state, String reason, int ttlMinutes)
    {
      this.state = state;
      this.reason = reason;
      this.ttlMinutes = ttlMinutes;
    }
  }

  public TelecomCallRoutingState()
  {
    ioLibrary = new IOLibrary(System.err);
  }

  public static void main(String[] args)
  {
    int exitCode = new TelecomCallRoutingState().run(args);
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

    System.out.println("=== Telecom call routing demo ===");
    System.out.println();
    System.out.println("Connecting to TimesTen");

    try (Connection connection = DriverManager.getConnection(url, username, password))
    {
      connection.setAutoCommit(true);

      System.out.println("✓ Connected");
      boolean demoSucceeded = false;
      try
      {
        runDemo(connection);
        demoSucceeded = true;
      }
      finally
      {
        dropTable(connection, demoSucceeded);
      }
    }
    catch (SQLException e)
    {
      printSQLException(e);
      return 1;
    }

    System.out.println("✓ Completed telecom call routing sample operations");
    return 0;
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
    // Recreate the table so each run starts from a clean routing state.
    dropTable(connection, false);
    createTable(connection);
    // Seed one expired row so TTL cleanup is visible.
    seedExpiredRouting(connection);

    // Each request exercises either replayed or freshly computed routing.
    for (CallRequest callRequest : CALL_REQUESTS)
    {
      routeCall(connection, callRequest);
    }

    // Show the live routing state before stale rows are removed.
    summarizeActiveRouting(connection);
    cleanupExpiredRouting(connection);
  }

  private void createTable(Connection connection) throws SQLException
  {
    executeStatement(connection, CREATE_TABLE_SQL);
    System.out.println("✓ Table " + TABLE_NAME + " created");
    executeStatement(connection, CREATE_TENANT_SUBSCRIBER_INDEX_SQL);
    System.out.println("✓ Index IDX_CALL_ROUTE_TENANT_SUB created");
    executeStatement(connection, CREATE_CALL_ID_INDEX_SQL);
    System.out.println("✓ Index IDX_CALL_ROUTE_CALL_ID created");
    executeStatement(connection, CREATE_EXPIRES_INDEX_SQL);
    System.out.println("✓ Index IDX_CALL_ROUTE_EXPIRES created");
  }

  private void seedExpiredRouting(Connection connection) throws SQLException
  {
    CallRequest request = EXPIRED_ROUTING;
    Timestamp now = currentTimestamp();
    Timestamp expiredAt = new Timestamp(now.getTime() - 60_000L);
    String routingKey = buildRoutingKey(request);
    String requestPayload = requestPayloadToJson(request);
    String decisionPayload = decisionPayloadToJson(request, "EXPIRED", "seeded_expired_state",
                                                   formatTimestamp(expiredAt));

    try (PreparedStatement statement = connection.prepareStatement(INSERT_ROUTING_SQL))
    {
      bindRouting(statement, routingKey, request, "EXPIRED", "seeded_expired_state",
                  requestPayload, decisionPayload, now, now, expiredAt);
      statement.executeUpdate();
    }

    System.out.println("✓ Seeded 1 expired routing record");
  }

  private String buildRoutingKey(CallRequest request)
  {
    try
    {
      MessageDigest digest = MessageDigest.getInstance("SHA-256");
      String keyText = request.tenantId + "|" + request.subscriberId + "|" + request.callId;
      byte[] hash = digest.digest(keyText.getBytes(StandardCharsets.UTF_8));
      return toHex(hash);
    }
    catch (NoSuchAlgorithmException e)
    {
      throw new IllegalStateException("Unable to create routing key", e);
    }
  }

  private String requestPayloadToJson(CallRequest request)
  {
    return "{"
      + "\"tenantId\":\"" + request.tenantId + "\","
      + "\"subscriberId\":\"" + request.subscriberId + "\","
      + "\"callId\":\"" + request.callId + "\","
      + "\"sourceRegion\":\"" + request.sourceRegion + "\","
      + "\"targetRegion\":\"" + request.targetRegion + "\","
      + "\"networkSlice\":\"" + request.networkSlice + "\","
      + "\"priorityClass\":\"" + request.priorityClass + "\","
      + "\"roamingAllowed\":" + request.roamingAllowed + ","
      + "\"preferredRoute\":\"" + request.preferredRoute + "\","
      + "\"holdMinutes\":" + request.holdMinutes
      + "}";
  }

  private String decisionPayloadToJson(CallRequest request, String state,
                                       String reason, String expiresAtText)
  {
    return "{"
      + "\"tenantId\":\"" + request.tenantId + "\","
      + "\"subscriberId\":\"" + request.subscriberId + "\","
      + "\"callId\":\"" + request.callId + "\","
      + "\"routeState\":\"" + state + "\","
      + "\"reason\":\"" + reason + "\","
      + "\"sourceRegion\":\"" + request.sourceRegion + "\","
      + "\"targetRegion\":\"" + request.targetRegion + "\","
      + "\"preferredRoute\":\"" + request.preferredRoute + "\","
      + "\"holdMinutes\":" + request.holdMinutes + ","
      + "\"expiresAt\":\"" + expiresAtText + "\","
      + "\"ruleVersion\":\"telecom-route-rules-v1\","
      + "\"simulatedSwitch\":true"
      + "}";
  }

  private RoutingDecision evaluateRouting(CallRequest request)
  {
    if ("emergency".equals(request.priorityClass))
    {
      return new RoutingDecision("PRIORITIZED", "emergency_route_override", 15);
    }

    if (!request.targetRegion.equals(request.sourceRegion) && !request.roamingAllowed)
    {
      return new RoutingDecision("BLOCKED", "roaming_not_allowed", 10);
    }

    if ("vip".equals(request.priorityClass))
    {
      return new RoutingDecision("FAST_TRACK", "vip_priority_route", 20);
    }

    return new RoutingDecision("ROUTED", "standard_route", request.holdMinutes);
  }

  private void routeCall(Connection connection, CallRequest request) throws SQLException
  {
    long startNanos = System.nanoTime();
    Timestamp now = currentTimestamp();
    String routingKey = buildRoutingKey(request);

    deleteExpiredRoutingForKey(connection, routingKey, now);
    String[] existing = findExistingRouting(connection, routingKey, now);
    if (existing != null)
    {
      printRoutingReplay(request, existing[0], existing[1], existing[2], startNanos);
      return;
    }

    RoutingDecision decision = evaluateRouting(request);
    Timestamp expiresAt = new Timestamp(now.getTime() + decision.ttlMinutes * 60_000L);
    String requestPayload = requestPayloadToJson(request);
    String decisionPayload = decisionPayloadToJson(request, decision.state, decision.reason,
                                                   formatTimestamp(expiresAt));

    // A concurrent copy of this event can win the insert. Reread its stored
    // decision instead of treating that duplicate key as an application error.
    try (PreparedStatement insert = connection.prepareStatement(INSERT_ROUTING_SQL))
    {
      bindRouting(insert, routingKey, request, decision.state, decision.reason,
                  requestPayload, decisionPayload, now, now, expiresAt);
      insert.executeUpdate();
    }
    catch (SQLException exception)
    {
      if (!isDuplicateKeyError(exception))
      {
        throw exception;
      }

      existing = findExistingRouting(connection, routingKey, currentTimestamp());
      if (existing == null)
      {
        throw exception;
      }
      printRoutingReplay(request, existing[0], existing[1], existing[2], startNanos);
      return;
    }

    System.out.println(
        "→ Routing decision: tenant=" + request.tenantId
        + " subscriber=" + request.subscriberId
        + " call_id=" + request.callId
        + " state=" + decision.state
        + " source=" + request.sourceRegion
        + " target=" + request.targetRegion
        + " slice=" + request.networkSlice
        + " reason=" + decision.reason
        + " hold_expires=" + formatTimestamp(expiresAt)
        + " elapsed_ms=" + formatElapsedMillis(startNanos));
  }

  private void deleteExpiredRoutingForKey(Connection connection, String routingKey,
                                          Timestamp now) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(DELETE_EXPIRED_ROUTING_FOR_KEY_SQL))
    {
      statement.setString(1, routingKey);
      statement.setTimestamp(2, now);
      statement.executeUpdate();
    }
  }

  private String[] findExistingRouting(Connection connection, String routingKey,
                                       Timestamp now) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(SELECT_EXISTING_ROUTING_SQL))
    {
      statement.setString(1, routingKey);
      statement.setTimestamp(2, now);
      try (ResultSet resultSet = statement.executeQuery())
      {
        if (!resultSet.next())
        {
          return null;
        }
        return new String[] {
          resultSet.getString(1), resultSet.getString(2), resultSet.getString(3)
        };
      }
    }
  }

  private void printRoutingReplay(CallRequest request, String state, String reason,
                                  String expiresAtText, long startNanos)
  {
    System.out.println(
        "→ Routing replay: tenant=" + request.tenantId
        + " subscriber=" + request.subscriberId
        + " call_id=" + request.callId
        + " state=" + state
        + " reason=" + reason
        + " expires_at=" + expiresAtText
        + " elapsed_ms=" + formatElapsedMillis(startNanos));
  }

  private boolean isDuplicateKeyError(SQLException exception)
  {
    return exception.getErrorCode() == 1
           || (exception.getMessage() != null && exception.getMessage().contains("ORA-00001"));
  }

  private void bindRouting(PreparedStatement statement, String routingKey,
                           CallRequest request, String state, String routeReason,
                           String requestPayload, String decisionPayload,
                           Timestamp createdAt, Timestamp updatedAt, Timestamp expiresAt)
      throws SQLException
  {
    statement.setString(1, routingKey);
    statement.setString(2, request.tenantId);
    statement.setString(3, request.subscriberId);
    statement.setString(4, request.callId);
    statement.setString(5, request.sourceRegion);
    statement.setString(6, request.targetRegion);
    statement.setString(7, request.networkSlice);
    statement.setString(8, request.priorityClass);
    statement.setString(9, state);
    statement.setString(10, routeReason);
    statement.setString(11, requestPayload);
    statement.setString(12, decisionPayload);
    statement.setTimestamp(13, createdAt);
    statement.setTimestamp(14, updatedAt);
    statement.setTimestamp(15, expiresAt);
  }

  private void summarizeActiveRouting(Connection connection) throws SQLException
  {
    // Show the active routing footprint grouped by tenant, subscriber, and state.
    System.out.println("⋯ Active routing decisions by tenant/subscriber/state:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_SUMMARY_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      try (ResultSet resultSet = statement.executeQuery())
      {
        while (resultSet.next())
        {
          String tenantId = resultSet.getString(1);
          String subscriberId = resultSet.getString(2);
          String state = resultSet.getString(3);
          long rowCount = resultSet.getLong(4);
          System.out.println(
              "  tenant=" + padRight(tenantId, 12)
              + " subscriber=" + padRight(subscriberId, 10)
              + " state=" + padRight(state, 11)
              + " rows=" + rowCount);
        }
      }
    }

    System.out.println("⋯ Active routing details:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_DETAILS_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      try (ResultSet resultSet = statement.executeQuery())
      {
        while (resultSet.next())
        {
          String callId = resultSet.getString(1);
          String sourceRegion = resultSet.getString(2);
          String targetRegion = resultSet.getString(3);
          String networkSlice = resultSet.getString(4);
          String priorityClass = resultSet.getString(5);
          String state = resultSet.getString(6);
          String reason = resultSet.getString(7);
          String expiresAtText = resultSet.getString(8);
          System.out.println(
              "  call_id=" + padRight(callId, 12)
              + " source=" + padRight(sourceRegion, 8)
              + " target=" + padRight(targetRegion, 8)
              + " slice=" + padRight(networkSlice, 8)
              + " priority=" + padRight(priorityClass, 10)
              + " state=" + padRight(state, 11)
              + " reason=" + padRight(reason, 24)
              + " expires_at=" + expiresAtText);
        }
      }
    }
  }

  private void cleanupExpiredRouting(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(DELETE_EXPIRED_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      int rows = statement.executeUpdate();
      System.out.println("✓ Deleted " + rows + " expired routing record" + (rows == 1 ? "" : "s"));
    }
  }

  private void dropTable(Connection connection, boolean reportFailure) throws SQLException
  {
    if (connection == null)
    {
      return;
    }

    try (PreparedStatement statement = connection.prepareStatement(DROP_TABLE_SQL))
    {
      statement.execute();
      System.out.println("✓ Table " + TABLE_NAME + " dropped");
    }
    catch (SQLException e)
    {
      if (reportFailure)
      {
        System.out.println("⚠ Table " + TABLE_NAME + " not dropped: " + e.getMessage());
        throw e;
      }
    }
  }

  private String formatElapsedMillis(long startNanos)
  {
    return String.format("%.2f", (System.nanoTime() - startNanos) / 1_000_000.0);
  }

  private String formatTimestamp(Timestamp timestamp)
  {
    if (timestamp == null)
    {
      return "";
    }
    return new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS").format(timestamp);
  }

  private void executeStatement(Connection connection, String sql) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.execute();
    }
  }

  private void printSQLException(SQLException e)
  {
    while (e != null)
    {
      System.err.println("SQLException: " + e.getMessage());
      System.err.println("SQLState: " + e.getSQLState());
      System.err.println("ErrorCode: " + e.getErrorCode());
      e = e.getNextException();
      if (e != null)
      {
        System.err.println();
      }
    }
  }

  private Timestamp currentTimestamp()
  {
    return new Timestamp(System.currentTimeMillis());
  }

  private String padRight(String text, int width)
  {
    if (text == null)
    {
      text = "";
    }

    if (text.length() >= width)
    {
      return text;
    }

    StringBuilder builder = new StringBuilder(text);
    while (builder.length() < width)
    {
      builder.append(' ');
    }
    return builder.toString();
  }

  private String toHex(byte[] bytes)
  {
    StringBuilder builder = new StringBuilder(bytes.length * 2);
    for (byte value : bytes)
    {
      builder.append(String.format("%02x", value));
    }
    return builder.toString();
  }
}
