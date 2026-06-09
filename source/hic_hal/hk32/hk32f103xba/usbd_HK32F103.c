/*
 * Copyright (c) 2004-2016 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 /*----------------------------------------------------------------------------
  *      RL-ARM - USB
  *----------------------------------------------------------------------------
  *      Name:    usbd_HK32F103.c
  *      Purpose: Hardware Layer module for HK32F103
  *      Rev.:    V4.70 (ported from STM32F103, adapted for HK32)
  *---------------------------------------------------------------------------*/

  /* Double Buffering is not supported                                         */

#include <rl_usb.h>
#include "hk32f103xxxxa.h"
#include "IO_Config.h"
#include "hal/hk32f10x_usb_regs.h"
#include "hal/hk32f10x_usb.h"
#include "hal/hk32f1xx_hal.h"
#include "cortex_m.h"
#include "string.h"
#include "hk32f103xxxxa_rcc.h"

#define __NO_USB_LIB_C
#include "usb_config.c"

#define VAL_MASK            0xFFFF
#define VAL_SHIFT           16
#define EP_NUM_MASK         0xFFFF
#define EP_NUM_SHIFT        0

#define EP_COUNT_MASK       0x03FF

#define USB_DBL_BUF_EP      0x0000

#define EP_BUF_ADDR (sizeof(EP_BUF_DSCR)*(USBD_EP_NUM+1))

EP_BUF_DSCR *pBUF_DSCR = (EP_BUF_DSCR *)USB_PMA_ADDR;

U16 FreeBufAddr;

uint32_t StatQueue[(USBD_EP_NUM + 1) * 2 + 1];
uint32_t StatQueueHead = 0;
uint32_t StatQueueTail = 0;
volatile uint16_t LastIstr = 0;

inline static void stat_enque(uint32_t stat)
{
    cortex_int_state_t state;
    state = cortex_int_get_and_disable();
    StatQueue[StatQueueTail] = stat;
    StatQueueTail = (StatQueueTail + 1) % (sizeof(StatQueue) / sizeof(StatQueue[0]));
    cortex_int_restore(state);
}

inline static uint32_t stat_deque()
{
    cortex_int_state_t state;
    uint32_t stat;
    state = cortex_int_get_and_disable();
    stat = StatQueue[StatQueueHead];
    StatQueueHead = (StatQueueHead + 1) % (sizeof(StatQueue) / sizeof(StatQueue[0]));
    cortex_int_restore(state);

    return stat;
}

inline static uint32_t stat_is_empty()
{
    cortex_int_state_t state;
    uint32_t empty;
    state = cortex_int_get_and_disable();
    empty = StatQueueHead == StatQueueTail;
    cortex_int_restore(state);
    return empty;
}


/*
 *  Reset Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void EP_Reset(U32 EPNum)
{
    U32 num;
    U16 val;
    num = EPNum & 0x0F;
    val = USB_EP_REG(num);

    if (EPNum & 0x80) {
        USB_EP_REG(num) = val & (EPREG_MASK | USB_EP0R_DTOG_TX);
    } else {
        USB_EP_REG(num) = val & (EPREG_MASK | USB_EP0R_DTOG_RX);
    }
}


/*
 *  Set Endpoint Status
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     stat: New Status
 *    Return Value:    None
 */

void EP_Status(U32 EPNum, U32 stat)
{
    U32 num;
    U16 val;
    num = EPNum & 0x0F;
    val = USB_EP_REG(num);

    if (EPNum & 0x80) {
        USB_EP_REG(num) = EP_VAL_UNCHANGED(val) | ((val ^ stat) & USB_EP0R_STAT_TX);
    } else {
        USB_EP_REG(num) = EP_VAL_UNCHANGED(val) | ((val ^ stat) & USB_EP0R_STAT_RX);
    }
}


/*
 *  USB Device Interrupt enable
 *   Called by USBD_Init to enable the USB Interrupt
 *    Return Value:    None
 */

#ifdef __RTX
void __svc(1) USBD_IntrEna(void);
void __SVC_1(void)
{
#else
void          USBD_IntrEna(void)
{
#endif
    NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);
}


/*
 *  USB Device Initialize Function
 *   Called by the User to initialize USB
 *    Return Value:    None
 */

