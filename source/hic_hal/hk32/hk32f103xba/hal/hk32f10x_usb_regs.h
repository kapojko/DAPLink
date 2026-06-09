/**
  ******************************************************************************
  * @file    hk32f10x_usb_regs.h
  * @author  DAPLink Team
  * @brief   USB Peripheral Register addresses for HK32F103
  *          Extracted from HK32F103x8xB User Manual V1.6 Section 18.5
  *          All registers are 16-bit, base address is 0x40005C00
  ******************************************************************************
  */

#ifndef __HK32F10x_USB_REGS_H
#define __HK32F10x_USB_REGS_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>

/*
 * USB Register Addresses (Section 18.5 USB Registers)
 * Base address: 0x40005C00
 * All USB registers are 16-bit
 */

/* USB peripheral base address */
#define USB_BASE            ((uint32_t)0x40005C00)

/* USB Packet Memory Area (PMA) - starts at 0x40006000, size 0x200 */
#define USB_PMA_BASE        ((uint32_t)0x40006000)

/*
 * USB Packet Memory Address - alias for USB_PMA_BASE
 * UM Section 18.5: PMA starts at 0x40006000
 */
#define USB_PMA_ADDR        (USB_PMA_BASE)

/* USB Endpoint Buffer Descriptor, located at USB_PMA_ADDR */
typedef struct _EP_BUF_DSCR {
    U32 ADDR_TX;
    U32 COUNT_TX;
    U32 ADDR_RX;
    U32 COUNT_RX;
} EP_BUF_DSCR;

/* USB Endpoint Registers (offset: x*4) */
#define USB_EP0R            (*((volatile uint16_t *)(USB_BASE + 0x00)))
#define USB_EP1R            (*((volatile uint16_t *)(USB_BASE + 0x04)))
#define USB_EP2R            (*((volatile uint16_t *)(USB_BASE + 0x08)))
#define USB_EP3R            (*((volatile uint16_t *)(USB_BASE + 0x0C)))
#define USB_EP4R            (*((volatile uint16_t *)(USB_BASE + 0x10)))
#define USB_EP5R            (*((volatile uint16_t *)(USB_BASE + 0x14)))
#define USB_EP6R            (*((volatile uint16_t *)(USB_BASE + 0x18)))
#define USB_EP7R            (*((volatile uint16_t *)(USB_BASE + 0x1C)))

/*
 * Endpoint register access by endpoint number
 * UM Section 18.5.2: USB_EPxR at offset x*4 from USB_BASE
 * Example: USB_EP_REG(0) = USB_EP0R, USB_EP_REG(3) = USB_EP3R
 */
#define USB_EP_REG(n)       (*((volatile uint16_t *)(USB_BASE + (uint32_t)(n) * 4)))

/* USB General Registers */
/* USB_CNTR - USB 控制寄存器 (USB_CNTR) - Offset: 0x40, Reset: 0x0003 */
#define USB_CNTR            (*((volatile uint16_t *)(USB_BASE + 0x40)))

/* USB_ISTR - USB 中断状态寄存器 (USB_ISTR) - Offset: 0x44, Reset: 0x0000 */
#define USB_ISTR            (*((volatile uint16_t *)(USB_BASE + 0x44)))

/* USB_FNR - USB 帧编号寄存器 (USB_FNR) - Offset: 0x48, Reset: undefined */
#define USB_FNR             (*((volatile uint16_t *)(USB_BASE + 0x48)))

/* USB_DADDR - USB 设备地址寄存器 (USB_DADDR) - Offset: 0x4C, Reset: 0x0000 */
#define USB_DADDR           (*((volatile uint16_t *)(USB_BASE + 0x4C)))

/* USB_BTABLE - USB 分组缓冲区描述表地址寄存器 (USB_BTABLE) - Offset: 0x50, Reset: 0x0000 */
#define USB_BTABLE          (*((volatile uint16_t *)(USB_BASE + 0x50)))

#ifdef __cplusplus
}
#endif

#endif /* __HK32F10x_USB_REGS_H */
