#
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
# DESCRIPTION
#   Demonstrates TimesTen JSON features using the python-oracledb driver.
#
"""Sample script demonstrating TimesTen JSON functionality via python-oracledb."""

import datetime
import os
from pathlib import Path
from typing import Tuple

import oracledb

import AccessControl

oracledb.init_oracle_client()

TABLE_NAME = "j_purchaseorder"
JSON_DOC1_PATH = os.path.join(
    "..",
    "..",
    "quickstart",
    "sample_code",
    "common",
    "jsondoc1.json",
)
JSON_DOC1_V2_PATH = os.path.join(
    "..",
    "..",
    "quickstart",
    "sample_code",
    "common",
    "jsondoc1-v2.json",
)
JSON_DOC2_PATH = os.path.join(
    "..",
    "..",
    "quickstart",
    "sample_code",
    "common",
    "jsondoc2.json",
)


def _current_timestamp() -> datetime.datetime:
    """Return the current timestamp with UTC timezone information."""

    return datetime.datetime.now(datetime.timezone.utc)


def locate_json(relative_path: str) -> str:
    """Resolve and return an accessible path for the given JSON file."""

    script_dir = Path(__file__).resolve().parent
    candidate = (script_dir / relative_path).resolve()
    if candidate.exists():
        return str(candidate)

    fallback = script_dir / Path(relative_path).name
    if fallback.exists():
        return str(fallback)

    absolute = Path(relative_path).expanduser().resolve()
    if absolute.exists():
        return str(absolute)

    raise FileNotFoundError(relative_path)


def read_json(relative_path: str) -> Tuple[str, str]:
    """Read and return the JSON contents and associated resolved path."""

    resolved_path = locate_json(relative_path)
    with open(resolved_path, "r", encoding="utf-8") as json_file:
        return json_file.read(), resolved_path


def drop_table(cursor, report_missing: bool) -> None:
    """Drop the purchase order table if it exists."""

    try:
        cursor.execute(f"DROP TABLE {TABLE_NAME}")
        print(f"Table {TABLE_NAME} dropped")
    except Exception as err:
        if report_missing:
            print(f"Table {TABLE_NAME} not dropped: {err}")


def create_table(cursor) -> None:
    """Create the JSON-enabled purchase order table."""

    ddl = (
        f"CREATE TABLE {TABLE_NAME} "
        "(id VARCHAR2(32) NOT NULL PRIMARY KEY, "
        "date_loaded TIMESTAMP, "
        "po_document JSON)"
    )
    cursor.execute(ddl)
    print(f"Table {TABLE_NAME} created")


def insert_row(cursor, po_id: str, json_text: str) -> None:
    """Insert a purchase order JSON document into the table."""

    sql = (
        f"INSERT INTO {TABLE_NAME} (id, date_loaded, po_document) "
        "VALUES (:1, :2, :3)"
    )
    cursor.execute(sql, [po_id, _current_timestamp(), json_text])
    print(f"Inserted purchase order with id {po_id}")


def update_row(cursor, po_id: str, json_text: str, source_path: str) -> None:
    """Update an existing purchase order JSON document with new content."""

    sql = (
        f"UPDATE {TABLE_NAME} SET date_loaded = :1, po_document = :2 "
        "WHERE id = :3"
    )
    cursor.execute(sql, [_current_timestamp(), json_text, po_id])
    if cursor.rowcount is not None and cursor.rowcount > 0:
        print(
            f"Updated purchase order with id {po_id} "
            f"using {os.path.basename(source_path)}"
        )
    else:
        print(f"No rows updated for id {po_id}")


def create_json_index(cursor) -> None:
    """Create an index that accelerates queries on the JSON `User` attribute."""

    sql = (
        f"CREATE INDEX idx_json_user ON {TABLE_NAME} "
        "(JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128) ERROR ON ERROR))"
    )
    cursor.execute(sql)
    print("JSON index IDX_JSON_USER created")


