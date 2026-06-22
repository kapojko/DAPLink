/*
 * Copyright (c) 2004-2013,2022 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

/*----------------------------------------------------------------------------
 *      RL-ARM - USB
 *----------------------------------------------------------------------------
 *      Name:    usbd_STM32H7xx.c
 *      Purpose: Hardware Layer module for ST STM32H723
 *               Uses USB1_OTG_HS in Full-Speed mode with the embedded FS PHY
 *               (no external ULPI). Clock source is HSI48.
 *
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2013 KEIL - An ARM Company. All rights reserved.
 *---------------------------------------------------------------------------*/

#include <rl_usb.h>
#include <stdio.h>
#include "stm32h7xx.h"
#include "cmsis_compiler.h"
#include "cmsis_gcc.h"
#include "usb_def.h"
#include "util.h"
#include "IO_Config.h"

#define __NO_USB_LIB_C
#include "usb_config.c"

// STM32H723 has only one OTG controller - the OTG_HS one. We use it in
// Full-Speed mode by selecting the embedded PHY via GUSBCFG.PHYSEL.
#define OTG             USB1_OTG_HS
#define USBx_BASE       USB1_OTG_HS_PERIPH_BASE

// USBx_DEVICE / USBx_INEP / USBx_OUTEP / USBx_DFIFO are provided by
// stm32h7xx_ll_usb.h (included transitively via stm32h7xx.h).

// 00: Control, 01: Isochronous, 10: Bulk, 11: Interrupt
#define EP_IN_TYPE(num)     ((USBx_INEP(num)->DIEPCTL & USB_OTG_DIEPCTL_EPTYP) >> USB_OTG_DIEPCTL_EPTYP_Pos)
#define EP_OUT_TYPE(num)    ((USBx_OUTEP(num)->DOEPCTL & USB_OTG_DOEPCTL_EPTYP) >> USB_OTG_DOEPCTL_EPTYP_Pos)

// Full-speed FIFO layout: 1.28 KB total, plenty for FS endpoints.
// 64-byte max packet on FS bulk / interrupt EPs.
#define RX_FIFO_SIZE    512     // RX (increased from 256 for bulk transfer headroom)
#define TX0_FIFO_SIZE   64      // EP0
#define TX1_FIFO_SIZE   64      // HID INT IN / MSC bulk IN / CDC INT IN
#define TX2_FIFO_SIZE   64
#define TX3_FIFO_SIZE   64
#define TX4_FIFO_SIZE   64
#define TX5_FIFO_SIZE   64

// FIFO total must fit in 1.25 KB of dedicated USB FS RAM on this chip.
COMPILER_ASSERT((RX_FIFO_SIZE + TX0_FIFO_SIZE + TX1_FIFO_SIZE + TX2_FIFO_SIZE +
                 TX3_FIFO_SIZE + TX4_FIFO_SIZE + TX5_FIFO_SIZE) <= 1280);

// EP size sanity checks
COMPILER_ASSERT((USBD_HID_EP_INTIN == 0) || (USBD_HID_EP_INTIN <= 5));
COMPILER_ASSERT((USBD_MSC_EP_BULKIN == 0) || (USBD_MSC_EP_BULKIN <= 5));
COMPILER_ASSERT((USBD_BULK_EP_BULKIN == 0) || (USBD_BULK_EP_BULKIN <= 5));
COMPILER_ASSERT((USBD_CDC_ACM_EP_INTIN == 0) || (USBD_CDC_ACM_EP_INTIN <= 5));
COMPILER_ASSERT((USBD_CDC_ACM_EP_BULKIN == 0) || (USBD_CDC_ACM_EP_BULKIN <= 5));

#define TXFIFO_WAIT_TIMEOUT_MS    (50)

#if (USBD_HID_ENABLE == 1)
static uint32_t HID_IntInPacketData[(USBD_HID_MAX_PACKET + 3) / 4];
#endif

