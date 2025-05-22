extern "C" {

#include "tusb.h"
#include "pico/stdlib.h"
#include <string.h>

typedef uint16_t const* u16cnstp;
typedef uint8_t const* u8cnstp;

#define CMDLEN 63

#define CMD_START_DESC 0x01
#define CMD_APPEND_DESC 0x02
#define CMD_FINALIZE_DESC 0x03

#define CMD_RUNTIME_START 0x10
#define CMD_RUNTIME_APPEND 0x11
#define CMD_RUNTIME_APPEND_2 0x12
#define CMD_RUNTIME_END 0x13

#define CMD_PACKED 0x40

#define CMD_LED_ON 0x80
#define CMD_LED_OFF 0x81
#define CMD_REQ_HELLOWORLD 0x90

#define CMDIN_OKYESNO 0x20
#define CMDIN_HELLOWORLD 0x80

#define LED_PIN 25

// Device descriptor
uint8_t const desc_device[] = {
    0x12,        // bLength
    0x01,        // bDescriptorType (Device)
    0x00, 0x02,  // bcdUSB 2.00
    0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
    0x00,        // bDeviceSubClass 
    0x00,        // bDeviceProtocol 
    0x40,        // bMaxPacketSize0 64
    0x19, 0xE7,  // idVendor 0xE719
    0xAF, 0x3E,  // idProduct 0x3EAF
    0x00, 0x01,  // bcdDevice 2.00
    0x01,        // iManufacturer (String Index)
    0x02,        // iProduct (String Index)
    0x03,        // iSerialNumber (String Index)
    0x01,        // bNumConfigurations 1
};

static uint8_t const hid_report_descriptor_default[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        // Report ID 1 for mouse
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)

    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x80,        //     Logical Minimum (-128)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)

    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x08,        //     Usage Maximum (0x08)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x08,        //     Report Count (8)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0xC0,              //   End Collection
    0xC0,              // End Collection
};

static uint8_t const hid_report_control_suffix[] = {
    0x05, 0xFF,         // Usage Page (Vendor Defined 0xFF)
    0x09, 0x01,         // Usage (Vendor Usage 1)

    0xA1, 0x01,         // Collection (Application)
        0x85, 0x7F,     //   Report ID 0x7F (Return message, device->host)
        0x15, 0x00,     //   Logical Minimum (0)
        0x25, 0xFF,     //   Logical Maximum (255)
        0x75, 0x08,     //   Report Size (8 bits)
        0x95, CMDLEN,   //   Report Count (CMDLEN)
        0x09, 0x01,     //   Usage (Vendor Usage 1)
        0x81, 0x02,     //   Input (Data,Var,Abs)
        
        0x85, 0x7E,     //   Report ID 0x7E (Output report, host->device)
        0x15, 0x00,     //   Logical Minimum (0)
        0x25, 0xFF,     //   Logical Maximum (255)
        0x75, 0x08,     //   Report Size (8 bits)
        0x95, CMDLEN,   //   Report Count (CMDLEN)
        0x09, 0x01,     //   Usage (Vendor Usage 1)
        0x91, 0x02,     //   Output (Data,Var,Abs)
    0xC0                // End Collection
};

