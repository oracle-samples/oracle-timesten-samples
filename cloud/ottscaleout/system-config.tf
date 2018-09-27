# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
# 
# OCI system configuration
#   Implements interface betweeen terraform and ansible
#   Copies files to bastion hosts
# 

locals {
  type = "string"
  ansibledir   = "service/ansible"
  ansiblehosts = "${local.ansibledir}/hosts"
  # dns search paths /etc/resolv.conf
  privsubnets  = "${replace(join(" ",oci_core_subnet.PrivateSubnet.*.subnet_domain_name), ",", " ")}"
  publsubnets  = "${oci_core_subnet.PublicSubnet_AD1.subnet_domain_name} ${oci_core_subnet.PublicSubnet_AD2.subnet_domain_name} ${oci_core_subnet.PublicSubnet_AD3.subnet_domain_name}" 
}

# Copy ssh key to bastion host
# First bastion becomes ansible controller
resource "null_resource" "copy-to-bastion" {
  connection {
    type        = "ssh"
    user        = "opc"
    private_key = "${var.ssh_private_key}"
    host        = "${oci_core_instance.bs_instance_AD1.public_ip}"
    timeout     = "10m"
  }

  triggers = {
    di_instance_ids = "${join(",", oci_core_instance.di_instance.*.id)}"
  }

  # copy opc ssh key
  provisioner "remote-exec" {
    inline = [
      "echo '${var.ssh_private_key}' > ${var.opc["opchome"]}/.ssh/id_rsa",
      "chmod 0400 ${var.opc["opchome"]}/.ssh/id_rsa"
    ]
  }

  # copy service directory
  provisioner "file" {
    source      = "./service"
    destination = "${var.opc["opchome"]}"
  }

  depends_on = ["null_resource.configvars", "null_resource.write-hosts-file", "null_resource.dns-searchpaths"]
}

#
# create script to set searchpaths in resolv.conf
# script installed as dhcp-client exit-hook
#
resource "null_resource" "dns-searchpaths" {
  provisioner "local-exec" {
    command = "rm -rf ${var.opc["scriptdir"]}/resolv.conf"
  }
  provisioner "local-exec" {
    command = "echo search ${local.publsubnets} >> ${var.opc["scriptdir"]}/resolv.conf"
  }
  provisioner "local-exec" {
    command = "echo search ${local.privsubnets} >> ${var.opc["scriptdir"]}/resolv.conf"
  }
} 	


# 
# write hosts file for use with ansible
#
resource "null_resource" "write-hosts-file" {

  triggers = {
    di_instance_ids = "${join(",", oci_core_instance.di_instance.*.id)}"
  }

  provisioner "local-exec" {
    command = "echo '${format("[bastion-hosts]\n%s\n%s%s[db-addresses]\n%s[mgmt-addresses]\n%s[zookeeper-addresses]\n%s",
           oci_core_instance.bs_instance_AD1.hostname_label,
           join("",formatlist("%s\n",oci_core_instance.bs_instance_AD2.*.hostname_label)),
           join("",formatlist("%s\n",oci_core_instance.bs_instance_AD3.*.hostname_label)),
           join("",formatlist("%s\n",oci_core_instance.di_instance.*.hostname_label)),
           join("",formatlist("%s\n",oci_core_instance.mg_instance.*.hostname_label)),
           join("",formatlist("%s\n",oci_core_instance.zk_instance.*.hostname_label)))}' > ${path.module}/${local.ansiblehosts}"
  }

  provisioner "local-exec" {
    command = "${var.opc["scriptdir"]}/crhostfile.py ${path.module}/${local.ansiblehosts} ${var.ksafety}"
  }

}

resource "null_resource" "install-ansible" {
  count = "${var.bsInstanceCount}"
  connection {
    type        = "ssh"
    user        = "opc"
    private_key = "${var.ssh_private_key}"
    host       = "${oci_core_instance.bs_instance_AD1.public_ip}"
    timeout     = "30m"
  }

  triggers = {
    bs_instance_id1 = "${oci_core_instance.bs_instance_AD1.id}"
    bs_instance_id2 = "${join(",", oci_core_instance.bs_instance_AD2.*.id)}"
    bs_instance_id3 = "${join(",", oci_core_instance.bs_instance_AD3.*.id)}"
  }

  provisioner "remote-exec" {
    inline = [
      # install ansible
      "sudo yum install -y ansible > /tmp/yum.install 2>&1",
      # copy ansible config file into place
      "cp ${var.opc["opchome"]}/${local.ansibledir}/ansible.cfg ${var.opc["opchome"]}/.ansible.cfg"
    ]
  }
  depends_on = ["null_resource.copy-to-bastion"]
}

# 
# Print variables to config file
#
locals {
  qt="\""
  cfgfile = "./${local.ansibledir}/roles/common/vars/main.yml"
}

# write dbname into new config file
resource "null_resource" "configvars" {
   
  triggers = {
    bs_instance_id1 = "${oci_core_instance.bs_instance_AD1.id}"
    bs_instance_id2 = "${join(",", oci_core_instance.bs_instance_AD2.*.id)}"
    bs_instance_id3 = "${join(",", oci_core_instance.bs_instance_AD3.*.id)}"
  }

  provisioner "local-exec" {
    command = "rm -rf ${local.cfgfile}" 
  }
  provisioner "local-exec" {
    command = "${var.opc["scriptdir"]}/getversion ${local.cfgfile}"
  }
  provisioner "local-exec" {
    command = "${var.opc["scriptdir"]}/crvarsfile.py variables.tf >> ${local.cfgfile}"
  }
  provisioner "local-exec" {
    command = "echo 'dbname              : ${local.qt}${var.service_name}${local.qt}' >> ${local.cfgfile}" 
  }
}
