#
# Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     This sample demonstrates how an application can use TimesTen to keep
#     current agent-run and tool-call state close to the service coordinating
#     the work. The application starts deterministic agent runs, records the
#     current plan and tool-call state in JSON, reuses a completed tool result
#     on retry, and removes expired state.
#
#     NOTE: Agent steps, tool calls, and responses are simulated. This sample
#     does not call an AI model, agent framework, external tool service,
#     perform vector search, or run in-database model inference.
#
#     The sample performs the following steps:
#       - Creates 'agent_runs' and 'agent_tool_calls' tables
#       - Creates indexes for tenant/agent summaries, tool calls by run, and expiration cleanup
#       - Seeds one expired agent run and tool call
#       - Starts sample agent runs and records their current state
#       - Completes simulated tool calls and replays a repeated tool call
#       - Rereads a completed tool call if a concurrent request inserts its key
#       - Stores run plans, tool inputs, and results in JSON
#       - Summarizes active runs and their tool calls
#       - Deletes expired runs and tool calls
#       - Drops the tables
#
import datetime
import hashlib
import json
import sys
import time

import oracledb

import AccessControl

RUNS_TABLE = "agent_runs"
TOOL_CALLS_TABLE = "agent_tool_calls"
RUN_TTL_MINUTES = 30

CREATE_RUNS_TABLE = f"""
  CREATE TABLE {RUNS_TABLE}(
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
"""

CREATE_TOOL_CALLS_TABLE = f"""
  CREATE TABLE {TOOL_CALLS_TABLE}(
    tool_call_key    VARCHAR2(64)   NOT NULL PRIMARY KEY,
    run_id           VARCHAR2(64)   NOT NULL,
    tool_name        VARCHAR2(60)   NOT NULL,
    status           VARCHAR2(20)   NOT NULL,
    input_payload    JSON,
    result_payload   JSON,
    created_at       TIMESTAMP      NOT NULL,
    completed_at     TIMESTAMP,
    expires_at       TIMESTAMP      NOT NULL)
"""

CREATE_RUNS_TENANT_AGENT_INDEX = f"""
  CREATE INDEX idx_agent_runs_tenant_agent
  ON {RUNS_TABLE} (tenant_id, agent_name)
"""

CREATE_RUNS_EXPIRES_INDEX = f"""
  CREATE INDEX idx_agent_runs_expires
  ON {RUNS_TABLE} (expires_at)
"""

CREATE_TOOL_CALLS_RUN_INDEX = f"""
  CREATE INDEX idx_agent_tools_run
  ON {TOOL_CALLS_TABLE} (run_id)
"""

CREATE_TOOL_CALLS_EXPIRES_INDEX = f"""
  CREATE INDEX idx_agent_tools_expires
  ON {TOOL_CALLS_TABLE} (expires_at)
"""

INSERT_RUN = f"""
  INSERT INTO {RUNS_TABLE}(
    run_id, tenant_id, user_id, agent_name, model_name, request_summary,
    status, current_step, run_state, final_response, created_at, updated_at,
    expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11, :12,
          TO_TIMESTAMP(:13, 'YYYY-MM-DD HH24:MI:SS.FF3'))
"""

SELECT_ACTIVE_RUN = f"""
  SELECT status,
         current_step,
         JSON_SERIALIZE(run_state RETURNING VARCHAR2(4000)),
         final_response,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM {RUNS_TABLE}
  WHERE run_id = :1 AND expires_at > :2
"""

UPDATE_RUN = f"""
  UPDATE {RUNS_TABLE}
  SET status = :1,
      current_step = :2,
      run_state = :3,
      final_response = :4,
      updated_at = :5,
      expires_at = TO_TIMESTAMP(:6, 'YYYY-MM-DD HH24:MI:SS.FF3')
  WHERE run_id = :7
"""

INSERT_TOOL_CALL = f"""
  INSERT INTO {TOOL_CALLS_TABLE}(
    tool_call_key, run_id, tool_name, status, input_payload, result_payload,
    created_at, completed_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8,
          TO_TIMESTAMP(:9, 'YYYY-MM-DD HH24:MI:SS.FF3'))
"""

SELECT_ACTIVE_TOOL_CALL = f"""
  SELECT status,
         JSON_SERIALIZE(result_payload RETURNING VARCHAR2(4000)),
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM {TOOL_CALLS_TABLE}
  WHERE tool_call_key = :1 AND expires_at > :2
"""

DELETE_EXPIRED_RUN_FOR_KEY = f"""
  DELETE FROM {RUNS_TABLE}
  WHERE run_id = :1 AND expires_at <= :2
"""

