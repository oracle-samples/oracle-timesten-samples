#
# Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     This sample demonstrates how an application can use TimesTen as the
#     primary store for active AI response cache state. The application
#     computes a deterministic cache key for each AI request, checks TimesTen
#     for a fresh cached response, simulates a model call on cache miss, and
#     stores the response and operational metadata with an expiration time.
#
#     NOTE: Responses are simulated; this sample does not call an AI model,
#     perform vector search, run in-database model inference, or demonstrate
#     TimesTen Cache for Oracle Database.
#
#     The sample performs the following steps:
#       - Creates an 'ai_response_cache' table
#       - Creates indexes for tenant/model and expiration lookups
#       - Seeds one expired cache entry
#       - Processes sample AI requests, showing cache misses and hits
#       - Updates hit counts and last-accessed timestamps on cache hits
#       - Queries cache summary and JSON metadata
#       - Deletes expired cache entries
#       - Drops the table
#
import datetime
import hashlib
import json

import oracledb

import AccessControl

TABLE_NAME = "ai_response_cache"
CACHE_TTL_MINUTES = 30

CREATE_TABLE = f"""
  CREATE TABLE {TABLE_NAME}(
    cache_key        VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id        VARCHAR2(30)   NOT NULL,
    user_id          VARCHAR2(30)   NOT NULL,
    model_name       VARCHAR2(60)   NOT NULL,
    prompt_hash      VARCHAR2(64)   NOT NULL,
    prompt_summary   VARCHAR2(500)  NOT NULL,
    response_text    VARCHAR2(4000) NOT NULL,
    metadata         JSON,
    hit_count        TT_INT         NOT NULL,
    created_at       TIMESTAMP      NOT NULL,
    last_accessed_at TIMESTAMP      NOT NULL,
    expires_at       TIMESTAMP      NOT NULL)
"""

CREATE_TENANT_MODEL_INDEX = f"""
  CREATE INDEX idx_ai_cache_tenant_model
  ON {TABLE_NAME} (tenant_id, model_name)
"""

CREATE_EXPIRES_INDEX = f"""
  CREATE INDEX idx_ai_cache_expires
  ON {TABLE_NAME} (expires_at)
"""

INSERT_CACHE_ENTRY = f"""
  INSERT INTO {TABLE_NAME} (
    cache_key, tenant_id, user_id, model_name, prompt_hash, prompt_summary,
    response_text, metadata, hit_count, created_at, last_accessed_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11, :12)
"""

SELECT_FRESH_ENTRY = f"""
  SELECT response_text, metadata, hit_count, expires_at
  FROM {TABLE_NAME}
  WHERE cache_key = :1 AND expires_at > :2
"""

UPDATE_CACHE_HIT = f"""
  UPDATE {TABLE_NAME}
  SET hit_count = hit_count + 1,
      last_accessed_at = :1
  WHERE cache_key = :2
"""

DELETE_EXPIRED = f"DELETE FROM {TABLE_NAME} WHERE expires_at <= :1"
DROP_TABLE = f"DROP TABLE {TABLE_NAME}"

SAMPLE_REQUESTS = [
  {
    "tenant_id": "retail_app",
    "user_id": "user_001",
    "model_name": "support-summary-v1",
    "prompt": "Summarize order 45012 for a support agent.",
    "temperature": 0.2,
  },
  {
    "tenant_id": "retail_app",
    "user_id": "user_001",
    "model_name": "support-summary-v1",
    "prompt": "Summarize order 45012 for a support agent.",
    "temperature": 0.2,
  },
  {
    "tenant_id": "field_service",
    "user_id": "user_204",
    "model_name": "technician-assist-v1",
    "prompt": "Draft troubleshooting steps for a router with intermittent packet loss.",
    "temperature": 0.1,
  },
]


def current_timestamp():
  """Return the current timestamp used for cache comparisons."""

  return datetime.datetime.now()


def connect():
  """Create and return a TimesTen connection."""

  oracledb.init_oracle_client()
  credentials = AccessControl.getCredentials("aiResponseCache.py")
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
  """Create the cache table and supporting indexes."""

  cursor.execute(CREATE_TABLE)
  print(f"Table {TABLE_NAME} created")
  cursor.execute(CREATE_TENANT_MODEL_INDEX)
  print("Index IDX_AI_CACHE_TENANT_MODEL created")
  cursor.execute(CREATE_EXPIRES_INDEX)
  print("Index IDX_AI_CACHE_EXPIRES created")


def build_cache_key(request):
  """Build a deterministic cache key from request fields."""

  key_text = "|".join([
      request["tenant_id"],
      request["model_name"],
      request["prompt"],
      str(request["temperature"]),
  ])
  return hashlib.sha256(key_text.encode("utf-8")).hexdigest()


def build_prompt_hash(prompt):
  """Build a hash of the prompt so the cache can identify repeated prompts."""

  return hashlib.sha256(prompt.encode("utf-8")).hexdigest()


def summarize_prompt(prompt):
  """Return a short prompt summary for display and cache inspection."""

  if len(prompt) <= 80:
    return prompt
  return prompt[:77] + "..."


