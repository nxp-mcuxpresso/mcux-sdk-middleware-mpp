#!/bin/bash

# This script is used to get the target test/example default
# configuration if exist.
# The default configuration is identified in the configuration
# file by the pattern "APP_DEFAULT".

file=$1

pattern="APP_DEFAULT"
default_configs=""

echoerr() { echo "$@" >&2; }

usage()
{
    echoerr "usage:"
    echoerr "$0 <path_to_configuration_file_name>"
}

if [ ! -f "${file}" ]; then
    echoerr "File [${file}] does not exist."
    usage
    exit 1
fi

# get the app default configuration
while IFS=':' read -r f1 f2; do
    if [[ "${f1}" == *"${pattern}"* ]]; then
        default_configs="${f2}"
        break
    fi
done < "${file}"

echo ${default_configs}
