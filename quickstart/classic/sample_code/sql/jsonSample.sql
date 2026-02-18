Rem --------------------------------------------------------------------
Rem Setup: reset purchase order table
Rem --------------------------------------------------------------------
DROP TABLE j_purchaseorder;

Rem --------------------------------------------------------------------
Rem Setup: create purchase-order table with JSON column
Rem --------------------------------------------------------------------
CREATE TABLE j_purchaseorder
  (id          VARCHAR2(32) NOT NULL PRIMARY KEY,
   date_loaded TIMESTAMP,
   po_document JSON);

Rem --------------------------------------------------------------------
Rem Data load: purchase order 1600 from jsondoc1.json
Rem (ttIsql parameter binding loads JSON via :JSON)
Rem --------------------------------------------------------------------
INSERT INTO j_purchaseorder (id, date_loaded, po_document)
VALUES ('1600', sysdate, :JSON);
@../common/jsondoc1.json

Rem --------------------------------------------------------------------
Rem Data load: purchase order 1721 from jsondoc2.json
Rem (Second sample purchase order document)
Rem --------------------------------------------------------------------
INSERT INTO j_purchaseorder (id, date_loaded, po_document)
VALUES ('1721', sysdate, :JSON);
@../common/jsondoc2.json

Rem --------------------------------------------------------------------
Rem Indexing: cache $.User for fast lookup
Rem (RETURNING type must match later queries)
Rem --------------------------------------------------------------------
CREATE INDEX idx_json_user ON j_purchaseorder
  (JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128) ERROR ON ERROR));

Rem --------------------------------------------------------------------
Rem Query: lookup by user with matching RETURNING type
Rem (Should leverage idx_json_user)
Rem --------------------------------------------------------------------
PROMPT JSON_VALUE returning VARCHAR2 uses index:
SELECT id
  FROM j_purchaseorder
 WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = 'ABULL';

EXPLAIN SELECT id
  FROM j_purchaseorder
 WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = 'ABULL';

Rem --------------------------------------------------------------------
Rem Query: lookup by user with NVARCHAR2 RETURNING type
Rem (Mismatch skips idx_json_user)
Rem --------------------------------------------------------------------
PROMPT JSON_VALUE returning NVARCHAR2 skips index:
SELECT id
  FROM j_purchaseorder
 WHERE JSON_VALUE(po_document, '$.User' RETURNING NVARCHAR2(128)) = 'ABULL';

EXPLAIN SELECT id
  FROM j_purchaseorder
 WHERE JSON_VALUE(po_document, '$.User' RETURNING NVARCHAR2(128)) = 'ABULL';

Rem --------------------------------------------------------------------
Rem Maintenance: update purchase order 1600 with revised JSON
Rem --------------------------------------------------------------------
UPDATE j_purchaseorder
   SET date_loaded = sysdate,
       po_document = :JSON
 WHERE id = '1600';
@../common/jsondoc1-v2.json

Rem --------------------------------------------------------------------
Rem Validation: compare stored JSON with latest file
Rem (JSON_EQUAL confirms persisted document matches new payload)
Rem --------------------------------------------------------------------
PROMPT Confirm purchase order 1600 matches new file:
SELECT JSON_EQUAL(a.po_document, b.po_document) AS docs_equal
  FROM j_purchaseorder a,
       (SELECT JSON(:JSON) po_document FROM dual) b
 WHERE a.id = '1600';
@../common/jsondoc1-v2.json

Rem --------------------------------------------------------------------
Rem Reporting: pretty-print purchase order 1600
Rem --------------------------------------------------------------------
PROMPT Purchase order 1600:
SELECT JSON_SERIALIZE(po_document PRETTY)
  FROM j_purchaseorder
 WHERE id = '1600';

Rem --------------------------------------------------------------------
Rem Reporting: pretty-print purchase order 1721
Rem --------------------------------------------------------------------
PROMPT Purchase order 1721:
SELECT JSON_SERIALIZE(po_document PRETTY)
  FROM j_purchaseorder
 WHERE id = '1721';

