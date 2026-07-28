#
# Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     This sample demonstrates how an application can use TimesTen as the
#     primary store for active chat session memory. The
#     application stores recent messages, tool-call metadata, safety labels,
#     and citations in JSON so it can restore context quickly between turns.
#
#     NOTE: Responses are simulated; this sample does not call an AI model,
#     perform vector search, or run in-database model inference.
#
#     The sample performs the following steps:
#       - Creates a 'chat_sessions' table
#       - Creates indexes for tenant/user and expiration lookups
#       - Seeds one expired session so cleanup behavior is visible
#       - Starts and resumes sample chat sessions
#       - Stores chat memory in a JSON column
#       - Queries JSON fields to summarize active sessions
#       - Deletes expired sessions
#       - Drops the table
#
import datetime
import hashlib
import json

import oracledb

import AccessControl

TABLE_NAME = "chat_sessions"
SESSION_TTL_MINUTES = 45

CREATE_TABLE = f"""
  CREATE TABLE {TABLE_NAME}(
    session_key        VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id          VARCHAR2(30)   NOT NULL,
    user_id            VARCHAR2(30)   NOT NULL,
    conversation_topic VARCHAR2(80)   NOT NULL,
    session_state      JSON,
    created_at         TIMESTAMP      NOT NULL,
    last_updated_at    TIMESTAMP      NOT NULL,
    expires_at         TIMESTAMP      NOT NULL)
"""

CREATE_TENANT_USER_INDEX = f"""
  CREATE INDEX idx_chat_sessions_tenant_user
  ON {TABLE_NAME} (tenant_id, user_id)
"""

CREATE_EXPIRES_INDEX = f"""
  CREATE INDEX idx_chat_sessions_expires
  ON {TABLE_NAME} (expires_at)
"""

INSERT_SESSION = f"""
  INSERT INTO {TABLE_NAME}(
    session_key, tenant_id, user_id, conversation_topic, session_state,
    created_at, last_updated_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8)
"""

SELECT_ACTIVE_SESSION = f"""
  SELECT JSON_SERIALIZE(session_state RETURNING VARCHAR2(4000)),
         last_updated_at,
         expires_at
  FROM {TABLE_NAME}
  WHERE session_key = :1 AND expires_at > :2
"""

UPDATE_SESSION_STATE = f"""
  UPDATE {TABLE_NAME}
  SET session_state = :1,
      last_updated_at = :2,
      expires_at = :3
  WHERE session_key = :4
"""

SELECT_ACTIVE_SUMMARY = f"""
  SELECT tenant_id,
         user_id,
         conversation_topic,
         JSON_VALUE(session_state, '$.turn_count' RETURNING TT_INT),
         JSON_VALUE(session_state, '$.model.name' RETURNING VARCHAR2(60)),
         JSON_VALUE(session_state, '$.safety.status' RETURNING VARCHAR2(20))
  FROM {TABLE_NAME}
  WHERE expires_at > :1
  ORDER BY tenant_id, user_id, conversation_topic
"""

SELECT_ACTIVE_STATE = f"""
  SELECT session_key,
         tenant_id,
         user_id,
         conversation_topic,
         JSON_SERIALIZE(session_state RETURNING VARCHAR2(4000)),
         last_updated_at,
         expires_at
  FROM {TABLE_NAME}
  WHERE expires_at > :1
  ORDER BY last_updated_at DESC
"""

DELETE_EXPIRED = f"DELETE FROM {TABLE_NAME} WHERE expires_at <= :1"
DROP_TABLE = f"DROP TABLE {TABLE_NAME}"