// Configuration descriptor
uint8_t desc_configuration[] = {
    0x09,        // bLength
    0x02,        // bDescriptorType (Configuration)
    0x42, 0x00,  // wTotalLength 66
    0x02,        // bNumInterfaces 2
    0x01,        // bConfigurationValue
    0x00,        // iConfiguration (String Index)
    0x80,        // bmAttributes
    0x32,        // bMaxPower 100mA

    // report interface
    0x09,        // bLength
    0x04,        // bDescriptorType (Interface)
    0x00,        // bInterfaceNumber 0
    0x00,        // bAlternateSetting
    0x01,        // bNumEndpoints 1
    0x03,        // bInterfaceClass
    0x01,        // bInterfaceSubClass
    0x02,        // bInterfaceProtocol
    0x00,        // iInterface (String Index)

    0x09,        // bLength
    0x21,        // bDescriptorType (HID)
    0x11, 0x01,  // bcdHID 1.11
    0x00,        // bCountryCode
    0x01,        // bNumDescriptors
    0x22,        // bDescriptorType[0] (HID)
    (sizeof(hid_report_descriptor_default)) & 0xFF, // 25
    (sizeof(hid_report_descriptor_default)) >> 8,   // 26

    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x82,        // bEndpointAddress (IN/D2H)
    0x03,        // bmAttributes (Interrupt)
    0x40, 0x00,  // wMaxPacketSize 64
    0x01,        // bInterval (1 ms)

    // control interface
    0x09,        // bLength
    0x04,        // bDescriptorType (Interface)
    0x01,        // bInterfaceNumber 1
    0x00,        // bAlternateSetting
    0x02,        // bNumEndpoints 2
    0x03,        // bInterfaceClass
    0x01,        // bInterfaceSubClass
    0x02,        // bInterfaceProtocol
    0x00,        // iInterface (String Index)

    0x09,        // bLength
    0x21,        // bDescriptorType (HID)
    0x11, 0x01,  // bcdHID 1.11
    0x00,        // bCountryCode
    0x01,        // bNumDescriptors
    0x22,        // bDescriptorType[0] (HID)
    (sizeof(hid_report_control_suffix)) & 0xFF,
    (sizeof(hid_report_control_suffix)) >> 8,

    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x81,        // bEndpointAddress (IN/D2H)
    0x03,        // bmAttributes (Interrupt)
    0x40, 0x00,  // wMaxPacketSize 64
    0x01,        // bInterval (1 ms)

    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x01,        // bEndpointAddress (OUT endpoint, address 1)
    0x03,        // bmAttributes (Interrupt)
    0x40, 0x00,  // wMaxPacketSize (64 bytes)
    0x01,        // bInterval (1 ms)
};

#define BUF_SIZE 1024

static uint8_t ctrl_descriptor[BUF_SIZE] = {};

static uint8_t descriptor[BUF_SIZE] = {};
static uint8_t descriptor_len = 0;
static uint8_t report_data[BUF_SIZE] = {};
static uint8_t report_len = 4;
static uint8_t report_data_2[BUF_SIZE] = {};
static uint8_t report_len_2 = 4;
static uint8_t scratch_data[BUF_SIZE] = {};
static uint8_t scratch_len = 0;
static uint8_t scratch_data_2[BUF_SIZE] = {};
static uint8_t scratch_len_2 = 0;
static uint8_t report_sent = 1;

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

void fail(void)
{
    while(1)
    {
        gpio_put(LED_PIN, 1);
        delay(250);
        gpio_put(LED_PIN, 0);
        delay(250);
    }
}

u16cnstp tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    static uint16_t desc_str[32];
    const char* string_desc;

    switch (index) {
        case STRID_LANGID:
            desc_str[0] = 0x0304;
            desc_str[1] = 0x0409;
            return desc_str;
        case STRID_MANUFACTURER:
            string_desc = "Definite Systems"; break;
        case STRID_PRODUCT:
            string_desc = "HID Puppet Device"; break;
        case STRID_SERIAL:
            string_desc = "1000001A"; break;
        default:
            return NULL;
    }

    uint8_t len = strlen(string_desc);
    if (len > 31)
        fail();

    desc_str[0] = 0x0302 + 2 * len;
    for (uint8_t i = 0; i < len; i++)
        desc_str[i + 1] = string_desc[i];

    return desc_str;
}

u8cnstp tud_hid_descriptor_report_cb(uint8_t interface)
{
    if (interface == 0)
        return descriptor;
    else
        return ctrl_descriptor;
}

u8cnstp tud_descriptor_device_cb(void)
{
    return desc_device;
}

u8cnstp tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

