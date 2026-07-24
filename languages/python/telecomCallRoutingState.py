#
# Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     This sample demonstrates how an application can use TimesTen as a fast
#     store for real-time telecom call routing state. The application keeps
#     hot routing decisions close to the service that needs them, applies
#     deterministic routing rules, stores request and decision metadata in
#     JSON, and removes expired routing state.
#
#     The sample uses simulated routing rules. It does not call a telecom
#     switch, perform network signaling, or depend on an external service.
#
#     The sample performs the following steps:
#       - Creates the 'call_routing_state' table
#       - Creates indexes for tenant/subscriber, call id, and expiration
#         lookups
#       - Seeds one expired routing record
#       - Processes sample call routing requests
#       - Shows idempotent replay for a repeated call request
#       - Stores request and decision metadata in JSON
#       - Summarizes active routing decisions by tenant/subscriber/state
#       - Deletes expired routing records
#       - Drops the table
#
import datetime
import hashlib
import json

import oracledb

import AccessControl

TABLE_NAME = "call_routing_state"

CREATE_TABLE = f"""
  CREATE TABLE {TABLE_NAME}(
    routing_key        VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id          VARCHAR2(30)   NOT NULL,
    subscriber_id      VARCHAR2(30)   NOT NULL,
    call_id            VARCHAR2(40)   NOT NULL,
    source_region      VARCHAR2(30)   NOT NULL,
    target_region      VARCHAR2(30)   NOT NULL,
    network_slice      VARCHAR2(30)   NOT NULL,
    priority_class     VARCHAR2(20)   NOT NULL,
    route_state        VARCHAR2(20)   NOT NULL,
    route_reason       VARCHAR2(120)  NOT NULL,
    request_payload    JSON,
    decision_payload   JSON,
    created_at         TIMESTAMP      NOT NULL,
    updated_at         TIMESTAMP      NOT NULL,
    expires_at         TIMESTAMP      NOT NULL)
"""

CREATE_TENANT_SUBSCRIBER_INDEX = f"""
  CREATE INDEX idx_call_routing_tenant_subscriber
  ON {TABLE_NAME} (tenant_id, subscriber_id)
"""

CREATE_CALL_ID_INDEX = f"""
  CREATE INDEX idx_call_routing_call_id
  ON {TABLE_NAME} (call_id)
"""

CREATE_EXPIRES_INDEX = f"""
  CREATE INDEX idx_call_routing_expires
  ON {TABLE_NAME} (expires_at)
"""

INSERT_ROUTING = f"""
  INSERT INTO {TABLE_NAME}(
    routing_key, tenant_id, subscriber_id, call_id, source_region,
    target_region, network_slice, priority_class, route_state, route_reason,
    request_payload, decision_payload, created_at, updated_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11, :12, :13, :14, :15)
"""

SELECT_EXISTING_ROUTING = f"""
  SELECT route_state,
         route_reason,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS')
  FROM {TABLE_NAME}
  WHERE routing_key = :1
    AND expires_at > :2
"""

SELECT_ACTIVE_SUMMARY = f"""
  SELECT tenant_id,
         subscriber_id,
         route_state,
         COUNT(*)
  FROM {TABLE_NAME}
  WHERE expires_at > :1
  GROUP BY tenant_id, subscriber_id, route_state
  ORDER BY tenant_id, subscriber_id, route_state
"""

SELECT_ACTIVE_DETAILS = f"""
  SELECT call_id,
         source_region,
         target_region,
         network_slice,
         priority_class,
         route_state,
         route_reason,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS')
  FROM {TABLE_NAME}
  WHERE expires_at > :1
  ORDER BY tenant_id, subscriber_id, call_id
"""

DELETE_EXPIRED = f"""
  DELETE FROM {TABLE_NAME}
  WHERE expires_at <= :1
"""

DROP_TABLE = f"DROP TABLE {TABLE_NAME}"

CALL_REQUESTS = [
    {
        "tenant_id": "north_mobile",
        "subscriber_id": "sub_1001",
        "call_id": "call_1001",
        "source_region": "us-east",
        "target_region": "us-east",
        "network_slice": "gold",
        "priority_class": "standard",
        "roaming_allowed": True,
        "preferred_route": "edge-east-1",
        "hold_minutes": 20,
    },
    {
        # Replay of the first request to demonstrate idempotent lookup.
        "tenant_id": "north_mobile",
        "subscriber_id": "sub_1001",
        "call_id": "call_1001",
        "source_region": "us-east",
        "target_region": "us-east",
        "network_slice": "gold",
        "priority_class": "standard",
        "roaming_allowed": True,
        "preferred_route": "edge-east-1",
        "hold_minutes": 20,
    },
    {
        "tenant_id": "north_mobile",
        "subscriber_id": "sub_2002",
        "call_id": "call_2001",
        "source_region": "us-east",
        "target_region": "eu-west",
        "network_slice": "silver",
        "priority_class": "standard",
        "roaming_allowed": False,
        "preferred_route": "edge-eu-2",
        "hold_minutes": 10,
    },
    {
        "tenant_id": "field_support",
        "subscriber_id": "sub_3003",
        "call_id": "call_3001",
        "source_region": "us-west",
        "target_region": "us-west",
        "network_slice": "platinum",
        "priority_class": "emergency",
        "roaming_allowed": True,
        "preferred_route": "edge-west-9",
        "hold_minutes": 15,
    },
]