SAMPLE_TURNS = [
    {
        "tenant_id": "retail_app",
        "user_id": "user_001",
        "conversation_topic": "order-status",
        "model_name": "support-summary-v1",
        "user_message": "Where is order 45012?",
        "response_hint": "The order is currently in transit and should arrive tomorrow.",
        "tool_name": "lookup_order_status",
        "citation_ref": "shipment-feed",
        "preferences": {
            "responseStyle": "concise",
            "locale": "en-US",
        },
    },
    {
        "tenant_id": "retail_app",
        "user_id": "user_001",
        "conversation_topic": "order-status",
        "model_name": "support-summary-v1",
        "user_message": "Can you repeat the earlier status in one sentence?",
        "response_hint": "The order is still in transit and is expected to arrive tomorrow.",
        "tool_name": "lookup_order_status",
        "citation_ref": "shipment-feed",
        "preferences": {
            "responseStyle": "concise",
            "locale": "en-US",
        },
    },
    {
        "tenant_id": "field_service",
        "user_id": "user_204",
        "conversation_topic": "router-troubleshooting",
        "model_name": "technician-assist-v1",
        "user_message": "I need the previous troubleshooting steps for the packet-loss issue.",
        "response_hint": "The router is still showing intermittent packet loss, so continue with the cable and firmware checks.",
        "tool_name": "lookup_device_notes",
        "citation_ref": "field-notes",
        "preferences": {
            "responseStyle": "detailed",
            "locale": "en-US",
        },
    },
]


def current_timestamp():
  """Return the current timestamp used for DB comparisons."""

  return datetime.datetime.now()


def current_timestamp_text():
  """Return a compact timestamp string for JSON payloads."""

  return current_timestamp().isoformat(timespec="seconds")


def connect():
  """Create and return a TimesTen connection."""

  oracledb.init_oracle_client()
  credentials = AccessControl.getCredentials("chatSessionMemory.py")
  connection = oracledb.connect(
      user=credentials.user,
      password=credentials.password,
      dsn=credentials.connstr)
  connection.autocommit = True
  return connection


def drop_table(cursor, report_missing):
  """Drop the sample table if it exists."""

  try:
    cursor.execute(DROP_TABLE)
    print(f"✓ Table {TABLE_NAME} dropped")
  except Exception as err:
    if report_missing:
      print(f"⚠ Table {TABLE_NAME} not dropped: {err}")


def create_schema(cursor):
  """Create the chat memory table and supporting indexes."""

  cursor.execute(CREATE_TABLE)
  print(f"✓ Table {TABLE_NAME} created")
  cursor.execute(CREATE_TENANT_USER_INDEX)
  print("✓ Index IDX_CHAT_SESSIONS_TENANT_USER created")
  cursor.execute(CREATE_EXPIRES_INDEX)
  print("✓ Index IDX_CHAT_SESSIONS_EXPIRES created")


def build_session_key(turn):
  """Build a deterministic session key from the conversation identity."""

  key_text = "|".join([
      turn["tenant_id"],
      turn["user_id"],
      turn["conversation_topic"],
  ])
  return hashlib.sha256(key_text.encode("utf-8")).hexdigest()


def shorten(text, limit=80):
  """Return a shortened string suitable for console output."""

  if len(text) <= limit:
    return text
  return text[:limit - 3] + "..."


def simulate_assistant_reply(turn, existing_state):
  """Create a simulated assistant reply that can reference prior chat memory."""

  memory_clause = ""
  if existing_state:
    messages = existing_state.get("messages", [])
    prior_user_messages = [
        msg.get("content", "")
        for msg in messages
        if msg.get("role") == "user"
    ]
    if prior_user_messages:
      memory_clause = (
          f" I remember your earlier message: "
          f"{shorten(prior_user_messages[-1], 55)}.")

  return (
      f"Simulated reply for {turn['tenant_id']} using {turn['model_name']}:"
      f"{memory_clause} {turn['response_hint']}"
  ).strip()


