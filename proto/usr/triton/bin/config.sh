#!/usr/bin/bash
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#
# Copyright 2020 Joyent Inc.
#
#
# functions for getting USB headnode config values (intended to be sourced
# from other scripts)
#
# It is also possible to use this to get a JSON hash of the config options using:
#
# bash config.sh -json
#

# Sets SDC_CONFIG_FILENAME with the location of the config file.
# On linux this is always the same, this function is here for
# compatibility purposes
function load_sdc_config_filename {
    # the default
    COMPUTE_NODE_CONFIG_FILENAME="/usr/triton/config/node.config"
    SDC_CONFIG_FILENAME=${COMPUTE_NODE_CONFIG_FILENAME}
    SDC_CONFIG_INC_DIR="$(dirname ${COMPUTE_NODE_CONFIG_FILENAME})"

}


# Loads config variables with prefix (default: CONFIG_)
function load_sdc_config {

    prefix=$1
    [[ -z ${prefix} ]] && prefix="CONFIG_"

    load_sdc_config_filename
    if [[ -f ${SDC_CONFIG_INC_DIR}/generic ]]; then
        GEN_FILE=${SDC_CONFIG_INC_DIR}/generic
    else
        GEN_FILE=/dev/null
    fi

    # Ignore comments, spaces at the beginning of lines and lines that don't
    # start with a letter.
    eval $((cat ${GEN_FILE} ${SDC_CONFIG_FILENAME}; echo "config_inc_dir=${SDC_CONFIG_INC_DIR}") | \
        sed -e "s/^ *//" | grep -v "^#" | grep "^[a-zA-Z]" | \
        sed -e "s/^/${prefix}/")
}
