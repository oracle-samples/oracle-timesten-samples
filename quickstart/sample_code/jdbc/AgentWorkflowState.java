/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates how an application can use TimesTen to keep current
 *   agent-run and tool-call state close to the service coordinating the work.
 *   The application starts deterministic agent runs, records the current plan
 *   and tool-call state in JSON, reuses a completed tool result on retry, and
 *   removes expired state.
 *
 *   NOTE: Agent steps, tool calls, and responses are simulated. This sample
 *   does not call an AI model, agent framework, external tool service,
 *   perform vector search, or run in-database model inference.
 *
 *   The sample performs the following steps:
 *     - Creates 'agent_runs' and 'agent_tool_calls' tables
 *     - Creates indexes for tenant/agent summaries, tool calls by run, and expiration cleanup
 *     - Seeds one expired agent run and tool call
 *     - Starts sample agent runs and records their current state
 *     - Completes simulated tool calls and replays a repeated tool call
 *     - Rereads a completed tool call if a concurrent request inserts its key
 *     - Stores run plans, tool inputs, and results in JSON
 *     - Summarizes active runs and their tool calls
 *     - Deletes expired runs and tool calls
 *     - Drops the tables
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
import java.util.List;

/** Provides a JDBC demonstration of agent orchestration state in TimesTen. */
public class AgentWorkflowState
{
  private static final String PROGRAM_NAME = "AgentWorkflowState";
  private static final String DEFAULT_USERNAME = "appuser";
  private static final String TIMESTEN_DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";
  private static final String TIMESTEN_CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";
  private static final String DIRECT_URL_PREFIX = "jdbc:timesten:direct:";
  private static final String CLIENT_URL_PREFIX = "jdbc:timesten:client:";
  private static final String RUNS_TABLE = "agent_runs";
  private static final String TOOL_CALLS_TABLE = "agent_tool_calls";
  private static final int RUN_TTL_MINUTES = 30;

  private static final String CREATE_RUNS_TABLE_SQL =
    "CREATE TABLE " + RUNS_TABLE + " ("
    + "run_id           VARCHAR2(64)   NOT NULL PRIMARY KEY, "
    + "tenant_id        VARCHAR2(30)   NOT NULL, "
    + "user_id          VARCHAR2(30)   NOT NULL, "
    + "agent_name       VARCHAR2(60)   NOT NULL, "
    + "model_name       VARCHAR2(60)   NOT NULL, "
    + "request_summary  VARCHAR2(500)  NOT NULL, "
    + "status           VARCHAR2(20)   NOT NULL, "
    + "current_step     VARCHAR2(60)   NOT NULL, "
    + "run_state        JSON, "
    + "final_response   VARCHAR2(4000), "
    + "created_at       TIMESTAMP      NOT NULL, "
    + "updated_at       TIMESTAMP      NOT NULL, "
    + "expires_at       TIMESTAMP      NOT NULL)";

  private static final String CREATE_TOOL_CALLS_TABLE_SQL =
    "CREATE TABLE " + TOOL_CALLS_TABLE + " ("
    + "tool_call_key    VARCHAR2(64)   NOT NULL PRIMARY KEY, "
    + "run_id           VARCHAR2(64)   NOT NULL, "
    + "tool_name        VARCHAR2(60)   NOT NULL, "
    + "status           VARCHAR2(20)   NOT NULL, "
    + "input_payload    JSON, "
    + "result_payload   JSON, "
    + "created_at       TIMESTAMP      NOT NULL, "
    + "completed_at     TIMESTAMP, "
    + "expires_at       TIMESTAMP      NOT NULL)";

  private static final String CREATE_RUNS_TENANT_AGENT_INDEX_SQL =
    "CREATE INDEX idx_agent_runs_tenant_agent ON " + RUNS_TABLE + " (tenant_id, agent_name)";
  private static final String CREATE_RUNS_EXPIRES_INDEX_SQL =
    "CREATE INDEX idx_agent_runs_expires ON " + RUNS_TABLE + " (expires_at)";
  private static final String CREATE_TOOL_CALLS_RUN_INDEX_SQL =
    "CREATE INDEX idx_agent_tools_run ON " + TOOL_CALLS_TABLE + " (run_id)";
  private static final String CREATE_TOOL_CALLS_EXPIRES_INDEX_SQL =
    "CREATE INDEX idx_agent_tools_expires ON " + TOOL_CALLS_TABLE + " (expires_at)";

