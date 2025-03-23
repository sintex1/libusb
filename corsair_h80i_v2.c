/* corsair_h80i_v2.c - Control Corsair H80i V2 liquid cooler via libusb
 * 
 * Compile with:
 * gcc -o corsair_h80i_v2 corsair_h80i_v2.c -lusb-1.0
 * 
 * Run with:
 * sudo ./corsair_h80i_v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

/* Device identifiers */
#define CORSAIR_VID      0x1b1c
#define H80I_V2_PID      0x0c12

/* Endpoints */
#define EP_OUT           0x02
#define EP_IN            0x81

/* Command opcodes */
#define CMD_INIT         0x00
#define CMD_GET_STATUS   0x01
#define CMD_SET_PUMP     0x13
#define CMD_SET_FAN      0x12
#define CMD_SET_LED      0x23

/* LED modes */
#define LED_MODE_STATIC  0x00
#define LED_MODE_BLINK   0x01
#define LED_MODE_PULSE   0x02
#define LED_MODE_RAINBOW 0x03

/* Timeouts */
#define USB_TIMEOUT      1000

typedef struct {
    libusb_device_handle *handle;
    unsigned char buffer[64];
} corsair_device_t;

/* Initialize libusb and open the H80i V2 device */
int corsair_init(corsair_device_t *dev) {
    int ret;
    
    /* Initialize libusb */
    ret = libusb_init(NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize libusb: %s\n", libusb_error_name(ret));
        return ret;
    }
    
    /* Set debug level (optional) */
    #ifdef DEBUG
    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
    #endif
    
    /* Find and open the H80i V2 device */
    dev->handle = libusb_open_device_with_vid_pid(NULL, CORSAIR_VID, H80I_V2_PID);
    if (!dev->handle) {
        fprintf(stderr, "Could not find or open Corsair H80i V2 device\n");
        libusb_exit(NULL);
        return -1;
    }
    
    /* Detach kernel driver if active */
    if (libusb_kernel_driver_active(dev->handle, 0)) {
        ret = libusb_detach_kernel_driver(dev->handle, 0);
        if (ret < 0) {
            fprintf(stderr, "Failed to detach kernel driver: %s\n", libusb_error_name(ret));
            libusb_close(dev->handle);
            libusb_exit(NULL);
            return ret;
        }
    }
    
    /* Claim interface */
    ret = libusb_claim_interface(dev->handle, 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to claim interface: %s\n", libusb_error_name(ret));
        libusb_close(dev->handle);
        libusb_exit(NULL);
        return ret;
    }
    
    /* Initialize the device */
    memset(dev->buffer, 0, sizeof(dev->buffer));
    dev->buffer[0] = CMD_INIT;
    
    ret = libusb_interrupt_transfer(dev->handle, EP_OUT, dev->buffer, sizeof(dev->buffer), 
                                   NULL, USB_TIMEOUT);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize device: %s\n", libusb_error_name(ret));
        corsair_close(dev);
        return ret;
    }
    
    /* Wait for device to initialize */
    usleep(500000);
    
    return 0;
}

/* Send a command to the device */
int corsair_send_command(corsair_device_t *dev, unsigned char cmd, unsigned char *data, int data_len) {
    int ret;
    
    memset(dev->buffer, 0, sizeof(dev->buffer));
    dev->buffer[0] = cmd;
    
    if (data && data_len > 0) {
        memcpy(dev->buffer + 1, data, data_len);
    }
    
    ret = libusb_interrupt_transfer(dev->handle, EP_OUT, dev->buffer, sizeof(dev->buffer), 
                                   NULL, USB_TIMEOUT);
    if (ret < 0) {
        fprintf(stderr, "Failed to send command: %s\n", libusb_error_name(ret));
        return ret;
    }
    
    return 0;
}

