#
# Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     This sample demonstrates how an application can use TimesTen as a fast
#     online feature store for real-time personalization support. The
#     application keeps the latest feature values close to the service that
#     needs them, so it can fetch the current state with very low latency,
#     refresh stale data, and store a JSON audit trail for downstream analysis.
#
#     NOTE: The sample uses simulated feature updates and does not call an AI
#     model, perform vector search, or run in-database model inference.
#
#     The sample performs the following steps:
#       - Creates a 'user_features' table
#       - Creates indexes for tenant/user and freshness lookups
#       - Seeds one stale feature row so cleanup behavior is visible
#       - Upserts fresh feature values for sample users
#       - Fetches the current feature set for a user with low latency
#       - Stores a JSON audit payload for the resulting personalization decision
#       - Deletes stale feature rows
#       - Drops the table
#
import datetime
import hashlib
import json
import time

import oracledb

import AccessControl

TABLE_NAME = "user_features"
FEATURE_TTL_MINUTES = 60

CREATE_TABLE = f"""
  CREATE TABLE {TABLE_NAME}(
    feature_key       VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id         VARCHAR2(30)   NOT NULL,
    user_id           VARCHAR2(30)   NOT NULL,
    feature_name      VARCHAR2(60)   NOT NULL,
    feature_value     JSON           NOT NULL,
    freshness_ts      TIMESTAMP      NOT NULL,
    model_version     VARCHAR2(40)   NOT NULL,
    audit_payload     JSON,
    created_at        TIMESTAMP      NOT NULL,
    updated_at        TIMESTAMP      NOT NULL,
    expires_at        TIMESTAMP      NOT NULL)
"""

CREATE_TENANT_USER_INDEX = f"""
  CREATE INDEX idx_user_features_tenant_user
  ON {TABLE_NAME} (tenant_id, user_id)
"""

CREATE_FRESHNESS_INDEX = f"""
  CREATE INDEX idx_user_features_freshness
  ON {TABLE_NAME} (expires_at)
"""

INSERT_FEATURE = f"""
  INSERT INTO {TABLE_NAME}(
    feature_key, tenant_id, user_id, feature_name, feature_value, freshness_ts,
    model_version, audit_payload, created_at, updated_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11)
"""

SELECT_ACTIVE_FEATURES = f"""
  SELECT feature_name,
         JSON_SERIALIZE(feature_value RETURNING VARCHAR2(4000)),
         TO_CHAR(freshness_ts, 'YYYY-MM-DD HH24:MI:SS.FF3'),
         model_version,
         JSON_SERIALIZE(audit_payload RETURNING VARCHAR2(4000))
  FROM {TABLE_NAME}
  WHERE tenant_id = :1
    AND user_id = :2
    AND expires_at > :3
  ORDER BY feature_name
"""

SELECT_FEATURE_SUMMARY = f"""
  SELECT tenant_id,
         user_id,
         COUNT(*),
         SUM(CASE
               WHEN JSON_VALUE(feature_value, '$.valueType' RETURNING VARCHAR2(20)) = 'numeric'
               THEN JSON_VALUE(feature_value, '$.value' RETURNING TT_INT)
               ELSE 0
             END)
  FROM {TABLE_NAME}
  WHERE expires_at > :1
  GROUP BY tenant_id, user_id
  ORDER BY tenant_id, user_id
"""

UPDATE_AUDIT = f"""
  UPDATE {TABLE_NAME}
  SET audit_payload = :1,
      updated_at = :2,
      expires_at = :3
  WHERE feature_key = :4
"""

DELETE_EXPIRED = f"DELETE FROM {TABLE_NAME} WHERE expires_at <= :1"
DROP_TABLE = f"DROP TABLE {TABLE_NAME}"

