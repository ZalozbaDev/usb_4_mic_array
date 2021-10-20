#ifndef USB_MIC_ARRAY_TUNING_H
#define USB_MIC_ARRAY_TUNING_H

#include <libusb-1.0/libusb.h>

typedef void(*usb_mic_array__vad_cb_fn)(int active);

int usb_mic_array__find_open_usb_device(
	libusb_device_handle **devHandle, 
	libusb_context **context, 
	uint8_t *interfaceNumber, 
	uint16_t vendorId, 
	uint16_t productId,
	uint8_t resetDevice);

int usb_mic_array__vad_request(
	libusb_device_handle *devHandle,
	usb_mic_array__vad_cb_fn callback
	);

int usb_mic_array__vad_process(
	libusb_context *context);

void usb_mic_array__vad_cleanup();

int usb_mic_array__close_usb_device(
	libusb_device_handle *devHandle,
	libusb_context *context 
	);

#endif
