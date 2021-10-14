
#include <libusb-1.0/libusb.h>
#include <stdint.h>
#include <stdio.h>

#define VENDOR_ID  0x2886
#define PRODUCT_ID 0x0018

int find_usb_device(
	libusb_device_handle **devHandle, 
	libusb_context **context, 
	uint8_t *interfaceNumber, 
	uint16_t vendorId, 
	uint16_t productId)
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

#ifdef STANDALONE_BUILD

int main(void)
{
	libusb_device_handle *devHandle; 
	libusb_context       *context;
	uint8_t              interfaceNumber; 
	int success;
	
	success = find_usb_device(&devHandle, &context, &interfaceNumber, VENDOR_ID, PRODUCT_ID);
	
	printf("Open status: %s.\n", (success != 0) ? "TRUE" : "FALSE");
	
	return 0;
}

#endif
