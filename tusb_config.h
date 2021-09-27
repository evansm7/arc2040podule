// ME

#ifndef _TUSB_CONFIG_H
#define _TUSB_CONFIG_H

#define CFG_TUSB_RHPORT0_MODE           (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE          64
#endif

#define CFG_TUD_CDC                     1

#define CFG_TUD_CDC_RX_BUFSIZE          1024
#define CFG_TUD_CDC_TX_BUFSIZE          1024
#define CFG_TUD_CDC_EP_BUFSIZE          1024

#endif