  private static final String INSERT_RUN_SQL =
    "INSERT INTO " + RUNS_TABLE + " ("
    + "run_id, tenant_id, user_id, agent_name, model_name, request_summary, "
    + "status, current_step, run_state, final_response, created_at, updated_at, expires_at) "
    + "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
  private static final String SELECT_ACTIVE_RUN_SQL =
    "SELECT status, current_step, JSON_SERIALIZE(run_state RETURNING VARCHAR2(4000)), "
    + "final_response, TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3') "
    + "FROM " + RUNS_TABLE + " WHERE run_id = ? AND expires_at > ?";
  private static final String UPDATE_RUN_SQL =
    "UPDATE " + RUNS_TABLE + " SET status = ?, current_step = ?, run_state = ?, "
    + "final_response = ?, updated_at = ?, expires_at = ? WHERE run_id = ?";
  private static final String INSERT_TOOL_CALL_SQL =
    "INSERT INTO " + TOOL_CALLS_TABLE + " ("
    + "tool_call_key, run_id, tool_name, status, input_payload, result_payload, "
    + "created_at, completed_at, expires_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
  private static final String SELECT_ACTIVE_TOOL_CALL_SQL =
    "SELECT status, JSON_SERIALIZE(result_payload RETURNING VARCHAR2(4000)), "
    + "TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3') FROM " + TOOL_CALLS_TABLE + " "
    + "WHERE tool_call_key = ? AND expires_at > ?";
  private static final String DELETE_EXPIRED_RUN_FOR_KEY_SQL =
    "DELETE FROM " + RUNS_TABLE + " WHERE run_id = ? AND expires_at <= ?";
  private static final String DELETE_EXPIRED_TOOL_FOR_KEY_SQL =
    "DELETE FROM " + TOOL_CALLS_TABLE + " WHERE tool_call_key = ? AND expires_at <= ?";
  private static final String SELECT_ACTIVE_RUN_SUMMARY_SQL =
    "SELECT tenant_id, agent_name, status, COUNT(*) FROM " + RUNS_TABLE + " "
    + "WHERE expires_at > ? GROUP BY tenant_id, agent_name, status "
    + "ORDER BY tenant_id, agent_name, status";
  private static final String SELECT_ACTIVE_RUN_DETAILS_SQL =
    "SELECT run_id, tenant_id, user_id, agent_name, status, current_step, request_summary, "
    + "TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3') FROM " + RUNS_TABLE + " "
    + "WHERE expires_at > ? ORDER BY updated_at DESC";
  private static final String SELECT_ACTIVE_TOOL_CALLS_SQL =
    "SELECT tool_name, status, JSON_SERIALIZE(input_payload RETURNING VARCHAR2(4000)), "
    + "JSON_SERIALIZE(result_payload RETURNING VARCHAR2(4000)), "
    + "TO_CHAR(completed_at, 'YYYY-MM-DD HH24:MI:SS.FF3') FROM " + TOOL_CALLS_TABLE + " "
    + "WHERE run_id = ? AND expires_at > ? ORDER BY created_at";
  private static final String DELETE_EXPIRED_TOOL_CALLS_SQL =
    "DELETE FROM " + TOOL_CALLS_TABLE + " WHERE expires_at <= ?";
  private static final String DELETE_EXPIRED_RUNS_SQL =
    "DELETE FROM " + RUNS_TABLE + " WHERE expires_at <= ?";
  private static final String DROP_TOOL_CALLS_TABLE_SQL = "DROP TABLE " + TOOL_CALLS_TABLE;
  private static final String DROP_RUNS_TABLE_SQL = "DROP TABLE " + RUNS_TABLE;