DELETE_EXPIRED_TOOL_FOR_KEY = f"""
  DELETE FROM {TOOL_CALLS_TABLE}
  WHERE tool_call_key = :1 AND expires_at <= :2
"""

SELECT_ACTIVE_RUN_SUMMARY = f"""
  SELECT tenant_id, agent_name, status, COUNT(*)
  FROM {RUNS_TABLE}
  WHERE expires_at > :1
  GROUP BY tenant_id, agent_name, status
  ORDER BY tenant_id, agent_name, status
"""

SELECT_ACTIVE_RUN_DETAILS = f"""
  SELECT run_id, tenant_id, user_id, agent_name, status, current_step,
         request_summary, TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM {RUNS_TABLE}
  WHERE expires_at > :1
  ORDER BY updated_at DESC
"""

SELECT_ACTIVE_TOOL_CALLS = f"""
  SELECT tool_name,
         status,
         JSON_SERIALIZE(input_payload RETURNING VARCHAR2(4000)),
         JSON_SERIALIZE(result_payload RETURNING VARCHAR2(4000)),
         TO_CHAR(completed_at, 'YYYY-MM-DD HH24:MI:SS.FF3')
  FROM {TOOL_CALLS_TABLE}
  WHERE run_id = :1 AND expires_at > :2
  ORDER BY created_at
"""

DELETE_EXPIRED_TOOL_CALLS = f"DELETE FROM {TOOL_CALLS_TABLE} WHERE expires_at <= :1"
DELETE_EXPIRED_RUNS = f"DELETE FROM {RUNS_TABLE} WHERE expires_at <= :1"
DROP_TOOL_CALLS_TABLE = f"DROP TABLE {TOOL_CALLS_TABLE}"
DROP_RUNS_TABLE = f"DROP TABLE {RUNS_TABLE}"

AGENT_RUNS = [
    {
        "tenant_id": "retail_app",
        "user_id": "user_001",
        "agent_name": "delivery-assist",
        "model_name": "support-summary-v1",
        "request": "Help a support agent explain the delivery status for order 45012.",
        "tools": [
            {
                "tool_name": "lookup_order",
                "input": {"orderId": "45012"},
                "result": {"orderId": "45012", "status": "in_transit", "carrier": "NorthStar"},
            },
            {
                "tool_name": "check_delivery_status",
                "input": {"orderId": "45012", "carrier": "NorthStar"},
                "result": {"deliveryWindow": "tomorrow", "confidence": 0.94},
            },
            # Repeat the first call to show that an agent retry uses the stored result.
            {
                "tool_name": "lookup_order",
                "input": {"orderId": "45012"},
                "result": {"orderId": "45012", "status": "in_transit", "carrier": "NorthStar"},
            },
        ],
        "final_response": "Order 45012 is in transit and is expected tomorrow.",
    },
    {
        "tenant_id": "field_service",
        "user_id": "user_204",
        "agent_name": "technician-assist",
        "model_name": "technician-assist-v1",
        "request": "Prepare the next troubleshooting step for a router with packet loss.",
        "tools": [
            {
                "tool_name": "lookup_device_notes",
                "input": {"deviceId": "router_204"},
                "result": {"lastAction": "firmware_check", "recommendedNext": "inspect_cable"},
            },
        ],
        "final_response": "Inspect the cable path, then continue with the firmware checks.",
    },
]

EXPIRED_RUN = {
    "tenant_id": "retail_app",
    "user_id": "user_expired",
    "agent_name": "delivery-assist",
    "model_name": "support-summary-v1",
    "request": "Expired agent run used to show cleanup.",
}


def current_timestamp():
  """Return the current timestamp used for database comparisons."""

  return datetime.datetime.now()


def format_timestamp(timestamp):
  """Format timestamps with fixed millisecond precision for display and SQL."""

  return timestamp.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def to_json(payload):
  """Serialize sample payloads consistently before storing them in JSON columns."""

  return json.dumps(payload, separators=(",", ":"))


def connect():
  """Create and return a TimesTen connection."""

  oracledb.init_oracle_client()
  credentials = AccessControl.getCredentials(
      "agentWorkflowState.py", password_env_var="TT_PASSWORD")
  print("Connecting to TimesTen")
  connection = oracledb.connect(
      user=credentials.user,
      password=credentials.password,
      dsn=credentials.connstr)
  connection.autocommit = True
  print("✓ Connected")
  return connection