Rem --------------------------------------------------------------------
Rem Query: retrieve all orders for user ABULL
Rem --------------------------------------------------------------------
PROMPT Purchase orders for ABULL:
SELECT JSON_SERIALIZE(po_document PRETTY)
  FROM j_purchaseorder
 WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = 'ABULL';

Rem --------------------------------------------------------------------
Rem Query: retrieve all orders for user CGIRAFFE
Rem --------------------------------------------------------------------
PROMPT Purchase orders for CGIRAFFE:
SELECT JSON_SERIALIZE(po_document PRETTY)
  FROM j_purchaseorder
 WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = 'CGIRAFFE';

Rem --------------------------------------------------------------------
Rem Detail: expand line items for PO 1600
Rem (JSON_TABLE pivots $.LineItems array into relational rows)
Rem --------------------------------------------------------------------
PROMPT Line items for PO 1600:
SELECT jt.line_number,
       jt.sku,
       jt.description,
       jt.quantity,
       jt.unit_price,
       jt.quantity * jt.unit_price AS extended
  FROM j_purchaseorder po,
       JSON_TABLE(po.po_document, '$.LineItems[*]'
         COLUMNS (
           line_number FOR ORDINALITY,
           sku         VARCHAR2(40)  PATH '$.Part.UPCCode',
           description VARCHAR2(256) PATH '$.Part.Description',
           quantity    NUMBER        PATH '$.Quantity',
           unit_price  NUMBER        PATH '$.Part.UnitPrice'
         )) jt
 WHERE po.id = '1600';

Rem --------------------------------------------------------------------
Rem Filter: detect orders with multiple line items
Rem --------------------------------------------------------------------
PROMPT Purchase orders with more than one line item:
SELECT id
  FROM j_purchaseorder
 WHERE JSON_EXISTS(po_document, '$.LineItems[1]');

Rem --------------------------------------------------------------------
Rem Filter: detect orders listing a mobile phone
Rem --------------------------------------------------------------------
PROMPT Purchase orders listing a Mobile phone:
SELECT id
  FROM j_purchaseorder
 WHERE JSON_EXISTS(po_document,
                   '$.ShippingInstructions.Phone[*]?(@.type == "Mobile")');

Rem --------------------------------------------------------------------
Rem Detail: fetch phone contacts for user ABULL
Rem (JSON_QUERY returns phone array as JSON fragment)
Rem --------------------------------------------------------------------
PROMPT Contact information for ABULL:
SELECT id,
       JSON_QUERY(po_document, '$.ShippingInstructions.Phone') AS phone_numbers
  FROM j_purchaseorder
 WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = 'ABULL';

Rem --------------------------------------------------------------------
Rem Detail: fetch phone contacts for user CGIRAFFE
Rem (JSON_QUERY returns phone array as JSON fragment)
Rem --------------------------------------------------------------------
PROMPT Contact information for CGIRAFFE:
SELECT id,
       JSON_QUERY(po_document, '$.ShippingInstructions.Phone') AS phone_numbers
  FROM j_purchaseorder
 WHERE JSON_VALUE(po_document, '$.User' RETURNING VARCHAR2(128)) = 'CGIRAFFE';

Rem --------------------------------------------------------------------
Rem Comparison: boolean stored in JSON vs SQL string
Rem (JSON_SCALAR retains literal; JSON_VALUE coerces to VARCHAR2)
Rem --------------------------------------------------------------------
PROMPT Allow-partial flag (SQL vs JSON literal):
SELECT po.id,
       JSON_VALUE(po.po_document, '$.AllowPartialShipment' RETURNING VARCHAR2(5)) AS allow_partial_value,
       JSON_SERIALIZE(JSON_SCALAR(JSON_QUERY(po.po_document, '$.AllowPartialShipment'))) AS allow_partial_literal
  FROM j_purchaseorder po
 ORDER BY po.id;

