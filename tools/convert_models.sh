#!/bin/bash

# Exit on error
set -ex

# get the path to the MPP_DIR, independent of the 
# location from which the script is called
MPP_DIR=$(dirname $(readlink -f $0 | xargs dirname))
CRT_DIR=${PWD}
cd $MPP_DIR

#check converter path
if [ "${CONVERTER_RT700_PATH}" == ""]; then
    echo "please set variable CONVERTER_RT700_PATH"
    exit 1
fi

if [ "${CONVERTER_MCXN_PATH}" == "" ]; then
    echo "please set variable CONVERTER_MCXN_PATH"
    exit 1
fi

for MODEL_BASENAME in "ultraface_slim_ultraslim" "nanodet_m_0.5x_nhwc_nopermute" "mobilenet_v1_0.25_128_quant_int8" "persondetect_160_128" "persondetect_220_220" "antispoofing" "mobilefacenet_96_96"; do
    for CHIP in "mcxn94x" "imxrt700"; do
        if [ $CHIP == "mcxn94x" ]; then
            NPU_VERSION="npu16"
            CONVERTER=${CONVERTER_MCXN_PATH}
            if [ ${MODEL_BASENAME} == "nanodet_m_0.5x_nhwc_nopermute" -o ${MODEL_BASENAME} == "persondetect_220_220" -o ${MODEL_BASENAME} == "antispoofing" -o ${MODEL_BASENAME} == "mobilefacenet_96_96" ]; then
                continue
            fi
        elif [ $CHIP == "imxrt700" ]; then
            NPU_VERSION="npu64"
            CONVERTER=${CONVERTER_RT700_PATH}
        else
            echo "chip model not recognized!!"
            exit 1;
        fi

        #manage directory/tflite/header naming inconsistency :-(
        if [ ${MODEL_BASENAME} == "ultraface_slim_ultraslim" ]; then
            MODEL_DIR="models/ultraface_slim_quant_int8"
            MODEL_H_NAME=${MODEL_BASENAME}
        elif [ ${MODEL_BASENAME} == "nanodet_m_0.5x_nhwc_nopermute" ]; then
            MODEL_DIR="models/nanodet_m_320_quant_int8"
            MODEL_H_NAME=${MODEL_BASENAME}
        elif [ ${MODEL_BASENAME} == "mobilenet_v1_0.25_128_quant_int8" ]; then
            MODEL_DIR="models/${MODEL_BASENAME}"
            MODEL_H_NAME="mobilenetv1_model_data"
        elif [ ${MODEL_BASENAME} == "persondetect_160_128" -o ${MODEL_BASENAME} == "persondetect_220_220" ]; then
            MODEL_DIR="models/persondetect"
            MODEL_H_NAME=${MODEL_BASENAME}
        elif [ ${MODEL_BASENAME} == "antispoofing" ]; then
            MODEL_DIR="internal/models/antispoofing"
            MODEL_H_NAME=${MODEL_BASENAME}
        elif [ ${MODEL_BASENAME} == "mobilefacenet_96_96" ]; then
            MODEL_DIR="internal/models/mobilefacenet"
            MODEL_H_NAME=${MODEL_BASENAME}
        else
            exit 1;
        fi

        ORI_TFLITE=${MPP_DIR}/${MODEL_DIR}/${MODEL_BASENAME}.tflite
        CONV_TFLITE=${MPP_DIR}/${MODEL_DIR}/${MODEL_BASENAME}_${NPU_VERSION}.tflite
        CONV_HEADER=${MPP_DIR}/${MODEL_DIR}/${MODEL_H_NAME}_${NPU_VERSION}_tflite.h
        TEMP_HEADER="temp_model.h"

        VERSION="$( $CONVERTER/neutron-converter --version )"
        $CONVERTER/neutron-converter --input ${ORI_TFLITE} --output ${CONV_TFLITE}  --target $CHIP

        #transform tflite into header
        xxd -i ${CONV_TFLITE} > ${TEMP_HEADER}
        #detect end of new array
        ARRAY_END_TEMP=$( awk '/};/{ print NR; exit }' ${TEMP_HEADER} )
        #get new array size
        ARRAY_SIZE=$( grep 'unsigned int' ${TEMP_HEADER} | sed 's/.* = \([[:digit:]]*\);/\1/' )
        #keep only new array data
        sed -i '1d;'${ARRAY_END_TEMP}',$d' ${TEMP_HEADER}

        #find start/end line of old array
        ARRAY_START=$( awk '/= {$/{ print NR; exit }' ${CONV_HEADER} )
        ARRAY_FIRSTLINE=$((${ARRAY_START} + 1))
        ARRAY_END=$( awk '/^};$/{ print NR; exit }' ${CONV_HEADER} )
        ARRAY_LASTLINE=$((${ARRAY_END} - 1))
        LINES="${ARRAY_FIRSTLINE},${ARRAY_LASTLINE}"
        #remove old array
        sed -i ''${LINES}'d' ${CONV_HEADER}
        #put new array
        sed -i ''${ARRAY_START}' r '${TEMP_HEADER}'' ${CONV_HEADER}
        #update size
        sed -i 's/len = [[:digit:]]*;/len = '${ARRAY_SIZE}';/' ${CONV_HEADER}
        #update version
        sed -i 's/converted by .*$/Converted by '"${VERSION}"'/I' ${CONV_HEADER}
        sed -i 's/Neutron Converter version .*$/Converted by '"${VERSION}"'/I' ${CONV_HEADER}
    done
done

cd ${CRT_DIR}