FEATURE_UPDATES = [
    {
        "tenant_id": "retail_app",
        "user_id": "user_001",
        "feature_name": "cart_value",
        "feature_value": {
            "valueType": "numeric",
            "value": 128,
            "source": "checkout-events",
            "freshness": "seconds",
        },
        "model_version": "feature-agg-v1",
        "decision": {
            "variant": "priority-shipping",
            "reason": "high_cart_value",
            "confidence": 0.92,
        },
    },
    {
        "tenant_id": "retail_app",
        "user_id": "user_001",
        "feature_name": "preferred_channel",
        "feature_value": {
            "valueType": "string",
            "value": "mobile",
            "source": "profile-service",
            "freshness": "minutes",
        },
        "model_version": "feature-agg-v1",
        "decision": {
            "variant": "mobile-first",
            "reason": "recent_mobile_usage",
            "confidence": 0.88,
        },
    },
    {
        "tenant_id": "field_service",
        "user_id": "user_204",
        "feature_name": "device_risk",
        "feature_value": {
            "valueType": "numeric",
            "value": 73,
            "source": "device-telemetry",
            "freshness": "seconds",
        },
        "model_version": "feature-agg-v2",
        "decision": {
            "variant": "proactive-support",
            "reason": "elevated_risk_score",
            "confidence": 0.81,
        },
    },
    {
        "tenant_id": "retail_app",
        "user_id": "user_001",
        "feature_name": "delivery_eta_hours",
        "feature_value": {
            "valueType": "numeric",
            "value": 18,
            "source": "shipping-service",
            "freshness": "hours",
        },
        "model_version": "feature-agg-v1",
        "decision": {
            "variant": "fast-tracked",
            "reason": "recent_support_contact",
            "confidence": 0.84,
        },
    },
]


def current_timestamp():
  """Return the current timestamp used for DB comparisons."""

  return datetime.datetime.now()


def current_timestamp_text():
  """Return a compact timestamp string for JSON payloads."""

  return current_timestamp().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def connect():
  """Create and return a TimesTen connection."""

  oracledb.init_oracle_client()
  credentials = AccessControl.getCredentials("featureStore.py")
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
  """Create the feature store table and supporting indexes."""

  cursor.execute(CREATE_TABLE)
  print(f"✓ Table {TABLE_NAME} created")
  cursor.execute(CREATE_TENANT_USER_INDEX)
  print("✓ Index IDX_USER_FEATURES_TENANT_USER created")
  cursor.execute(CREATE_FRESHNESS_INDEX)
  print("✓ Index IDX_USER_FEATURES_FRESHNESS created")


def build_feature_key(feature):
  """Build a deterministic feature key."""

  key_text = "|".join([
      feature["tenant_id"],
      feature["user_id"],
      feature["feature_name"],
  ])
  return hashlib.sha256(key_text.encode("utf-8")).hexdigest()


def feature_value_to_json(feature_value):
  """Serialize a feature value payload."""

  return json.dumps(feature_value, separators=(",", ":"))


def audit_payload_to_json(feature, decision, reason):
  """Serialize the personalization audit payload."""

  payload = {
      "tenantId": feature["tenant_id"],
      "userId": feature["user_id"],
      "featureName": feature["feature_name"],
      "modelVersion": feature["model_version"],
      "decision": decision,
      "reason": reason,
      "updatedAt": current_timestamp_text(),
      "simulatedModelCall": True,
  }
  return json.dumps(payload, separators=(",", ":"))


def insert_feature(cursor, feature, freshness_ts, expires_at, audit_payload):
  """Insert or replace a feature record."""

  feature_key = build_feature_key(feature)
  cursor.execute(INSERT_FEATURE, [
      feature_key,
      feature["tenant_id"],
      feature["user_id"],
      feature["feature_name"],
      feature_value_to_json(feature["feature_value"]),
      freshness_ts,
      feature["model_version"],
      audit_payload,
      freshness_ts,
      freshness_ts,
      expires_at,
  ])
  return feature_key


