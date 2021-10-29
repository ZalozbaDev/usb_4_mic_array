
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#define VENDOR_ID  0x2886
#define PRODUCT_ID 0x0018

// #define USB_MIC_ARRAY_VERBOSITY

#include "tuning.h"

int usb_mic_array__find_open_usb_device(
	libusb_device_handle **devHandle, 
	libusb_context **context, 
	uint8_t *interfaceNumber, 
	uint16_t vendorId, 
	uint16_t productId,
	uint8_t resetDevice)
{
	libusb_device **devicelist;
	libusb_device *device;
	struct libusb_device_descriptor device_descriptor;
	struct libusb_config_descriptor *config_descriptor;
	int number_devices;
	int found_device;

	// exit very early if libusb cannot even be initialized
	if (libusb_init(context) != 0) {
		printf("Error on libusb init!");
		goto error_out;
	}

	libusb_set_debug(*context, 0);

	// this allocated memory, needs to be free'd upon exit
	number_devices = libusb_get_device_list(*context, &devicelist);

	printf("Found %d USB devices.\n", number_devices);

	found_device = -1;

	// iterate through all devices found and check for VID/PID
	if (number_devices < 1) {
		goto error_out_usbcontext;
	} else {
		for (int i = 0; i < number_devices; i++) {
			device = devicelist[i];
    
			if (libusb_get_device_descriptor(device, &device_descriptor) != 0) {
				printf("Error querying for descriptor!");
				continue;
			}

			// the first VID/PID match is our device
			if ((device_descriptor.idVendor == vendorId) && (device_descriptor.idProduct == productId)) {
				found_device = i;
				printf("Using item=%d.\n", i);
				break;
			}
		}
	}

	if (found_device < 0) {
		goto error_out_usbdevlist;
	} else {
		
		// if we found the device, get the correct interface number
		// currently, the last one found is used
		*interfaceNumber = -1;

		// get device descriptor
		if (libusb_get_device_descriptor(devicelist[found_device], &device_descriptor) != 0) {
			printf("Error querying for device descriptor!");
			goto error_out_usbdevlist;
		} else {
			// iterate through all configurations
			printf("Iterating over %d configurations!\n", device_descriptor.bNumConfigurations);
			for (int i = 0; i < device_descriptor.bNumConfigurations; i++) {
				// get config descriptor for selected configuration
				if (libusb_get_config_descriptor(devicelist[found_device], i, &config_descriptor) != 0) {
					printf("Error querying config descriptor!\n");
					goto error_out_usbdevlist;
				} else {
					// iterate through all interfaces of this configuration
					printf("Configuration %d has %d interfaces.\n", i, config_descriptor->bNumInterfaces);
					for (int k = 0; k < config_descriptor->bNumInterfaces; k++) {
						// interfaces have a number of alternative settings
						printf("Interface %d has %d alternative settings.\n", k, config_descriptor->interface->num_altsetting);
					    for (int m = 0; m < config_descriptor->interface->num_altsetting; m++) {
							*interfaceNumber = config_descriptor->interface->altsetting[k].bInterfaceNumber;
							printf("Alternative setting %d has interface number %d.\n", m, *interfaceNumber);
						}
					}
			
					libusb_free_config_descriptor(config_descriptor);
				}
			}
			
		// the device descriptor does not need to be free'd
		}
	}

	// try to open and claim
	if (*interfaceNumber > -1) {
		int open_error;
		open_error = libusb_open(devicelist[found_device], devHandle);
	    if (open_error != 0) {
			printf("Error opening device = %d!\n", open_error);
			goto error_out_usbdevlist;
		} else {
#if 0			
			if (libusb_claim_interface(*devHandle, *interfaceNumber) != 0) {
				printf("Error claiming interface!\n");
				libusb_close(*devHandle);
				goto error_out_usbdevlist;
			}
#endif
			// libusb_set_configuration(*devHandle, 0);
			// libusb_clear_halt(*devHandle, 0);
			if (resetDevice != 0) 
			{
				libusb_reset_device(*devHandle);
			}
		}
	}
	
	// the device list can now be free'd, whether the
	// device was openend or not does not matter
	libusb_free_device_list(devicelist, 1);

	return 1;

error_out_usbdevlist:

	// the device list can now be free'd, whether the
	// device was openend or not does not matter
	libusb_free_device_list(devicelist, 1);

error_out_usbcontext:

	libusb_exit(*context);

error_out:

	return 0;
  
}

