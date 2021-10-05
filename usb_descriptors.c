/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Derived from tusb cdc example, by Matt Evans, 27 Sep 2021
 */

#include "tusb.h"

/* A combination of interfaces must have a unique product id, since PC
 * will save device driver after the first plug.  Same VID/PID with
 * different interface e.g MSC (first), then CDC (later) will possibly
 * cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]       MIDI | HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) |       \
                           _PID_MAP(HID, 2) |                                   \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and
    // protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum
{
  ITF_NUM_CDC_0 = 0,
  ITF_NUM_CDC_0_DATA,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CFG_TUD_CDC * TUD_CDC_DESC_LEN)

#define EPNUM_CDC_0_NOTIF   0x81
#define EPNUM_CDC_0_DATA    0x02


// CDC Descriptor Template (ME modified to change poll interval)
// Interface number, string index, EP notification address and size, EP data
// address (out, in) and size.
#define MTUD_CDC_DESCRIPTOR(_itfnum, _stridx, _ep_notif,                \
                            _ep_notif_size, _epout, _epin, _epsize)     \
	/* Interface Associate */					\
	8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_CDC, \
                CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,               \
                CDC_COMM_PROTOCOL_NONE, 0,                              \
		/* CDC Control Interface */				\
		9, TUSB_DESC_INTERFACE, _itfnum, 0, 1, TUSB_CLASS_CDC,  \
                CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL,               \
                CDC_COMM_PROTOCOL_NONE, _stridx,                        \
		/* CDC Header */					\
		5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER,        \
                U16_TO_U8S_LE(0x0120),                                  \
		/* CDC Call */						\
		5, TUSB_DESC_CS_INTERFACE,                              \
                CDC_FUNC_DESC_CALL_MANAGEMENT, 0,                       \
                (uint8_t)((_itfnum) + 1),                               \
		/* CDC ACM: support line request */			\
		4, TUSB_DESC_CS_INTERFACE,                              \
                CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 2,           \
		/* CDC Union */						\
		5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION,         \
                _itfnum, (uint8_t)((_itfnum) + 1),                      \
		/* Endpoint Notification */				\
		7, TUSB_DESC_ENDPOINT, _ep_notif, TUSB_XFER_INTERRUPT,  \
                U16_TO_U8S_LE(_ep_notif_size), /* Interval */1,         \
		/* CDC Data Interface */				\
		9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 2,   \
                TUSB_CLASS_CDC_DATA, 0, 0, 0,                           \
		/* Endpoint Out */					\
		7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK,          \
                U16_TO_U8S_LE(_epsize),                                 \
                0,                                                      \
		/* Endpoint In */					\
		7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK,           \
                U16_TO_U8S_LE(_epsize), 0

uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute,
  // power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 10),

  // 1st CDC: Interface number, string index, EP notification address and size,
  // EP data address (out, in) and size.
  MTUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_DATA,
                      0x80 | EPNUM_CDC_0_DATA, 64),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "axio.ms",                     // 1: Manufacturer
  "ArcPipePodule ",              // 2: Product
  "0000",                        // 3: Serials, should use chip ID
  "TinyUSB CDC",                 // 4: CDC Interface
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

  return _desc_str;
}
