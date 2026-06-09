#pragma once

#define USB_INTMASK_ALL (USB_CNTR_ESOFM | USB_CNTR_SOFM | USB_CNTR_RESETM | \
    USB_CNTR_SUSPM | USB_CNTR_WKUPM | USB_CNTR_ERRM | USB_CNTR_PMAOVRM | \
    USB_CNTR_CTRM)

#define USB_ISTR_ALL (USB_ISTR_ESOF | \
    USB_ISTR_SOF | USB_ISTR_RESET | USB_ISTR_SUSP | USB_ISTR_WKUP | \
    USB_ISTR_ERR | USB_ISTR_PMAOVR)

/* Endpoint numbers (0-7) - not hardware register definitions */
#define ENDP0   0
#define ENDP1   1
#define ENDP2   2
#define ENDP3   3
#define ENDP4   4
#define ENDP5   5
#define ENDP6   6
#define ENDP7   7

/* Endpoint status values - TX bits are at bits 4-5, RX bits at bits 12-13 */
#define EP_TX_DIS       0x0000
#define EP_TX_STALL     0x0010
#define EP_TX_NAK       0x0020
#define EP_TX_VALID     0x0030

#define EP_RX_DIS       0x0000
#define EP_RX_STALL     0x1000
#define EP_RX_NAK       0x2000
#define EP_RX_VALID     0x3000

/* Endpoint type values - EP_TYPE bits are at bits 9-10 */
#define EP_CONTROL      0x0200
#define EP_BULK         0x0000
#define EP_ISOCHRONOUS  USB_EP0R_EP_TYPE_1
#define EP_INTERRUPT    USB_EP0R_EP_TYPE

/* EndPoint Register Mask (No Toggle Fields) */
#define EPREG_MASK      (USB_EP0R_CTR_RX|USB_EP0R_SETUP|USB_EP0R_EP_TYPE| \
    USB_EP0R_EP_KIND|USB_EP0R_CTR_TX|USB_EP0R_EA)

/* EndPoint Register Mask (Write zero to clear) */
#define EPREG_MASK_RC_W0   (USB_EP0R_CTR_RX|USB_EP0R_CTR_TX)

/* Mask off all toggle bits and set write zero to clear bits to 1.          */
/* This creates a value that can be written back to the EndPoint register   */
/* which does not change any status bits.                                   */
#define EP_VAL_UNCHANGED(val)       (((val) & EPREG_MASK) | EPREG_MASK_RC_W0)

