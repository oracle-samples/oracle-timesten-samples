#
# Copyright (c) 2026 Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
#   DESCRIPTION
#     This sample demonstrates how an application can use TimesTen as a fast
#     store for real-time payment authorization state. The application keeps
#     hot authorization records close to the service that needs them, applies
#     deterministic authorization rules, stores the resulting decision and
#     metadata in JSON, and cleans up expired state.
#
#     The sample uses simulated authorization rules. It does not call an
#     external payment gateway, perform fraud-model inference, or depend on an
#     external service.
#
#     The sample performs the following steps:
#       - Creates the 'payment_authorizations' table
#       - Creates indexes for tenant/account, payment id, and expiration lookups
#       - Seeds one expired authorization record
#       - Processes sample payment authorization requests
#       - Shows idempotent replay for a repeated payment request
#       - Stores request and decision metadata in JSON
#       - Summarizes active authorizations by tenant/account/status
#       - Deletes expired authorization records
#       - Drops the table
#
import datetime
import hashlib
import json

import oracledb

import AccessControl

TABLE_NAME = "payment_authorizations"

CREATE_TABLE = f"""
  CREATE TABLE {TABLE_NAME}(
    authorization_key  VARCHAR2(64)   NOT NULL PRIMARY KEY,
    tenant_id          VARCHAR2(30)   NOT NULL,
    account_id         VARCHAR2(30)   NOT NULL,
    merchant_id        VARCHAR2(60)   NOT NULL,
    payment_id         VARCHAR2(40)   NOT NULL,
    amount_cents       TT_INT         NOT NULL,
    currency           VARCHAR2(3)    NOT NULL,
    payment_method     VARCHAR2(20)   NOT NULL,
    risk_score         NUMBER(5,2)    NOT NULL,
    status             VARCHAR2(20)   NOT NULL,
    decision_reason    VARCHAR2(120)  NOT NULL,
    request_payload    JSON,
    decision_payload   JSON,
    created_at         TIMESTAMP      NOT NULL,
    updated_at         TIMESTAMP      NOT NULL,
    expires_at         TIMESTAMP      NOT NULL)
"""

CREATE_TENANT_ACCOUNT_INDEX = f"""
  CREATE INDEX idx_payment_auth_tenant_account
  ON {TABLE_NAME} (tenant_id, account_id)
"""

CREATE_PAYMENT_ID_INDEX = f"""
  CREATE INDEX idx_payment_auth_payment_id
  ON {TABLE_NAME} (payment_id)
"""

CREATE_EXPIRES_INDEX = f"""
  CREATE INDEX idx_payment_auth_expires
  ON {TABLE_NAME} (expires_at)
"""

INSERT_AUTHORIZATION = f"""
  INSERT INTO {TABLE_NAME}(
    authorization_key, tenant_id, account_id, merchant_id, payment_id,
    amount_cents, currency, payment_method, risk_score, status,
    decision_reason, request_payload, decision_payload, created_at,
    updated_at, expires_at)
  VALUES (:1, :2, :3, :4, :5, :6, :7, :8, :9, :10, :11, :12, :13, :14, :15, :16)
"""

SELECT_EXISTING_AUTHORIZATION = f"""
  SELECT status,
         decision_reason,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS')
  FROM {TABLE_NAME}
  WHERE authorization_key = :1
    AND expires_at > :2
"""

SELECT_ACTIVE_SUMMARY = f"""
  SELECT tenant_id,
         account_id,
         status,
         COUNT(*),
         SUM(amount_cents)
  FROM {TABLE_NAME}
  WHERE expires_at > :1
  GROUP BY tenant_id, account_id, status
  ORDER BY tenant_id, account_id, status
"""

SELECT_ACTIVE_DETAILS = f"""
  SELECT payment_id,
         merchant_id,
         status,
         decision_reason,
         amount_cents,
         risk_score,
         TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS')
  FROM {TABLE_NAME}
  WHERE expires_at > :1
  ORDER BY tenant_id, account_id, payment_id
"""

DELETE_EXPIRED = f"""
  DELETE FROM {TABLE_NAME}
  WHERE expires_at <= :1
"""

DROP_TABLE = f"DROP TABLE {TABLE_NAME}"