void USBD_Init(void)
{
    /* Reset USB register module */
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_USB, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_USB, DISABLE);

    // NOTE: IMPORTANT!
    // According to the User Manual, USB prescaler must be configured
    // before enabling USB clock.
    // USBPRE：USB预分频（USB prescaler）
    // 在RCC_APB1ENR寄存器中使能USB时钟之前，必须保证该位已经有效。如果USB时钟被使能，该位不能被清零。

    /* Select USBCLK source (72 MHz PLL / 1.5 = 48 MHz) */
    RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5);

    /* Enable USB clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);

    USBD_IntrEna();
    USB_CONNECT_OFF();
}


/*
 *  USB Device Connect Function
 *   Called by the User to Connect/Disconnect USB Device
 *    Parameters:      con:   Connect/Disconnect
 *    Return Value:    None
 */

void USBD_Connect(BOOL con)
{
    if (con) {
        // Enable cable pullup
        USB_CONNECT_ON();

        // Perform reset
        SET_BIT(USB_CNTR, USB_CNTR_FRES);
        CLEAR_BIT(USB_CNTR, USB_CNTR_FRES);

        // while ((USB_ISTR & USB_ISTR_RESET) != 0);

        // Clear interrupts
        CLEAR_BIT(USB_ISTR, USB_ISTR_ALL);

        // Enable interrupts (mask)
        SET_BIT(USB_CNTR, USB_CNTR_RESETM | USB_CNTR_SUSPM | USB_CNTR_WKUPM);
    } else {
        // Set reset
        SET_BIT(USB_CNTR, USB_CNTR_FRES);

        // Clear interrupts
        CLEAR_BIT(USB_ISTR, USB_ISTR_ALL);

        // Disable cable pullup
        USB_CONNECT_OFF();
    }
}


/*
 *  USB Device Reset Function
 *   Called automatically on USB Device Reset
 *    Return Value:    None
 */

void USBD_Reset(void)
{
    U16 wRegVal;

    NVIC_DisableIRQ(USB_LP_CAN_RX0_IRQn);

    CLEAR_BIT(USB_ISTR, USB_ISTR_ALL);
    wRegVal = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_CNTR_SUSPM | USB_CNTR_WKUPM;
#ifdef __RTX
    if (USBD_RTX_DevTask != 0) {
        wRegVal |= USB_CNTR_ERRM | USB_CNTR_PMAOVRM | USB_CNTR_SOFM | USB_CNTR_ESOFM;
    }
#else
    if (USBD_P_Error_Event != 0) {
        wRegVal |= USB_CNTR_ERRM | USB_CNTR_PMAOVRM;
    }
    if (USBD_P_SOF_Event != 0) {
        wRegVal |= USB_CNTR_SOFM | USB_CNTR_ESOFM;
    }
#endif
    SET_BIT(USB_CNTR, wRegVal);

    FreeBufAddr = EP_BUF_ADDR;
    MODIFY_REG(USB_BTABLE, USB_BTABLE_BTABLE, 0);

    /* Setup Control Endpoint 0 */
    pBUF_DSCR->ADDR_TX = FreeBufAddr;
    FreeBufAddr += USBD_MAX_PACKET0;
    pBUF_DSCR->ADDR_RX = FreeBufAddr;
    FreeBufAddr += USBD_MAX_PACKET0;

    if (USBD_MAX_PACKET0 > 62) {
        pBUF_DSCR->COUNT_RX = ((USBD_MAX_PACKET0 << 5) - 1) | 0x8000;
    } else {
        pBUF_DSCR->COUNT_RX =   USBD_MAX_PACKET0 << 9;
    }

    USB_EP_REG(0) = EP_CONTROL | EP_RX_VALID;
    USB_DADDR = USB_DADDR_EF | 0; /* Enable USB Default Address         */

    NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);
}


/*
 *  USB Device Suspend Function
 *   Called automatically on USB Device Suspend
 *    Return Value:    None
 */

void USBD_Suspend(void)
{
    U16 wRegVal = USB_CNTR;
    wRegVal |= USB_CNTR_FSUSP;
    USB_CNTR = wRegVal;
}


