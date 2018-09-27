# Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
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
    tenancy_ocid = "${var.tenancy_ocid}"
    user_ocid = "${var.user_ocid}"
    fingerprint = "${var.fingerprint}"
    private_key_path = "${var.private_key_path}"
    region = "${var.region}"
    version = "~> 2.0"
}
#alternative if provisioning from within oci
#provider "oci" {
#  auth = "InstancePrincipal"
#  region = "${var.region}"
#}

data "oci_identity_availability_domains" "ADs" {
    compartment_id = "${var.tenancy_ocid}"
}

resource "oci_core_virtual_network" "CoreVCN" {
    cidr_block = "${var.network["cidr"]}"
    compartment_id = "${var.compartment_ocid}"
    display_name = "vcn${var.service_name}"
    dns_label = "vcn${var.service_name}"
}

resource "oci_core_internet_gateway" "PublicIG" {
    compartment_id = "${var.compartment_ocid}"
    display_name = "PublicIG"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
}

resource "oci_core_route_table" "PublicRouteTable" {
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
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
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"

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
}

resource "oci_core_subnet" "PublicSubnet_AD1" {
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[0],"name")}"
    cidr_block = "${var.network["cidr_prefix"]}.${var.network["public_octet"]}.0/${var.network["subnet_mask"]}"
    display_name = "PublicSubnetAD1"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    route_table_id = "${oci_core_route_table.PublicRouteTable.id}"
    security_list_ids = ["${oci_core_security_list.PublicSecurityList.id}"]
    dhcp_options_id = "${oci_core_virtual_network.CoreVCN.default_dhcp_options_id}"
    dns_label = "pubsnad1"
}
resource "oci_core_subnet" "PublicSubnet_AD2" {
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[1],"name")}"
    cidr_block = "${var.network["cidr_prefix"]}.${var.network["public_octet"] + 1}.0/${var.network["subnet_mask"]}"
    display_name = "PublicSubnetAD2"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    route_table_id = "${oci_core_route_table.PublicRouteTable.id}"
    security_list_ids = ["${oci_core_security_list.PublicSecurityList.id}"]
    dhcp_options_id = "${oci_core_virtual_network.CoreVCN.default_dhcp_options_id}"
    dns_label = "pubsnad2"
}
resource "oci_core_subnet" "PublicSubnet_AD3" {
    availability_domain = "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[2],"name")}"
    cidr_block = "${var.network["cidr_prefix"]}.${var.network["public_octet"] + 2}.0/${var.network["subnet_mask"]}"
    display_name = "PublicSubnetAD3"
    compartment_id = "${var.compartment_ocid}"
    vcn_id = "${oci_core_virtual_network.CoreVCN.id}"
    route_table_id = "${oci_core_route_table.PublicRouteTable.id}"
    security_list_ids = ["${oci_core_security_list.PublicSecurityList.id}"]
    dhcp_options_id = "${oci_core_virtual_network.CoreVCN.default_dhcp_options_id}"
    dns_label = "pubsnad3"
}



# Bastion_server_count == {1, 2, 3}; 1 by default
# Use private IP(s) from Bastion servers as route target(s) enabling NAT
# Allows hosts on private subnets to pull software downloads, updates from public internet

resource "oci_core_instance" "bs_instance_AD1" {
  availability_domain = 
    "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[0],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-bs-%03d", var.service_name, 1)}"
  hostname_label      = "${format("%s-bs-%03d", var.service_name, 1)}"
  shape               = "${var.bsInstanceShape}"
  create_vnic_details {
    subnet_id              = "${oci_core_subnet.PublicSubnet_AD1.id}"
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
    create = "10m"
  }
}

resource "oci_core_instance" "bs_instance_AD2" {
  count               = "${(var.bsInstanceCount > 1 ? 1 : 0)}"
  availability_domain = 
    "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[1],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-bs-%03d", var.service_name, 2)}"
  hostname_label      = "${format("%s-bs-%03d", var.service_name, 2)}"
  shape               = "${var.bsInstanceShape}"
  create_vnic_details {
    subnet_id              = "${oci_core_subnet.PublicSubnet_AD2.id}"
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
    create = "10m"
  }
}

resource "oci_core_instance" "bs_instance_AD3" {
  count               = "${(var.bsInstanceCount > 2 ? 1 : 0)}"
  availability_domain = 
    "${lookup(data.oci_identity_availability_domains.ADs.availability_domains[2],"name")}"
  compartment_id      = "${var.compartment_ocid}"
  display_name        = "${format("%s-bs-%03d", var.service_name, 3)}"
  hostname_label      = "${format("%s-bs-%03d", var.service_name, 3)}"
  shape               = "${var.bsInstanceShape}"
  create_vnic_details {
    subnet_id              = "${oci_core_subnet.PublicSubnet_AD3.id}"
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
    create = "10m"
  }
}


