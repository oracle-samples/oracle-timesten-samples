--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

REM
REM Grant privilegese for xla demos
REM

GRANT SELECT ON appuser.mydata TO xlauser;
GRANT SELECT ON appuser.mytable TO xlauser;
GRANT SELECT ON appuser.abc TO xlauser;
GRANT SELECT ON appuser.icf TO xlauser;

