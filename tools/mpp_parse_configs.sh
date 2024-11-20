#!/bin/bash

# This script is used to parse the supported configurations
# described in the source file.
# Each supported configuration is identified in the file
# with an index and a pattern APP_CONFIGx,
# where x specify the index of the config
# The end of the supported configurations list is
# identified in the file with pattern APP_CONFIGS_END.

file=$1
pattern="APP_CONFIG$2"

configs=""

echoerr() { echo "$@" >&2; }

usage()
{
    echoerr "usage:"
    echoerr "$0 <file_name> <configuration_index>"
}

re='^[0-9]+$'
if [ ! -f "${file}" ]; then
    echoerr "File [${file}] does not exist."
    usage
    exit 1
elif ! [[ $2 =~ $re ]] ;  then
    echoerr "Index [$2] is not valid."
    usage
    exit 1
fi

while IFS=':' read -r f1 f2; do
    if [[ "${f1}" == *"${pattern}"* ]]; then
        configs="${f2}"
	break
    elif [[ "${f1}" == *"APP_CONFIGS_END"* ]]; then break
    fi
done < "${file}"

echo ${configs}