  private static final AgentRun[] AGENT_RUNS = new AgentRun[] {
    new AgentRun("retail_app", "user_001", "delivery-assist", "support-summary-v1",
                 "Help a support agent explain the delivery status for order 45012.",
                 new ToolCall[] {
                   new ToolCall("lookup_order", "{\"orderId\":\"45012\"}",
                                "{\"orderId\":\"45012\",\"status\":\"in_transit\",\"carrier\":\"NorthStar\"}"),
                   new ToolCall("check_delivery_status", "{\"orderId\":\"45012\",\"carrier\":\"NorthStar\"}",
                                "{\"deliveryWindow\":\"tomorrow\",\"confidence\":0.94}"),
                   new ToolCall("lookup_order", "{\"orderId\":\"45012\"}",
                                "{\"orderId\":\"45012\",\"status\":\"in_transit\",\"carrier\":\"NorthStar\"}")
                 },
                 "Order 45012 is in transit and is expected tomorrow."),
    new AgentRun("field_service", "user_204", "technician-assist", "technician-assist-v1",
                 "Prepare the next troubleshooting step for a router with packet loss.",
                 new ToolCall[] {
                   new ToolCall("lookup_device_notes", "{\"deviceId\":\"router_204\"}",
                                "{\"lastAction\":\"firmware_check\",\"recommendedNext\":\"inspect_cable\"}")
                 },
                 "Inspect the cable path, then continue with the firmware checks.")
  };

  private static final AgentRun EXPIRED_RUN =
    new AgentRun("retail_app", "user_expired", "delivery-assist", "support-summary-v1",
                 "Expired agent run used to show cleanup.", new ToolCall[0], null);

  private final IOLibrary ioLibrary;
  private String userOverride;
  private String passwordOverride;

  private static final class AgentRun
  {
    private final String tenantId;
    private final String userId;
    private final String agentName;
    private final String modelName;
    private final String request;
    private final ToolCall[] tools;
    private final String finalResponse;

    AgentRun(String tenantId, String userId, String agentName, String modelName,
             String request, ToolCall[] tools, String finalResponse)
    {
      this.tenantId = tenantId;
      this.userId = userId;
      this.agentName = agentName;
      this.modelName = modelName;
      this.request = request;
      this.tools = tools;
      this.finalResponse = finalResponse;
    }
  }

  private static final class ToolCall
  {
    private final String name;
    private final String inputJson;
    private final String resultJson;

    ToolCall(String name, String inputJson, String resultJson)
    {
      this.name = name;
      this.inputJson = inputJson;
      this.resultJson = resultJson;
    }
  }

  private static final class ToolCallState
  {
    private final String status;
    private final String resultJson;
    private final String expiresAtText;

    ToolCallState(String status, String resultJson, String expiresAtText)
    {
      this.status = status;
      this.resultJson = resultJson;
      this.expiresAtText = expiresAtText;
    }
  }

  public AgentWorkflowState()
  {
    ioLibrary = new IOLibrary(System.err);
  }

  public static void main(String[] args)
  {
    int exitCode = new AgentWorkflowState().run(args);
    if (exitCode != 0)
    {
      System.exit(exitCode);
    }
  }

