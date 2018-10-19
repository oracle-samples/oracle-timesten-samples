# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl

# Allocates private subnets and compute instances
# Bastion hosts are attached to public subnet
# All other hosts are attached to private subnet, provisioned here
# Block volumes, if requested, are allocated here
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
            "max" = "${var.timesten["mgmtdaemonport"]}"
            "min" = "${var.timesten["mgmtdaemonport"]}"
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        tcp_options {
            "max" = "${var.timesten["mgmtcsport"] }"
            "min" = "${var.timesten["mgmtcsport"] }"
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        tcp_options {
            "max" = "${var.timesten["mgmtreplport"]}"
            "min" = "${var.timesten["mgmtreplport"]}"
        }
        protocol = "6"
        source = "${var.network["cidr"]}"
    },
    {
        tcp_options {
            "max" = "${var.timesten["chnlporthi"]}"
            "min" = "${var.timesten["chnlportlo"]}"
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

# Private Route Table
# Create routing for private subnets through NAT gateway
resource "oci_core_route_table" "PrivateRouteTable" {
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    display_name = "PrivateRouteTable"
    route_rules {
        #cidr_block = "0.0.0.0/0"
        destination = "0.0.0.0/0"
	destination_type = "CIDR_BLOCK"
	network_entity_id = "${oci_core_nat_gateway.nat_gateway.id}"
    }
}

# Subnets, one in each AD
# Route through NAT gateway
resource "oci_core_subnet" "PrivateSubnet" {
    count = 3
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[count.index],"name")}"
    cidr_block = "${cidrsubnet(var.network["cidr"],var.network["subnets"],(count.index + 3))}"
    display_name = "PrivateSubnetAD${count.index + 1}"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    route_table_id = "${oci_core_route_table.PrivateRouteTable.id}"
    security_list_ids = ["${oci_core_security_list.PrivateSecurityList.id}"]
    dhcp_options_id = "${oci_core_virtual_network.CoreVCN.default_dhcp_options_id}"
    dns_label="privsnad${count.index + 1}"
    prohibit_public_ip_on_vnic = "true"
}

locals {
  zkcount1 = "${(var.zkInstanceCount == 0 ||
                 var.zkInstanceCount == 3) ?
                 var.zkInstanceCount : 0}"
  firstAD  = "${(var.initialAD > 0 && 
                 var.initialAD < 4) ? 
		 var.initialAD - 1 : 0}"
  ksafeval = "${(var.ksafety >= 1 && var.ksafety <= 2) ? var.ksafety : 2}"		 
  numADs   = "${(var.singleAD == "true") ? 1 : local.ksafeval}"

  # data instances
  dicount1 = "${(var.diInstanceCount * local.ksafeval)}"
  dicount2 = "${(local.dicount1 + var.mgInstanceCount + var.zkInstanceCount >= 3) ? local.dicount1 : 4 }"

  # block volumes
  standard = "${substr(var.diInstanceShape,3,8)}"
  bvcount1 = "${(local.standard == "Standard") ? 1 : 0}"
  bvcount2 = "${(var.diBlockVolumeSizeGB >= 50 && 
                 var.diBlockVolumeSizeGB <= 32768) ? 
                local.dicount2 : 0}"
  # set to indicate error condition if no block volume and Standard shape
  bvcheck1 = "${(local.bvcount2 == 0 && local.bvcount1 == 1) ? 1 : 0}"
  bvcheck2 = "${(local.bvcount2 == 0 && var.system["storage"] == "MD-RAID-10") ? 1 : 0}"
  dicheck1 = "${(var.diInstanceShape == "VM.Standard1.1") ? 1 : 0}"
}

# workaround for error checking
# prevent use of standard shape without block volume
resource "null_resource" "bv_check_1" {
  count = "${local.bvcheck1}"
  "\nERROR: No block volumes specified with Standard shape" = true
}
resource "null_resource" "bv_check_2" {
  count = "${local.bvcheck2}"
  "\nERROR: MD-RAID-10 storage not permitted with block volume" = true
}
# prevent use of standard shape without block volume
resource "null_resource" "di_shape_check_1" {
  count = "${local.dicheck1}"
  "\nERROR: VM.Standard1.1 shape has insufficient resources for use as data instance.\nPlease specify different diInstanceShape" = true
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
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % local.numADs) + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-mg-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-mg-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.mgInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.PrivateSubnet.*.id, ((count.index % local.numADs) + local.firstAD) % 3)}"
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


resource "oci_core_instance" "di_instance" {
  count               = "${local.dicount2}"
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % local.numADs) + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-di-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-di-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.diInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.PrivateSubnet.*.id, ((count.index % local.numADs) + local.firstAD) % 3)}"
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

resource "oci_core_instance" "cl_instance" {
  count               = "${var.clInstanceCount}"
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % local.numADs) + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-cl-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-cl-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.clInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.PrivateSubnet.*.id, ((count.index % local.numADs) + local.firstAD) % 3)}"
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


# Optional block volume attachments
# May only used with Standard shape

resource "oci_core_volume" "di_volume" {
  count = "${local.bvcount2}"
  #Required
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % local.numADs) + local.firstAD) % 3],"name")}"
  compartment_id = "${var.compartment_ocid}"

  #Optional
  display_name = "${format("%s-bv-%03d", var.service_name, count.index + 1)}"
  size_in_gbs = "${var.diBlockVolumeSizeGB}"
}

resource "oci_core_volume_attachment" "di_volume_attachments" {
  count = "${local.bvcount2 }"
  #Required
  instance_id = "${oci_core_instance.di_instance.*.id[count.index]}"
  attachment_type = "iscsi"
  volume_id = "${oci_core_volume.di_volume.*.id[count.index]}"

  #Optional
  display_name = "${format("%s-bvat-%03d", var.service_name, count.index + 1)}"
}

