# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

# Allocates private subnets and compute instances
# Bastion hosts are attached to public subnet
# All other hosts are attached to private subnet, provisioned here
#
# zkInstanceCount == {0,3};  0 collocates; if 3 on its own vms
# mgInstanceCount == {0, 2}; 0 collocates; if 2 on its own vms
# diInstanceCount > 0


resource "oci_core_security_list" "PrivateSecurityList" {
    compartment_id = "${var.compartment_ocid}"
    display_name = "PrivateSecurityList"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    egress_security_rules = [{
	protocol = "all"
	destination = "0.0.0.0/0"
    }]

    ingress_security_rules = [
    {
        tcp_options {
            "max" = 6625
            "min" = 6624
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        tcp_options {
            "max" = 3754
            "min" = 3754
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        tcp_options {
            "max" = 61000
            "min" = 32768
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        tcp_options {
            "max" = 2181
            "min" = 2181
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        tcp_options {
            "max" = 2889
            "min" = 2888
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        udp_options {
            "max" = 53
            "min" = 53
        }
        protocol = "17"
        source = "${var.network["cidr"]}"
    },
    {
        tcp_options {
            "max" = 53
            "min" = 53
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        protocol = "6"
        tcp_options {
            "min" = 22
            "max" = 22
        }
        source = "${var.network["cidr"]}"
    },
    {
        protocol = "1"
        source = "0.0.0.0/0"
        icmp_options {
            "type" = 8
            "code" = 0
        }
    }]
}

# Created NAT'd routing through bastion host
# All subnets route through bastion host
# To get NAT H/A configuration, configure more than one bastion host

# Gets a list of VNIC attachments on the bastion (NAT) instance
data "oci_core_vnic_attachments" "bsInstanceVnicAD1" {
    compartment_id = "${var.compartment_ocid}"
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[0],"name")}"
    instance_id = "${oci_core_instance.bs_instance_AD1.id}"
}

# Get private IP address
resource "oci_core_private_ip" "bsInstancePrivateIPAD1" {
    vnic_id = "${lookup(data.oci_core_vnic_attachments.bsInstanceVnicAD1.vnic_attachments[0],"vnic_id")}"
    display_name = "bsInstancePrivateIPAD1"
}


# Route Tables
resource "oci_core_route_table" "PrivateRouteTableAD1" {
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    display_name = "PrivateRouteTableAD1"
    route_rules {
        #cidr_block = "0.0.0.0/0"
        destination = "0.0.0.0/0"
	destination_type = "CIDR_BLOCK"
	network_entity_id = "${oci_core_private_ip.bsInstancePrivateIPAD1.id}"
    }
}

# Subnets, one in each AD
# All NAT through Bastion host in AD1
resource "oci_core_subnet" "PrivateSubnet" {
    count = 3
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[count.index],"name")}"
    cidr_block = "${var.network["cidr_prefix"]}.${var.network["private_octet"] + (count.index - 1)}.0/${var.network["subnet_mask"]}"
    display_name = "PrivateSubnetAD${count.index + 1}"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    route_table_id = "${oci_core_route_table.PrivateRouteTableAD1.id}"
    security_list_ids = ["${oci_core_security_list.PrivateSecurityList.id}"]
    dhcp_options_id = "${oci_core_virtual_network.CoreVCN.default_dhcp_options_id}"
    dns_label="privsnad${count.index + 1}"
    prohibit_public_ip_on_vnic = "true"
}

# If more than one bastion host, then create another route table.
# Check NAT/Bastion Whitepaper
# AD2
data "oci_core_vnic_attachments" "bsInstanceVnicAD2" {
    count = "${(var.bsInstanceCount > 1) ? 1 : 0}"
    compartment_id = "${var.compartment_ocid}"
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[1],"name")}"
    instance_id = "${oci_core_instance.bs_instance_AD2.id}"
}
# AD2
resource "oci_core_private_ip" "bsInstancePrivateIPAD2" {
    count = "${(var.bsInstanceCount > 1) ? 1 : 0}"
    vnic_id = "${lookup(data.oci_core_vnic_attachments.bsInstanceVnicAD2.vnic_attachments[0],"vnic_id")}"
    display_name = "bsInstancePrivateIPAD2"
}
# AD2
resource "oci_core_route_table" "PrivateRouteTableAD2" {
    count = "${(var.bsInstanceCount > 1) ? 1 : 0}"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    display_name = "PrivateRouteTableAD2"
    route_rules {
        #cidr_block = "0.0.0.0/0"
        destination = "0.0.0.0/0"
	destination_type = "CIDR_BLOCK"
	network_entity_id = "${oci_core_private_ip.bsInstancePrivateIPAD2.id}"
    }
}
# AD3
data "oci_core_vnic_attachments" "bsInstanceVnicAD3" {
    count = "${(var.bsInstanceCount > 2) ? 1 : 0}"
    compartment_id = "${var.compartment_ocid}"
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[2],"name")}"
    instance_id = "${oci_core_instance.bs_instance_AD3.id}"
}
# AD3
resource "oci_core_private_ip" "bsInstancePrivateIPAD3" {
    count = "${(var.bsInstanceCount > 1) ? 1 : 0}"
    vnic_id = "${lookup(data.oci_core_vnic_attachments.bsInstanceVnicAD3.vnic_attachments[2],"vnic_id")}"
    display_name = "bsInstancePrivateIPAD3"
}
# AD3
resource "oci_core_route_table" "PrivateRouteTableAD3" {
    count = "${(var.bsInstanceCount > 1) ? 1 : 0}"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    display_name = "PrivateRouteTableAD3"
    route_rules {
        #cidr_block = "0.0.0.0/0"
        destination = "0.0.0.0/0"
	destination_type = "CIDR_BLOCK"
	network_entity_id = "${oci_core_private_ip.bsInstancePrivateIPAD3.id}"
    }
}


