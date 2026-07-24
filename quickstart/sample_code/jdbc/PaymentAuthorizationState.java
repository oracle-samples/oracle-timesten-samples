/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 *
 * Licensed under the Universal Permissive License v 1.0 as shown
 * at http://oss.oracle.com/licenses/upl
 *
 * DESCRIPTION
 *   Demonstrates how an application can use TimesTen as a fast store for
 *   real-time payment authorization state. The application keeps hot
 *   authorization decisions close to the service that needs them, applies
 *   deterministic authorization rules, stores the resulting decision and
 *   metadata in JSON, and cleans up expired state.
 *
 *   The sample uses simulated authorization rules. It does not call an
 *   external payment gateway, perform fraud-model inference, or depend on an
 *   external service.
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

/**
 * Provides a JDBC-based demonstration of a real-time payment authorization
 * state store backed by TimesTen.
 */
public class PaymentAuthorizationState
{
  private static final String PROGRAM_NAME = "PaymentAuthorizationState";
  private static final String DEFAULT_USERNAME = "appuser";

  private static final String TIMESTEN_DIRECT_DRIVER = "com.timesten.jdbc.TimesTenDriver";
  private static final String TIMESTEN_CLIENT_DRIVER = "com.timesten.jdbc.TimesTenClientDriver";

  private static final String DIRECT_URL_PREFIX = "jdbc:timesten:direct:";
  private static final String CLIENT_URL_PREFIX = "jdbc:timesten:client:";

  private static final String TABLE_NAME = "payment_authorizations";

  private static final String CREATE_TABLE_SQL =
    "CREATE TABLE " + TABLE_NAME + " ("
    + "authorization_key  VARCHAR2(64)   NOT NULL PRIMARY KEY, "
    + "tenant_id          VARCHAR2(30)   NOT NULL, "
    + "account_id         VARCHAR2(30)   NOT NULL, "
    + "merchant_id        VARCHAR2(60)   NOT NULL, "
    + "payment_id         VARCHAR2(40)   NOT NULL, "
    + "amount_cents       TT_INT         NOT NULL, "
    + "currency           VARCHAR2(3)    NOT NULL, "
    + "payment_method     VARCHAR2(20)   NOT NULL, "
    + "risk_score         NUMBER(5,2)    NOT NULL, "
    + "status             VARCHAR2(20)   NOT NULL, "
    + "decision_reason    VARCHAR2(120)  NOT NULL, "
    + "request_payload    JSON, "
    + "decision_payload   JSON, "
    + "created_at         TIMESTAMP      NOT NULL, "
    + "updated_at         TIMESTAMP      NOT NULL, "
    + "expires_at         TIMESTAMP      NOT NULL)";

  private static final String CREATE_TENANT_ACCOUNT_INDEX_SQL =
    "CREATE INDEX idx_payment_auth_tenant_account "
    + "ON " + TABLE_NAME + " (tenant_id, account_id)";

  private static final String CREATE_PAYMENT_ID_INDEX_SQL =
    "CREATE INDEX idx_payment_auth_payment_id "
    + "ON " + TABLE_NAME + " (payment_id)";

  private static final String CREATE_EXPIRES_INDEX_SQL =
    "CREATE INDEX idx_payment_auth_expires "
    + "ON " + TABLE_NAME + " (expires_at)";

  private static final String INSERT_AUTHORIZATION_SQL =
    "INSERT INTO " + TABLE_NAME + " ("
    + "authorization_key, tenant_id, account_id, merchant_id, payment_id, "
    + "amount_cents, currency, payment_method, risk_score, status, "
    + "decision_reason, request_payload, decision_payload, created_at, "
    + "updated_at, expires_at) "
    + "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  private static final String SELECT_EXISTING_AUTHORIZATION_SQL =
    "SELECT status, decision_reason, TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS') "
    + "FROM " + TABLE_NAME + " "
    + "WHERE authorization_key = ? "
    + "  AND expires_at > ?";

  private static final String SELECT_ACTIVE_SUMMARY_SQL =
    "SELECT tenant_id, account_id, status, COUNT(*), SUM(amount_cents) "
    + "FROM " + TABLE_NAME + " "
    + "WHERE expires_at > ? "
    + "GROUP BY tenant_id, account_id, status "
    + "ORDER BY tenant_id, account_id, status";