  private int run(String[] args)
  {
    String usage = buildUsage();
    if (!parseOptions(args, usage) || !loadDrivers())
    {
      return 1;
    }
    if (ioLibrary.opt_doTrace)
    {
      DriverManager.setLogWriter(new PrintWriter(System.out, true));
    }

    AccessControl accessControl = new AccessControl();
    String username = (userOverride != null) ? userOverride : resolveUsername(accessControl);
    String password = passwordOverride;
    if (password == null)
    {
      password = System.getenv("TT_PASSWORD");
    }
    if (password == null || password.isEmpty())
    {
      password = accessControl.getPassword(username);
    }

    System.out.println("=== Agent workflow state demo ===");
    System.out.println();
    System.out.println("Connecting to TimesTen");
    try (Connection connection = DriverManager.getConnection(buildJdbcUrl(), username, password))
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
        dropTables(connection, demoSucceeded);
      }
    }
    catch (SQLException e)
    {
      printSQLException(e);
      return 1;
    }
    System.out.println("✓ Completed agent workflow state sample operations");
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
    return ioLibrary.parseOpts(forwardedArgs.toArray(new String[forwardedArgs.size()]), usage);
  }

  private String buildUsage()
  {
    return ioLibrary.getUsageString(PROGRAM_NAME)
      + "\n  -u, -user <name>       supply username non-interactively"
      + "\n  -p, -password <pw>     supply password, or set TT_PASSWORD";
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
      System.err.println("Unable to load TimesTen JDBC driver(s): " + e.getMessage());
      return false;
    }
  }

  private String resolveUsername(AccessControl accessControl)
  {
    String username = accessControl.getUsername();
    return (username == null || username.isEmpty()) ? DEFAULT_USERNAME : username;
  }

  private String buildJdbcUrl()
  {
    return (ioLibrary.opt_doClient ? CLIENT_URL_PREFIX : DIRECT_URL_PREFIX) + ioLibrary.opt_connstr;
  }

  private void runDemo(Connection connection) throws SQLException
  {
    dropTables(connection, false);
    createSchema(connection);
    seedExpiredState(connection);
    List<String> runIds = new ArrayList<String>();
    for (AgentRun run : AGENT_RUNS)
    {
      runIds.add(processAgentRun(connection, run));
    }
    summarizeActiveRuns(connection, runIds);
    cleanupExpiredState(connection);
  }

  private void createSchema(Connection connection) throws SQLException
  {
    executeStatement(connection, CREATE_RUNS_TABLE_SQL);
    System.out.println("✓ Table " + RUNS_TABLE + " created");
    executeStatement(connection, CREATE_TOOL_CALLS_TABLE_SQL);
    System.out.println("✓ Table " + TOOL_CALLS_TABLE + " created");
    executeStatement(connection, CREATE_RUNS_TENANT_AGENT_INDEX_SQL);
    System.out.println("✓ Index IDX_AGENT_RUNS_TENANT_AGENT created");
    executeStatement(connection, CREATE_RUNS_EXPIRES_INDEX_SQL);
    System.out.println("✓ Index IDX_AGENT_RUNS_EXPIRES created");
    executeStatement(connection, CREATE_TOOL_CALLS_RUN_INDEX_SQL);
    System.out.println("✓ Index IDX_AGENT_TOOLS_RUN created");
    executeStatement(connection, CREATE_TOOL_CALLS_EXPIRES_INDEX_SQL);
    System.out.println("✓ Index IDX_AGENT_TOOLS_EXPIRES created");
  }

  private void seedExpiredState(Connection connection) throws SQLException
  {
    Timestamp now = currentTimestamp();
    Timestamp expiredAt = new Timestamp(now.getTime() - 60_000L);
    String runId = buildRunId(EXPIRED_RUN);
    insertRun(connection, runId, EXPIRED_RUN, "EXPIRED", "expired",
              buildRunState(EXPIRED_RUN, "EXPIRED", "expired", new ArrayList<String>()),
              null, now, expiredAt);
    ToolCall expiredTool = new ToolCall("lookup_order", "{\"orderId\":\"expired\"}",
                                        "{\"status\":\"expired\"}");
    insertToolCall(connection, buildToolCallKey(runId, expiredTool), runId, expiredTool,
                   "COMPLETED", now, now, expiredAt);
    System.out.println("✓ Seeded 1 expired agent run and tool call");
  }

  private String processAgentRun(Connection connection, AgentRun run) throws SQLException
  {
    String runId = startRun(connection, run);
    List<String> toolEvents = new ArrayList<String>();
    for (ToolCall tool : run.tools)
    {
      runToolCall(connection, run, runId, tool, toolEvents);
      updateRunProgress(connection, run, runId, tool.name, toolEvents);
    }
    completeRun(connection, run, runId, toolEvents);
    return runId;
  }

  private String startRun(Connection connection, AgentRun run) throws SQLException
  {
    long startNanos = System.nanoTime();
    String runId = buildRunId(run);
    Timestamp now = currentTimestamp();
    deleteExpiredRunForKey(connection, runId, now);
    String[] existing = findActiveRun(connection, runId, now);
    if (existing != null)
    {
      System.out.println("→ Agent run resume: tenant=" + run.tenantId + " agent=" + run.agentName
        + " status=" + existing[0] + " step=" + existing[1] + " expires_at=" + existing[2]
        + " elapsed_ms=" + elapsedMs(startNanos));
      return runId;
    }
    Timestamp expiresAt = addMinutes(now, RUN_TTL_MINUTES);
    insertRun(connection, runId, run, "RUNNING", "plan_ready",
              buildRunState(run, "RUNNING", "plan_ready", new ArrayList<String>()),
              null, now, expiresAt);
    System.out.println("→ Agent run started: tenant=" + run.tenantId + " user=" + run.userId
      + " agent=" + run.agentName + " status=RUNNING step=plan_ready elapsed_ms="
      + elapsedMs(startNanos));
    return runId;
  }

  private void runToolCall(Connection connection, AgentRun run, String runId, ToolCall tool,
                           List<String> toolEvents) throws SQLException
  {
    long startNanos = System.nanoTime();
    String toolCallKey = buildToolCallKey(runId, tool);
    deleteExpiredToolForKey(connection, toolCallKey, currentTimestamp());
    ToolCallState existing = findActiveToolCall(connection, toolCallKey, currentTimestamp());
    if (existing != null)
    {
      toolEvents.add(toolEventJson(tool.name, existing.status, true));
      printToolReplay(run, tool, existing, startNanos);
      return;
    }

    Timestamp now = currentTimestamp();
    Timestamp expiresAt = addMinutes(now, RUN_TTL_MINUTES);
    try
    {
      insertToolCall(connection, toolCallKey, runId, tool, "COMPLETED", now, now, expiresAt);
    }
    catch (SQLException e)
    {
      if (!isDuplicateKeyError(e))
      {
        throw e;
      }
      existing = findActiveToolCall(connection, toolCallKey, currentTimestamp());
      if (existing == null)
      {
        throw e;
      }
      toolEvents.add(toolEventJson(tool.name, existing.status, true));
      printToolReplay(run, tool, existing, startNanos);
      return;
    }
    toolEvents.add(toolEventJson(tool.name, "COMPLETED", false));
    System.out.println("→ Tool call completed: agent=" + run.agentName + " tool=" + tool.name
      + " status=COMPLETED elapsed_ms=" + elapsedMs(startNanos));
  }

  private void updateRunProgress(Connection connection, AgentRun run, String runId,
                                 String currentStep, List<String> toolEvents) throws SQLException
  {
    Timestamp now = currentTimestamp();
    updateRun(connection, runId, run, "RUNNING", currentStep,
              buildRunState(run, "RUNNING", currentStep, toolEvents), null, now,
              addMinutes(now, RUN_TTL_MINUTES));
  }

  private void completeRun(Connection connection, AgentRun run, String runId,
                           List<String> toolEvents) throws SQLException
  {
    long startNanos = System.nanoTime();
    Timestamp now = currentTimestamp();
    updateRun(connection, runId, run, "COMPLETED", "response_ready",
              buildRunState(run, "COMPLETED", "response_ready", toolEvents), run.finalResponse,
              now, addMinutes(now, RUN_TTL_MINUTES));
    System.out.println("→ Agent run completed: tenant=" + run.tenantId + " agent=" + run.agentName
      + " status=COMPLETED elapsed_ms=" + elapsedMs(startNanos));
  }

  private void summarizeActiveRuns(Connection connection, List<String> runIds) throws SQLException
  {
    Timestamp now = currentTimestamp();
    System.out.println("⋯ Active agent runs by tenant/agent/status:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_RUN_SUMMARY_SQL))
    {
      statement.setTimestamp(1, now);
      try (ResultSet results = statement.executeQuery())
      {
        while (results.next())
        {
          System.out.println("  tenant=" + padRight(results.getString(1), 14)
            + " agent=" + padRight(results.getString(2), 20)
            + " status=" + padRight(results.getString(3), 10) + " runs=" + results.getLong(4));
        }
      }
    }
    System.out.println("⋯ Active agent run details:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_RUN_DETAILS_SQL))
    {
      statement.setTimestamp(1, now);
      try (ResultSet results = statement.executeQuery())
      {
        while (results.next())
        {
          String runId = results.getString(1);
          System.out.println("  run=" + runId.substring(0, 12) + "... tenant="
            + padRight(results.getString(2), 14) + " user=" + padRight(results.getString(3), 10)
            + " agent=" + padRight(results.getString(4), 20) + " status="
            + padRight(results.getString(5), 10) + " step=" + results.getString(6));
          System.out.println("    request: " + results.getString(7));
          System.out.println("    expires_at=" + results.getString(8));
        }
      }
    }
    System.out.println("⋯ Tool calls for active runs:");
    for (String runId : runIds)
    {
      try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_TOOL_CALLS_SQL))
      {
        statement.setString(1, runId);
        statement.setTimestamp(2, now);
        try (ResultSet results = statement.executeQuery())
        {
          while (results.next())
          {
            System.out.println("  run=" + runId.substring(0, 12) + "... tool="
              + padRight(results.getString(1), 24) + " status=" + padRight(results.getString(2), 10)
              + " completed_at=" + results.getString(5));
            System.out.println("    input=" + results.getString(3));
            System.out.println("    result=" + results.getString(4));
          }
        }
      }
    }
  }

  private void cleanupExpiredState(Connection connection) throws SQLException
  {
    Timestamp now = currentTimestamp();
    int toolCalls = executeUpdate(connection, DELETE_EXPIRED_TOOL_CALLS_SQL, now);
    int runs = executeUpdate(connection, DELETE_EXPIRED_RUNS_SQL, now);
    System.out.println("✓ Deleted " + runs + " expired agent run" + (runs == 1 ? "" : "s")
      + " and " + toolCalls + " expired tool call" + (toolCalls == 1 ? "" : "s"));
  }

  private void insertRun(Connection connection, String runId, AgentRun run, String status,
                         String currentStep, String runState, String finalResponse,
                         Timestamp now, Timestamp expiresAt) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(INSERT_RUN_SQL))
    {
      statement.setString(1, runId);
      statement.setString(2, run.tenantId);
      statement.setString(3, run.userId);
      statement.setString(4, run.agentName);
      statement.setString(5, run.modelName);
      statement.setString(6, run.request);
      statement.setString(7, status);
      statement.setString(8, currentStep);
      statement.setString(9, runState);
      statement.setString(10, finalResponse);
      statement.setTimestamp(11, now);
      statement.setTimestamp(12, now);
      statement.setTimestamp(13, expiresAt);
      statement.executeUpdate();
    }
  }

  private void updateRun(Connection connection, String runId, AgentRun run, String status,
                         String currentStep, String runState, String finalResponse,
                         Timestamp now, Timestamp expiresAt) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(UPDATE_RUN_SQL))
    {
      statement.setString(1, status);
      statement.setString(2, currentStep);
      statement.setString(3, runState);
      statement.setString(4, finalResponse);
      statement.setTimestamp(5, now);
      statement.setTimestamp(6, expiresAt);
      statement.setString(7, runId);
      statement.executeUpdate();
    }
  }

  private void insertToolCall(Connection connection, String toolCallKey, String runId, ToolCall tool,
                              String status, Timestamp createdAt, Timestamp completedAt,
                              Timestamp expiresAt) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(INSERT_TOOL_CALL_SQL))
    {
      statement.setString(1, toolCallKey);
      statement.setString(2, runId);
      statement.setString(3, tool.name);
      statement.setString(4, status);
      statement.setString(5, tool.inputJson);
      statement.setString(6, tool.resultJson);
      statement.setTimestamp(7, createdAt);
      statement.setTimestamp(8, completedAt);
      statement.setTimestamp(9, expiresAt);
      statement.executeUpdate();
    }
  }

  private void deleteExpiredRunForKey(Connection connection, String runId, Timestamp now) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(DELETE_EXPIRED_RUN_FOR_KEY_SQL))
    {
      statement.setString(1, runId);
      statement.setTimestamp(2, now);
      statement.executeUpdate();
    }
  }

  private void deleteExpiredToolForKey(Connection connection, String toolCallKey, Timestamp now)
      throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(DELETE_EXPIRED_TOOL_FOR_KEY_SQL))
    {
      statement.setString(1, toolCallKey);
      statement.setTimestamp(2, now);
      statement.executeUpdate();
    }
  }

  private String[] findActiveRun(Connection connection, String runId, Timestamp now) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_RUN_SQL))
    {
      statement.setString(1, runId);
      statement.setTimestamp(2, now);
      try (ResultSet results = statement.executeQuery())
      {
        return results.next() ? new String[] { results.getString(1), results.getString(2), results.getString(5) }
                             : null;
      }
    }
  }

  private ToolCallState findActiveToolCall(Connection connection, String toolCallKey, Timestamp now)
      throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_TOOL_CALL_SQL))
    {
      statement.setString(1, toolCallKey);
      statement.setTimestamp(2, now);
      try (ResultSet results = statement.executeQuery())
      {
        return results.next() ? new ToolCallState(results.getString(1), results.getString(2), results.getString(3))
                             : null;
      }
    }
  }

  private void dropTables(Connection connection, boolean reportFailure) throws SQLException
  {
    dropTable(connection, DROP_TOOL_CALLS_TABLE_SQL, TOOL_CALLS_TABLE, reportFailure);
    dropTable(connection, DROP_RUNS_TABLE_SQL, RUNS_TABLE, reportFailure);
  }

  private void dropTable(Connection connection, String sql, String tableName, boolean reportFailure)
      throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.execute();
      System.out.println("✓ Table " + tableName + " dropped");
    }
    catch (SQLException e)
    {
      if (reportFailure)
      {
        System.out.println("⚠ Table " + tableName + " not dropped: " + e.getMessage());
        throw e;
      }
    }
  }

  private void executeStatement(Connection connection, String sql) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.execute();
    }
  }

  private int executeUpdate(Connection connection, String sql, Timestamp timestamp) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(sql))
    {
      statement.setTimestamp(1, timestamp);
      return statement.executeUpdate();
    }
  }

  private String buildRunId(AgentRun run)
  {
    return sha256(run.tenantId + "|" + run.userId + "|" + run.agentName + "|" + run.request);
  }

  private String buildToolCallKey(String runId, ToolCall tool)
  {
    return sha256(runId + "|" + tool.name + "|" + tool.inputJson);
  }

  private String sha256(String value)
  {
    try
    {
      byte[] bytes = MessageDigest.getInstance("SHA-256").digest(value.getBytes(StandardCharsets.UTF_8));
      StringBuilder hex = new StringBuilder();
      for (byte valueByte : bytes)
      {
        hex.append(String.format("%02x", valueByte));
      }
      return hex.toString();
    }
    catch (NoSuchAlgorithmException e)
    {
      throw new IllegalStateException("Unable to create deterministic key", e);
    }
  }

  private String buildRunState(AgentRun run, String status, String currentStep, List<String> toolEvents)
  {
    StringBuilder plan = new StringBuilder();
    List<String> planSteps = new ArrayList<String>();
    for (int i = 0; i < run.tools.length; i++)
    {
      // Keep the logical plan concise; a repeated tool entry represents a retry.
      if (!planSteps.contains(run.tools[i].name))
      {
        planSteps.add(run.tools[i].name);
      }
    }
    for (int i = 0; i < planSteps.size(); i++)
    {
      if (i > 0)
      {
        plan.append(',');
      }
      plan.append("\"").append(planSteps.get(i)).append("\"");
    }
    return "{\"tenantId\":\"" + run.tenantId + "\",\"userId\":\"" + run.userId
      + "\",\"agent\":{\"name\":\"" + run.agentName + "\",\"model\":\"" + run.modelName
      + "\"},\"plan\":[" + plan + "],\"status\":\"" + status + "\",\"currentStep\":\""
      + currentStep + "\",\"toolEvents\":[" + join(toolEvents) + "],\"simulatedAgent\":true}";
  }

  private String toolEventJson(String toolName, String status, boolean reused)
  {
    return "{\"tool\":\"" + toolName + "\",\"status\":\"" + status
      + "\",\"reused\":" + reused + "}";
  }

  private String join(List<String> values)
  {
    StringBuilder result = new StringBuilder();
    for (int i = 0; i < values.size(); i++)
    {
      if (i > 0)
      {
        result.append(',');
      }
      result.append(values.get(i));
    }
    return result.toString();
  }

  private void printToolReplay(AgentRun run, ToolCall tool, ToolCallState state, long startNanos)
  {
    System.out.println("→ Tool call replay: agent=" + run.agentName + " tool=" + tool.name
      + " status=" + state.status + " expires_at=" + state.expiresAtText
      + " elapsed_ms=" + elapsedMs(startNanos));
  }

  private boolean isDuplicateKeyError(SQLException e)
  {
    return e.getErrorCode() == 1 || (e.getMessage() != null && e.getMessage().contains("ORA-00001"));
  }

  private Timestamp currentTimestamp()
  {
    return new Timestamp(System.currentTimeMillis());
  }

  private Timestamp addMinutes(Timestamp timestamp, int minutes)
  {
    return new Timestamp(timestamp.getTime() + minutes * 60_000L);
  }

  private String elapsedMs(long startNanos)
  {
    return String.format("%.2f", (System.nanoTime() - startNanos) / 1_000_000.0);
  }

  private String padRight(String value, int width)
  {
    return String.format("%-" + width + "s", value);
  }

  private void printSQLException(SQLException e)
  {
    System.err.println("✗ Sample failed: " + e.getMessage());
    SQLException next = e.getNextException();
    while (next != null)
    {
      System.err.println("  " + next.getMessage());
      next = next.getNextException();
    }
  }
}
