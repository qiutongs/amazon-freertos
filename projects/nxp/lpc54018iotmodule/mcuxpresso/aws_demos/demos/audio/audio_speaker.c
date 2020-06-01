/*
 * The Clear BSD License
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided
 * that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "audio_speaker.h"
#include "fsl_device_registers.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_debug_console.h"
#include <stdio.h>
#include <stdlib.h>

#include "fsl_sctimer.h"

#include "pin_mux.h"
#include "fsl_i2c.h"
#include "fsl_i2s.h"
#include "fsl_i2s_dma.h"
#include "fsl_wm8904.h"
#include "fsl_power.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define AUDIO_FORMAT_CHANNELS (0x01)
#define AUDIO_FORMAT_BITS (16)
#define AUDIO_SAMPLE_RATE (16000U)

#define BOARD_I2S_DEMO_I2C_BASEADDR (I2C2)
#define DEMO_I2C_MASTER_CLOCK_FREQUENCY (12000000)
#define DEMO_I2S_TX (I2S0)
#define DEMO_DMA (DMA0)
#define DEMO_I2S_TX_CHANNEL (13)
#define I2S_CLOCK_DIVIDER (24576000 / AUDIO_SAMPLE_RATE / AUDIO_FORMAT_BITS / AUDIO_FORMAT_CHANNELS)

/*******************************************************************************
 * Variables
 ******************************************************************************/
static i2s_config_t s_TxConfig;
static i2s_dma_handle_t s_TxHandle;
static dma_handle_t s_DmaTxHandle;

/*******************************************************************************
* Code
******************************************************************************/
extern void WM8904_Audio_Init(void *I2CBase);

void BOARD_Codec_Init()
{
    WM8904_Audio_Init(BOARD_I2S_DEMO_I2C_BASEADDR);
}

void I2S_Audio_TxInit(I2S_Type *SAIBase)
{
    /*
     * masterSlave = kI2S_MasterSlaveNormalMaster;
     * mode = kI2S_ModeI2sClassic;
     * rightLow = false;
     * leftJust = false;
     * pdmData = false;
     * sckPol = false;
     * wsPol = false;
     * divider = 1;
     * oneChannel = false;
     * dataLength = 16;
     * frameLength = 32;
     * position = 0;
     * watermark = 4;
     * txEmptyZero = true;
     * pack48 = false;
     */
    I2S_TxGetDefaultConfig(&s_TxConfig);
    s_TxConfig.divider = I2S_CLOCK_DIVIDER;
    I2S_TxInit(DEMO_I2S_TX, &s_TxConfig);
}

void BOARD_Audio_TxInit()
{
    I2S_Audio_TxInit(DEMO_I2S_TX);
}

void BOARD_I2C_LPI2C_Init()
{
    i2c_master_config_t i2cConfig;
    /*
     * enableMaster = true;
     * baudRate_Bps = 100000U;
     * enableTimeout = false;
     */
    I2C_MasterGetDefaultConfig(&i2cConfig);
    i2cConfig.baudRate_Bps = WM8904_I2C_BITRATE;
    I2C_MasterInit(BOARD_I2S_DEMO_I2C_BASEADDR, &i2cConfig, DEMO_I2C_MASTER_CLOCK_FREQUENCY);
}

static void TxCallback(I2S_Type *base, i2s_dma_handle_t *handle, status_t completionStatus, void *userData)
{
	int i = 2;
	i++;
}

void playData(size_t dataSize, uint8_t *data)
{
	if (dataSize > 0)
	{
		i2s_transfer_t txTransfer;
		txTransfer.dataSize = dataSize;
		txTransfer.data = data;
		I2S_TxTransferSendDMA(DEMO_I2S_TX, &s_TxHandle, txTransfer);
	}
}

void BOARD_DMA_EDMA_Config()
{
    DMA_Init(DEMO_DMA);

    DMA_EnableChannel(DEMO_DMA, DEMO_I2S_TX_CHANNEL);
    DMA_SetChannelPriority(DEMO_DMA, DEMO_I2S_TX_CHANNEL, kDMA_ChannelPriority3);
}

void BOARD_Create_Audio_DMA_EDMA_Handle()
{
    DMA_CreateHandle(&s_DmaTxHandle, DEMO_DMA, DEMO_I2S_TX_CHANNEL);
}

void BOARD_DMA_EDMA_Start()
{
    static uint8_t audioPlayDMATempBuff[192];
	memset(audioPlayDMATempBuff, 0, sizeof(audioPlayDMATempBuff));

	i2s_transfer_t txTransfer;
    txTransfer.dataSize = sizeof(audioPlayDMATempBuff);
    txTransfer.data = audioPlayDMATempBuff;

    I2S_TxTransferCreateHandleDMA(DEMO_I2S_TX, &s_TxHandle, &s_DmaTxHandle, TxCallback, (void *)&txTransfer);
    /* need to queue two transmit buffers so when the first one
     * finishes transfer, the other immediatelly starts */

    I2S_TxTransferSendDMA(DEMO_I2S_TX, &s_TxHandle, txTransfer);
    I2S_TxTransferSendDMA(DEMO_I2S_TX, &s_TxHandle, txTransfer);
}

/* Initialize the structure information for sai. */
void Init_Board_Sai_Codec(void)
{
    printf(("Init Audio SAI and CODEC\r\n"));

    BOARD_Audio_TxInit();
    BOARD_I2C_LPI2C_Init();
    BOARD_Codec_Init();
    BOARD_DMA_EDMA_Config();
    BOARD_Create_Audio_DMA_EDMA_Handle();
}
