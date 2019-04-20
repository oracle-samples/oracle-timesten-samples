# Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

output "InstanceIPAddresses" {
  value = ["${format("bastion host instances (public addresses): 
    %s", join("",formatlist("ssh opc@%s\n    ",oci_core_instance.bs_instance.*.public_ip)))}"]
  value = ["${format("database [datainstance|mgmt|zookeeper] hosts (private addresses):
    %s%s%s",
           join("",formatlist("%s %s\n    ",oci_core_instance.di_instance.*.hostname_label,oci_core_instance.di_instance.*.private_ip)),
           join("",formatlist("%s %s\n    ",oci_core_instance.mg_instance.*.hostname_label,oci_core_instance.mg_instance.*.private_ip)),
           join("",formatlist("%s %s\n    ",oci_core_instance.zk_instance.*.hostname_label,oci_core_instance.zk_instance.*.private_ip)))}"]
   value = ["${format("client host instances (private addresses):
    %s", join("",formatlist("%s %s\n    ",oci_core_instance.cl_instance.*.hostname_label,oci_core_instance.cl_instance.*.private_ip)))}"]
}
