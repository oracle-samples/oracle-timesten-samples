--
-- Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
--
-- Licensed under the Universal Permissive License v 1.0 as shown
-- at http://oss.oracle.com/licenses/upl
--

# hr_idx_tt - Creates the indexes; run this after hr_cre_tt

CREATE INDEX emp_manager_ix
       ON employees (manager_id);
CREATE INDEX emp_name_ix
       ON employees (last_name, first_name);
CREATE INDEX loc_city_ix
       ON locations (city);
CREATE INDEX loc_state_province_ix
       ON locations (state_province);