static unsigned char vadbuf[16];
static struct libusb_transfer *vadtransfer = NULL;
static usb_mic_array__vad_cb_fn vad_cb;

static void usb_mic_array__vad_callback(struct libusb_transfer *transfer)
{
	unsigned char *buf = libusb_control_transfer_get_data(transfer);

#ifdef USB_MIC_ARRAY_VERBOSITY
	printf("usb_mic_array__vad_callback:\n");
#endif	

	// if the device is working properly, the last 4 bytes returned are non-zero

	if ((buf[4] == 0) || (buf[5] == 0) || (buf[6] == 0) || (buf[7] == 0))
	{
		printf("Invalid vendor cmd response: ");
		for (int f = 0; f < 8; f++)
		{
			printf("%02X ", buf[f]);
		}
		printf("\n");
	}

	if (buf[0] != 0)
	{
		vad_cb(1);		
	}
	else
	{
		vad_cb(0);
	}
}

int usb_mic_array__vad_request(
	libusb_device_handle *devHandle,
	usb_mic_array__vad_cb_fn callback
	)
{
	uint16_t command;
	uint16_t id;
	int status;
	
	if (vadtransfer == NULL)
	{
#ifdef USB_MIC_ARRAY_VERBOSITY
		printf("usb_mic_array__vad_request: alloc\n");
#endif		
		
		vad_cb = callback;
		
		memset(&vadbuf, 0, 16);
		command = 32;
		command |= 0x80;
		command |= 0x40;
		
		id = 19;
			
		vadtransfer = libusb_alloc_transfer(0);
			
		if (vadtransfer == NULL)
		{
			printf("usb_mic_array__vad_request: Error allocating transfer!!!\n");
			status = -1;
		}
		else
		{
			libusb_fill_control_setup(vadbuf, 
				LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
				0x00,
				command,
				id,
				8);
			
			libusb_fill_control_transfer(vadtransfer,
				devHandle,
				vadbuf,
				usb_mic_array__vad_callback,
				NULL,
				1000);
			
			status = libusb_submit_transfer(vadtransfer);
		}
	}
	else
	{
#ifdef USB_MIC_ARRAY_VERBOSITY
		printf("usb_mic_array__vad_request: ignore\n");
#endif
		status = -2;
	}
	
	return status;
}

int usb_mic_array__vad_process(
	libusb_context *context
	)
{
	int status;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
#ifdef USB_MIC_ARRAY_VERBOSITY
	printf("usb_mic_array__vad_process:\n");
#endif
	
	status = libusb_handle_events_timeout_completed(context, &tv, NULL);
		
	return status;
}

void usb_mic_array__vad_cleanup()
{
	if (vadtransfer != NULL)
	{
#ifdef USB_MIC_ARRAY_VERBOSITY
		printf("usb_mic_array__vad_cleanup:\n");
#endif
		
		libusb_free_transfer(vadtransfer);
		vadtransfer = NULL;
	}
}

int usb_mic_array__close_usb_device(
	libusb_device_handle *devHandle,
	libusb_context *context 
	)
{
    libusb_close(devHandle);
	libusb_exit(context);
}

