/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_I2C_H_
#define BOARD_I2C_H_

#include "fsl_lpi2c.h"
#include "fsl_sccb.h"

#define BOARD_CAMERA_I2C_BASEADDR         LPI2C0
#define BOARD_CAMERA_I2C_CLOCK_FREQ       CLOCK_GetBusClkFreq()

void BOARD_LPI2C_Init(LPI2C_Type *base, uint32_t clkSrc_Hz);
status_t BOARD_LPI2C_Send(LPI2C_Type *base,
                          uint8_t deviceAddress,
                          uint32_t subAddress,
                          uint8_t subaddressSize,
                          uint8_t *txBuff,
                          uint8_t txBuffSize);
status_t BOARD_LPI2C_Receive(LPI2C_Type *base,
                             uint8_t deviceAddress,
                             uint32_t subAddress,
                             uint8_t subaddressSize,
                             uint8_t *rxBuff,
                             uint8_t rxBuffSize);

void BOARD_Camera_I2C_Init(void);
status_t BOARD_Camera_I2C_SendSCCB(
        uint8_t deviceAddress,
        uint32_t reg,
        sccb_reg_addr_t addrType,
        uint8_t val,
        uint8_t txBuffSize);
status_t BOARD_Camera_I2C_ReceiveSCCB(
        uint8_t deviceAddress,
        uint32_t reg,
        sccb_reg_addr_t addrType,
        const uint8_t *rxBuff,
        uint8_t rxBuffSize);

#endif /* BOARD_I2C_H_ */
