#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

// Corsair H80i v2 USB identifiers
#define CORSAIR_VID 0x1b1c
#define H80I_V2_PID 0x0c04  // You may need to verify this PID

int main() {
    libusb_device **devs;
    libusb_device_handle *dev_handle = NULL;
    libusb_context *ctx = NULL;
    int r;
    ssize_t cnt;
    
    // Initialize libusb
    r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "Error initializing libusb: %s\n", libusb_error_name(r));
        return 1;
    }
    
    // Set debugging level
    libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, 3);
    
    // Open device with VID/PID
    dev_handle = libusb_open_device_with_vid_pid(ctx, CORSAIR_VID, H80I_V2_PID);
    if (dev_handle == NULL) {
        fprintf(stderr, "Error: Could not find/open H80i v2 device\n");
        libusb_exit(ctx);
        return 1;
    }
    
    // Detach kernel driver if active
    if (libusb_kernel_driver_active(dev_handle, 0) == 1) {
        printf("Kernel driver active, detaching...\n");
        if (libusb_detach_kernel_driver(dev_handle, 0) != 0) {
            fprintf(stderr, "Error detaching kernel driver\n");
            libusb_close(dev_handle);
            libusb_exit(ctx);
            return 1;
        }
    }
    
    // Claim interface
    r = libusb_claim_interface(dev_handle, 0);
    if (r < 0) {
        fprintf(stderr, "Error claiming interface: %s\n", libusb_error_name(r));
        libusb_close(dev_handle);
        libusb_exit(ctx);
        return 1;
    }
    
    printf("Successfully connected to H80i v2\n");
    
    // Here you would implement the specific commands to control the cooler
    // This requires knowledge of the device's protocol
    
    // Example: send a control transfer (you'll need to know the correct parameters)
    // unsigned char data[64] = {0};
    // r = libusb_control_transfer(dev_handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, 
    //                            0x09, 0x300, 0, data, sizeof(data), 5000);
    
    // Release interface
    libusb_release_interface(dev_handle, 0);
    
    // Close device and exit
    libusb_close(dev_handle);
    libusb_exit(ctx);
    
    return 0;
}