/* Receive a response from the device */
int corsair_receive_response(corsair_device_t *dev) {
    int ret, transferred;
    
    memset(dev->buffer, 0, sizeof(dev->buffer));
    
    ret = libusb_interrupt_transfer(dev->handle, EP_IN, dev->buffer, sizeof(dev->buffer), 
                                   &transferred, USB_TIMEOUT);
    if (ret < 0) {
        fprintf(stderr, "Failed to receive response: %s\n", libusb_error_name(ret));
        return ret;
    }
    
    return transferred;
}

/* Set pump speed (0-100%) */
int corsair_set_pump_speed(corsair_device_t *dev, unsigned char speed_percent) {
    unsigned char data[2] = {0};
    
    /* Clamp speed between 0-100% */
    if (speed_percent > 100) speed_percent = 100;
    
    data[0] = speed_percent;
    
    return corsair_send_command(dev, CMD_SET_PUMP, data, 2);
}

/* Set fan speed (0-100%) */
int corsair_set_fan_speed(corsair_device_t *dev, unsigned char fan_id, unsigned char speed_percent) {
    unsigned char data[2] = {0};
    
    /* Clamp speed between 0-100% */
    if (speed_percent > 100) speed_percent = 100;
    
    data[0] = fan_id;
    data[1] = speed_percent;
    
    return corsair_send_command(dev, CMD_SET_FAN, data, 2);
}

/* Set LED color and mode */
int corsair_set_led(corsair_device_t *dev, unsigned char mode, 
                   unsigned char r, unsigned char g, unsigned char b) {
    unsigned char data[4] = {0};
    
    data[0] = mode;
    data[1] = r;
    data[2] = g;
    data[3] = b;
    
    return corsair_send_command(dev, CMD_SET_LED, data, 4);
}

/* Get device status (temperatures, fan speeds, etc.) */
int corsair_get_status(corsair_device_t *dev) {
    int ret;
    
    ret = corsair_send_command(dev, CMD_GET_STATUS, NULL, 0);
    if (ret < 0) return ret;
    
    /* Wait for device to process command */
    usleep(50000);
    
    ret = corsair_receive_response(dev);
    if (ret < 0) return ret;
    
    /* Parse and display status information */
    printf("H80i V2 Status:\n");
    printf("---------------\n");
    printf("Liquid Temperature: %.1f°C\n", dev->buffer[1] + dev->buffer[2]/10.0);
    printf("Pump Speed: %d RPM\n", (dev->buffer[3] << 8) | dev->buffer[4]);
    printf("Fan 1 Speed: %d RPM\n", (dev->buffer[5] << 8) | dev->buffer[6]);
    printf("Fan 2 Speed: %d RPM\n", (dev->buffer[7] << 8) | dev->buffer[8]);
    
    return 0;
}

/* Close the device and clean up */
void corsair_close(corsair_device_t *dev) {
    if (dev->handle) {
        libusb_release_interface(dev->handle, 0);
        libusb_attach_kernel_driver(dev->handle, 0);
        libusb_close(dev->handle);
        libusb_exit(NULL);
        dev->handle = NULL;
    }
}

int main(int argc, char *argv[]) {
    corsair_device_t dev = {0};
    int ret;
    
    printf("Corsair H80i V2 Control Utility\n");
    printf("-------------------------------\n");
    
    /* Initialize device */
    ret = corsair_init(&dev);
    if (ret < 0) {
        return 1;
    }
    
    printf("Device connected successfully\n\n");
    
    /* Get current status */
    corsair_get_status(&dev);
    
    printf("\nSetting pump to 70%%...\n");
    corsair_set_pump_speed(&dev, 70);
    
    printf("Setting fans to 60%%...\n");
    corsair_set_fan_speed(&dev, 0, 60);  /* Fan 1 */
    corsair_set_fan_speed(&dev, 1, 60);  /* Fan 2 */
    
    printf("Setting LED to blue pulse...\n");
    corsair_set_led(&dev, LED_MODE_PULSE, 0, 0, 255);
    
    /* Wait for changes to take effect */
    sleep(2);
    
    /* Get updated status */
    printf("\nUpdated status:\n");
    corsair_get_status(&dev);
    
    /* Clean up */
    corsair_close(&dev);
    printf("\nDevice closed\n");
    
    return 0;
}
