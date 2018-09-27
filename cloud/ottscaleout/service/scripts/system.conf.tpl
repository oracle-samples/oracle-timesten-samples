#!/bin/bash
# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

function main {
  doyum
  multivnic
  dns
}

function doyum {
  yum update -y
  #don't need bind unless using terraform NAT model
  #yum install -y yum-plugin-security python-oci-cli bind nc dstat numactl
  yum install -y yum-plugin-security python-oci-cli ansible nc dstat numactl
}

function copyit {
  cp $1 $2
  chmod 0744 $2
  chown root:root $2
}

function multivnic {
  #multivnic
  copyit ${scripts}/secondary_vnic_all_configure.sh /usr/local/bin/secondary_vnic_all_configure.sh
  copyit ${scripts}/multivnic.service /usr/lib/systemd/system/multivnic.service
  systemctl start multivnic.service
  systemctl enable multivnic.service
}

function dns {
  # dsn
  firewall-offline-cmd --add-port=53/udp
  firewall-offline-cmd --add-port=53/tcp
  /bin/systemctl restart firewalld
}

main "$@"
