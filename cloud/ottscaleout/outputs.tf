# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

output "InstanceIPAddresses" {
  value = ["${format("bastion host instances (public addresses): 
    ssh opc@%s %s %s",
           oci_core_instance.bs_instance_AD1.public_ip,
	   join("",formatlist("%s\n    ",oci_core_instance.bs_instance_AD2.*.public_ip)),
	   join("",formatlist("%s\n    ",oci_core_instance.bs_instance_AD3.*.public_ip)))}"]
  value = ["${format("database [mgmt|zookeeper] hosts (private addresses):
    %s%s%s",
           join("",formatlist("%s %s\n    ",oci_core_instance.di_instance.*.hostname_label,oci_core_instance.di_instance.*.private_ip)),
           join("",formatlist("%s %s\n    ",oci_core_instance.mg_instance.*.hostname_label,oci_core_instance.mg_instance.*.private_ip)),
           join("",formatlist("%s %s\n    ",oci_core_instance.zk_instance.*.hostname_label,oci_core_instance.zk_instance.*.private_ip)))}"]
}



