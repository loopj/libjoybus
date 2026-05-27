#pragma once

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUSB_MCU            OPT_MCU_RP2040
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE
#define CFG_TUD_ENDPOINT0_SIZE  8

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#define CFG_TUD_HID             1

// HID buffer size (must match w/ endpoint size in descriptor)
#define CFG_TUD_HID_EP_BUFSIZE  64