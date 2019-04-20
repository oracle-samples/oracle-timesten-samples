# Copyright (c) 1999, 2019, Oracle and/or its affiliates. All rights reserved.
#
# Licensed under the Universal Permissive License v 1.0 as shown
# at http://oss.oracle.com/licenses/upl
#
# Sets up public networks for TimesTen 
# Only bastion servers live on public subnets
# Such servers enable hosts on private networks to pull [install/update] software
# Bastions also allow ssh access from public internet and to private subnet hosts
#
# Configures public security lists, enables DNS
# 
# A subnet is configured in each AD
# Don't really need 3rd AD until k=3 supported
# For now it is used for a Bastion server or ZooKeeper instance
#


provider "oci" {
    tenancy_ocid     = "${var.tenancy_ocid}"
    user_ocid        = "${var.user_ocid}"
    fingerprint      = "${var.fingerprint}"
    private_key_path = "${var.private_key_path}"
    region           = "${var.region}"
    version = ">= 3.4, < 4.0"
}
#alternative to provider data above if provisioning within oci
#provider "oci" {
#  auth = "InstancePrincipal"
#  region = "${var.region}"
#  version = ">= 3.4.0, < 4.0"
#}
provider "null" {
    version = "~> 1.0"
}

data "oci_identity_availability_domains" "ADs" {
    compartment_id = "${var.tenancy_ocid}"
}

resource "oci_core_vcn" "CoreVCN" {
    cidr_block = "${var.network["cidr"]}"
    compartment_id = "${var.compartment_ocid}"
    display_name = "vcn${var.service_name}"
    dns_label = "vcn${var.service_name}"
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

resource "oci_core_internet_gateway" "PublicIG" {
    compartment_id = "${var.compartment_ocid}"
    display_name = "PublicIG"
    vcn_id = "${oci_core_vcn.CoreVCN.id}"
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

resource "oci_core_route_table" "PublicRouteTable" {
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_vcn.CoreVCN.id}"
    display_name = "PublicRouteTable"
    route_rules {
        #cidr_block = "0.0.0.0/0"
        destination = "0.0.0.0/0"
        destination_type = "CIDR_BLOCK"
        network_entity_id = "${oci_core_internet_gateway.PublicIG.id}"
    }
}

resource "oci_core_security_list" "PublicSecurityList" {
    compartment_id = "${var.compartment_ocid}"
    display_name = "PublicSecurityList"
    vcn_id = "${oci_core_vcn.CoreVCN.id}"

    egress_security_rules = [{
        protocol = "all"
        destination = "0.0.0.0/0"
    }]

    ingress_security_rules = [
    {
        tcp_options {
            "max" = 443
            "min" = 443
        }
        protocol = "6"
        source = "0.0.0.0/0"
    },
    {
        protocol = "6"
        source = "0.0.0.0/0"
        tcp_options {
            "min" = 22
            "max" = 22
        }
    },
    {
        protocol = "all"
        source = "${var.network["cidr"]}"
    },
    {
        protocol = "1"
        source = "0.0.0.0/0"
        icmp_options {
            "type" = 3
            "code" = 4
        }
    }]
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

locals {
  bscount1 = "${ (var.bsInstanceCount >= 1 && var.bsInstanceCount <= 3) ? var.bsInstanceCount : 1 }"
  bsad = "${ (var.bsInstanceInitialAD >= 1 && var.bsInstanceInitialAD <= 3) ? var.bsInstanceInitialAD - 1 : 0 }"
}

resource "oci_core_nat_gateway" "nat_gateway" {
    #Required
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_vcn.CoreVCN.id}"
    #Optional
    display_name = "nat_gateway_${var.service_name}"
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}

resource "oci_core_subnet" "public_subnet" {
    count = 3
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[count.index],"name")}"
    cidr_block = "${cidrsubnet(var.network["cidr"],var.network["subnets"],count.index)}"
    display_name = "public_subnet_AD${ count.index + 1 }"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_vcn.CoreVCN.id}"
    route_table_id = "${oci_core_route_table.PublicRouteTable.id}"
    security_list_ids = ["${oci_core_security_list.PublicSecurityList.id}"]
    dhcp_options_id = "${oci_core_vcn.CoreVCN.default_dhcp_options_id}"
    dns_label = "pubsnad${ count.index + 1 }"
    freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}


# Bastion_server_count == {1, 2, 3}; 1 by default

resource "oci_core_instance" "bs_instance" {
  count = "${local.bscount1}"
  availability_domain = 
    "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[(count.index + local.bsad) % 3],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-bs-%03d", var.service_name, count.index + 1 )}"
  hostname_label      = "${format("%s-bs-%03d", var.service_name, count.index + 1 )}"
  shape               = "${var.bsInstanceShape}"
  create_vnic_details {
    subnet_id              = "${element(oci_core_subnet.public_subnet.*.id, (count.index + local.bsad) % 3)}"
    skip_source_dest_check = true
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
    create = "${var.instance_create_timeout}"
  }
  freeform_tags = "${map(var.opc["tagkey"],var.service_name)}"
}



