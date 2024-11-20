/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "board.h"
#include "fsl_lpi2c.h"
#include "board_i2c.h"
#include "fsl_clock.h"
#include "fsl_sccb.h"
#include "fsl_lpflexcomm.h"
#include "fsl_debug_console.h"


void BOARD_LPI2C_Init(LPI2C_Type *base, uint32_t clkSrc_Hz)
{
    lpi2c_master_config_t lpi2cConfig = {0};

    /*
     * lpi2cConfig.debugEnable = false;
     * lpi2cConfig.ignoreAck = false;
     * lpi2cConfig.pinConfig = kLPI2C_2PinOpenDrain;
     * lpi2cConfig.baudRate_Hz = 100000U;
     * lpi2cConfig.busIdleTimeout_ns = 0;
     * lpi2cConfig.pinLowTimeout_ns = 0;
     * lpi2cConfig.sdaGlitchFilterWidth_ns = 0;
     * lpi2cConfig.sclGlitchFilterWidth_ns = 0;
     */
    LPI2C_MasterGetDefaultConfig(&lpi2cConfig);
    LPI2C_MasterInit(base, &lpi2cConfig, clkSrc_Hz);
}

void BOARD_Camera_I2C_Init(void)
{
    LP_FLEXCOMM_Init(0, LP_FLEXCOMM_PERIPH_LPI2C);
    BOARD_LPI2C_Init(BOARD_CAMERA_I2C_BASEADDR, CLOCK_GetFreq(kCLOCK_Fro12M));
}

/*
 * For SCCB receive, the workflow is Start -> Write -> Stop -> Start -> Read -> Stop,
 * Repeat start is not supported.
 */
status_t BOARD_Camera_I2C_ReceiveSCCB(
        uint8_t deviceAddress,
        uint32_t reg,
        sccb_reg_addr_t addrType,
        const uint8_t *rxBuff,
        uint8_t rxBuffSize)
{
    status_t ret;
    lpi2c_master_transfer_t xfer;

    /* write slave reg address */
    xfer.flags          = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress   = deviceAddress;
    xfer.direction      = kLPI2C_Write;
    xfer.subaddress     = 0;
    xfer.subaddressSize = 0;
    xfer.data           = &reg;
    xfer.dataSize       = 1;

    ret = LPI2C_MasterTransferBlocking(BOARD_CAMERA_I2C_BASEADDR, &xfer);
    if (ret != kStatus_Success) {
        PRINTF("\nLPI2C error: failed Sending the Read address to slave.");
        return ret;
    }

    /* read slave reg value */
    xfer.flags          = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress   = deviceAddress;
    xfer.direction      = kLPI2C_Read;
    xfer.subaddress     = 0;
    xfer.subaddressSize = 0;
    xfer.data           = (uint8_t *) rxBuff;
    xfer.dataSize       = 1;

    if (ret != kStatus_Success) {
        PRINTF("\nLPI2C error: failed Reading data from slave.");
        return ret;
    }
    return LPI2C_MasterTransferBlocking(BOARD_CAMERA_I2C_BASEADDR, &xfer);
}

status_t BOARD_Camera_I2C_SendSCCB(
        uint8_t deviceAddress,
        uint32_t reg,
        sccb_reg_addr_t addrType,
        uint8_t val,
        uint8_t txBuffSize)
{
    status_t ret;
    lpi2c_master_transfer_t xfer;
    uint8_t txData[2] = {reg, val};

    /* write slave reg address */
    xfer.flags          = kLPI2C_TransferDefaultFlag;
    xfer.slaveAddress   = deviceAddress;
    xfer.direction      = kLPI2C_Write;
    xfer.subaddress     = 0;
    xfer.subaddressSize = 0;
    xfer.data           = txData;
    xfer.dataSize       = 2;

    ret = LPI2C_MasterTransferBlocking(BOARD_CAMERA_I2C_BASEADDR, &xfer);
    if (ret != kStatus_Success) {
        PRINTF("\nLPI2C error: failed Sending the Read address to slave.");
        return ret;
    }
    return ret;
}