  private static final String SELECT_ACTIVE_DETAILS_SQL =
    "SELECT payment_id, merchant_id, status, decision_reason, amount_cents, risk_score, "
    + "       TO_CHAR(expires_at, 'YYYY-MM-DD HH24:MI:SS') "
    + "FROM " + TABLE_NAME + " "
    + "WHERE expires_at > ? "
    + "ORDER BY tenant_id, account_id, payment_id";

  private static final String DELETE_EXPIRED_SQL =
    "DELETE FROM " + TABLE_NAME + " WHERE expires_at <= ?";

  private static final String DROP_TABLE_SQL =
    "DROP TABLE " + TABLE_NAME;

  private static final PaymentRequest[] PAYMENT_REQUESTS = new PaymentRequest[] {
    new PaymentRequest("retail_app", "acct_1001", "orchard-books", "pay_1001",
                       4995, "USD", "debit_card", 0.12, 25000, 0.75, 15),
    new PaymentRequest("retail_app", "acct_1001", "orchard-books", "pay_1001",
                       4995, "USD", "debit_card", 0.12, 25000, 0.75, 15),
    new PaymentRequest("retail_app", "acct_1002", "pro-office-supplies", "pay_2001",
                       39900, "USD", "credit_card", 0.08, 25000, 0.75, 10),
    new PaymentRequest("field_service", "acct_2001", "route-parts", "pay_3001",
                       14900, "USD", "mobile_wallet", 0.87, 20000, 0.75, 20)
  };

  private static final PaymentRequest EXPIRED_AUTHORIZATION =
    new PaymentRequest("retail_app", "acct_9999", "legacy-outlet", "pay_expired_0001",
                       2599, "USD", "debit_card", 0.18, 15000, 0.70, 5);

  private final IOLibrary ioLibrary;

  private static final class PaymentRequest
  {
    private final String tenantId;
    private final String accountId;
    private final String merchantId;
    private final String paymentId;
    private final int amountCents;
    private final String currency;
    private final String paymentMethod;
    private final double riskScore;
    private final int spendLimitCents;
    private final double riskThreshold;
    private final int holdMinutes;

    PaymentRequest(String tenantId, String accountId, String merchantId, String paymentId,
                   int amountCents, String currency, String paymentMethod, double riskScore,
                   int spendLimitCents, double riskThreshold, int holdMinutes)
    {
      this.tenantId = tenantId;
      this.accountId = accountId;
      this.merchantId = merchantId;
      this.paymentId = paymentId;
      this.amountCents = amountCents;
      this.currency = currency;
      this.paymentMethod = paymentMethod;
      this.riskScore = riskScore;
      this.spendLimitCents = spendLimitCents;
      this.riskThreshold = riskThreshold;
      this.holdMinutes = holdMinutes;
    }
  }

  private static final class AuthorizationDecision
  {
    private final String status;
    private final String reason;
    private final int ttlMinutes;

    AuthorizationDecision(String status, String reason, int ttlMinutes)
    {
      this.status = status;
      this.reason = reason;
      this.ttlMinutes = ttlMinutes;
    }
  }

  public PaymentAuthorizationState()
  {
    ioLibrary = new IOLibrary(System.err);
  }

  public static void main(String[] args)
  {
    int exitCode = new PaymentAuthorizationState().run(args);
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
      System.out.println("Completed payment authorization sample operations");
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
    createTable(connection);
    seedExpiredAuthorization(connection);

    for (PaymentRequest request : PAYMENT_REQUESTS)
    {
      authorizePayment(connection, request);
    }

    summarizeActiveAuthorizations(connection);
    cleanupExpiredAuthorizations(connection);
  }

  private void createTable(Connection connection) throws SQLException
  {
    executeStatement(connection, CREATE_TABLE_SQL);
    System.out.println("Table " + TABLE_NAME + " created");
    executeStatement(connection, CREATE_TENANT_ACCOUNT_INDEX_SQL);
    System.out.println("Index IDX_PAYMENT_AUTH_TENANT_ACCOUNT created");
    executeStatement(connection, CREATE_PAYMENT_ID_INDEX_SQL);
    System.out.println("Index IDX_PAYMENT_AUTH_PAYMENT_ID created");
    executeStatement(connection, CREATE_EXPIRES_INDEX_SQL);
    System.out.println("Index IDX_PAYMENT_AUTH_EXPIRES created");
  }