def drop_table(cursor, statement, table_name, report_missing):
  """Drop a sample table if it exists."""

  try:
    cursor.execute(statement)
    print(f"✓ Table {table_name} dropped")
  except Exception as err:
    if report_missing:
      print(f"⚠ Table {table_name} not dropped: {err}")


def create_schema(cursor):
  """Create agent-run and tool-call tables with indexes for this workflow."""

  cursor.execute(CREATE_RUNS_TABLE)
  print(f"✓ Table {RUNS_TABLE} created")
  cursor.execute(CREATE_TOOL_CALLS_TABLE)
  print(f"✓ Table {TOOL_CALLS_TABLE} created")
  cursor.execute(CREATE_RUNS_TENANT_AGENT_INDEX)
  print("✓ Index IDX_AGENT_RUNS_TENANT_AGENT created")
  cursor.execute(CREATE_RUNS_EXPIRES_INDEX)
  print("✓ Index IDX_AGENT_RUNS_EXPIRES created")
  cursor.execute(CREATE_TOOL_CALLS_RUN_INDEX)
  print("✓ Index IDX_AGENT_TOOLS_RUN created")
  cursor.execute(CREATE_TOOL_CALLS_EXPIRES_INDEX)
  print("✓ Index IDX_AGENT_TOOLS_EXPIRES created")


def build_run_id(run):
  """Build a deterministic identifier for one logical agent request."""

  key_text = "|".join([
      run["tenant_id"], run["user_id"], run["agent_name"], run["request"]])
  return hashlib.sha256(key_text.encode("utf-8")).hexdigest()


def build_tool_call_key(run_id, tool):
  """Build a deterministic key so retried logical tool calls reuse their result."""

  key_text = "|".join([run_id, tool["tool_name"], to_json(tool["input"])])
  return hashlib.sha256(key_text.encode("utf-8")).hexdigest()


def build_run_state(run, status, current_step, tool_events):
  """Create flexible run state that an application can restore between steps."""

  # Keep the logical plan concise; a repeated tool entry below represents a retry.
  plan = list(dict.fromkeys(tool["tool_name"] for tool in run.get("tools", [])))
  return to_json({
      "tenantId": run["tenant_id"],
      "userId": run["user_id"],
      "agent": {"name": run["agent_name"], "model": run["model_name"]},
      "plan": plan,
      "status": status,
      "currentStep": current_step,
      "toolEvents": tool_events,
      "simulatedAgent": True,
  })


def is_duplicate_key_error(error):
  """Return whether a database error reports a primary-key conflict."""

  error_info = error.args[0] if error.args else None
  return (getattr(error_info, "code", None) == 1
          or "ORA-00001" in str(error_info))


def seed_expired_state(cursor):
  """Insert an expired run and its tool call so cleanup behavior is visible."""

  now = current_timestamp()
  expired_at = now - datetime.timedelta(minutes=1)
  run_id = build_run_id(EXPIRED_RUN)
  run_state = build_run_state(EXPIRED_RUN, "EXPIRED", "expired", [])
  cursor.execute(
      INSERT_RUN,
      (run_id, EXPIRED_RUN["tenant_id"], EXPIRED_RUN["user_id"],
       EXPIRED_RUN["agent_name"], EXPIRED_RUN["model_name"], EXPIRED_RUN["request"],
       "EXPIRED", "expired", run_state, None, now, now, format_timestamp(expired_at)))
  expired_tool = {"tool_name": "lookup_order", "input": {"orderId": "expired"}}
  cursor.execute(
      INSERT_TOOL_CALL,
      (build_tool_call_key(run_id, expired_tool), run_id, expired_tool["tool_name"],
       "COMPLETED", to_json(expired_tool["input"]), to_json({"status": "expired"}),
       now, now, format_timestamp(expired_at)))
  print("✓ Seeded 1 expired agent run and tool call")


def delete_expired_run_for_key(cursor, run_id, now):
  """Remove expired state before a run with the same logical identity starts."""

  cursor.execute(DELETE_EXPIRED_RUN_FOR_KEY, (run_id, now))


def delete_expired_tool_for_key(cursor, tool_call_key, now):
  """Remove an expired tool result before the same logical call is retried."""

  cursor.execute(DELETE_EXPIRED_TOOL_FOR_KEY, (tool_call_key, now))


def lookup_active_run(cursor, run_id, now):
  """Return the active run, if the application has already started it."""

  cursor.execute(SELECT_ACTIVE_RUN, (run_id, now))
  return cursor.fetchone()