def build_session_state(turn, assistant_text):
  """Create the initial JSON state for a new chat session."""

  timestamp_text = current_timestamp_text()
  return {
      "tenantId": turn["tenant_id"],
      "userId": turn["user_id"],
      "topic": turn["conversation_topic"],
      "model": {
          "name": turn["model_name"],
          "provider": "simulated",
      },
      "preferences": turn.get("preferences", {}),
      "safety": {
          "status": "allowed",
      },
      "turn_count": 1,
      "messages": [
          {
              "role": "system",
              "content": (
                  "Keep recent chat context, citations, and safety metadata "
                  "available for low-latency retrieval."
              ),
              "timestamp": timestamp_text,
          },
          {
              "role": "user",
              "content": turn["user_message"],
              "timestamp": timestamp_text,
          },
          {
              "role": "assistant",
              "content": assistant_text,
              "timestamp": timestamp_text,
              "tool_calls": [
                  {
                      "name": turn["tool_name"],
                      "status": "simulated",
                  }
              ],
              "citations": [
                  {
                      "source": turn["citation_ref"],
                      "status": "simulated",
                  }
              ],
          },
      ],
  }


def append_turn(session_state, turn, assistant_text):
  """Add a new turn to an existing session state."""

  timestamp_text = current_timestamp_text()
  messages = session_state.setdefault("messages", [])
  messages.append({
      "role": "user",
      "content": turn["user_message"],
      "timestamp": timestamp_text,
  })
  messages.append({
      "role": "assistant",
      "content": assistant_text,
      "timestamp": timestamp_text,
      "tool_calls": [
          {
              "name": turn["tool_name"],
              "status": "simulated",
          }
      ],
      "citations": [
          {
              "source": turn["citation_ref"],
              "status": "simulated",
          }
      ],
  })
  session_state["tenantId"] = turn["tenant_id"]
  session_state["userId"] = turn["user_id"]
  session_state["topic"] = turn["conversation_topic"]
  session_state["model"] = {
      "name": turn["model_name"],
      "provider": "simulated",
  }
  session_state["preferences"] = turn.get("preferences", {})
  session_state["safety"] = {
      "status": "allowed",
  }
  session_state["turn_count"] = int(session_state.get("turn_count", 0)) + 1
  session_state["updatedAt"] = timestamp_text


def insert_session(cursor, session_key, turn, session_state, created_at, expires_at):
  """Insert a new chat session."""

  cursor.execute(INSERT_SESSION, [
      session_key,
      turn["tenant_id"],
      turn["user_id"],
      turn["conversation_topic"],
      json.dumps(session_state),
      created_at,
      created_at,
      expires_at,
  ])


def update_session(cursor, session_key, session_state, updated_at, expires_at):
  """Update an existing chat session with refreshed memory."""

  cursor.execute(UPDATE_SESSION_STATE, [
      json.dumps(session_state),
      updated_at,
      expires_at,
      session_key,
  ])


def load_active_session(cursor, session_key):
  """Return a fresh session state if one already exists."""

  cursor.execute(SELECT_ACTIVE_SESSION, [session_key, current_timestamp()])
  row = cursor.fetchone()
  if not row:
    return None

  session_state_json, last_updated_at, expires_at = row
  return {
      "session_state": json.loads(session_state_json),
      "last_updated_at": last_updated_at,
      "expires_at": expires_at,
  }


def seed_expired_session(cursor):
  """Insert one expired session so cleanup behavior is visible."""

  expired_turn = {
      "tenant_id": "retail_app",
      "user_id": "user_099",
      "conversation_topic": "expired-order-status",
      "model_name": "support-summary-v1",
      "user_message": "What happened with the old order update?",
      "response_hint": "This is an expired session that will be cleaned up.",
      "tool_name": "lookup_order_status",
      "citation_ref": "shipment-feed",
      "preferences": {
          "responseStyle": "concise",
          "locale": "en-US",
      },
  }

  session_key = build_session_key(expired_turn)
  created_at = current_timestamp() - datetime.timedelta(hours=2)
  expires_at = current_timestamp() - datetime.timedelta(minutes=1)
  assistant_text = simulate_assistant_reply(expired_turn, None)
  session_state = build_session_state(expired_turn, assistant_text)

  # Give the expired session a little more history so the cleanup step has
  # something realistic to remove.
  append_turn(session_state, expired_turn, assistant_text)

  insert_session(cursor, session_key, expired_turn, session_state, created_at, expires_at)
  print("✓ Seeded 1 expired chat session")


