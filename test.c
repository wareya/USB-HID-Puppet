#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>

// Windows example.
// -lhid -lsetupapi -lwinmm

#define CMDLEN 64

#define VENDOR_ID  0xE719
#define PRODUCT_ID 0x3EAF

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

HANDLE get_my_hid_device(void)
{
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;
    
    SP_DEVICE_INTERFACE_DATA devData;
    memset(&devData, 0, sizeof(SP_DEVICE_INTERFACE_DATA));
    devData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &hidGuid, i, &devData); i++)
    {
        DWORD requiredLength = 0;
        
        SetLastError(NO_ERROR);
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &devData, NULL, 0, &requiredLength, NULL);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            continue;
        
        PSP_DEVICE_INTERFACE_DETAIL_DATA devDetail = malloc(requiredLength);
        if(!devDetail) // malloc failure. might be because of a broken device instead of memory pressure.
            continue;
        
        devDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &devData, devDetail, requiredLength, NULL, NULL))
        {
            free(devDetail);
            continue;
        }
        
        HANDLE devHandle = CreateFile(
            devDetail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL
        );
        
        if (devHandle == INVALID_HANDLE_VALUE)
        {
            free(devDetail);
            continue;
        }
        
        // Check VID/PID
        HIDD_ATTRIBUTES hidAttr;
        hidAttr.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(devHandle, &hidAttr))
        {
            if (hidAttr.VendorID != VENDOR_ID || hidAttr.ProductID != PRODUCT_ID)
                continue;
            
            puts("Checking if a device of ours is usable...");
            
            BYTE outputReport[CMDLEN] = {0};
            outputReport[0] = 0x7E; // dummy report
            
            if (HidD_SetOutputReport(devHandle, outputReport, sizeof(outputReport)))
            {
                puts("Usable!");
                SetupDiDestroyDeviceInfoList(deviceInfoSet);
                return devHandle;
            }
            else
                puts("Not usable.");
        }
    }

    if (deviceInfoSet)
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
    
    return INVALID_HANDLE_VALUE;
}

int main(void)
{
    timeBeginPeriod(1);
    setvbuf(stdout, NULL, _IOFBF, 4096);
    
    HANDLE hidHandle = get_my_hid_device();
    if (hidHandle == INVALID_HANDLE_VALUE)
    {
        puts("Failed to open HID device\n");
        return 1;
    }
    puts("Found our device!");
}
