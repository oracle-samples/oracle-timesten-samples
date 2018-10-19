#!/bin/bash
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

function main {
  dns
}

function dns {
  # dsn
  firewall-offline-cmd --add-port=53/udp
  firewall-offline-cmd --add-port=53/tcp
  /bin/systemctl restart firewalld
}

main "$@"