def upsert_feature(cursor, feature):
  """Upsert a fresh feature value for a user."""

  start_time = time.perf_counter()
  freshness_ts = current_timestamp()
  expires_at = freshness_ts + datetime.timedelta(minutes=FEATURE_TTL_MINUTES)
  audit_payload = audit_payload_to_json(feature, feature["decision"], "fresh_feature_upsert")
  # Each upsert represents the latest signal the app wants to keep available now.
  insert_feature(cursor, feature, freshness_ts, expires_at, audit_payload)
  print(
      "→ Feature upsert: "
      f"tenant={feature['tenant_id']} user={feature['user_id']} "
      f"feature={feature['feature_name']} model={feature['model_version']} "
      f"elapsed_ms={(time.perf_counter() - start_time) * 1000:.2f}")


def seed_stale_feature(cursor):
  """Insert one stale feature so cleanup behavior is visible."""

  stale_feature = {
      "tenant_id": "retail_app",
      "user_id": "user_999",
      "feature_name": "cart_value",
      "feature_value": {
          "valueType": "numeric",
          "value": 12,
          "source": "old-events",
          "freshness": "minutes",
      },
      "model_version": "feature-agg-v0",
      "decision": {
          "variant": "standard-shipping",
          "reason": "legacy_state",
          "confidence": 0.5,
      },
  }

  freshness_ts = current_timestamp() - datetime.timedelta(hours=3)
  expires_at = current_timestamp() - datetime.timedelta(minutes=1)
  audit_payload = audit_payload_to_json(stale_feature, stale_feature["decision"], "stale_seed")
  insert_feature(cursor, stale_feature, freshness_ts, expires_at, audit_payload)
  print("✓ Seeded 1 stale feature row")


def fetch_active_features(cursor, tenant_id, user_id):
  """Fetch the current active feature set for a user."""

  cursor.execute(SELECT_ACTIVE_FEATURES, [tenant_id, user_id, current_timestamp()])
  return cursor.fetchall()


def print_feature_summary(cursor):
  """Print summary information for active feature rows."""

  # This summary shows the hot feature set grouped by tenant and user.
  print("⋯ Active feature groups:")
  cursor.execute(SELECT_FEATURE_SUMMARY, [current_timestamp()])
  rows = cursor.fetchall()
  for tenant_id, user_id, feature_count, numeric_sum in rows:
    print(
        f"  tenant={tenant_id:<14} user={user_id:<10} "
        f"features={int(feature_count)} numeric_sum={int(numeric_sum)}")


def print_feature_set(cursor, tenant_id, user_id):
  """Print the current features for a single user."""

  start_time = time.perf_counter()
  rows = fetch_active_features(cursor, tenant_id, user_id)
  print(f"Current features for tenant={tenant_id} user={user_id}:")
  for feature_name, feature_value_json, freshness_ts, model_version, audit_json in rows:
    print(f"  feature={feature_name} freshness={freshness_ts} model={model_version}")
    print(f"    value={feature_value_json}")
    print(f"    audit={audit_json}")
  print(f"  readback_elapsed_ms={(time.perf_counter() - start_time) * 1000:.2f}")


def delete_expired_features(cursor):
  """Delete expired feature rows."""

  cursor.execute(DELETE_EXPIRED, [current_timestamp()])
  deleted = cursor.rowcount if cursor.rowcount is not None else 0
  print(f"✓ Deleted {deleted} expired feature row")


def run():
  """Run the online feature store sample."""

  connection = None
  cursor = None
  try:
    print("=== Feature store demo ===")
    connection = connect()
    cursor = connection.cursor()
    # Rebuild the table so every run starts from the same feature snapshot.
    drop_table(cursor, False)
    create_schema(cursor)
    # Seed a stale row so freshness cleanup is visible in the walkthrough.
    seed_stale_feature(cursor)

    # Upserts simulate fresh feature updates arriving from the application.
    for feature in FEATURE_UPDATES:
      upsert_feature(cursor, feature)

    # Read back the current state before removing what has gone stale.
    print_feature_summary(cursor)
    print_feature_set(cursor, "retail_app", "user_001")
    print_feature_set(cursor, "field_service", "user_204")
    delete_expired_features(cursor)
    drop_table(cursor, True)
    print("✓ Completed feature store sample operations")
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