  private void seedExpiredAuthorization(Connection connection) throws SQLException
  {
    PaymentRequest request = EXPIRED_AUTHORIZATION;
    Timestamp now = currentTimestamp();
    Timestamp expiredAt = new Timestamp(now.getTime() - 60_000L);
    String authorizationKey = buildAuthorizationKey(request);
    String requestPayload = requestPayloadToJson(request);
    String decisionPayload = decisionPayloadToJson(request, "EXPIRED", "seeded_expired_state",
                                                   expiredAt.toString());

    try (PreparedStatement statement = connection.prepareStatement(INSERT_AUTHORIZATION_SQL))
    {
      bindAuthorization(statement, authorizationKey, request, "EXPIRED", "seeded_expired_state",
                        requestPayload, decisionPayload, now, now, expiredAt);
      statement.executeUpdate();
    }

    System.out.println("Seeded 1 expired authorization record");
  }

  private String buildAuthorizationKey(PaymentRequest request)
  {
    try
    {
      MessageDigest digest = MessageDigest.getInstance("SHA-256");
      String keyText = request.tenantId + "|" + request.accountId + "|" +
                       request.merchantId + "|" + request.paymentId;
      byte[] hash = digest.digest(keyText.getBytes(StandardCharsets.UTF_8));
      return toHex(hash);
    }
    catch (NoSuchAlgorithmException e)
    {
      throw new IllegalStateException("Unable to create authorization key", e);
    }
  }

  private String requestPayloadToJson(PaymentRequest request)
  {
    return "{"
      + "\"tenantId\":\"" + request.tenantId + "\","
      + "\"accountId\":\"" + request.accountId + "\","
      + "\"merchantId\":\"" + request.merchantId + "\","
      + "\"paymentId\":\"" + request.paymentId + "\","
      + "\"amountCents\":" + request.amountCents + ","
      + "\"currency\":\"" + request.currency + "\","
      + "\"paymentMethod\":\"" + request.paymentMethod + "\","
      + "\"riskScore\":" + request.riskScore + ","
      + "\"spendLimitCents\":" + request.spendLimitCents + ","
      + "\"riskThreshold\":" + request.riskThreshold
      + "}";
  }

  private String decisionPayloadToJson(PaymentRequest request, String status,
                                       String reason, String expiresAtText)
  {
    return "{"
      + "\"tenantId\":\"" + request.tenantId + "\","
      + "\"accountId\":\"" + request.accountId + "\","
      + "\"merchantId\":\"" + request.merchantId + "\","
      + "\"paymentId\":\"" + request.paymentId + "\","
      + "\"decision\":\"" + status + "\","
      + "\"reason\":\"" + reason + "\","
      + "\"holdMinutes\":" + request.holdMinutes + ","
      + "\"expiresAt\":\"" + expiresAtText + "\","
      + "\"ruleVersion\":\"payment-auth-rules-v1\","
      + "\"simulatedRiskService\":true"
      + "}";
  }

  private AuthorizationDecision evaluatePayment(PaymentRequest request)
  {
    if (request.amountCents > request.spendLimitCents)
    {
      return new AuthorizationDecision("DECLINED", "amount_exceeds_limit", 10);
    }

    if (request.riskScore >= request.riskThreshold)
    {
      return new AuthorizationDecision("REVIEW", "risk_score_requires_review", 20);
    }

    return new AuthorizationDecision("APPROVED", "within_limit_and_low_risk", request.holdMinutes);
  }

  private void authorizePayment(Connection connection, PaymentRequest request) throws SQLException
  {
    Timestamp now = currentTimestamp();
    String authorizationKey = buildAuthorizationKey(request);

    try (PreparedStatement lookup = connection.prepareStatement(SELECT_EXISTING_AUTHORIZATION_SQL))
    {
      lookup.setString(1, authorizationKey);
      lookup.setTimestamp(2, now);

      try (ResultSet resultSet = lookup.executeQuery())
      {
        if (resultSet.next())
        {
          String status = resultSet.getString(1);
          String reason = resultSet.getString(2);
          String expiresAtText = resultSet.getString(3);
          System.out.println(
              "AUTH REPLAY tenant=" + request.tenantId
              + " account=" + request.accountId
              + " merchant=" + request.merchantId
              + " payment_id=" + request.paymentId
              + " status=" + status
              + " reason=" + reason
              + " expires_at=" + expiresAtText);
          return;
        }
      }
    }

    AuthorizationDecision decision = evaluatePayment(request);
    Timestamp expiresAt = new Timestamp(now.getTime() + decision.ttlMinutes * 60_000L);
    String requestPayload = requestPayloadToJson(request);
    String decisionPayload = decisionPayloadToJson(request, decision.status, decision.reason,
                                                   expiresAt.toString());

    try (PreparedStatement insert = connection.prepareStatement(INSERT_AUTHORIZATION_SQL))
    {
      bindAuthorization(insert, authorizationKey, request, decision.status, decision.reason,
                        requestPayload, decisionPayload, now, now, expiresAt);
      insert.executeUpdate();
    }

    System.out.println(
        "AUTH DECISION tenant=" + request.tenantId
        + " account=" + request.accountId
        + " merchant=" + request.merchantId
        + " payment_id=" + request.paymentId
        + " status=" + decision.status
        + " amount=" + formatMoney(request.amountCents)
        + " risk=" + String.format("%.2f", request.riskScore)
        + " reason=" + decision.reason
        + " hold_expires=" + expiresAt.toString());
  }