/*
 *  USB Device Resume Function
 *   Called automatically on USB Device Resume
 *    Return Value:    None
 */

void USBD_Resume(void)
{
}


/*
 *  USB Device Remote Wakeup Function
 *   Called automatically on USB Device Remote Wakeup
 *    Return Value:    None
 */

void USBD_WakeUp(void)
{
    U16 wRegVal = USB_CNTR;
    wRegVal &= ~USB_CNTR_FSUSP;
    USB_CNTR = wRegVal;
}


/*
 *  USB Device Remote Wakeup Configuration Function
 *    Parameters:      cfg:   Device Enable/Disable
 *    Return Value:    None
 */

void USBD_WakeUpCfg(BOOL cfg)
{
}


/*
 *  USB Device Set Address Function
 *    Parameters:      adr:   USB Device Address
 *                     setup: Called in setup stage (!=0), else after status stage
 *    Return Value:    None
 */

void USBD_SetAddress(U32 adr, U32 setup)
{
    if (setup) {
        return;
    }

    USB_DADDR = USB_DADDR_EF | adr;
}


/*
 *  USB Device Configure Function
 *    Parameters:      cfg:   Device Configure/Deconfigure
 *    Return Value:    None
 */

void USBD_Configure(BOOL cfg)
{
    if (cfg == __FALSE) {
        FreeBufAddr = EP_BUF_ADDR;
        FreeBufAddr += 2 * USBD_MAX_PACKET0;
    }
}


/*
 *  Configure USB Device Endpoint according to Descriptor
 *    Parameters:      pEPD:  Pointer to Device Endpoint Descriptor
 *    Return Value:    None
 */

void USBD_ConfigEP(USB_ENDPOINT_DESCRIPTOR * pEPD)
{
    U32 num, val;
    num = pEPD->bEndpointAddress & 0x0F;
    val = pEPD->wMaxPacketSize;

    if (pEPD->bEndpointAddress & USB_ENDPOINT_DIRECTION_MASK) {
        (pBUF_DSCR + num)->ADDR_TX = FreeBufAddr;
        val = (val + 1) & ~1;
    } else {
        (pBUF_DSCR + num)->ADDR_RX = FreeBufAddr;

        if (val > 62) {
            val = (val + 31) & ~31;
            (pBUF_DSCR + num)->COUNT_RX = ((val << 5) - 1) | 0x8000;
        } else {
            val = (val + 1) & ~1;
            (pBUF_DSCR + num)->COUNT_RX =   val << 9;
        }
    }

    FreeBufAddr += val;

    switch (pEPD->bmAttributes & USB_ENDPOINT_TYPE_MASK) {
    case USB_ENDPOINT_TYPE_CONTROL:
        val = EP_CONTROL;
        break;

    case USB_ENDPOINT_TYPE_ISOCHRONOUS:
        val = EP_ISOCHRONOUS;
        break;

    case USB_ENDPOINT_TYPE_BULK:
        val = EP_BULK;

        if (USB_DBL_BUF_EP & (1 << num)) {
            val |= USB_EP0R_EP_KIND;
        }

        break;

    case USB_ENDPOINT_TYPE_INTERRUPT:
        val = EP_INTERRUPT;
        break;
    }

    val |= num;
    USB_EP_REG(num) = val;
}


/*
 *  Set Direction for USB Device Control Endpoint
 *    Parameters:      dir:   Out (dir == 0), In (dir <> 0)
 *    Return Value:    None
 */

void USBD_DirCtrlEP(U32 dir)
{
}


/*
 *  Enable USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_EnableEP(U32 EPNum)
{
    EP_Status(EPNum, EP_TX_NAK | EP_RX_VALID);    /* EP is able to receive      */
}


/*
 *  Disable USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_DisableEP(U32 EPNum)
{
    EP_Status(EPNum, EP_TX_DIS | EP_RX_DIS);
}


/*
 *  Reset USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_ResetEP(U32 EPNum)
{
    EP_Reset(EPNum);
}


/*
 *  Set Stall for USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_SetStallEP(U32 EPNum)
{
    EP_Status(EPNum, EP_TX_STALL | EP_RX_STALL);
}


/*
 *  Clear Stall for USB Device Endpoint
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_ClrStallEP(U32 EPNum)
{
    EP_Reset(EPNum);                      /* reset DTog Bits                    */
    EP_Status(EPNum, EP_TX_VALID | EP_RX_VALID);
}