#if (USBD_CDC_ACM_ENABLE == 1)
static uint32_t CDC_ACM_IntInPacketData[(USBD_CDC_ACM_MAX_PACKET + 3) / 4];
#endif

// InPacketDataPtr[EP#] points to the intermediate buffer used for asynchronous
// writes from USBD_WriteEP. Indexed by EP number 0..USBD_EP_NUM-1.
static uint32_t *InPacketDataPtr[USBD_EP_NUM] =
{
    0,
#if (USBD_EP_NUM > 1)
#if ((USBD_HID_ENABLE == 1) && (USBD_HID_EP_INTIN == 1))
    HID_IntInPacketData,
#else
    0,
#endif
#endif
#if (USBD_EP_NUM > 2)
#if ((USBD_MSC_ENABLE == 1) && (USBD_MSC_EP_BULKIN == 2))
    0,
#elif ((USBD_BULK_ENABLE == 1) && (USBD_BULK_EP_BULKIN == 2))
    0,
#else
    0,
#endif
#endif
#if (USBD_EP_NUM > 3)
#if ((USBD_CDC_ACM_ENABLE == 1) && (USBD_CDC_ACM_EP_INTIN == 3))
    CDC_ACM_IntInPacketData,
#else
    0,
#endif
#endif
#if (USBD_EP_NUM > 4)
#if ((USBD_CDC_ACM_ENABLE == 1) && (USBD_CDC_ACM_EP_BULKIN == 4))
    0,
#else
    0,
#endif
#endif
};

static uint32_t OutMaxPacketSize[USBD_EP_NUM];
static uint8_t  OutPacketCnt[USBD_EP_NUM];
static uint8_t  InPacketCnt[USBD_EP_NUM];
static uint32_t InPacketDataCnt[USBD_EP_NUM];
static uint32_t InPacketDataReady;
static uint32_t SyncWriteEP;

/*
 *  USB Device Interrupt enable
 */
static void USBD_IntrEna(void)
{
    NVIC_EnableIRQ(OTG_HS_IRQn); /* Enable OTG_HS interrupt */
}

/*
 *  USB Device Initialize Function
 *
 *  Configures the OTG_HS peripheral in device mode, full-speed,
 *  embedded FS PHY. Clock source is HSI48, configured by sdk.c.
 */
