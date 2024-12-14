#include <stdlib.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"  // Added for set_sys_clock_khz
#include "pio_usb.h"
#include "tusb.h"

// Core1: handle host events
void core1_main() {
    sleep_ms(10);

    // Configure the USB PIO
    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

    // Initialize USB host stack on PIO-USB
    tuh_init(1);

    while (true) {
        tuh_task(); // tinyusb host task
    }
}

int main(void) {
    // Initialize USB pins are 0 and 1 by default
    
    // Set system clock to 120MHz (good for USB)
    set_sys_clock_khz(120000, true);
    
    // Initialize stdio
    stdio_init_all();
    sleep_ms(10);

    printf("Raspberry Pi Pico USB Host Test\n");
    printf("Waiting for keyboard...\n");

    // Launch USB host task on core1
    multicore_reset_core1();
    multicore_launch_core1(core1_main);

    // Main loop does nothing - all USB handling is on core1
    while(1) {
        sleep_ms(1000);
    }

    return 0;
}

// Invoked when device is mounted (connected)
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    (void)desc_report;
    (void)desc_len;

    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);
    
    printf("USB device connected: VID = %04x, PID = %04x\n", vid, pid);

    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("Error: Cannot request report\n");
    }
}

// Invoked when device is unmounted (disconnected)
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("USB device disconnected: addr = %d, instance = %d\n", dev_addr, instance);
}

// Invoked when received report from device
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)len; // Silence unused parameter warning
    
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    
    // Process keyboard reports
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        hid_keyboard_report_t const* kbd_report = (hid_keyboard_report_t const*) report;
        
        // Print modifier keys
        printf("Keyboard: modifiers = %02x, keys = ", kbd_report->modifier);
        
        // Print pressed keys
        for (uint8_t i = 0; i < 6; i++) {
            if (kbd_report->keycode[i]) {
                printf("%02x ", kbd_report->keycode[i]);
            }
        }
        printf("\n");
    }
    // Process mouse reports
    else if (itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        hid_mouse_report_t const* mouse_report = (hid_mouse_report_t const*) report;
        printf("Mouse: buttons = %02x, x = %d, y = %d, wheel = %d\n", 
               mouse_report->buttons,
               mouse_report->x,
               mouse_report->y,
               mouse_report->wheel);
    }

    // Request the next report
    if (!tuh_hid_receive_report(dev_addr, instance)) {
        printf("Error: Cannot request report\n");
    }
}