/*
 *  Clear USB Device Endpoint Buffer
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USBD_ClearEPBuf(U32 EPNum)
{
}


/*
 *  Read USB Device Endpoint Data
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *    Return Value:    Number of bytes read
 */

U32 USBD_ReadEP(U32 EPNum, U8 * pData, U32 bufsz)
{
    U32 num, cnt, *pv, n;

    num = EPNum & 0x0F;
    pv  = (U32 *)(USB_PMA_ADDR + 2 * ((pBUF_DSCR + num)->ADDR_RX));
    cnt = (pBUF_DSCR + num)->COUNT_RX & EP_COUNT_MASK;

    if (cnt > bufsz) {
        cnt = bufsz;
    }

    for (n = 0; n < (cnt + 1) / 2; n++) {
        __UNALIGNED_UINT16_WRITE(pData, *pv++);
        pData += 2;
    }

    EP_Status(EPNum, EP_RX_VALID);
    return (cnt);
}


/*
 *  Write USB Device Endpoint Data
 *    Parameters:      EPNum: Device Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *                     cnt:   Number of bytes to write
 *    Return Value:    Number of bytes written
 */

U32 USBD_WriteEP(U32 EPNum, U8 * pData, U32 cnt)
{
    U32 num, *pv, n;
    U16 statusEP;

    num = EPNum & 0x0F;
    pv  = (U32 *)(USB_PMA_ADDR + 2 * ((pBUF_DSCR + num)->ADDR_TX));

    for (n = 0; n < (cnt + 1) / 2; n++) {
        *pv++ = __UNALIGNED_UINT16_READ(pData);
        pData += 2;
    }

    (pBUF_DSCR + num)->COUNT_TX = cnt;
    statusEP = USB_EP_REG(num);

    // FIXME: For unknown reason, this correct STALL
    // state machine processing breaks USB initialization
    // When commented out, works fine
    // if ((statusEP & USB_EP0R_STAT_TX) != EP_TX_STALL) {
        EP_Status(EPNum, EP_TX_VALID);      /* do not make EP valid if stalled    */
    // }

    return (cnt);
}


/*
 *  Get USB Device Last Frame Number
 *    Parameters:      None
 *    Return Value:    Frame Number
 */

U32 USBD_GetFrame(void)
{
    return (USB_FNR & USB_FNR_FN);
}


#ifdef __RTX
U32 LastError;

U32 USBD_GetError(void)
{
    return (LastError);
}
#endif


/*
 *  USB Device Interrupt Service Routine
 */


void USB_LP_CAN1_RX0_IRQHandler(void)
{
    volatile uint16_t istr;
    uint16_t num;
    uint16_t val;

    istr = USB_ISTR;
    // Zero out endpoint ID since this is read from the queue
    LastIstr |= istr & ~(USB_ISTR_DIR | USB_ISTR_EP_ID);
    // Clear interrupts that are pending
    CLEAR_BIT(USB_ISTR, USB_ISTR_ALL);

    if (istr & USB_ISTR_RESET) {
        // NOTE: IMPORTANT!
        // According to the datasheet, RESET must be processed within 10ms!
        // 当复位中断产生时，应用程序必须在中断产生后的10 ms之内使能端点0的传输。
        USBD_Reset();
    }

    if (istr & USB_ISTR_CTR) {
        while ((istr = USB_ISTR) & USB_ISTR_CTR) {
            num = istr & USB_ISTR_EP_ID;
            val = USB_EP_REG(num);

            if ((0 == num) && (val & USB_EP0R_CTR_RX) && !(val & USB_EP0R_SETUP)
                    && (0 == ((pBUF_DSCR + num)->COUNT_RX & EP_COUNT_MASK))) {
                if (val & USB_EP0R_CTR_TX) {
                    stat_enque((((val & VAL_MASK) & ~USB_EP0R_CTR_RX) << VAL_SHIFT) |
                        ((num & EP_NUM_MASK) << EP_NUM_SHIFT));
                }
            } else {
                stat_enque(((val & VAL_MASK) << VAL_SHIFT) |
                    ((num & EP_NUM_MASK) << EP_NUM_SHIFT));
            }

            if (val & USB_EP0R_CTR_RX) {
                USB_EP_REG(num) = EP_VAL_UNCHANGED(val) & ~USB_EP0R_CTR_RX;
            }

            if (val & USB_EP0R_CTR_TX) {
                USB_EP_REG(num) = EP_VAL_UNCHANGED(val) & ~USB_EP0R_CTR_TX;
            }
        }
    }

    USBD_SignalHandler();
}

