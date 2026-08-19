/*
 * Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   This sample demonstrates how an application can use TimesTen to keep
 *   current agent-run and tool-call state close to the service coordinating
 *   the work. The application starts deterministic agent runs, records the
 *   current plan and tool-call state in JSON, reuses a completed tool result
 *   on retry, and removes expired state.
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
'use strict';

const crypto = require('crypto');
const oracledb = require('oracledb');
const accessControl = require('./AccessControl');

oracledb.initOracleClient();
oracledb.autoCommit = true;

const RUNS_TABLE = 'agent_runs';
const TOOL_CALLS_TABLE = 'agent_tool_calls';
const RUN_TTL_MINUTES = 30;

const CREATE_RUNS_TABLE = `
  CREATE TABLE agent_runs(
    run_id           VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id        VARCHAR2(30)   NOT NULL,
    user_id          VARCHAR2(30)   NOT NULL,
    agent_name       VARCHAR2(60)   NOT NULL,
    model_name       VARCHAR2(60)   NOT NULL,
    request_summary  VARCHAR2(500)  NOT NULL,
    status           VARCHAR2(20)   NOT NULL,
    current_step     VARCHAR2(60)   NOT NULL,
    run_state        JSON,
    final_response   VARCHAR2(4000),
    created_at       TIMESTAMP      NOT NULL,
    updated_at       TIMESTAMP      NOT NULL,
    expires_at       TIMESTAMP      NOT NULL)
`;

const CREATE_TOOL_CALLS_TABLE = `
  CREATE TABLE agent_tool_calls(
    tool_call_key    VARCHAR2(64)   NOT NULL PRIMARY KEY,
    run_id           VARCHAR2(64)   NOT NULL,
    tool_name        VARCHAR2(60)   NOT NULL,
    status           VARCHAR2(20)   NOT NULL,
    input_payload    JSON,
    result_payload   JSON,
    created_at       TIMESTAMP      NOT NULL,
    completed_at     TIMESTAMP,
    expires_at       TIMESTAMP      NOT NULL)
`;

const CREATE_RUNS_TENANT_AGENT_INDEX = `
  CREATE INDEX idx_agent_runs_tenant_agent
  ON agent_runs (tenant_id, agent_name)
`;
const CREATE_RUNS_EXPIRES_INDEX = 'CREATE INDEX idx_agent_runs_expires ON agent_runs (expires_at)';
const CREATE_TOOL_CALLS_RUN_INDEX = 'CREATE INDEX idx_agent_tools_run ON agent_tool_calls (run_id)';
const CREATE_TOOL_CALLS_EXPIRES_INDEX = 'CREATE INDEX idx_agent_tools_expires ON agent_tool_calls (expires_at)';

const INSERT_RUN = `
  INSERT INTO agent_runs(
    run_id, tenant_id, user_id, agent_name, model_name, request_summary,
    status, current_step, run_state, final_response, created_at, updated_at,
    expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11, :12, :13)
`;

const SELECT_ACTIVE_RUN = `
  SELECT status, current_step,
         JSON_SERIALIZE(run_state RETURNING VARCHAR2(4000)),
         final_response,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM agent_runs
  WHERE run_id = :1 AND expires_at > :2
`;

const UPDATE_RUN = `
  UPDATE agent_runs
  SET status = :1, current_step = :2, run_state = :3, final_response = :4,
      updated_at = :5, expires_at = :6
  WHERE run_id = :7
`;

const INSERT_TOOL_CALL = `
  INSERT INTO agent_tool_calls(
    tool_call_key, run_id, tool_name, status, input_payload, result_payload,
    created_at, completed_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9)
`;

const SELECT_ACTIVE_TOOL_CALL = `
  SELECT status, JSON_SERIALIZE(result_payload RETURNING VARCHAR2(4000)),
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM agent_tool_calls
  WHERE tool_call_key = :1 AND expires_at > :2
`;

const DELETE_EXPIRED_RUN_FOR_KEY = 'DELETE FROM agent_runs WHERE run_id = :1 AND expires_at <= :2';
const DELETE_EXPIRED_TOOL_FOR_KEY = 'DELETE FROM agent_tool_calls WHERE tool_call_key = :1 AND expires_at <= :2';
const SELECT_ACTIVE_RUN_SUMMARY = `
  SELECT tenant_id, agent_name, status, COUNT(*)
  FROM agent_runs
  WHERE expires_at > :1
  GROUP BY tenant_id, agent_name, status
  ORDER BY tenant_id, agent_name, status
`;
const SELECT_ACTIVE_RUN_DETAILS = `
  SELECT run_id, tenant_id, user_id, agent_name, status, current_step,
         request_summary, TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM agent_runs
  WHERE expires_at > :1
  ORDER BY updated_at DESC
`;
const SELECT_ACTIVE_TOOL_CALLS = `
  SELECT tool_name, status,
         JSON_SERIALIZE(input_payload RETURNING VARCHAR2(4000)),
         JSON_SERIALIZE(result_payload RETURNING VARCHAR2(4000)),
         TO_CHAR(completed_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM agent_tool_calls
  WHERE run_id = :1 AND expires_at > :2
  ORDER BY created_at
`;
const DELETE_EXPIRED_TOOL_CALLS = 'DELETE FROM agent_tool_calls WHERE expires_at <= :1';
const DELETE_EXPIRED_RUNS = 'DELETE FROM agent_runs WHERE expires_at <= :1';
const DROP_TOOL_CALLS_TABLE = 'DROP TABLE agent_tool_calls';
const DROP_RUNS_TABLE = 'DROP TABLE agent_runs';

const AGENT_RUNS = [
  {
    tenant_id: 'retail_app', user_id: 'user_001', agent_name: 'delivery-assist',
    model_name: 'support-summary-v1',
    request: 'Help a support agent explain the delivery status for order 45012.',
    tools: [
      { tool_name: 'lookup_order', input: { orderId: '45012' },
        result: { orderId: '45012', status: 'in_transit', carrier: 'NorthStar' } },
      { tool_name: 'check_delivery_status', input: { orderId: '45012', carrier: 'NorthStar' },
        result: { deliveryWindow: 'tomorrow', confidence: 0.94 } },
      // Repeats the first call to show that a retry reuses the completed result.
      { tool_name: 'lookup_order', input: { orderId: '45012' },
        result: { orderId: '45012', status: 'in_transit', carrier: 'NorthStar' } }
    ],
    final_response: 'Order 45012 is in transit and is expected tomorrow.'
  },
  {
    tenant_id: 'field_service', user_id: 'user_204', agent_name: 'technician-assist',
    model_name: 'technician-assist-v1',
    request: 'Prepare the next troubleshooting step for a router with packet loss.',
    tools: [
      { tool_name: 'lookup_device_notes', input: { deviceId: 'router_204' },
        result: { lastAction: 'firmware_check', recommendedNext: 'inspect_cable' } }
    ],
    final_response: 'Inspect the cable path, then continue with the firmware checks.'
  }
];

const EXPIRED_RUN = {
  tenant_id: 'retail_app', user_id: 'user_expired', agent_name: 'delivery-assist',
  model_name: 'support-summary-v1', request: 'Expired agent run used to show cleanup.'
};

main();

async function main() {
  let connection;
  let completed = false;

  try {
    console.log('=== Agent workflow state demo ===');
    connection = await connect();
    // Recreate the tables so the demo starts with known state every time.
    await dropTable(connection, DROP_TOOL_CALLS_TABLE, TOOL_CALLS_TABLE, false);
    await dropTable(connection, DROP_RUNS_TABLE, RUNS_TABLE, false);
    await createSchema(connection);
    // Seed expired records so application-managed cleanup is visible.
    await seedExpiredState(connection);

    const runIds = [];
    for (const run of AGENT_RUNS) {
      runIds.push(await processAgentRun(connection, run));
    }
    await summarizeActiveRuns(connection, runIds);
    await cleanupExpiredState(connection);
    await dropTable(connection, DROP_TOOL_CALLS_TABLE, TOOL_CALLS_TABLE, false);
    await dropTable(connection, DROP_RUNS_TABLE, RUNS_TABLE, false);
    completed = true;
  } catch (err) {
    console.error(err);
    process.exitCode = 1;
  } finally {
    await closeConnection(connection);
    if (completed) {
      console.log('✓ Completed agent workflow state sample operations');
    }
  }
}

async function connect() {
  const credentials = accessControl.getCredentials('agentWorkflowState.js', 'TT_PASSWORD');
  console.log('Connecting to TimesTen');
  const connection = await oracledb.getConnection({
    user: credentials['-u'], password: credentials['-p'], connectString: credentials['-c']
  });
  console.log('✓ Connected');
  return connection;
}

async function dropTable(connection, statement, tableName, reportMissing) {
  try {
    await connection.execute(statement);
    console.log(`✓ Table ${tableName} dropped`);
  } catch (err) {
    if (reportMissing) {
      console.log(`⚠ Table ${tableName} not dropped: ${err.message}`);
    }
  }
}

async function createSchema(connection) {
  await connection.execute(CREATE_RUNS_TABLE);
  console.log(`✓ Table ${RUNS_TABLE} created`);
  await connection.execute(CREATE_TOOL_CALLS_TABLE);
  console.log(`✓ Table ${TOOL_CALLS_TABLE} created`);
  await connection.execute(CREATE_RUNS_TENANT_AGENT_INDEX);
  console.log('✓ Index IDX_AGENT_RUNS_TENANT_AGENT created');
  await connection.execute(CREATE_RUNS_EXPIRES_INDEX);
  console.log('✓ Index IDX_AGENT_RUNS_EXPIRES created');
  await connection.execute(CREATE_TOOL_CALLS_RUN_INDEX);
  console.log('✓ Index IDX_AGENT_TOOLS_RUN created');
  await connection.execute(CREATE_TOOL_CALLS_EXPIRES_INDEX);
  console.log('✓ Index IDX_AGENT_TOOLS_EXPIRES created');
}

function buildRunId(run) {
  return hash([run.tenant_id, run.user_id, run.agent_name, run.request].join('|'));
}

function buildToolCallKey(runId, tool) {
  return hash([runId, tool.tool_name, JSON.stringify(tool.input)].join('|'));
}

function hash(value) {
  return crypto.createHash('sha256').update(value, 'utf8').digest('hex');
}

function currentTimestamp() {
  return new Date();
}

function formatTimestamp(date) {
  const pad2 = (value) => String(value).padStart(2, '0');
  const pad3 = (value) => String(value).padStart(3, '0');
  return `${date.getUTCFullYear()}-${pad2(date.getUTCMonth() + 1)}-${pad2(date.getUTCDate())} ` +
         `${pad2(date.getUTCHours())}:${pad2(date.getUTCMinutes())}:${pad2(date.getUTCSeconds())}.` +
         `${pad3(date.getUTCMilliseconds())}`;
}

function elapsedMs(startTime) {
  return Number(process.hrtime.bigint() - startTime) / 1e6;
}

function buildRunState(run, status, currentStep, toolEvents) {
  // Keep the logical plan concise; a repeated tool entry below represents a retry.
  const plan = run.tools ? [...new Set(run.tools.map((tool) => tool.tool_name))] : [];
  return JSON.stringify({
    tenantId: run.tenant_id,
    userId: run.user_id,
    agent: { name: run.agent_name, model: run.model_name },
    plan,
    status,
    currentStep,
    toolEvents,
    simulatedAgent: true
  });
}

function isDuplicateKeyError(err) {
  return err.errorNum === 1 || err.code === 'ORA-00001' || /ORA-00001/.test(err.message || '');
}

async function seedExpiredState(connection) {
  const now = currentTimestamp();
  const expiredAt = new Date(now.getTime() - 60 * 1000);
  const runId = buildRunId(EXPIRED_RUN);
  await connection.execute(INSERT_RUN, [
    runId, EXPIRED_RUN.tenant_id, EXPIRED_RUN.user_id, EXPIRED_RUN.agent_name,
    EXPIRED_RUN.model_name, EXPIRED_RUN.request, 'EXPIRED', 'expired',
    buildRunState(EXPIRED_RUN, 'EXPIRED', 'expired', []), null, now, now, expiredAt
  ]);
  const expiredTool = { tool_name: 'lookup_order', input: { orderId: 'expired' } };
  await connection.execute(INSERT_TOOL_CALL, [
    buildToolCallKey(runId, expiredTool), runId, expiredTool.tool_name, 'COMPLETED',
    JSON.stringify(expiredTool.input), JSON.stringify({ status: 'expired' }), now, now, expiredAt
  ]);
  console.log('✓ Seeded 1 expired agent run and tool call');
}

async function lookupActiveRun(connection, runId) {
  const result = await connection.execute(SELECT_ACTIVE_RUN, [runId, currentTimestamp()]);
  return result.rows.length > 0 ? result.rows[0] : null;
}

async function startRun(connection, run) {
  const runId = buildRunId(run);
  const startTime = process.hrtime.bigint();
  await connection.execute(DELETE_EXPIRED_RUN_FOR_KEY, [runId, currentTimestamp()]);
  const existing = await lookupActiveRun(connection, runId);
  if (existing !== null) {
    const [status, currentStep, , , expiresAtText] = existing;
    console.log(
      `→ Agent run resume: tenant=${run.tenant_id} agent=${run.agent_name} status=${status} ` +
      `step=${currentStep} expires_at=${expiresAtText} elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
    );
    return { runId, toolEvents: [] };
  }

  const now = currentTimestamp();
  const expiresAt = new Date(now.getTime() + RUN_TTL_MINUTES * 60000);
  await connection.execute(INSERT_RUN, [
    runId, run.tenant_id, run.user_id, run.agent_name, run.model_name, run.request,
    'RUNNING', 'plan_ready', buildRunState(run, 'RUNNING', 'plan_ready', []), null,
    now, now, expiresAt
  ]);
  console.log(
    `→ Agent run started: tenant=${run.tenant_id} user=${run.user_id} agent=${run.agent_name} ` +
    `status=RUNNING step=plan_ready elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
  );
  return { runId, toolEvents: [] };
}

async function lookupActiveToolCall(connection, toolCallKey) {
  const result = await connection.execute(SELECT_ACTIVE_TOOL_CALL, [toolCallKey, currentTimestamp()]);
  return result.rows.length > 0 ? result.rows[0] : null;
}

function printToolReplay(run, tool, status, expiresAtText, startTime) {
  console.log(
    `→ Tool call replay: agent=${run.agent_name} tool=${tool.tool_name} status=${status} ` +
    `expires_at=${expiresAtText} elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
  );
}

async function runToolCall(connection, run, runId, tool, toolEvents) {
  const startTime = process.hrtime.bigint();
  const toolCallKey = buildToolCallKey(runId, tool);
  await connection.execute(DELETE_EXPIRED_TOOL_FOR_KEY, [toolCallKey, currentTimestamp()]);
  const existing = await lookupActiveToolCall(connection, toolCallKey);
  if (existing !== null) {
    const [status, resultJson, expiresAtText] = existing;
    toolEvents.push({ tool: tool.tool_name, status, reused: true });
    printToolReplay(run, tool, status, expiresAtText, startTime);
    return JSON.parse(resultJson);
  }

  const now = currentTimestamp();
  const expiresAt = new Date(now.getTime() + RUN_TTL_MINUTES * 60000);
  try {
    await connection.execute(INSERT_TOOL_CALL, [
      toolCallKey, runId, tool.tool_name, 'COMPLETED', JSON.stringify(tool.input),
      JSON.stringify(tool.result), now, now, expiresAt
    ]);
  } catch (err) {
    if (!isDuplicateKeyError(err)) {
      throw err;
    }
    const concurrentToolCall = await lookupActiveToolCall(connection, toolCallKey);
    if (concurrentToolCall === null) {
      throw err;
    }
    const [status, resultJson, expiresAtText] = concurrentToolCall;
    toolEvents.push({ tool: tool.tool_name, status, reused: true });
    printToolReplay(run, tool, status, expiresAtText, startTime);
    return JSON.parse(resultJson);
  }
  toolEvents.push({ tool: tool.tool_name, status: 'COMPLETED', reused: false });
  console.log(
    `→ Tool call completed: agent=${run.agent_name} tool=${tool.tool_name} status=COMPLETED ` +
    `elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
  );
  return tool.result;
}

async function updateRunProgress(connection, run, runId, toolEvents) {
  const now = currentTimestamp();
  const expiresAt = new Date(now.getTime() + RUN_TTL_MINUTES * 60000);
  const currentStep = toolEvents[toolEvents.length - 1].tool;
  await connection.execute(UPDATE_RUN, [
    'RUNNING', currentStep, buildRunState(run, 'RUNNING', currentStep, toolEvents), null,
    now, expiresAt, runId
  ]);
}

async function completeRun(connection, run, runId, toolEvents) {
  const startTime = process.hrtime.bigint();
  const now = currentTimestamp();
  const expiresAt = new Date(now.getTime() + RUN_TTL_MINUTES * 60000);
  await connection.execute(UPDATE_RUN, [
    'COMPLETED', 'response_ready', buildRunState(run, 'COMPLETED', 'response_ready', toolEvents),
    run.final_response, now, expiresAt, runId
  ]);
  console.log(
    `→ Agent run completed: tenant=${run.tenant_id} agent=${run.agent_name} ` +
    `status=COMPLETED elapsed_ms=${elapsedMs(startTime).toFixed(2)}`
  );
}

async function processAgentRun(connection, run) {
  const { runId, toolEvents } = await startRun(connection, run);
  for (const tool of run.tools) {
    await runToolCall(connection, run, runId, tool, toolEvents);
    await updateRunProgress(connection, run, runId, toolEvents);
  }
  await completeRun(connection, run, runId, toolEvents);
  return runId;
}

async function summarizeActiveRuns(connection, runIds) {
  const now = currentTimestamp();
  console.log('⋯ Active agent runs by tenant/agent/status:');
  const summary = await connection.execute(SELECT_ACTIVE_RUN_SUMMARY, [now]);
  for (const [tenantId, agentName, status, rowCount] of summary.rows) {
    console.log(
      `  tenant=${tenantId.padEnd(14)} agent=${agentName.padEnd(20)} ` +
      `status=${status.padEnd(10)} runs=${rowCount}`
    );
  }

  console.log('⋯ Active agent run details:');
  const details = await connection.execute(SELECT_ACTIVE_RUN_DETAILS, [now]);
  for (const [runId, tenantId, userId, agentName, status, currentStep, request, expiresAt] of details.rows) {
    console.log(
      `  run=${runId.slice(0, 12)}... tenant=${tenantId.padEnd(14)} user=${userId.padEnd(10)} ` +
      `agent=${agentName.padEnd(20)} status=${status.padEnd(10)} step=${currentStep}`
    );
    console.log(`    request: ${request}`);
    console.log(`    expires_at=${expiresAt}`);
  }

  console.log('⋯ Tool calls for active runs:');
  for (const runId of runIds) {
    const toolCalls = await connection.execute(SELECT_ACTIVE_TOOL_CALLS, [runId, now]);
    for (const [toolName, status, inputJson, resultJson, completedAt] of toolCalls.rows) {
      console.log(
        `  run=${runId.slice(0, 12)}... tool=${toolName.padEnd(24)} ` +
        `status=${status.padEnd(10)} completed_at=${completedAt}`
      );
      console.log(`    input=${inputJson}`);
      console.log(`    result=${resultJson}`);
    }
  }
}

async function cleanupExpiredState(connection) {
  const now = currentTimestamp();
  const toolCallResult = await connection.execute(DELETE_EXPIRED_TOOL_CALLS, [now]);
  const runResult = await connection.execute(DELETE_EXPIRED_RUNS, [now]);
  const runSuffix = runResult.rowsAffected === 1 ? '' : 's';
  const toolSuffix = toolCallResult.rowsAffected === 1 ? '' : 's';
  console.log(
    `✓ Deleted ${runResult.rowsAffected} expired agent run${runSuffix} and ` +
    `${toolCallResult.rowsAffected} expired tool call${toolSuffix}`
  );
}

async function closeConnection(connection) {
  if (connection) {
    try {
      await connection.close();
      console.log('Connection has been closed');
    } catch (err) {
      console.error(err);
      process.exitCode = 1;
    }
  }
}