  private void bindAuthorization(PreparedStatement statement, String authorizationKey,
                                 PaymentRequest request, String status, String decisionReason,
                                 String requestPayload, String decisionPayload,
                                 Timestamp createdAt, Timestamp updatedAt, Timestamp expiresAt)
      throws SQLException
  {
    statement.setString(1, authorizationKey);
    statement.setString(2, request.tenantId);
    statement.setString(3, request.accountId);
    statement.setString(4, request.merchantId);
    statement.setString(5, request.paymentId);
    statement.setInt(6, request.amountCents);
    statement.setString(7, request.currency);
    statement.setString(8, request.paymentMethod);
    statement.setDouble(9, request.riskScore);
    statement.setString(10, status);
    statement.setString(11, decisionReason);
    statement.setString(12, requestPayload);
    statement.setString(13, decisionPayload);
    statement.setTimestamp(14, createdAt);
    statement.setTimestamp(15, updatedAt);
    statement.setTimestamp(16, expiresAt);
  }

  private void summarizeActiveAuthorizations(Connection connection) throws SQLException
  {
    System.out.println("Active authorizations by tenant/account/status:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_SUMMARY_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      try (ResultSet resultSet = statement.executeQuery())
      {
        while (resultSet.next())
        {
          String tenantId = resultSet.getString(1);
          String accountId = resultSet.getString(2);
          String status = resultSet.getString(3);
          long rowCount = resultSet.getLong(4);
          long amountCents = resultSet.getLong(5);
          System.out.println(
              "  tenant=" + padRight(tenantId, 12)
              + " account=" + padRight(accountId, 10)
              + " status=" + padRight(status, 8)
              + " rows=" + padRight(Long.toString(rowCount), 2)
              + " total=" + formatMoney(amountCents));
        }
      }
    }

    System.out.println("Active authorization details:");
    try (PreparedStatement statement = connection.prepareStatement(SELECT_ACTIVE_DETAILS_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      try (ResultSet resultSet = statement.executeQuery())
      {
        while (resultSet.next())
        {
          String paymentId = resultSet.getString(1);
          String merchantId = resultSet.getString(2);
          String status = resultSet.getString(3);
          String reason = resultSet.getString(4);
          int amountCents = resultSet.getInt(5);
          double riskScore = resultSet.getDouble(6);
          String expiresAtText = resultSet.getString(7);
          System.out.println(
              "  payment_id=" + padRight(paymentId, 10)
              + " merchant=" + padRight(merchantId, 20)
              + " status=" + padRight(status, 8)
              + " amount=" + padRight(formatMoney(amountCents), 8)
              + " risk=" + String.format("%.2f", riskScore)
              + " reason=" + padRight(reason, 28)
              + " expires_at=" + expiresAtText);
        }
      }
    }
  }

  private void cleanupExpiredAuthorizations(Connection connection) throws SQLException
  {
    try (PreparedStatement statement = connection.prepareStatement(DELETE_EXPIRED_SQL))
    {
      statement.setTimestamp(1, currentTimestamp());
      int rows = statement.executeUpdate();
      System.out.println("Deleted " + rows + " expired authorization record" + (rows == 1 ? "" : "s"));
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

  private String formatMoney(long amountCents)
  {
    return formatMoney((int) amountCents);
  }

  private String formatMoney(int amountCents)
  {
    return String.format("$%.2f", amountCents / 100.0);
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