EXPIRED_ROUTING = {
    "tenant_id": "north_mobile",
    "subscriber_id": "sub_9999",
    "call_id": "call_expired_0001",
    "source_region": "us-west",
    "target_region": "us-west",
    "network_slice": "bronze",
    "priority_class": "standard",
    "roaming_allowed": True,
    "preferred_route": "edge-west-1",
    "hold_minutes": 5,
}


def current_timestamp():
  """Return the current timestamp used for database comparisons."""

  return datetime.datetime.now()


def connect():
  """Create and return a TimesTen connection."""

  oracledb.init_oracle_client()
  credentials = AccessControl.getCredentials("telecomCallRoutingState.py")
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
    print(f"Table {TABLE_NAME} dropped")
  except Exception as err:
    if report_missing:
      print(f"Table {TABLE_NAME} not dropped: {err}")


def create_schema(cursor):
  """Create the call routing table and supporting indexes."""

  cursor.execute(CREATE_TABLE)
  print(f"Table {TABLE_NAME} created")
  cursor.execute(CREATE_TENANT_SUBSCRIBER_INDEX)
  print("Index IDX_CALL_ROUTING_TENANT_SUBSCRIBER created")
  cursor.execute(CREATE_CALL_ID_INDEX)
  print("Index IDX_CALL_ROUTING_CALL_ID created")
  cursor.execute(CREATE_EXPIRES_INDEX)
  print("Index IDX_CALL_ROUTING_EXPIRES created")


def build_routing_key(call_request):
  """Build a deterministic routing key."""

  key_text = "|".join([
      call_request["tenant_id"],
      call_request["subscriber_id"],
      call_request["call_id"],
  ])
  return hashlib.sha256(key_text.encode("utf-8")).hexdigest()


def request_payload_to_json(call_request):
  """Serialize the request payload."""

  payload = {
      "tenantId": call_request["tenant_id"],
      "subscriberId": call_request["subscriber_id"],
      "callId": call_request["call_id"],
      "sourceRegion": call_request["source_region"],
      "targetRegion": call_request["target_region"],
      "networkSlice": call_request["network_slice"],
      "priorityClass": call_request["priority_class"],
      "roamingAllowed": call_request["roaming_allowed"],
      "preferredRoute": call_request["preferred_route"],
      "holdMinutes": call_request["hold_minutes"],
  }
  return json.dumps(payload, separators=(",", ":"))


def decision_payload_to_json(call_request, state, reason, expires_at_text):
  """Serialize the routing decision payload."""

  payload = {
      "tenantId": call_request["tenant_id"],
      "subscriberId": call_request["subscriber_id"],
      "callId": call_request["call_id"],
      "routeState": state,
      "reason": reason,
      "sourceRegion": call_request["source_region"],
      "targetRegion": call_request["target_region"],
      "preferredRoute": call_request["preferred_route"],
      "holdMinutes": call_request["hold_minutes"],
      "expiresAt": expires_at_text,
      "ruleVersion": "telecom-route-rules-v1",
      "simulatedSwitch": True,
  }
  return json.dumps(payload, separators=(",", ":"))


def evaluate_routing(call_request):
  """Apply deterministic routing rules to a call request."""

  if call_request["priority_class"] == "emergency":
    state = "PRIORITIZED"
    reason = "emergency_route_override"
    ttl_minutes = 15
  elif (
      call_request["target_region"] != call_request["source_region"]
      and not call_request["roaming_allowed"]
  ):
    state = "BLOCKED"
    reason = "roaming_not_allowed"
    ttl_minutes = 10
  elif call_request["priority_class"] == "vip":
    state = "FAST_TRACK"
    reason = "vip_priority_route"
    ttl_minutes = 20
  else:
    state = "ROUTED"
    reason = "standard_route"
    ttl_minutes = call_request["hold_minutes"]

  expires_at = current_timestamp() + datetime.timedelta(minutes=ttl_minutes)
  return state, reason, expires_at