static int vendor_request(libusb_device_handle *devHandle, 
  uint8_t direction, 
  uint16_t command,
  uint16_t id,
  uint8_t *buffer, 
  uint16_t length)
{
	int status;
	
	uint8_t requestType =  LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_VENDOR | ((direction == 0) ? LIBUSB_ENDPOINT_OUT : LIBUSB_ENDPOINT_IN);
	int size;
	
	size = libusb_control_transfer(devHandle, 
				 requestType, // bmRequestType
				 0x00, // bmRequest
				 command, // wValue
				 id, // wIndex
				 buffer,
				 length,
				 1000);

	if (size == length) {
		status = 1;
	} else {
		printf("Return value of vendor request is %d.\n", size);
		status = 0;
	}
	
	return status;
}

static void control_cb(struct libusb_transfer *transfer)
{
	printf("Control callback received!\n");
	
	unsigned char *buf = libusb_control_transfer_get_data(transfer);
	
	for (int f = 0; f < 8; f++)
	{
		printf("%02X ", buf[f]);
	}
	printf("\n");
	
}

#ifdef STANDALONE_BUILD

static int vad_completed = 0;

static void vad_status(int active)
{
	fprintf (stderr, "%d", active);
	
	vad_completed = 1;
}

int main(void)
{
	libusb_device_handle *devHandle; 
	libusb_context       *context;
	uint8_t              interfaceNumber; 
	int success;
	
	success = usb_mic_array__find_open_usb_device(&devHandle, &context, &interfaceNumber, VENDOR_ID, PRODUCT_ID, 1);
	
	printf("Open status: %s.\n", (success != 0) ? "TRUE" : "FALSE");

#if 0	
	for (int i = 0; i < 10000; i++)
	{
		uint8_t buffer[8];
		uint16_t command;
		uint16_t id;
		
		memset(&buffer, 0, 8);
		command = 32;
		command |= 0x80;
		command |= 0x40;
		
		id = 19;
		
		success = vendor_request(devHandle, 1, command, id, buffer, 8);
		
		printf("Success = %s. ", (success != 0) ? "TRUE" : "FALSE");
		
		for (int f = 0; f < 8; f++)
		{
			printf("%02X ", buffer[f]);
		}
		printf("\n");
	}
#endif

#if 0
	struct libusb_transfer *transfer;
	int status;
	
	for (int i = 0; i < 10000; i++)
	{
		unsigned char buf[16];
		uint16_t command;
		uint16_t id;
		
		memset(&buf, 0, 16);
		command = 32;
		command |= 0x80;
		command |= 0x40;
		
		id = 19;
		
		transfer = libusb_alloc_transfer(0);
		
		if (transfer == NULL)
		{
			printf("Error allocating transfer!!!\n");	
		}
		
		libusb_fill_control_setup(buf, 
			LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
			0x00,
			command,
			id,
			8);
		
		libusb_fill_control_transfer(transfer,
			devHandle,
			buf,
			control_cb,
			NULL,
			1000);
		
		status = libusb_submit_transfer(transfer);
		
		printf("Submit success = %s.\n", (success != 0) ? "TRUE" : "FALSE");
		
		//for (int z = 0; z < 100; z++)
		//{
			status = libusb_handle_events(context);
			if (status != 0)
			{
				printf("Handle events error: %d\n", status);
			}
		// }
		
		libusb_free_transfer(transfer);
	}

    libusb_close(devHandle);
	libusb_exit(context);
	
#endif

	int status;

	for (int i = 0; i < 10; i++)
	{
		vad_completed = 0;
		
		status = usb_mic_array__vad_request(devHandle, vad_status);
		
		if (status != 0)
		{
			printf("VAD request error!\n");	
		}

		while (vad_completed == 0)
		{
			status = usb_mic_array__vad_process(context);
	
			if (status != 0)
			{
				printf("VAD process error!\n");	
			}
		}
		
		usb_mic_array__vad_cleanup();
	}
	
	usb_mic_array__close_usb_device(devHandle, context);
	
	return 0;
}

#endif