def select_po_document_by_id(cursor, po_id: str) -> None:
    """Print the JSON document for a purchase order by identifier."""

    sql = (
        f"SELECT JSON_SERIALIZE(po_document PRETTY) FROM {TABLE_NAME} "
        "WHERE id = :1"
    )
    cursor.execute(sql, [po_id])
    row = cursor.fetchone()
    if row:
        print(f"Purchase order for id {po_id}:")
        print(row[0])
    else:
        print(f"No purchase order found for id {po_id}")


def select_po_documents_by_user(cursor, user: str) -> None:
    """Print all purchase orders associated with a given user name."""

    sql = (
        f"SELECT JSON_SERIALIZE(po_document PRETTY) FROM {TABLE_NAME} "
        "WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = :1"
    )
    cursor.execute(sql, [user])
    rows = cursor.fetchall()
    if rows:
        print(f"Purchase orders for user {user}:")
        for json_doc, in rows:
            print(json_doc)
    else:
        print(f"No purchase orders found for user {user}")


def select_line_items_for_order(cursor, po_id: str) -> None:
    """Print a tabular view of all line items for a purchase order."""

    sql = (
        "SELECT jt.line_number, jt.sku, jt.description, jt.quantity, jt.unit_price "
        f"FROM {TABLE_NAME} po, "
        "JSON_TABLE(po.po_document, '$.LineItems[*]' COLUMNS ("
        "line_number FOR ORDINALITY, "
        "sku VARCHAR2(40) PATH '$.Part.UPCCode', "
        "description VARCHAR2(256) PATH '$.Part.Description', "
        "quantity NUMBER PATH '$.Quantity', "
        "unit_price NUMBER PATH '$.Part.UnitPrice')"
        ") jt WHERE po.id = :1"
    )
    cursor.execute(sql, [po_id])
    rows = cursor.fetchall()
    if rows:
        print(f"Line items for purchase order {po_id}:")
        print(
            "Line  SKU           Description                     "
            "Qty  Unit Price  Extended"
        )
        for line_number, sku, description, quantity, unit_price in rows:
            extended = None
            if quantity is not None and unit_price is not None:
                extended = quantity * unit_price
            print(
                f"{int(line_number):4d}  {sku:<12} {description:<30} "
                f"{quantity:4.0f}  {unit_price:10.2f}  {extended:8.2f}"
            )
    else:
        print(f"No line items found for purchase order {po_id}")


def run() -> None:
    """Execute the sample workflow end-to-end."""

    connection = None
    cursor = None
    try:
        credentials = AccessControl.getCredentials("jsonSample.py")
        connection = oracledb.connect(
            user=credentials.user,
            password=credentials.password,
            dsn=credentials.connstr,
        )
        connection.autocommit = True
        cursor = connection.cursor()

        drop_table(cursor, False)
        create_table(cursor)

        json_doc1, json_doc1_path = read_json(JSON_DOC1_PATH)
        json_doc1_v2, json_doc1_v2_path = read_json(JSON_DOC1_V2_PATH)
        json_doc2, _ = read_json(JSON_DOC2_PATH)

        insert_row(cursor, "1600", json_doc1)
        insert_row(cursor, "1721", json_doc2)

        create_json_index(cursor)

        update_row(cursor, "1600", json_doc1_v2, json_doc1_v2_path)

        select_po_document_by_id(cursor, "1600")
        select_po_document_by_id(cursor, "1721")

        select_po_documents_by_user(cursor, "ABULL")
        select_po_documents_by_user(cursor, "CGIRAFFE")
        select_line_items_for_order(cursor, "1600")

        drop_table(cursor, True)
    except FileNotFoundError as err:
        print(f"Unable to locate JSON document: {err}")
    except oracledb.DatabaseError as err:
        error_obj, = err.args
        print(f"SQLState: {getattr(error_obj, 'sqlstate', 'N/A')}")
        print(f"ErrorCode: {getattr(error_obj, 'code', 'N/A')}")
        print(f"Message: {getattr(error_obj, 'message', err)}")
    except Exception as err:
        print(f"An unexpected error occurred: {err}")
    finally:
        if cursor is not None:
            try:
                cursor.close()
            except Exception:
                pass
        if connection is not None:
            try:
                connection.close()
            except Exception:
                pass


if __name__ == "__main__":
    run()