void USBD_Handler(void)
{
    U32 istr, num, val, num_val;
    cortex_int_state_t state;

    state = cortex_int_get_and_disable();
    istr = LastIstr;
    LastIstr = 0;
    cortex_int_restore(state);

    if (istr & USB_ISTR_RESET) {
        // NOTE: USBD_Reset is processed directly in the IRQ, see NOTE there
        // USBD_Reset();
        usbd_reset_core();
#ifdef __RTX
        if (USBD_RTX_DevTask) {
            isr_evt_set(USBD_EVT_RESET, USBD_RTX_DevTask);
        }
#else
        if (USBD_P_Reset_Event) {
            USBD_P_Reset_Event();
        }
#endif
    }

    if (istr & USB_ISTR_SUSP) {
        USBD_Suspend();
#ifdef __RTX
        if (USBD_RTX_DevTask) {
            isr_evt_set(USBD_EVT_SUSPEND, USBD_RTX_DevTask);
        }
#else
        if (USBD_P_Suspend_Event) {
            USBD_P_Suspend_Event();
        }
#endif
    }

    if (istr & USB_ISTR_WKUP) {
        USBD_WakeUp();
#ifdef __RTX
        if (USBD_RTX_DevTask) {
            isr_evt_set(USBD_EVT_RESUME, USBD_RTX_DevTask);
        }
#else
        if (USBD_P_Resume_Event) {
            USBD_P_Resume_Event();
        }
#endif
    }

    if (istr & USB_ISTR_SOF) {
#ifdef __RTX
        if (USBD_RTX_DevTask) {
            isr_evt_set(USBD_EVT_SOF, USBD_RTX_DevTask);
        }
#else
        if (USBD_P_SOF_Event) {
            USBD_P_SOF_Event();
        }
#endif
    }

    if (istr & USB_ISTR_PMAOVR) {
#ifdef __RTX
        LastError = 2;
        if (USBD_RTX_DevTask) {
            isr_evt_set(USBD_EVT_ERROR, USBD_RTX_DevTask);
        }
#else
        if (USBD_P_Error_Event) {
            USBD_P_Error_Event(2);
        }
#endif
    }

    if (istr & USB_ISTR_ERR) {
#ifdef __RTX
        LastError = 1;
        if (USBD_RTX_DevTask) {
            isr_evt_set(USBD_EVT_ERROR, USBD_RTX_DevTask);
        }
#else
        if (USBD_P_Error_Event) {
            USBD_P_Error_Event(1);
        }
#endif
    }

    while ((istr & USB_ISTR_CTR) && !stat_is_empty()) {
        num_val = stat_deque();
        num = (num_val >> EP_NUM_SHIFT) & EP_NUM_MASK;
        val = (num_val >> VAL_SHIFT) & VAL_MASK;
        if (val & USB_EP0R_CTR_TX) {
#ifdef __RTX
            if (USBD_RTX_EPTask[num]) {
                isr_evt_set(USBD_EVT_IN, USBD_RTX_EPTask[num]);
            }
#else
            if (USBD_P_EP[num]) {
                USBD_P_EP[num](USBD_EVT_IN);
            }
#endif
        }

        if (val & USB_EP0R_CTR_RX) {
#ifdef __RTX
            if (USBD_RTX_EPTask[num]) {
                isr_evt_set((val & USB_EP0R_SETUP) ? USBD_EVT_SETUP : USBD_EVT_OUT, USBD_RTX_EPTask[num]);
            }
#else
            if (USBD_P_EP[num]) {
                USBD_P_EP[num]((val &USB_EP0R_SETUP) ? USBD_EVT_SETUP : USBD_EVT_OUT);
            }
#endif
        }
    }
}