int reception_mode = 0;

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                                uint8_t* buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                            const uint8_t* report, uint16_t len)
{
    (void)instance;
    (void)report_id;
    (void)report_type;

    if (report_id == 0 && report_type == HID_REPORT_TYPE_OUTPUT)
    {
        report_id = report[0];
        report += 1;
        len -= 1;
    }
    
    //if (len == 0) return;
    //uint8_t rid = report[0];
    //if (rid != 0x7E || len != 35) return;

    uint8_t cmd = report[0];
    uint8_t len_data = 0;
    const uint8_t* buffer = report + 2;

    if (cmd == CMD_APPEND_DESC || cmd == CMD_RUNTIME_APPEND || cmd == CMD_RUNTIME_APPEND_2)
        len_data = report[1];

    char reponse_report[CMDLEN];
    memset(reponse_report, 0, CMDLEN);

    switch (cmd)
    {
        case CMD_START_DESC:
        {
            if (reception_mode != 0) return;
            reception_mode = 1;

            memset(scratch_data, 0, sizeof(scratch_data));
            scratch_len = 0;
        } break;
        case CMD_APPEND_DESC:
        {
            if (reception_mode != 1) return;

            memcpy(scratch_data + scratch_len, buffer, len_data);
            scratch_len += len_data;
        } break;
        case CMD_FINALIZE_DESC:
        {
            if (reception_mode != 1) return;
            reception_mode = 0;

            memcpy(descriptor, scratch_data, scratch_len);
            descriptor_len = scratch_len;

            desc_configuration[25] = descriptor_len & 0xFF;
            desc_configuration[26] = descriptor_len >> 8;

            gpio_put(LED_PIN, 1);
            tud_disconnect();
            sleep_ms(20); // sometimes needed to force re-enumeration so the host gets the new descriptor
            tud_connect();
        } break;
        case CMD_RUNTIME_START:
        {
            uint8_t ok = reception_mode == 0;
            reponse_report[0] = CMDIN_OKYESNO;
            reponse_report[1] = ok;
            tud_hid_n_report(1, 0x7F, reponse_report, CMDLEN);
            if (!ok) return;
            
            reception_mode = 2;

            report_sent = 0;
            scratch_len = 0;
            scratch_len_2 = 0;
        } break;
        case CMD_RUNTIME_APPEND:
        {
            if (reception_mode != 2) return;
            memcpy(scratch_data + scratch_len, buffer, len_data);
            scratch_len += len_data;
        } break;
        case CMD_RUNTIME_APPEND_2:
        {
            if (reception_mode != 2) return;
            memcpy(scratch_data_2 + scratch_len_2, buffer, len_data);
            scratch_len_2 += len_data;
        } break;
        case CMD_RUNTIME_END:
        {
            if (reception_mode != 2) return;
            reception_mode = 0;

            memcpy(report_data, scratch_data, scratch_len);
            report_len = scratch_len;
            memcpy(report_data_2, scratch_data_2, scratch_len_2);
            report_len_2 = scratch_len_2;
        } break;
        case CMD_LED_ON:
            gpio_put(LED_PIN, 1);
            break;
        case CMD_LED_OFF:
            gpio_put(LED_PIN, 0);
            break;
        case CMD_REQ_HELLOWORLD:
        {
            reponse_report[0] = CMDIN_HELLOWORLD;
            reponse_report[1] = 13;
            strncpy(reponse_report+2, "Hello, world!", 13);
            tud_hid_n_report(1, 0x7F, reponse_report, CMDLEN);
        } break;
        case CMD_PACKED:
        {
            buffer -= 1;
            int i = 0;
            while (i < CMDLEN && buffer[i] != 0)
            {
                tud_hid_set_report_cb(instance, report_id, report_type, buffer + i + 1, buffer[i]);
                i += buffer[i] + 1;
            }
        } break;
    }
}


uint8_t sent = 0;
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
    if (instance == 1)
        return;
    sent = 0;
}

void tud_hid_report_failed_cb(uint8_t instance, hid_report_type_t report_type, uint8_t const* report, uint16_t xferred_bytes)
{
    if (instance == 1)
        return;
    sent = 0;
}
int n = 0;
void send_report()
{
    if (!tud_hid_ready() || sent)
        return;
    report_sent = 1;
    sent = 1;
    tud_hid_n_report(0, report_data[0], report_data+1, report_len-1);
    gpio_put(LED_PIN, ((int8_t)report_data[3]) < n);
    n = !n;
}

int main()
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    report_data[0] = 1;
    report_data[1] = 1;
    report_len = 4;

    memcpy(descriptor, hid_report_descriptor_default, sizeof(hid_report_descriptor_default));
    descriptor_len = sizeof(hid_report_descriptor_default);
    memcpy(ctrl_descriptor, hid_report_control_suffix, sizeof(hid_report_control_suffix));
    tusb_init();
    
    while (1)
    {
        if (!report_sent && tud_hid_ready())
            send_report();
        tud_task();
    }
}

// Nonfunctional MSC stubs to prevent linker errors
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    return false;
}
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) { }
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) { }
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
    return 1;
}
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
    return 1;
}
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
    return 0;
}

}