def start_run(cursor, run):
  """Create or resume an active agent run."""

  run_id = build_run_id(run)
  start_time = time.perf_counter()
  now = current_timestamp()
  delete_expired_run_for_key(cursor, run_id, now)
  existing = lookup_active_run(cursor, run_id, now)
  if existing is not None:
    status, current_step, _, _, expires_at_text = existing
    print(
        "→ Agent run resume: "
        f"tenant={run['tenant_id']} agent={run['agent_name']} status={status} "
        f"step={current_step} expires_at={expires_at_text} "
        f"elapsed_ms={(time.perf_counter() - start_time) * 1000:.2f}")
    return run_id, []

  expires_at = now + datetime.timedelta(minutes=RUN_TTL_MINUTES)
  run_state = build_run_state(run, "RUNNING", "plan_ready", [])
  cursor.execute(
      INSERT_RUN,
      (run_id, run["tenant_id"], run["user_id"], run["agent_name"],
       run["model_name"], run["request"], "RUNNING", "plan_ready", run_state,
       None, now, now, format_timestamp(expires_at)))
  print(
      "→ Agent run started: "
      f"tenant={run['tenant_id']} user={run['user_id']} agent={run['agent_name']} "
      f"status=RUNNING step=plan_ready elapsed_ms="
      f"{(time.perf_counter() - start_time) * 1000:.2f}")
  return run_id, []


def lookup_active_tool_call(cursor, tool_call_key, now):
  """Return a completed active tool call when a retry can reuse its result."""

  cursor.execute(SELECT_ACTIVE_TOOL_CALL, (tool_call_key, now))
  return cursor.fetchone()


def run_tool_call(cursor, run, run_id, tool, tool_events):
  """Store or replay one deterministic tool call for an active agent run."""

  start_time = time.perf_counter()
  now = current_timestamp()
  tool_call_key = build_tool_call_key(run_id, tool)
  delete_expired_tool_for_key(cursor, tool_call_key, now)
  existing = lookup_active_tool_call(cursor, tool_call_key, now)
  if existing is not None:
    status, result_json, expires_at_text = existing
    tool_events.append({"tool": tool["tool_name"], "status": status, "reused": True})
    print(
        "→ Tool call replay: "
        f"agent={run['agent_name']} tool={tool['tool_name']} status={status} "
        f"expires_at={expires_at_text} elapsed_ms="
        f"{(time.perf_counter() - start_time) * 1000:.2f}")
    return json.loads(result_json)

  expires_at = now + datetime.timedelta(minutes=RUN_TTL_MINUTES)
  input_json = to_json(tool["input"])
  result_json = to_json(tool["result"])
  try:
    cursor.execute(
        INSERT_TOOL_CALL,
        (tool_call_key, run_id, tool["tool_name"], "COMPLETED", input_json, result_json,
         now, now, format_timestamp(expires_at)))
  except oracledb.DatabaseError as error:
    if not is_duplicate_key_error(error):
      raise
    existing = lookup_active_tool_call(cursor, tool_call_key, current_timestamp())
    if existing is None:
      raise
    status, stored_result_json, expires_at_text = existing
    tool_events.append({"tool": tool["tool_name"], "status": status, "reused": True})
    print(
        "→ Tool call replay: "
        f"agent={run['agent_name']} tool={tool['tool_name']} status={status} "
        f"expires_at={expires_at_text} elapsed_ms="
        f"{(time.perf_counter() - start_time) * 1000:.2f}")
    return json.loads(stored_result_json)

  tool_events.append({"tool": tool["tool_name"], "status": "COMPLETED", "reused": False})
  print(
      "→ Tool call completed: "
      f"agent={run['agent_name']} tool={tool['tool_name']} status=COMPLETED "
      f"elapsed_ms={(time.perf_counter() - start_time) * 1000:.2f}")
  return tool["result"]


def complete_run(cursor, run, run_id, tool_events):
  """Store the final response and completed state after the tool work is done."""

  start_time = time.perf_counter()
  now = current_timestamp()
  expires_at = now + datetime.timedelta(minutes=RUN_TTL_MINUTES)
  run_state = build_run_state(run, "COMPLETED", "response_ready", tool_events)
  cursor.execute(
      UPDATE_RUN,
      ("COMPLETED", "response_ready", run_state, run["final_response"], now,
       format_timestamp(expires_at), run_id))
  print(
      "→ Agent run completed: "
      f"tenant={run['tenant_id']} agent={run['agent_name']} status=COMPLETED "
      f"elapsed_ms={(time.perf_counter() - start_time) * 1000:.2f}")


