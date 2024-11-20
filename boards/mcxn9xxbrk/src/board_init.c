/*
 * Copyright 2022-2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "board_init.h"
#include "pin_mux.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "board_i2c.h"

void BOARD_Init()
{
    /* Init uart. */
    CLOCK_AttachClk(kFRO12M_to_FLEXCOMM6);
    CLOCK_EnableClock(kCLOCK_LPFlexComm6);
    CLOCK_EnableClock(kCLOCK_LPUart6);
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom6Clk, 1u);

    /* init I2C0 clocks */
    CLOCK_AttachClk(kFRO12M_to_FLEXCOMM0);
    CLOCK_EnableClock(kCLOCK_LPFlexComm0);
    CLOCK_EnableClock(kCLOCK_LPI2c0);
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom0Clk, 1u);

    BOARD_InitPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
    BOARD_Camera_I2C_Init();

    /* Enable caching of flash memory */
    SYSCON->LPCAC_CTRL = SYSCON->LPCAC_CTRL & !SYSCON_LPCAC_CTRL_DIS_LPCAC_MASK;
    SYSCON->NVM_CTRL = SYSCON->NVM_CTRL & !SYSCON_NVM_CTRL_DIS_FLASH_DATA_MASK;
}
