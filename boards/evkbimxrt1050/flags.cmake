IF(NOT DEFINED FPU)  
    SET(FPU "-mfloat-abi=hard -mfpu=fpv5-d16")  
ENDIF()  

IF(NOT DEFINED SPECS)  
    SET(SPECS "--specs=nano.specs --specs=nosys.specs")  
ENDIF()  

IF(NOT DEFINED DEBUG_CONSOLE_CONFIG)  
    SET(DEBUG_CONSOLE_CONFIG "-DSDK_DEBUGCONSOLE_UART")  
ENDIF()  

SET(CMAKE_ASM_FLAGS_FLEXSPI_NOR_SDRAM_RELEASE " \
    ${CMAKE_ASM_FLAGS_FLEXSPI_NOR_SDRAM_RELEASE} \
    -D__STARTUP_CLEAR_BSS \
    -DNDEBUG \
    -D__STARTUP_INITIALIZE_NONCACHEDATA \
    -mcpu=cortex-m7 \
    -mthumb \
    ${FPU} \
")
SET(CMAKE_ASM_FLAGS_FLEXSPI_NOR_SDRAM_DEBUG " \
    ${CMAKE_ASM_FLAGS_FLEXSPI_NOR_SDRAM_DEBUG} \
    -D__STARTUP_CLEAR_BSS \
    -DDEBUG \
    -D__STARTUP_INITIALIZE_NONCACHEDATA \
    -mcpu=cortex-m7 \
    -mthumb \
    ${FPU} \
")
SET(CMAKE_C_FLAGS_FLEXSPI_NOR_SDRAM_RELEASE " \
    ${CMAKE_C_FLAGS_FLEXSPI_NOR_SDRAM_RELEASE} \
    -DXIP_EXTERNAL_FLASH=1 \
    -DXIP_BOOT_HEADER_ENABLE=1 \
    -DXIP_BOOT_HEADER_DCD_ENABLE=1 \
    -DSKIP_SYSCLK_INIT \
    -DDATA_SECTION_IS_CACHEABLE=1 \
    -DNDEBUG \
    -DCPU_MIMXRT1052DVL6B \
    -DARM_MATH_CM7 \
    -D__FPU_PRESENT=1 \
    -DSDK_I2C_BASED_COMPONENT_USED=1 \
    -DTF_LITE_STATIC_MEMORY \
    -DMCUXPRESSO_SDK \
    -DSDK_I2C_BASED_COMPONENT_USED=1 \
    -DSERIAL_PORT_TYPE_UART=1 \
    -DCR_INTEGER_PRINTF \
    -DPRINTF_FLOAT_ENABLE=1 \
    -DPRINTF_ADVANCED_ENABLE=1 \
    -O3 \
    -Wall \
    -Wno-maybe-uninitialized \
    -Wno-strict-aliasing \
    -mcpu=cortex-m7 \
    -mthumb \
    -MMD \
    -MP \
    -fno-common \
    -ffunction-sections \
    -fdata-sections \
    -mapcs \
    -std=gnu99 \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
")
SET(CMAKE_C_FLAGS_FLEXSPI_NOR_SDRAM_DEBUG " \
    ${CMAKE_C_FLAGS_FLEXSPI_NOR_SDRAM_DEBUG} \
    -DXIP_EXTERNAL_FLASH=1 \
    -DXIP_BOOT_HEADER_ENABLE=1 \
    -DXIP_BOOT_HEADER_DCD_ENABLE=1 \
    -DSKIP_SYSCLK_INIT \
    -DDATA_SECTION_IS_CACHEABLE=1 \
    -DDEBUG \
    -DCPU_MIMXRT1052DVL6B \
    -DARM_MATH_CM7 \
    -D__FPU_PRESENT=1 \
    -DSDK_I2C_BASED_COMPONENT_USED=1 \
    -DTF_LITE_STATIC_MEMORY \
    -DMCUXPRESSO_SDK \
    -DSDK_I2C_BASED_COMPONENT_USED=1 \
    -DSERIAL_PORT_TYPE_UART=1 \
    -DCR_INTEGER_PRINTF \
    -DPRINTF_FLOAT_ENABLE=1 \
    -DPRINTF_ADVANCED_ENABLE=1 \
    -g \
    -O0 \
    -Wall \
    -Wno-maybe-uninitialized \
    -Wno-strict-aliasing \
    -mcpu=cortex-m7 \
    -mthumb \
    -MMD \
    -MP \
    -fno-common \
    -ffunction-sections \
    -fdata-sections \
    -mapcs \
    -std=gnu99 \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
")
SET(CMAKE_CXX_FLAGS_FLEXSPI_NOR_SDRAM_RELEASE " \
    ${CMAKE_CXX_FLAGS_FLEXSPI_NOR_SDRAM_RELEASE} \
    -DNDEBUG \
    -DCPU_MIMXRT1052DVL6B \
    -DARM_MATH_CM7 \
    -D__FPU_PRESENT=1 \
    -DSDK_I2C_BASED_COMPONENT_USED=1 \
    -DTF_LITE_STATIC_MEMORY \
    -DMCUXPRESSO_SDK \
    -DSERIAL_PORT_TYPE_UART=1 \
    -DPRINTF_FLOAT_ENABLE=1 \
    -DPRINTF_ADVANCED_ENABLE=1 \
    -O3 \
    -Wall \
    -fno-rtti \
    -fno-exceptions \
    -Wno-maybe-uninitialized \
    -Wno-sign-compare \
    -Wno-strict-aliasing \
    -Wno-deprecated-declarations \
    -mcpu=cortex-m7 \
    -mthumb \
    -MMD \
    -MP \
    -fno-common \
    -ffunction-sections \
    -fdata-sections \
    -mapcs \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
")
SET(CMAKE_CXX_FLAGS_FLEXSPI_NOR_SDRAM_DEBUG " \
    ${CMAKE_CXX_FLAGS_FLEXSPI_NOR_SDRAM_DEBUG} \
    -DDEBUG \
    -DCPU_MIMXRT1052DVL6B \
    -DARM_MATH_CM7 \
    -D__FPU_PRESENT=1 \
    -DSDK_I2C_BASED_COMPONENT_USED=1 \
    -DTF_LITE_STATIC_MEMORY \
    -DMCUXPRESSO_SDK \
    -DSERIAL_PORT_TYPE_UART=1 \
    -DPRINTF_FLOAT_ENABLE=1 \
    -DPRINTF_ADVANCED_ENABLE=1 \
    -g \
    -O0 \
    -Wall \
    -fno-rtti \
    -fno-exceptions \
    -Wno-maybe-uninitialized \
    -Wno-sign-compare \
    -Wno-strict-aliasing \
    -Wno-deprecated-declarations \
    -mcpu=cortex-m7 \
    -mthumb \
    -MMD \
    -MP \
    -fno-common \
    -ffunction-sections \
    -fdata-sections \
    -mapcs \
    ${FPU} \
    ${DEBUG_CONSOLE_CONFIG} \
")
SET(CMAKE_EXE_LINKER_FLAGS_FLEXSPI_NOR_SDRAM_RELEASE " \
    ${CMAKE_EXE_LINKER_FLAGS_FLEXSPI_NOR_SDRAM_RELEASE} \
    -mcpu=cortex-m7 \
    -Wall \
    -fno-common \
    -ffunction-sections \
    -fdata-sections \
    -mthumb \
    -mapcs \
    -Xlinker \
    --gc-sections \
    -Xlinker \
    -static \
    -Xlinker \
    -z \
    -Xlinker \
    muldefs \
    -Xlinker \
    -Map=output.map \
    -Wl,--print-memory-usage \
    -Xlinker \
    --defsym=__heap_size__=0x80000 \
    -Xlinker \
    --defsym=__stack_size__=0x2000 \
    ${FPU} \
    ${SPECS} \
    -T${S}/boards/evkbimxrt1050/MIMXRT1052xxxxx_flexspi_nor_sdram.ld -static \
")
SET(CMAKE_EXE_LINKER_FLAGS_FLEXSPI_NOR_SDRAM_DEBUG " \
    ${CMAKE_EXE_LINKER_FLAGS_FLEXSPI_NOR_SDRAM_DEBUG} \
    -g \
    -mcpu=cortex-m7 \
    -Wall \
    -fno-common \
    -ffunction-sections \
    -fdata-sections \
    -mthumb \
    -mapcs \
    -Xlinker \
    --gc-sections \
    -Xlinker \
    -static \
    -Xlinker \
    -z \
    -Xlinker \
    muldefs \
    -Xlinker \
    -Map=output.map \
    -Wl,--print-memory-usage \
    -Xlinker \
    --defsym=__heap_size__=0x80000 \
    -Xlinker \
    --defsym=__stack_size__=0x2000 \
    ${FPU} \
    ${SPECS} \
    -T${S}/boards/evkbimxrt1050/MIMXRT1052xxxxx_flexspi_nor_sdram.ld -static \
")
