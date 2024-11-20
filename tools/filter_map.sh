#!/bin/bash

# this script allows to extract from memory map the N biggest objects for a given section (data, text, bss, etc...)
# usage: ./filter_map.sh <path to memory.map> <section name> <number of objects>

INPUT=$1
SECT=$2
TOPN=$3
TMPF="/tmp/output.map"

RED='\033[0;31m'
LBLUE='\033[1;34m'
NC='\033[0m' # No Color

echoerr() { echo -e ${RED}"$@"${NC} >&2; }

usage()
{
    echoerr "usage:"
    echoerr "$0 <path to memory.map> <section name> <number of objects>"
}

if [ $# != 3 ] ;  then
    echoerr "Invalid number of parameters."
    usage
    exit 1
elif [ ! -f "${INPUT}" ]; then
    echoerr "File [${INPUT}] does not exist."
    usage
    exit 1
fi

# first remove the line-ending just after section names (happens for long lines)
# put result in temporary file
sed ':a;N;$!ba;s/\( \.[[:alnum:]]*\)\n/\1/g' $INPUT > $TMPF
echo -e ${LBLUE}"Top $TOPN biggest objects in .$SECT section of memory map:"${NC}
# then sort numerically the lines using the 3rd column (size) for the given 'section'
# keep only the 'N' first sorted lines
grep ' \.'$SECT $TMPF | sort -g -r -k 3 | head -$TOPN
#remove temporary file
rm $TMPF