void USBD_Init(void)
{
    int32_t timeout_cntr;

    GPIO_InitTypeDef gpio_init = {0};

    // Enable the USB voltage detector (required for the H7 OTG peripheral).
    // The internal USB regulator (USBREGEN) is NOT enabled here: this board
    // provides VDD33USB externally, matching the cube demo for the same PCB
    // (verified: cube example does not call HAL_PWREx_EnableUSBReg either).
    HAL_PWREx_EnableUSBVoltageDetector();

    // Enable the USB OTG HS peripheral clock (and the embedded PHY clock).
    __HAL_RCC_USB1_OTG_HS_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // PA11 = OTG1_FS_DM, PA12 = OTG1_FS_DP, AF10. Must be configured as
    // AF before any OTG register access - the H743 reference (ULPI) does
    // this in USBD_Init too.
    gpio_init.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF10_OTG1_FS;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    // Select the embedded FS PHY (reset value of GUSBCFG.PHYSEL = 0 selects
    // ULPI, which is wrong for H723 - this chip has only the embedded PHY
    // on USB1_OTG_HS). Power up the FS transceiver via GCCFG.PWRDWN.
    // USB_CoreReset inside USB_SetCurrentMode below takes care of the
    // post-PHYSEL core reset.
    OTG->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL;
    OTG->GCCFG   |= USB_OTG_GCCFG_PWRDWN;

    // This board has VBUS sensing pin (PA9 is connected to VBUS), but it is not used currently.
    // Force the OTG core to consider B-session always valid so it responds
    // to host packets. Without this, the device never sees USBRST and the
    // host does not enumerate. Same logic as HAL PCD's USB_DevInit() when
    // cfg.vbus_sensing_enable == 0.
    OTG->GCCFG    &= ~USB_OTG_GCCFG_VBDEN;
    OTG->GOTGCTL  |= USB_OTG_GOTGCTL_BVALOEN;
    OTG->GOTGCTL  |= USB_OTG_GOTGCTL_BVALOVAL;

    // Wait for AHB idle after the OTG clock is brought up.
    timeout_cntr = 1000;
    while (!(OTG->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL))
    {
        if (!timeout_cntr)
        {
            break;
        }
        timeout_cntr--;
        HAL_Delay(1);
    }

    // Core soft reset
    OTG->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;
    timeout_cntr = 1000;
    while (OTG->GRSTCTL & USB_OTG_GRSTCTL_CSRST)
    {
        if (!timeout_cntr)
        {
            break;
        }
        timeout_cntr--;
        HAL_Delay(1);
    }
    HAL_Delay(3);

    // Configure for full-speed with embedded PHY.
    USB_SetCurrentMode(OTG, USB_DEVICE_MODE);
    USB_SetDevSpeed(OTG, USB_OTG_SPEED_FULL);

    OTG->GINTMSK =
        USB_OTG_GINTMSK_USBSUSPM |  // suspend int unmask
        USB_OTG_GINTMSK_USBRST |    // reset int unmask
        USB_OTG_GINTMSK_ENUMDNEM |  // enumeration done int unmask
        USB_OTG_GINTMSK_RXFLVLM |   // receive fifo non-empty int unmask
        USB_OTG_GINTMSK_IEPINT |    // IN EP int unmask
        USB_OTG_GINTMSK_OEPINT |    // OUT EP int unmask
        USB_OTG_GINTMSK_WUIM |      // resume int unmask
        ((USBD_P_SOF_Event != 0) ? USB_OTG_GINTMSK_SOFM : 0); // SOF int unmask

    USBD_IntrEna();
    USB_EnableGlobalInt(OTG);
}

/*
 *  USB Device Connect Function
 */
void USBD_Connect(BOOL con)
{
    if (con)
    {
        USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_SDIS;
    }
    else
    {
        USBx_DEVICE->DCTL |= USB_OTG_DCTL_SDIS;
    }
}

/*
 *  USB Device Reset Function
 */
