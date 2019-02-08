# Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
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
    vcn_id = "${oci_core_vcn.CoreVCN.id}"
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
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

# If NAT Gateway is not present,
# Create NAT routing through bastion host
# All subnets route through bastion host

# Gets a list of VNIC attachments on the bastion (NAT) instance
data "oci_core_vnic_attachments" "bsInstanceVnic" {
    compartment_id = "${var.compartment_ocid}"
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[local.bsad],"name")}"
    instance_id = "${element(oci_core_instance.bs_instance.*.id, 0)}"
}

# Get private IP address
resource "oci_core_private_ip" "bsInstancePrivateIP" {
    vnic_id = "${lookup(data.oci_core_vnic_attachments.bsInstanceVnic.vnic_attachments[0],"vnic_id")}"
    display_name = "bsInstancePrivateIP"
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

# Private Route Table
# Create routing for private subnets through NAT gateway or NAT instance
locals {
  # hashicorp issue 11210 workaround
  has_pip  = "${(length(oci_core_private_ip.bsInstancePrivateIP.*.id) > 0) ? 
                 element(concat(oci_core_private_ip.bsInstancePrivateIP.*.id,list("")),0) :
                 oci_core_nat_gateway.nat_gateway.id }"
  nat_type = "${(var.network["use_nat_gateway"] == 0) ? 
                 local.has_pip : oci_core_nat_gateway.nat_gateway.id}"
}

resource "oci_core_route_table" "PrivateRouteTable" {
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_vcn.CoreVCN.id}"
    display_name = "PrivateRouteTable"
    route_rules {
        destination = "0.0.0.0/0"
	destination_type = "CIDR_BLOCK"
	network_entity_id = "${local.nat_type}"
    }
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}


# Subnets, one in each AD
# Route through NAT gateway
resource "oci_core_subnet" "private_subnet" {
    count = 3
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[count.index],"name")}"
    cidr_block = "${cidrsubnet(var.network["cidr"],var.network["subnets"],(count.index + 3))}"
    display_name = "private_subnet_AD${count.index + 1}"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_vcn.CoreVCN.id}"
    route_table_id = "${oci_core_route_table.PrivateRouteTable.id}"
    security_list_ids = ["${oci_core_security_list.PrivateSecurityList.id}"]
    dhcp_options_id = "${oci_core_vcn.CoreVCN.default_dhcp_options_id}"
    dns_label="privsnad${count.index + 1}"
    prohibit_public_ip_on_vnic = "true"
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

locals {
  # availability domains
  firstAD  = "${(var.initialAD >= 1 && 
                 var.initialAD <= 3) ? 
		 var.initialAD - 1 : 0}"
  # validate ksafety or fall back to 2
  ksafeval = "${(var.ksafety >= 1 && var.ksafety <= 2) ? var.ksafety : 2}"
  # By default ksafety > 1 spans ADs
  numADs   = "${(var.singleAD == "true") ? 1 : local.ksafeval}"

  # data instances
  # adjust data instances for ksafety
  dicount1 = "${(var.diInstanceCount * local.ksafeval) }"
  # disallow 1x1 configuration
  dicheck0 = "${(local.dicount1 <= 1) ? 1 : 0}"
  # disallow single core shapes
  dicheck1 = "${(substr(var.diInstanceShape,-2,2) == ".1") ? 1 : 0}"

  # zookeeper servers
  zkcount0 = "${(var.zkInstanceCount == 0 ||
                 var.zkInstanceCount == 3) ?
                 var.zkInstanceCount : 0}"
  # minimum of 3 compute instances needed for zk servers
  # increase zk count if < 3 compute instances for zk servers
  # zkcount1 == 0 if co-located, == 1 if 1x2 or 2x1 config, == 2 if 1x1 config, == 3 if offloaded
  zkcount1 = "${(local.dicount1 + var.mgInstanceCount + local.zkcount0 < 3) ? (3 - local.dicount1) : local.zkcount0 }"

  # block volumes
  standard = "${substr(var.diInstanceShape,3,8)}"
  bvcount1 = "${(local.standard == "Standard") ? 1 : 0}"
  # validate min/max OCI block volume storage limits or error out
  bvcount2 = "${(var.diBlockVolumeSizeGB >= 50 && 
                 var.diBlockVolumeSizeGB <= 32768) ? 
                local.dicount1 : 0}"
  # set to indicate error condition if no block volume and Standard shape or size out of range
  bvcheck1 = "${(local.bvcount2 == 0 && local.bvcount1 == 1) ? 1 : 0}"
  bvcheck2 = "${(local.bvcount2 == 0 && var.system["storage"] == "MD-RAID-10") ? 1 : 0}"

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
# ensure minimum data instances
resource "null_resource" "di_count_check_1" {
  count = "${local.dicheck0}"
  "\nERROR: Insufficient number of data instances; 1x1 grids not permitted.\nIncrease diInstanceCount or ksafety" = true
}
# prevent use of standard shape without block volume
resource "null_resource" "di_shape_check_1" {
  count = "${local.dicheck1}"
  "\nERROR: Single core shapes have insufficient resources for use as data instances.\nSpecify different diInstanceShape" = true
}

resource "oci_core_instance" "zk_instance" {
  count               = "${local.zkcount1}"
  availability_domain = 
    "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[(count.index + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-zk-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-zk-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.zkInstanceShape}"

  # uncomment to prevent accidental destroy
  # lifecycle {
  #    prevent_destroy      = "true"
  # }
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.private_subnet.*.id, (count.index + local.firstAD) % 3)}"
    assign_public_ip       = "false"
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
  freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

resource "oci_core_instance" "mg_instance" {
  count               = "${(var.mgInstanceCount == 0 || 
                            var.mgInstanceCount == 2) ?
			    var.mgInstanceCount : 0}"
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % local.numADs) + local.firstAD) % 3],"name")}"
  compartment_id        = "${var.compartment_ocid}"
  display_name          = "${format("%s-mg-%03d", var.service_name, count.index + 1)}"
  hostname_label        = "${format("%s-mg-%03d", var.service_name, count.index + 1)}"
  shape                 = "${var.mgInstanceShape}"

  # uncomment to prevent accidental destroy
  # lifecycle {
  #    prevent_destroy   = "true"
  # }
  create_vnic_details {
    subnet_id           = "${element(oci_core_subnet.private_subnet.*.id, ((count.index % local.numADs) + local.firstAD) % 3)}"
    assign_public_ip    = "false"
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
    create              = "10m"
  }
  freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}


resource "oci_core_instance" "di_instance" {
  count               = "${local.dicount1}"
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % local.numADs) + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-di-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-di-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.diInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.private_subnet.*.id, ((count.index % local.numADs) + local.firstAD) % 3)}"
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
  freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

resource "oci_core_instance" "cl_instance" {
  count               = "${var.clInstanceCount}"
  availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[((count.index % local.numADs) + local.firstAD) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-cl-%03d", var.service_name, count.index + 1)}"
  hostname_label      = "${format("%s-cl-%03d", var.service_name, count.index + 1)}"
  shape               = "${var.clInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.private_subnet.*.id, ((count.index % local.numADs) + local.firstAD) % 3)}"
    assign_public_ip       = "false"
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
  freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
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
  freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
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