Rem --------------------------------------------------------------------
Rem Comparison: missing vs null instructions
Rem (Shows difference between SQL NULL and JSON literal null)
Rem --------------------------------------------------------------------
PROMPT Special instructions (SQL value vs JSON literal):
SELECT po.id,
       JSON_VALUE(po.po_document, '$."Special Instructions"' RETURNING VARCHAR2(64)) AS special_instr_value,
       JSON_SERIALIZE(JSON_SCALAR(JSON_QUERY(po.po_document, '$."Special Instructions"'))) AS special_instr_literal
  FROM j_purchaseorder po
 ORDER BY po.id;

Rem --------------------------------------------------------------------
Rem Filter: match shipping state using JSON literal comparison
Rem (Avoids double conversion by staying in JSON domain)
Rem --------------------------------------------------------------------
PROMPT Orders shipped to California (JSON literal):
SELECT po.id,
       JSON_VALUE(po.po_document, '$.ShippingInstructions.Address.city' RETURNING VARCHAR2(64)) AS city
  FROM j_purchaseorder po
 WHERE JSON_SERIALIZE(JSON_SCALAR(JSON_QUERY(po.po_document, '$.ShippingInstructions.Address.state'))) = '"CA"'
 ORDER BY po.id;

Rem --------------------------------------------------------------------
Rem Flatten: phone numbers array via JSON_TABLE
Rem --------------------------------------------------------------------
PROMPT Phone numbers across purchase orders:
SELECT po.id,
       phones.phone_type,
       phones.phone_number
  FROM j_purchaseorder po,
       JSON_TABLE(po.po_document, '$.ShippingInstructions.Phone[*]'
         COLUMNS (
           phone_type   VARCHAR2(16) PATH '$.type',
           phone_number VARCHAR2(32) PATH '$.number'
         )) phones
 ORDER BY po.id;

Rem --------------------------------------------------------------------
Rem Defaulting: handle missing order status
Rem --------------------------------------------------------------------
PROMPT Order status defaulted when missing:
SELECT po.id,
       JSON_VALUE(po.po_document,
                  '$.Status'
                  RETURNING VARCHAR2(64)
                  DEFAULT 'PENDING' ON EMPTY
                  NULL ON ERROR) AS order_status
  FROM j_purchaseorder po
 ORDER BY po.id;

Rem --------------------------------------------------------------------
Rem Filter: regex on line item descriptions
Rem --------------------------------------------------------------------
PROMPT Purchase orders containing items with descriptions matching 'Weapon':
SELECT id
  FROM j_purchaseorder
 WHERE JSON_EXISTS(po_document,
                   '$.LineItems[*]?(@.Part.Description like_regex ".*Weapon.*")');

Rem --------------------------------------------------------------------
Rem Error handling: JSON_VALUE numeric conversion failure
Rem (ERROR ON ERROR vs NULL ON ERROR)
Rem --------------------------------------------------------------------
PROMPT JSON_VALUE with ERROR ON ERROR raises conversion failure:
SELECT JSON_VALUE('{"value":"abc"}', '$.value' RETURNING NUMBER ERROR ON ERROR)
  FROM dual;

PROMPT JSON_VALUE with NULL ON ERROR avoids conversion failure:
SELECT JSON_VALUE('{"value":"abc"}', '$.value' RETURNING NUMBER NULL ON ERROR)
  FROM dual;

Rem --------------------------------------------------------------------
Rem Aggregation: sum quantities via JSON_TABLE
Rem --------------------------------------------------------------------
PROMPT Total quantity per purchase order:
SELECT po.id,
       SUM(line_items.quantity) AS total_quantity
  FROM j_purchaseorder po,
       JSON_TABLE(po.po_document, '$.LineItems[*]'
         COLUMNS (
           quantity NUMBER PATH '$.Quantity'
         )) line_items
 GROUP BY po.id
 ORDER BY po.id;

Rem --------------------------------------------------------------------
Rem Teardown: drop purchase-order table
Rem --------------------------------------------------------------------
PROMPT Dropping table j_purchaseorder:
DROP TABLE j_purchaseorder;