variable  "ksafety" { default = "2" }

locals {
  zkcount1 = "${(var.zkInstanceCount == 0 ||
                 var.zkInstanceCount == 3) ?
                 var.zkInstanceCount : 0}"
  firstAD  = "${(var.initial_AD > 0 && 
                 var.initial_AD < 4) ? 
		 var.initial_AD - 1 : 0}"
}

resource "oci_core_instance" "zk_instance" {
  count               = "${local.zkcount1}"
  availability_domain = 
    "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[(count.index + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-zk-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-zk-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.zkInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.PrivateSubnet.*.id, (count.index + local.firstAD) % 3)}"
    assign_public_ip       = false
  }
  source_details {
    source_type = "image"
    source_id = "${var.InstanceImageOCID[var.region]}"
  }
  metadata {
    ssh_authorized_keys = "${var.ssh_public_key}"
    user_data           = "${base64encode(file("service/scripts/user_data.tpl"))}"
  }
  timeouts {
    create = "10m"
  }
}

resource "oci_core_instance" "mg_instance" {
  count               = "${(var.mgInstanceCount == 0 || 
                            var.mgInstanceCount == 2) ?
			    var.mgInstanceCount : 0}"
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % var.ksafety) + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-mg-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-mg-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.mgInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.PrivateSubnet.*.id, ((count.index % var.ksafety) + local.firstAD) % 3)}"
    assign_public_ip       = false
  }
  source_details {
    source_type = "image"
    source_id = "${var.InstanceImageOCID[var.region]}"
  }
  metadata {
    ssh_authorized_keys = "${var.ssh_public_key}"
    user_data           = "${base64encode(file("service/scripts/user_data.tpl"))}"
  }
  timeouts {
    create = "10m"
  }
}

locals {
  dicount1 = "${(var.diInstanceCount * var.ksafety)}"
  dicount2 = "${(local.dicount1 + var.mgInstanceCount + var.zkInstanceCount >= 3) ? local.dicount1 : 4 }"
}

resource "oci_core_instance" "di_instance" {
  count               = "${local.dicount2}"
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % var.ksafety) + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-di-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-di-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.diInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.PrivateSubnet.*.id, ((count.index % var.ksafety) + local.firstAD) % 3)}"
    assign_public_ip       = false
  }
  source_details {
    source_type = "image"
    source_id = "${var.InstanceImageOCID[var.region]}"
  }
  metadata {
    ssh_authorized_keys = "${var.ssh_public_key}"
    user_data           = "${base64encode(file("service/scripts/user_data.tpl"))}"
  }
  timeouts {
    create = "10m"
  }
}