PAYMENT_REQUESTS = [
    {
        "tenant_id": "retail_app",
        "account_id": "acct_1001",
        "merchant_id": "orchard-books",
        "payment_id": "pay_1001",
        "amount_cents": 4995,
        "currency": "USD",
        "payment_method": "debit_card",
        "risk_score": 0.12,
        "spend_limit_cents": 25000,
        "risk_threshold": 0.75,
        "hold_minutes": 15,
    },
    {
        # Replay of the first request to demonstrate idempotent lookup.
        "tenant_id": "retail_app",
        "account_id": "acct_1001",
        "merchant_id": "orchard-books",
        "payment_id": "pay_1001",
        "amount_cents": 4995,
        "currency": "USD",
        "payment_method": "debit_card",
        "risk_score": 0.12,
        "spend_limit_cents": 25000,
        "risk_threshold": 0.75,
        "hold_minutes": 15,
    },
    {
        "tenant_id": "retail_app",
        "account_id": "acct_1002",
        "merchant_id": "pro-office-supplies",
        "payment_id": "pay_2001",
        "amount_cents": 39900,
        "currency": "USD",
        "payment_method": "credit_card",
        "risk_score": 0.08,
        "spend_limit_cents": 25000,
        "risk_threshold": 0.75,
        "hold_minutes": 10,
    },
    {
        "tenant_id": "field_service",
        "account_id": "acct_2001",
        "merchant_id": "route-parts",
        "payment_id": "pay_3001",
        "amount_cents": 14900,
        "currency": "USD",
        "payment_method": "mobile_wallet",
        "risk_score": 0.87,
        "spend_limit_cents": 20000,
        "risk_threshold": 0.75,
        "hold_minutes": 20,
    },
]

EXPIRED_AUTHORIZATION = {
    "tenant_id": "retail_app",
    "account_id": "acct_9999",
    "merchant_id": "legacy-outlet",
    "payment_id": "pay_expired_0001",
    "amount_cents": 2599,
    "currency": "USD",
    "payment_method": "debit_card",
    "risk_score": 0.18,
    "spend_limit_cents": 15000,
    "risk_threshold": 0.70,
    "hold_minutes": 5,
}


def current_timestamp():
  """Return the current timestamp used for database comparisons."""

  return datetime.datetime.now()


def current_timestamp_text():
  """Return a compact timestamp string for JSON payloads."""

  return current_timestamp().isoformat(timespec="seconds")


def format_money(amount_cents):
  """Format an amount stored as cents for display."""

  return f"${amount_cents / 100:.2f}"


def connect():
  """Create and return a TimesTen connection."""

  oracledb.init_oracle_client()
  credentials = AccessControl.getCredentials("paymentAuthorizationState.py")
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
  """Create the payment authorization table and supporting indexes."""

  cursor.execute(CREATE_TABLE)
  print(f"Table {TABLE_NAME} created")
  cursor.execute(CREATE_TENANT_ACCOUNT_INDEX)
  print("Index IDX_PAYMENT_AUTH_TENANT_ACCOUNT created")
  cursor.execute(CREATE_PAYMENT_ID_INDEX)
  print("Index IDX_PAYMENT_AUTH_PAYMENT_ID created")
  cursor.execute(CREATE_EXPIRES_INDEX)
  print("Index IDX_PAYMENT_AUTH_EXPIRES created")


def build_authorization_key(payment):
  """Build a deterministic authorization key."""

  key_text = "|".join([
      payment["tenant_id"],
      payment["account_id"],
      payment["merchant_id"],
      payment["payment_id"],
  ])
  return hashlib.sha256(key_text.encode("utf-8")).hexdigest()


def request_payload_to_json(payment):
  """Serialize the request payload."""

  payload = {
      "tenantId": payment["tenant_id"],
      "accountId": payment["account_id"],
      "merchantId": payment["merchant_id"],
      "paymentId": payment["payment_id"],
      "amountCents": payment["amount_cents"],
      "currency": payment["currency"],
      "paymentMethod": payment["payment_method"],
      "riskScore": payment["risk_score"],
      "spendLimitCents": payment["spend_limit_cents"],
      "riskThreshold": payment["risk_threshold"],
  }
  return json.dumps(payload, separators=(",", ":"))


def decision_payload_to_json(payment, status, reason, expires_at_text):
  """Serialize the authorization decision payload."""

  payload = {
      "tenantId": payment["tenant_id"],
      "accountId": payment["account_id"],
      "merchantId": payment["merchant_id"],
      "paymentId": payment["payment_id"],
      "decision": status,
      "reason": reason,
      "holdMinutes": payment["hold_minutes"],
      "expiresAt": expires_at_text,
      "ruleVersion": "payment-auth-rules-v1",
      "simulatedRiskService": True,
  }
  return json.dumps(payload, separators=(",", ":"))


def evaluate_payment(payment):
  """Apply deterministic authorization rules to a payment request."""

  if payment["amount_cents"] > payment["spend_limit_cents"]:
    status = "DECLINED"
    reason = "amount_exceeds_limit"
    ttl_minutes = 10
  elif payment["risk_score"] >= payment["risk_threshold"]:
    status = "REVIEW"
    reason = "risk_score_requires_review"
    ttl_minutes = 20
  else:
    status = "APPROVED"
    reason = "within_limit_and_low_risk"
    ttl_minutes = payment["hold_minutes"]

  expires_at = current_timestamp() + datetime.timedelta(minutes=ttl_minutes)
  return status, reason, expires_at


