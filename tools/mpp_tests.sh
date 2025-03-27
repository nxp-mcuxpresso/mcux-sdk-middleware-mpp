#!/bin/bash

set -e

ORIDIR=$PWD
IMGFOLDER="$1"
BOARD=$2
IFS=''

RED='\033[0;31m'
LBLUE='\033[1;34m'
NC='\033[0m' # No Color

echoerr() { echo -e ${RED}"$@"${NC} >&2; }

usage()
{
    echoerr "usage:"
    echoerr "$0 <path to elf images> <board name>"
}

if [ $# != 2 ] ;  then
    echoerr "Invalid number of parameters."
    usage
    exit 1
elif [ ! -d "${IMGFOLDER}" ]; then
    echoerr "Folder [${IMGFOLDER}] does not exist."
    usage
    exit 1
fi

case "${BOARD}" in
    frdmmcxn947)
        BOARDID="MCXN947:FRDM-MCXN947"
        ;;
    mimxrt700evk)
        BOARDID="MIMXRT798S:MIMXRT700-EVK"
        ;;
    evkbmimxrt1170)
        BOARDID="MIMXRT1176xxxxx:MIMXRT1170-EVK"
        ;;
    *)
        echoerr "Unknown board name"
        exit 1
esac

cd $IMGFOLDER

# get list of filenames
array=(*)

#filter ELF files with "test_" in name
for f in ${array[@]}; do
    if [ "${f:0:5}" != "test_" ] ; then 
        continue
    fi
    ftype="$(file $f | sed -n '/ELF/p')"
    if [ -n "$ftype" ]; then
        elfs+=($f)
    fi
done

# run the tests on target
cd $ORIDIR
for f in ${elfs[@]}; do
    echo "========= $f ==========="
    ./mpp_test.exp $IMGFOLDER/$f $BOARDID
done