def simulate_model_response(request):
  """Return a simulated model response and operational metadata."""

  prompt = request["prompt"]
  prompt_tokens = max(8, len(prompt.split()) + 4)
  response_text = (
      "Simulated response for "
      f"{request['tenant_id']} using {request['model_name']}: "
      f"{summarize_prompt(prompt)}")
  response_tokens = max(12, len(response_text.split()) + 6)
  latency_ms = 40 + (len(prompt) % 35)
  metadata = {
      "temperature": request["temperature"],
      "promptTokens": prompt_tokens,
      "responseTokens": response_tokens,
      "latencyMs": latency_ms,
      "safetyLabel": "allowed",
      "simulatedModelCall": True,
  }
  return response_text, metadata


def insert_cache_entry(cursor, request, response_text, metadata, created_at, expires_at):
  """Insert a cache entry for a simulated AI response."""

  cache_key = build_cache_key(request)
  cursor.execute(INSERT_CACHE_ENTRY, [
      cache_key,
      request["tenant_id"],
      request["user_id"],
      request["model_name"],
      build_prompt_hash(request["prompt"]),
      summarize_prompt(request["prompt"]),
      response_text,
      json.dumps(metadata),
      0,
      created_at,
      created_at,
      expires_at,
  ])
  return cache_key


def seed_expired_entry(cursor):
  """Insert one expired entry so cleanup behavior is visible."""

  expired_request = {
      "tenant_id": "retail_app",
      "user_id": "user_099",
      "model_name": "support-summary-v1",
      "prompt": "Summarize an expired support request.",
      "temperature": 0.2,
  }
  created_at = current_timestamp() - datetime.timedelta(hours=2)
  expires_at = current_timestamp() - datetime.timedelta(minutes=1)
  response_text, metadata = simulate_model_response(expired_request)
  insert_cache_entry(cursor, expired_request, response_text, metadata, created_at, expires_at)
  print("Seeded 1 expired cache entry")


def find_cached_response(cursor, cache_key):
  """Return a fresh cached response for a cache key, if one exists."""

  cursor.execute(SELECT_FRESH_ENTRY, [cache_key, current_timestamp()])
  return cursor.fetchone()


def process_request(cursor, request):
  """Process one AI request using TimesTen as the active response cache."""

  cache_key = build_cache_key(request)
  cached_row = find_cached_response(cursor, cache_key)
  if cached_row:
    response_text, metadata_json, hit_count, expires_at = cached_row
    cursor.execute(UPDATE_CACHE_HIT, [current_timestamp(), cache_key])
    metadata = json.loads(metadata_json)
    print(
        "CACHE HIT  "
        f"tenant={request['tenant_id']} model={request['model_name']} "
        f"hits={hit_count + 1} expires={expires_at}")
    print(f"  Response: {response_text}")
    print(f"  Safety label from metadata: {metadata['safetyLabel']}")
    return

  response_text, metadata = simulate_model_response(request)
  created_at = current_timestamp()
  expires_at = created_at + datetime.timedelta(minutes=CACHE_TTL_MINUTES)
  insert_cache_entry(cursor, request, response_text, metadata, created_at, expires_at)
  print(
      "CACHE MISS "
      f"tenant={request['tenant_id']} model={request['model_name']} "
      f"stored_for_minutes={CACHE_TTL_MINUTES}")
  print(f"  Response: {response_text}")


def print_cache_summary(cursor):
  """Print summary information about active cache entries."""

  print("Cache summary by tenant and model:")
  cursor.execute(f"""
    SELECT tenant_id, model_name, COUNT(*), SUM(hit_count)
    FROM {TABLE_NAME}
    GROUP BY tenant_id, model_name
    ORDER BY tenant_id, model_name
  """)
  for tenant_id, model_name, entry_count, hit_count in cursor:
    print(
        f"  tenant={tenant_id:<14} model={model_name:<22} "
        f"entries={int(entry_count)} hits={int(hit_count)}")

  print("Metadata safety labels:")
  cursor.execute(f"""
    SELECT cache_key,
           JSON_VALUE(metadata, '$.safetyLabel' RETURNING VARCHAR2(30))
    FROM {TABLE_NAME}
    ORDER BY created_at
  """)
  for cache_key, safety_label in cursor:
    print(f"  cache_key={cache_key[:12]}... safetyLabel={safety_label}")


def delete_expired_entries(cursor):
  """Delete expired cache entries."""

  cursor.execute(DELETE_EXPIRED, [current_timestamp()])
  deleted = cursor.rowcount if cursor.rowcount is not None else 0
  print(f"Deleted {deleted} expired cache entry")


def run():
  """Run the AI response cache sample."""

  connection = None
  cursor = None
  try:
    connection = connect()
    cursor = connection.cursor()
    drop_table(cursor, False)
    create_schema(cursor)
    seed_expired_entry(cursor)

    for request in SAMPLE_REQUESTS:
      process_request(cursor, request)

    print_cache_summary(cursor)
    delete_expired_entries(cursor)
    drop_table(cursor, True)
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