def seed_expired_authorization(cursor):
  """Insert one expired authorization row so cleanup behavior is visible."""

  auth_key = build_authorization_key(EXPIRED_AUTHORIZATION)
  now = current_timestamp()
  expired_at = now - datetime.timedelta(minutes=1)
  request_payload = request_payload_to_json(EXPIRED_AUTHORIZATION)
  decision_payload = decision_payload_to_json(
      EXPIRED_AUTHORIZATION,
      "EXPIRED",
      "seeded_expired_state",
      expired_at.isoformat(timespec="seconds"))

  cursor.execute(
      INSERT_AUTHORIZATION,
      (
          auth_key,
          EXPIRED_AUTHORIZATION["tenant_id"],
          EXPIRED_AUTHORIZATION["account_id"],
          EXPIRED_AUTHORIZATION["merchant_id"],
          EXPIRED_AUTHORIZATION["payment_id"],
          EXPIRED_AUTHORIZATION["amount_cents"],
          EXPIRED_AUTHORIZATION["currency"],
          EXPIRED_AUTHORIZATION["payment_method"],
          EXPIRED_AUTHORIZATION["risk_score"],
          "EXPIRED",
          "seeded_expired_state",
          request_payload,
          decision_payload,
          now,
          now,
          expired_at,
      ))
  print("Seeded 1 expired authorization record")


def lookup_existing_authorization(cursor, auth_key, now):
  """Return an active authorization if one already exists."""

  cursor.execute(SELECT_EXISTING_AUTHORIZATION, (auth_key, now))
  return cursor.fetchone()


def store_authorization(cursor, payment, status, reason, expires_at):
  """Persist a new authorization decision."""

  now = current_timestamp()
  auth_key = build_authorization_key(payment)
  request_payload = request_payload_to_json(payment)
  decision_payload = decision_payload_to_json(
      payment, status, reason, expires_at.isoformat(timespec="seconds"))

  cursor.execute(
      INSERT_AUTHORIZATION,
      (
          auth_key,
          payment["tenant_id"],
          payment["account_id"],
          payment["merchant_id"],
          payment["payment_id"],
          payment["amount_cents"],
          payment["currency"],
          payment["payment_method"],
          payment["risk_score"],
          status,
          reason,
          request_payload,
          decision_payload,
          now,
          now,
          expires_at,
      ))
  return auth_key


def authorize_payment(cursor, payment):
  """Authorize a payment request, returning an idempotent stored decision."""

  now = current_timestamp()
  auth_key = build_authorization_key(payment)
  existing = lookup_existing_authorization(cursor, auth_key, now)
  if existing is not None:
    status, reason, expires_at_text = existing
    print(
        "AUTH REPLAY "
        f"tenant={payment['tenant_id']} account={payment['account_id']} "
        f"merchant={payment['merchant_id']} payment_id={payment['payment_id']} "
        f"status={status} reason={reason} expires_at={expires_at_text}")
    return status

  status, reason, expires_at = evaluate_payment(payment)
  store_authorization(cursor, payment, status, reason, expires_at)
  print(
      "AUTH DECISION "
      f"tenant={payment['tenant_id']} account={payment['account_id']} "
      f"merchant={payment['merchant_id']} payment_id={payment['payment_id']} "
      f"status={status} amount={format_money(payment['amount_cents'])} "
      f"risk={payment['risk_score']:.2f} reason={reason} "
      f"hold_expires={expires_at.isoformat(timespec='seconds')}")
  return status


def summarize_active_authorizations(cursor):
  """Print a compact summary of active authorizations."""

  print("Active authorizations by tenant/account/status:")
  cursor.execute(SELECT_ACTIVE_SUMMARY, (current_timestamp(),))
  for tenant_id, account_id, status, row_count, amount_cents in cursor:
    print(
        f"  tenant={tenant_id:<12} account={account_id:<10} "
        f"status={status:<8} rows={row_count:<2} total={format_money(amount_cents or 0)}")

  print("Active authorization details:")
  cursor.execute(SELECT_ACTIVE_DETAILS, (current_timestamp(),))
  for payment_id, merchant_id, status, reason, amount_cents, risk_score, expires_at_text in cursor:
    print(
        f"  payment_id={payment_id:<10} merchant={merchant_id:<20} "
        f"status={status:<8} amount={format_money(amount_cents):<8} "
        f"risk={risk_score:.2f} reason={reason:<28} expires_at={expires_at_text}")


def cleanup_expired_authorizations(cursor):
  """Delete expired authorization records."""

  now = current_timestamp()
  cursor.execute(DELETE_EXPIRED, (now,))
  print(f"Deleted {cursor.rowcount} expired authorization record" +
        ("" if cursor.rowcount == 1 else "s"))


def run():
  """Run the sample."""

  connection = connect()
  cursor = connection.cursor()

  try:
    drop_table(cursor, False)
    create_schema(cursor)
    seed_expired_authorization(cursor)

    for payment in PAYMENT_REQUESTS:
      authorize_payment(cursor, payment)

    summarize_active_authorizations(cursor)
    cleanup_expired_authorizations(cursor)
    drop_table(cursor, False)
    print("Connection has been released")
  finally:
    cursor.close()
    connection.close()


if __name__ == "__main__":
  run()
