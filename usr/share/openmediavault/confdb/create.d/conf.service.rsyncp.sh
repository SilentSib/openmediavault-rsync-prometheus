#!/bin/sh
#
# This file is part of openmediavault-rsync-prometheus.
#
# Seeds /etc/openmediavault/config.xml with the default configuration
# for the rsync-prometheus plugin. Called by:
#   omv-confdbadm create "conf.service.rsyncp"
#
# It is idempotent — safe to run on upgrade as well as fresh install.

set -e

. /usr/share/openmediavault/scripts/helper-functions

SERVICE_XPATH_NAME="rsyncp"
SERVICE_XPATH="/config/services/${SERVICE_XPATH_NAME}"

if ! omv_config_exists "${SERVICE_XPATH}"; then
    omv_config_add_node "/config/services" "${SERVICE_XPATH_NAME}"
    omv_config_add_key "${SERVICE_XPATH}" "enable"           "0"
    omv_config_add_key "${SERVICE_XPATH}" "pushgateway_url"  ""
    omv_config_add_key "${SERVICE_XPATH}" "username"         ""
    omv_config_add_key "${SERVICE_XPATH}" "password"         ""
    omv_config_add_key "${SERVICE_XPATH}" "job_label_prefix" "omv_rsync"
    omv_config_add_key "${SERVICE_XPATH}" "instance"         ""
    omv_config_add_key "${SERVICE_XPATH}" "tls_verify"       "1"
fi

exit 0