def seed_expired_routing(cursor):
  """Insert one expired routing row so cleanup behavior is visible."""

  routing_key = build_routing_key(EXPIRED_ROUTING)
  now = current_timestamp()
  expired_at = now - datetime.timedelta(minutes=1)
  request_payload = request_payload_to_json(EXPIRED_ROUTING)
  decision_payload = decision_payload_to_json(
      EXPIRED_ROUTING,
      "EXPIRED",
      "seeded_expired_state",
      expired_at.isoformat(timespec="seconds"))

  cursor.execute(
      INSERT_ROUTING,
      (
          routing_key,
          EXPIRED_ROUTING["tenant_id"],
          EXPIRED_ROUTING["subscriber_id"],
          EXPIRED_ROUTING["call_id"],
          EXPIRED_ROUTING["source_region"],
          EXPIRED_ROUTING["target_region"],
          EXPIRED_ROUTING["network_slice"],
          EXPIRED_ROUTING["priority_class"],
          "EXPIRED",
          "seeded_expired_state",
          request_payload,
          decision_payload,
          now,
          now,
          expired_at,
      ))
  print("Seeded 1 expired routing record")


def lookup_existing_routing(cursor, routing_key, now):
  """Return an active routing decision if one already exists."""

  cursor.execute(SELECT_EXISTING_ROUTING, (routing_key, now))
  return cursor.fetchone()


def store_routing(cursor, call_request, state, reason, expires_at):
  """Persist a new routing decision."""

  now = current_timestamp()
  routing_key = build_routing_key(call_request)
  request_payload = request_payload_to_json(call_request)
  decision_payload = decision_payload_to_json(
      call_request, state, reason, expires_at.isoformat(timespec="seconds"))

  cursor.execute(
      INSERT_ROUTING,
      (
          routing_key,
          call_request["tenant_id"],
          call_request["subscriber_id"],
          call_request["call_id"],
          call_request["source_region"],
          call_request["target_region"],
          call_request["network_slice"],
          call_request["priority_class"],
          state,
          reason,
          request_payload,
          decision_payload,
          now,
          now,
          expires_at,
      ))
  return routing_key


def route_call(cursor, call_request):
  """Route a call request, returning an idempotent stored decision."""

  now = current_timestamp()
  routing_key = build_routing_key(call_request)
  existing = lookup_existing_routing(cursor, routing_key, now)
  if existing is not None:
    state, reason, expires_at_text = existing
    print(
        "ROUTE REPLAY "
        f"tenant={call_request['tenant_id']} subscriber={call_request['subscriber_id']} "
        f"call_id={call_request['call_id']} state={state} reason={reason} "
        f"expires_at={expires_at_text}")
    return state

  state, reason, expires_at = evaluate_routing(call_request)
  store_routing(cursor, call_request, state, reason, expires_at)
  print(
      "ROUTE DECISION "
      f"tenant={call_request['tenant_id']} subscriber={call_request['subscriber_id']} "
      f"call_id={call_request['call_id']} state={state} "
      f"source={call_request['source_region']} target={call_request['target_region']} "
      f"slice={call_request['network_slice']} reason={reason} "
      f"hold_expires={expires_at.isoformat(timespec='seconds')}")
  return state


def summarize_active_routing(cursor):
  """Print a compact summary of active routing decisions."""

  print("Active routing decisions by tenant/subscriber/state:")
  cursor.execute(SELECT_ACTIVE_SUMMARY, (current_timestamp(),))
  for tenant_id, subscriber_id, state, row_count in cursor:
    print(
        f"  tenant={tenant_id:<12} subscriber={subscriber_id:<10} "
        f"state={state:<11} rows={row_count}")

  print("Active routing details:")
  cursor.execute(SELECT_ACTIVE_DETAILS, (current_timestamp(),))
  for call_id, source_region, target_region, network_slice, priority_class, state, reason, expires_at_text in cursor:
    print(
        f"  call_id={call_id:<12} source={source_region:<8} target={target_region:<8} "
        f"slice={network_slice:<8} priority={priority_class:<10} state={state:<11} "
        f"reason={reason:<24} expires_at={expires_at_text}")


def cleanup_expired_routing(cursor):
  """Delete expired routing records."""

  now = current_timestamp()
  cursor.execute(DELETE_EXPIRED, (now,))
  print(f"Deleted {cursor.rowcount} expired routing record" +
        ("" if cursor.rowcount == 1 else "s"))


def run():
  """Run the sample."""

  connection = connect()
  cursor = connection.cursor()

  try:
    drop_table(cursor, False)
    create_schema(cursor)
    seed_expired_routing(cursor)

    for call_request in CALL_REQUESTS:
      route_call(cursor, call_request)

    summarize_active_routing(cursor)
    cleanup_expired_routing(cursor)
    drop_table(cursor, False)
    print("Connection has been released")
  finally:
    cursor.close()
    connection.close()


if __name__ == "__main__":
  run()