def process_turn(cursor, turn):
  """Insert or update a chat session turn."""

  session_key = build_session_key(turn)
  active_session = load_active_session(cursor, session_key)
  now = current_timestamp()
  expires_at = now + datetime.timedelta(minutes=SESSION_TTL_MINUTES)

  if active_session:
    session_state = active_session["session_state"]
    assistant_text = simulate_assistant_reply(turn, session_state)
    append_turn(session_state, turn, assistant_text)
    update_session(cursor, session_key, session_state, now, expires_at)
    print(
        "→ Session resume: "
        f"tenant={turn['tenant_id']} user={turn['user_id']} "
        f"topic={turn['conversation_topic']} turns={session_state['turn_count']}")
    print(f"  Assistant: {assistant_text}")
  else:
    assistant_text = simulate_assistant_reply(turn, None)
    session_state = build_session_state(turn, assistant_text)
    insert_session(cursor, session_key, turn, session_state, now, expires_at)
    print(
        "→ Session start: "
        f"tenant={turn['tenant_id']} user={turn['user_id']} "
        f"topic={turn['conversation_topic']} turns=1")
    print(f"  Assistant: {assistant_text}")


def print_active_summary(cursor):
  """Print summary information for active chat sessions."""

  print("⋯ Active sessions by tenant/user/topic:")
  cursor.execute(SELECT_ACTIVE_SUMMARY, [current_timestamp()])
  rows = cursor.fetchall()
  for tenant_id, user_id, conversation_topic, turn_count, model_name, safety_status in rows:
    print(
        f"  tenant={tenant_id:<14} user={user_id:<10} topic={conversation_topic:<24} "
        f"turns={int(turn_count)} model={model_name:<22} safety={safety_status}")

  print("⋯ Latest session memory snapshots:")
  cursor.execute(SELECT_ACTIVE_STATE, [current_timestamp()])
  for session_key, tenant_id, user_id, conversation_topic, session_state_json, last_updated_at, expires_at in cursor.fetchall():
    session_state = json.loads(session_state_json)
    messages = session_state.get("messages", [])
    last_user = next((msg.get("content", "") for msg in reversed(messages) if msg.get("role") == "user"), "")
    last_assistant = next((msg.get("content", "") for msg in reversed(messages) if msg.get("role") == "assistant"), "")
    turn_count = session_state.get("turn_count", 0)
    print(
        f"  session={session_key[:12]}... tenant={tenant_id} topic={conversation_topic} "
        f"turns={turn_count} updated={last_updated_at} expires={expires_at}")
    print(f"    last user: {shorten(last_user, 90)}")
    print(f"    last assistant: {shorten(last_assistant, 90)}")


def delete_expired_sessions(cursor):
  """Delete expired chat sessions."""

  cursor.execute(DELETE_EXPIRED, [current_timestamp()])
  deleted = cursor.rowcount if cursor.rowcount is not None else 0
  print(f"✓ Deleted {deleted} expired chat session")


def run():
  """Run the AI chat/session memory sample."""

  connection = None
  cursor = None
  try:
    print("=== Chat session memory demo ===")
    connection = connect()
    cursor = connection.cursor()
    drop_table(cursor, False)
    create_schema(cursor)
    seed_expired_session(cursor)

    for turn in SAMPLE_TURNS:
      process_turn(cursor, turn)

    print_active_summary(cursor)
    delete_expired_sessions(cursor)
    drop_table(cursor, True)
    print("✓ Completed chat session memory sample operations")
  except Exception as err:
    print("An error occurred", str(err))
  finally:
    if cursor:
      cursor.close()
    if connection:
      connection.close()
      print("Connection has been released")


if __name__ == "__main__":
  run()