def update_run_progress(cursor, run, run_id, tool_events):
  """Persist the latest completed tool step while the agent run remains active."""

  now = current_timestamp()
  expires_at = now + datetime.timedelta(minutes=RUN_TTL_MINUTES)
  current_step = tool_events[-1]["tool"]
  run_state = build_run_state(run, "RUNNING", current_step, tool_events)
  cursor.execute(
      UPDATE_RUN,
      ("RUNNING", current_step, run_state, None, now,
       format_timestamp(expires_at), run_id))


def process_agent_run(cursor, run):
  """Run a deterministic agent workflow and persist its current state."""

  run_id, tool_events = start_run(cursor, run)
  for tool in run["tools"]:
    run_tool_call(cursor, run, run_id, tool, tool_events)
    update_run_progress(cursor, run, run_id, tool_events)
  complete_run(cursor, run, run_id, tool_events)
  return run_id


def summarize_active_runs(cursor, run_ids):
  """Print active agent-run summaries and the tool results for each run."""

  now = current_timestamp()
  print("⋯ Active agent runs by tenant/agent/status:")
  cursor.execute(SELECT_ACTIVE_RUN_SUMMARY, (now,))
  for tenant_id, agent_name, status, row_count in cursor:
    print(
        f"  tenant={tenant_id:<14} agent={agent_name:<20} "
        f"status={status:<10} runs={row_count}")

  print("⋯ Active agent run details:")
  cursor.execute(SELECT_ACTIVE_RUN_DETAILS, (now,))
  for run_id, tenant_id, user_id, agent_name, status, current_step, request_summary, expires_at in cursor:
    print(
        f"  run={run_id[:12]}... tenant={tenant_id:<14} user={user_id:<10} "
        f"agent={agent_name:<20} status={status:<10} step={current_step}")
    print(f"    request: {request_summary}")
    print(f"    expires_at={expires_at}")

  print("⋯ Tool calls for active runs:")
  for run_id in run_ids:
    cursor.execute(SELECT_ACTIVE_TOOL_CALLS, (run_id, now))
    for tool_name, status, input_json, result_json, completed_at in cursor:
      print(
          f"  run={run_id[:12]}... tool={tool_name:<24} status={status:<10} "
          f"completed_at={completed_at}")
      print(f"    input={input_json}")
      print(f"    result={result_json}")


def cleanup_expired_state(cursor):
  """Remove expired tool calls first, then their expired parent runs."""

  now = current_timestamp()
  cursor.execute(DELETE_EXPIRED_TOOL_CALLS, (now,))
  tool_call_count = cursor.rowcount
  cursor.execute(DELETE_EXPIRED_RUNS, (now,))
  run_count = cursor.rowcount
  print(
      f"✓ Deleted {run_count} expired agent run" +
      ("" if run_count == 1 else "s") +
      f" and {tool_call_count} expired tool call" +
      ("" if tool_call_count == 1 else "s"))


def run():
  """Run the sample."""

  connection = None
  cursor = None
  completed = False
  exit_code = 0

  try:
    print("=== Agent workflow state demo ===")
    connection = connect()
    cursor = connection.cursor()
    # Recreate the tables so the demo starts with known state every time.
    drop_table(cursor, DROP_TOOL_CALLS_TABLE, TOOL_CALLS_TABLE, False)
    drop_table(cursor, DROP_RUNS_TABLE, RUNS_TABLE, False)
    create_schema(cursor)
    # Seed expired records so the application-managed cleanup path is visible.
    seed_expired_state(cursor)

    run_ids = [process_agent_run(cursor, agent_run) for agent_run in AGENT_RUNS]
    summarize_active_runs(cursor, run_ids)
    cleanup_expired_state(cursor)
    drop_table(cursor, DROP_TOOL_CALLS_TABLE, TOOL_CALLS_TABLE, False)
    drop_table(cursor, DROP_RUNS_TABLE, RUNS_TABLE, False)
    completed = True
  except Exception as err:
    print(f"✗ Sample failed: {err}", file=sys.stderr)
    exit_code = 1
  finally:
    if cursor is not None:
      try:
        cursor.close()
      except Exception as err:
        print(f"⚠ Cursor close failed: {err}", file=sys.stderr)
        exit_code = 1
    if connection is not None:
      try:
        connection.close()
        print("Connection has been closed")
      except Exception as err:
        print(f"⚠ Connection close failed: {err}", file=sys.stderr)
        exit_code = 1

  if completed and exit_code == 0:
    print("✓ Completed agent workflow state sample operations")
  return exit_code


if __name__ == "__main__":
  sys.exit(run())