void USBD_Reset(void)
{
    uint32_t fifo_offset;

    SyncWriteEP = 0;
    InPacketDataReady = 0;

    USB_StopDevice(OTG);

    for (uint32_t i = 0; i < (USBD_EP_NUM + 1); i++)
    {
        if (USBx_OUTEP(i)->DOEPCTL & USB_OTG_DOEPCTL_EPENA)
        {
            USBx_OUTEP(i)->DOEPCTL = USB_OTG_DOEPCTL_EPDIS | USB_OTG_DOEPCTL_SNAK;
        }
        if (USBx_INEP(i)->DIEPCTL & USB_OTG_DIEPCTL_EPENA)
        {
            USBx_INEP(i)->DIEPCTL = USB_OTG_DIEPCTL_EPDIS | USB_OTG_DIEPCTL_SNAK;
        }
    }

    USBD_SetAddress(0, 1);

    USBx_DEVICE->DAINTMSK = (1 << USB_OTG_DAINTMSK_OEPM_Pos) |
                            (1 << USB_OTG_DAINTMSK_IEPM_Pos);
    USBx_DEVICE->DOEPMSK = USB_OTG_DOEPMSK_STUPM |
                           USB_OTG_DOEPMSK_EPDM |
                           USB_OTG_DOEPMSK_XFRCM;
    USBx_DEVICE->DIEPMSK = USB_OTG_DIEPMSK_EPDM |
                           USB_OTG_DIEPMSK_XFRCM;

    // Program the RX FIFO size
    OTG->GRXFSIZ = RX_FIFO_SIZE / 4;
    fifo_offset = RX_FIFO_SIZE;

    // EP0 TX FIFO
    OTG->DIEPTXF0_HNPTXFSIZ = (fifo_offset / 4) | ((TX0_FIFO_SIZE / 4) << 16);
    fifo_offset += TX0_FIFO_SIZE;
    // EP1..EP5 TX FIFOs
    OTG->DIEPTXF[0] = (fifo_offset / 4) | ((TX1_FIFO_SIZE / 4) << 16);
    fifo_offset += TX1_FIFO_SIZE;
    OTG->DIEPTXF[1] = (fifo_offset / 4) | ((TX2_FIFO_SIZE / 4) << 16);
    fifo_offset += TX2_FIFO_SIZE;
    OTG->DIEPTXF[2] = (fifo_offset / 4) | ((TX3_FIFO_SIZE / 4) << 16);
    fifo_offset += TX3_FIFO_SIZE;
    OTG->DIEPTXF[3] = (fifo_offset / 4) | ((TX4_FIFO_SIZE / 4) << 16);
    fifo_offset += TX4_FIFO_SIZE;
    OTG->DIEPTXF[4] = (fifo_offset / 4) | ((TX5_FIFO_SIZE / 4) << 16);

    USBx_OUTEP(0)->DOEPTSIZ = USB_OTG_DOEPTSIZ_STUPCNT_0 |
                              (USBD_OUT_PACKET_CNT0 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |
                              USBD_MAX_PACKET0;

    OutMaxPacketSize[0] = USBD_MAX_PACKET0;
    OutPacketCnt[0] = USBD_OUT_PACKET_CNT0;
    InPacketCnt[0] = USBD_IN_PACKET_CNT0;
}

void USBD_Suspend(void) {}
void USBD_Resume(void)  {}
void USBD_WakeUp(void)  {}

void USBD_WakeUpCfg(BOOL cfg) {}

void USBD_SetAddress(U32 adr, U32 setup)
{
    if (setup)
    {
        USB_SetDevAddress(OTG, (uint8_t)adr);
    }
}

void USBD_Configure(BOOL cfg)
{
    InPacketDataReady &= ~1UL;
}

void USBD_ConfigEP(USB_ENDPOINT_DESCRIPTOR *pEPD)
{
    uint32_t num = (pEPD->bEndpointAddress & ~USB_ENDPOINT_DIRECTION_MASK);
    uint32_t val = pEPD->wMaxPacketSize;
    uint32_t type = (pEPD->bmAttributes & USB_ENDPOINT_TYPE_MASK);

    if (pEPD->bEndpointAddress & USB_ENDPOINT_DIRECTION_MASK)
    {
        InPacketCnt[num] = 1;

        USBx_DEVICE->DAINTMSK |= (1 << num);
        USBx_INEP(num)->DIEPCTL = (num << USB_OTG_DIEPCTL_TXFNUM_Pos) |
                                  (type << USB_OTG_DIEPCTL_EPTYP_Pos) |
                                  (val & USB_OTG_DIEPCTL_MPSIZ);
        if ((type == USB_ENDPOINT_TYPE_BULK) || (type == USB_ENDPOINT_TYPE_INTERRUPT))
        {
            USBx_INEP(num)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
        }
    }
    else
    {
        OutMaxPacketSize[num] = val & USB_OTG_DOEPCTL_MPSIZ;
        OutPacketCnt[num] = 1;

        USBx_DEVICE->DAINTMSK |= (1 << (num + 16));

        USBx_OUTEP(num)->DOEPCTL = (type << USB_OTG_DOEPCTL_EPTYP_Pos) |
                                   (val & USB_OTG_DOEPCTL_MPSIZ);

        USBx_OUTEP(num)->DOEPTSIZ = (OutPacketCnt[num] << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |
                                    (val & USB_OTG_DOEPCTL_MPSIZ);
        if ((type == USB_ENDPOINT_TYPE_BULK) || (type == USB_ENDPOINT_TYPE_INTERRUPT))
        {
            USBx_OUTEP(num)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
        }
    }
}

void USBD_DirCtrlEP(U32 dir) {}

void USBD_EnableEP(U32 EPNum)
{
    if (EPNum & USB_ENDPOINT_DIRECTION_MASK)
    {
        EPNum &= ~USB_ENDPOINT_DIRECTION_MASK;
        USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_USBAEP |
                                     USB_OTG_DIEPCTL_SNAK;
        if (USBx_INEP(EPNum)->DIEPCTL & USB_OTG_DIEPCTL_EPENA)
        {
            USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;
        }

        InPacketDataReady &= ~(1 << EPNum);
    }
    else
    {
        USBx_OUTEP(EPNum)->DOEPCTL |= USB_OTG_DOEPCTL_USBAEP |
                                      USB_OTG_DOEPCTL_EPENA |
                                      USB_OTG_DOEPCTL_CNAK;
    }
}

void USBD_DisableEP(U32 EPNum)
{
    uint32_t wcnt;

    if (EPNum & USB_ENDPOINT_DIRECTION_MASK)
    {
        EPNum &= ~USB_ENDPOINT_DIRECTION_MASK;
        InPacketDataReady &= ~(1 << EPNum);

        if (USBx_INEP(EPNum)->DIEPCTL & USB_OTG_DIEPCTL_EPENA)
        {
            USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;
        }
        USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
        USBx_INEP(EPNum)->DIEPCTL &= ~USB_OTG_DIEPCTL_USBAEP;
    }
    else
    {
        USBx_DEVICE->DCTL |= USB_OTG_DCTL_SGONAK;

        wcnt = 1000;
        while (!(OTG->GINTSTS & USB_OTG_GINTSTS_BOUTNAKEFF))
        {
            if ((wcnt--) == 0) { break; }
        }

        if (USBx_OUTEP(EPNum)->DOEPCTL & USB_OTG_DOEPCTL_EPENA)
        {
            USBx_OUTEP(EPNum)->DOEPCTL |= USB_OTG_DOEPCTL_EPDIS;
        }
        USBx_OUTEP(EPNum)->DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
        USBx_OUTEP(EPNum)->DOEPCTL &= ~USB_OTG_DOEPCTL_USBAEP;

        wcnt = 1000;
        while (!(USBx_OUTEP(EPNum)->DOEPINT & USB_OTG_DOEPINT_EPDISD))
        {
            if ((wcnt--) == 0) { break; }
        }
        USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGONAK;
    }
}

void USBD_ResetEP(U32 EPNum)
{
    if (EPNum & USB_ENDPOINT_DIRECTION_MASK)
    {
        EPNum &= ~USB_ENDPOINT_DIRECTION_MASK;
        InPacketDataReady &= ~(1 << EPNum);
        if (USBx_INEP(EPNum)->DIEPCTL & USB_OTG_DIEPCTL_EPENA)
        {
            USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;
        }
        USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_SNAK;

        USB_FlushTxFifo(OTG, EPNum);
    }
}

void USBD_SetStallEP(U32 EPNum)
{
    uint32_t wcnt;

    if (!(EPNum & USB_ENDPOINT_DIRECTION_MASK))
    {
        USBx_DEVICE->DCTL |= USB_OTG_DCTL_SGONAK;
        wcnt = 1000;
        while (!(OTG->GINTSTS & USB_OTG_GINTSTS_BOUTNAKEFF))
        {
            if ((wcnt--) == 0) { break; }
        }

        if (USBx_OUTEP(EPNum)->DOEPCTL & USB_OTG_DOEPCTL_EPENA)
        {
            USBx_OUTEP(EPNum)->DOEPCTL |= USB_OTG_DOEPCTL_EPDIS;
        }
        USBx_OUTEP(EPNum)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;

        wcnt = 1000;
        while (!(USBx_OUTEP(EPNum)->DOEPINT & USB_OTG_DOEPINT_EPDISD))
        {
            if ((wcnt--) == 0) { break; }
        }
        USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGONAK;
    }
    else
    {
        EPNum &= ~USB_ENDPOINT_DIRECTION_MASK;
        if (USBx_INEP(EPNum)->DIEPCTL & USB_OTG_DIEPCTL_EPENA)
        {
            USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;
        }
        USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;

        USB_FlushTxFifo(OTG, EPNum);
    }
}

void USBD_ClrStallEP(U32 EPNum)
{
    if (!(EPNum & USB_ENDPOINT_DIRECTION_MASK))
    {
        if (EP_OUT_TYPE(EPNum) > 1)
        {
            USBx_OUTEP(EPNum)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
            USBx_OUTEP(EPNum)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
        }
    }
    else
    {
        EPNum &= ~USB_ENDPOINT_DIRECTION_MASK;

        if (USBx_INEP(EPNum)->DIEPCTL & USB_OTG_DIEPCTL_EPENA)
        {
            USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;
        }

        USB_FlushTxFifo(OTG, EPNum);

        if (EP_IN_TYPE(EPNum) > 1)
        {
            USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
        }
        USBx_INEP(EPNum)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
    }
}

void USBD_ClearEPBuf(U32 EPNum)
{
    if (EPNum & USB_ENDPOINT_DIRECTION_MASK)
    {
        EPNum &= ~USB_ENDPOINT_DIRECTION_MASK;
        USB_FlushTxFifo(OTG, EPNum);
    }
    else
    {
        USB_FlushRxFifo(OTG);
    }
}

uint32_t USBD_ReadEP(U32 EPNum, U8 *pData, uint32_t bufsz)
{
    U32 val, sz;

    if ((USBx_OUTEP(EPNum)->DOEPCTL & USB_OTG_DOEPCTL_USBAEP) == 0)
    {
        return (0);
    }

    sz = (OTG->GRXSTSP & USB_OTG_GRXSTSP_BCNT) >> USB_OTG_GRXSTSP_BCNT_Pos;

    if (sz > bufsz)
    {
        // Not enough buffer space - still must pop the FIFO entry.
        util_assert(false);
    }
    else
    {
        for (val = 0; val < (uint32_t)((sz + 3) / 4); val++)
        {
            __UNALIGNED_UINT32_WRITE(pData, USBx_DFIFO(0U));
            pData += 4;
        }
    }

    /* Re-enable RX FIFO interrupt. The completion entry (STS_XFER_COMP /
     * STS_SETUP_COMP) will be handled by the RXFLVL handler in USBD_Handler
     * on the next interrupt — no need to busy-wait for it here. */
    OTG->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;

    return (sz);
}

uint32_t USBD_WriteEP(U32 EPNum, U8 *pData, U32 cnt)
{
    U32 *ptr, val;
    U32 txfifo_wait_start_time_ms;

    EPNum &= ~USB_ENDPOINT_DIRECTION_MASK;

    if ((USBx_INEP(EPNum)->DIEPCTL & USB_OTG_DIEPCTL_USBAEP) == 0)
    {
        return (0);
    }

    if (!SyncWriteEP && InPacketDataPtr[EPNum])
    {
        if (!(InPacketDataReady & (1 << EPNum)))
        {
            InPacketDataCnt[EPNum] = cnt;
            ptr = InPacketDataPtr[EPNum];
            val = (cnt + 3) / 4;
            if (val)
            {
                while (val--)
                {
                    *ptr++ = *((U32 *)pData);
                    pData += 4;
                }
            }
            InPacketDataReady |= 1 << EPNum;
            USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
            USBx_DEVICE->DIEPMSK |= USB_OTG_DIEPMSK_INEPNEM;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        txfifo_wait_start_time_ms = HAL_GetTick();
        while (cnt > 0 && (USBx_INEP(EPNum)->DTXFSTS * 4) < cnt)
        {
            if (HAL_GetTick() - txfifo_wait_start_time_ms >= TXFIFO_WAIT_TIMEOUT_MS)
            {
                USBD_ResetEP(EPNum | USB_ENDPOINT_DIRECTION_MASK);
                return 0;
            }
        }

        USBx_INEP(EPNum)->DIEPTSIZ =
            cnt | (InPacketCnt[EPNum] << USB_OTG_DIEPTSIZ_PKTCNT_Pos) |
                 (InPacketCnt[EPNum] << USB_OTG_DIEPTSIZ_MULCNT_Pos);

        USBx_INEP(EPNum)->DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;
        if (cnt)
        {
            ptr = (uint32_t *)pData;
            val = (cnt + 3) / 4;
            while (val--)
            {
                USBx_DFIFO((uint32_t)EPNum) = __UNALIGNED_UINT32_READ(pData);
                pData += 4;
            }
        }
        InPacketDataReady &= ~(1 << EPNum);
    }
    return (cnt);
}

uint32_t USBD_GetFrame(void)
{
    return ((USBx_DEVICE->DSTS & USB_OTG_DSTS_FNSOF) >> USB_OTG_DSTS_FNSOF_Pos);
}

/*
 *  USB Device IRQ handler (just signal the deferred handler).
 */
void OTG_HS_IRQHandler(void)
{
    NVIC_DisableIRQ(OTG_HS_IRQn);
    USBD_SignalHandler();
}

void USBD_Handler(void)
{
    uint32_t istr, val, num, i, msk;

    istr = OTG->GINTSTS & OTG->GINTMSK;

    // Reset
    if (istr & USB_OTG_GINTSTS_USBRST)
    {
        USBD_Reset();
        usbd_reset_core();
        if (USBD_P_Reset_Event)
        {
            USBD_P_Reset_Event();
        }
        OTG->GINTSTS = USB_OTG_GINTSTS_USBRST;
    }

    // Suspend
    if (istr & USB_OTG_GINTSTS_USBSUSP)
    {
        USBD_Suspend();
        if (USBD_P_Suspend_Event)
        {
            USBD_P_Suspend_Event();
        }
        OTG->GINTSTS = USB_OTG_GINTSTS_USBSUSP;
    }

    // Resume
    if (istr & USB_OTG_GINTSTS_WKUINT)
    {
        USBD_Resume();
        if (USBD_P_Resume_Event)
        {
            USBD_P_Resume_Event();
        }
        OTG->GINTSTS = USB_OTG_GINTSTS_WKUINT;
    }

    // Enumeration done - we are full-speed, set EP0 MPS
    if (istr & USB_OTG_GINTSTS_ENUMDNE)
    {
        USBD_HighSpeed = 0;
        USBx_INEP(0)->DIEPCTL &= ~USB_OTG_DIEPCTL_MPSIZ;
        USBx_INEP(0)->DIEPCTL |= OutMaxPacketSize[0];
        USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGINAK;
        USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGONAK;
        OTG->GINTSTS |= USB_OTG_GINTSTS_ENUMDNE;
    }

    // Start Of Frame
    if (istr & USB_OTG_GINTSTS_SOF)
    {
        if (USBD_P_SOF_Event)
        {
            USBD_P_SOF_Event();
        }
        OTG->GINTSTS = USB_OTG_GINTSTS_SOF;
    }

    // RxFIFO non-empty
    if (istr & USB_OTG_GINTSTS_RXFLVL)
    {
        val = OTG->GRXSTSR;
        num = val & USB_OTG_GRXSTSP_EPNUM;

        switch ((val & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos)
        {
            case STS_SETUP_UPDT:
                if (USBD_P_EP[num])
                {
                    USBD_P_EP[num](USBD_EVT_SETUP);
                }
                break;

            case STS_DATA_UPDT:
                OTG->GINTMSK &= ~USB_OTG_GINTMSK_RXFLVLM;
                if (USBD_P_EP[num])
                {
                    USBD_P_EP[num](USBD_EVT_OUT);
                }
                break;

            case STS_XFER_COMP:
            case STS_SETUP_COMP:
                OTG->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
                OTG->GRXSTSP;
                break;

            case STS_GOUT_NAK:
                OTG->GRXSTSP;
                break;

            default:
                OTG->GRXSTSP;
                util_assert(false);
                break;
        }
    }

    // OUT EP interrupt
    if (istr & USB_OTG_GINTSTS_OEPINT)
    {
        msk = ((USBx_DEVICE->DAINT & USBx_DEVICE->DAINTMSK & USB_OTG_DAINTMSK_OEPM) >> USB_OTG_DAINTMSK_OEPM_Pos);
        i = 0;
        while (msk)
        {
            num = 0;
            for (; i < (USBD_EP_NUM + 1); i++)
            {
                if ((msk >> i) & 1)
                {
                    num = i;
                    msk &= ~(1 << i);
                    break;
                }
            }

            if (USBx_OUTEP(num)->DOEPINT & USB_OTG_DOEPINT_EPDISD)
            {
                USBx_OUTEP(num)->DOEPINT |= USB_OTG_DOEPINT_EPDISD;
            }

            if ((USBx_OUTEP(num)->DOEPINT & USB_OTG_DOEPINT_XFRC) |
                (USBx_OUTEP(num)->DOEPINT & USB_OTG_DOEPINT_STUP))
            {
                USBx_OUTEP(num)->DOEPTSIZ =
                    (OutPacketCnt[num] << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |
                    OutMaxPacketSize[num];
                if (num == 0)
                {
                    USBx_OUTEP(0)->DOEPTSIZ |= USB_OTG_DOEPTSIZ_STUPCNT_0;
                }
                USBx_OUTEP(num)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
                USBx_OUTEP(num)->DOEPINT |= USB_OTG_DOEPINT_XFRC;
            }
        }
    }

    // IN EP interrupt
    if (istr & USB_OTG_GINTSTS_IEPINT)
    {
        msk = (USBx_DEVICE->DAINT & USBx_DEVICE->DAINTMSK & USB_OTG_DAINTMSK_IEPM);
        i = 0;
        while (msk)
        {
            num = 0;
            for (; i < (USBD_EP_NUM + 1); i++)
            {
                if ((msk >> i) & 1)
                {
                    num = i;
                    msk &= ~(1 << i);
                    break;
                }
            }

            if (USBx_INEP(num)->DIEPINT & USB_OTG_DIEPINT_EPDISD)
            {
                USBx_INEP(num)->DIEPINT = USB_OTG_DIEPINT_EPDISD;
            }

            if (USBx_INEP(num)->DIEPINT & USB_OTG_DIEPINT_INEPNE)
            {
                if (InPacketDataPtr[num] &&
                    (InPacketDataReady & (1 << num)))
                {
                    SyncWriteEP = 1;
                    USBD_WriteEP(num, (uint8_t *)InPacketDataPtr[num],
                                 InPacketDataCnt[num]);
                    SyncWriteEP = 0;
                    if (!InPacketDataReady)
                    {
                        USBx_DEVICE->DIEPMSK &= ~USB_OTG_DIEPMSK_INEPNEM;
                    }
                    continue;
                }
                else
                {
                    USBx_INEP(num)->DIEPCTL |= USB_OTG_DIEPCTL_CNAK;
                }
                USBx_INEP(num)->DIEPINT = USB_OTG_DIEPINT_INEPNE;
            }

            if (USBx_INEP(num)->DIEPINT & USB_OTG_DIEPINT_XFRC)
            {
                USBx_INEP(num)->DIEPINT = USB_OTG_DIEPINT_XFRC;
                SyncWriteEP = 1;
                if (USBD_P_EP[num])
                {
                    USBD_P_EP[num](USBD_EVT_IN);
                }
                SyncWriteEP = 0;
            }
        }
    }

    NVIC_EnableIRQ(OTG_HS_IRQn);
}