#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>

#include "hidbad.c"

// WARNING: Largely AI-generated, do not use in production code. MEANT ONLY AS A USAGE EXAMPLE.

// Windows example.
// -lhid -lsetupapi -luser32 -lwinmm

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

void DumpReportDescriptorFromCaps(HANDLE hidHandle)
{
    PHIDP_PREPARSED_DATA preparsedData;
    if (!HidD_GetPreparsedData(hidHandle, &preparsedData)) {
        puts("getpreparseddata failed");
    }
    
    BYTE * desc;
    DWORD desc_len;
    
    if (!ReconstructHidReportDescriptor(preparsedData, &desc, &desc_len))
    {
        puts("ReconstructHidReportDescriptor failed");
        return;
    }
    
    for (DWORD i = 0; i < desc_len; i++)
        printf("%02X ", desc[i]);
    puts("");
}


// Helper: Open HID device by VID/PID (returns handle for output)
HANDLE OpenHidDevice(void)
{
    HANDLE ret_handle = INVALID_HANDLE_VALUE;
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &hidGuid, i, &deviceInterfaceData); i++)
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA deviceDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        if (!deviceDetail) break;
        deviceDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, deviceDetail, requiredSize, NULL, NULL))
        {
            free(deviceDetail);
            continue;
        }

        HANDLE deviceHandle = CreateFile(deviceDetail->DevicePath,
                                         GENERIC_READ | GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                                         NULL,
                                         OPEN_EXISTING,
                                         FILE_FLAG_OVERLAPPED,
                                         NULL);
        if (deviceHandle == INVALID_HANDLE_VALUE)
        {
            free(deviceDetail);
            continue;
        }

        // Check VID/PID
        HIDD_ATTRIBUTES attributes;
        attributes.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(deviceHandle, &attributes))
        {
            if (attributes.VendorID == VENDOR_ID && attributes.ProductID == PRODUCT_ID)
            {
                PHIDP_PREPARSED_DATA preparsedData;
                if (!HidD_GetPreparsedData(deviceHandle, &preparsedData)) {
                    puts("HidD_GetPreparsedData failed");
                    CloseHandle(deviceHandle);
                    free(deviceDetail);
                    continue;
                }
                
                
                puts("Testing a candidate device....");
                DumpReportDescriptorFromCaps(deviceHandle);
                
                BYTE outputReport[CMDLEN] = {0};
                outputReport[0] = 0x7E; // dummy report
                
                if (HidD_SetOutputReport(deviceHandle, outputReport, sizeof(outputReport)))
                {
                    puts("OK!");
                    ret_handle = deviceHandle;
                    deviceHandle = INVALID_HANDLE_VALUE;
                }
                else
                    puts("Not the one.");
            }
        }
        
        CloseHandle(deviceHandle);
        free(deviceDetail);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return ret_handle;
}

BOOL RegisterRawInputForHid(HWND hwnd)
{
    RAWINPUTDEVICE rid[1];
    
    rid[0].usUsagePage = 0xFF; // Vendor-defined
    rid[0].usUsage = 0x00;
    rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_PAGEONLY;
    rid[0].hwndTarget = hwnd;

    if (!RegisterRawInputDevices(rid, sizeof(rid)/sizeof(rid[0]), sizeof(rid[0])))
    {
        printf("RegisterRawInputDevices failed: %lu\n", GetLastError());
        return FALSE;
    }
    return TRUE;
}

uint64_t ack_count = 0;

// Parse raw input and print report
void ProcessRawInput(LPARAM lParam)
{
    UINT dwSize = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
    BYTE* lpb = (BYTE*)malloc(dwSize);
    if (!lpb) return;

    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
    {
        RAWINPUT* raw = (RAWINPUT*)lpb;
        if (raw->header.dwType == RIM_TYPEHID)
        {
            if (raw->data.hid.dwSizeHid > 2
                && raw->data.hid.bRawData[0] == 0x7F
                && raw->data.hid.bRawData[1] > 0x20)
            {
                printf("Raw HID input received: size=%lu\n", raw->data.hid.dwSizeHid);
                printf("Received:\n");
                fflush(stdout);
                for (DWORD i = 0; i < raw->data.hid.dwSizeHid; i++)
                    printf("%02X ", raw->data.hid.bRawData[i]);
                puts("");
                for (DWORD i = 0; i < raw->data.hid.dwSizeHid; i++)
                    printf(" %c ", isprint(raw->data.hid.bRawData[i]) ? raw->data.hid.bRawData[i] : '.');
                puts("");
                fflush(stdout);
            }
        }
    }
    free(lpb);
}

void device_reconfigure(HANDLE hidHandle, const char * desc, size_t desc_len)
{
    (void)desc;
    (void)desc_len;
    
    BYTE outputReport[CMDLEN] = {0};
    outputReport[0] = 0x7E;
    outputReport[1] = 0x80;
    
    if (!HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport)))
        printf("HidD_SetOutputReport failed: %lu\n", GetLastError());
    else
        printf("Output report sent\n");

}

double get_time_ms(void) {
    LARGE_INTEGER _def = {0};
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0) {
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0) {
            frequency = _def;
            return -1.0; // failure
        }
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (1000.0 * now.QuadPart) / frequency.QuadPart;
}

static uint8_t const hid_report_descriptor_alt[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        // Report ID 1 for mouse
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)

    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x30,        //     Usage (X)
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
int main(void)
{
    timeBeginPeriod(1);

    setvbuf(stdout, NULL, _IOFBF, 4096);
    
    // Create dummy window for Raw Input messages
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "RawInputWindowClass";
    RegisterClass(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, "Raw Input Window",
                             0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);

    if (!RegisterRawInputForHid(hwnd))
        return 1;

    HANDLE hidHandle = OpenHidDevice();
    if (hidHandle == INVALID_HANDLE_VALUE)
    {
        printf("Failed to open HID device\n");
        return 1;
    }
    
    MSG msg;
    
    HidD_SetNumInputBuffers(hidHandle, 512);
    
    // Example output report to send (adjust to your device report format)
    BYTE outputReport[CMDLEN] = {0};
    outputReport[0] = 0x7E;
    outputReport[1] = CMD_REQ_HELLOWORLD;

    if (!HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport)))
        printf("HidD_SetOutputReport failed: %lu\n", GetLastError());
    else
        printf("Output report sent (asked for hello-world)\n");
    
    // flush input buffer
    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) && GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_INPUT)
            ProcessRawInput(msg.lParam);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    outputReport[1] = CMD_START_DESC;
    HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport));
    
    outputReport[1] = CMD_APPEND_DESC;
    outputReport[2] = sizeof(hid_report_descriptor_alt);
    for (int i = 0; i < sizeof(hid_report_descriptor_alt); i++)
        outputReport[i+3] = hid_report_descriptor_alt[i];
    HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport));
    
    outputReport[1] = CMD_FINALIZE_DESC;
    HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport));
    
    puts("waiting for reenumeration...");
    fflush(stdout);
    
    Sleep(100);
    
    hidHandle = OpenHidDevice();
    while (hidHandle == INVALID_HANDLE_VALUE)
    {
        printf("Trying again...\n");
        fflush(stdout);
        Sleep(100);
        hidHandle = OpenHidDevice();
    }
    
    puts("starting loop...");
    fflush(stdout);
    // Message loop to receive raw input
    int j = 0;
    
    OVERLAPPED ov[16];
    memset(ov, 0, sizeof(ov));
    
    while (j < 3000)
    {
        if (!PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            j++;
            
            memset(outputReport + 1, 0, sizeof(outputReport) - 1);
            
            int i = 1;
            outputReport[i++] = CMD_PACKED;
            
            outputReport[i++] = 1;
            outputReport[i++] = CMD_RUNTIME_START;
            
            outputReport[i++] = 6; // subcmd len
            outputReport[i++] = CMD_RUNTIME_APPEND;
            outputReport[i++] = 4; // len
            outputReport[i++] = 1; // mouse report id
            outputReport[i++] = round(sin(j*0.01)*1.4); // data
            outputReport[i++] = round(cos(j*0.01)*1.4);
            outputReport[i++] = 0;
            
            outputReport[i++] = 1;
            outputReport[i++] = CMD_RUNTIME_END;
            
            outputReport[i++] = 0;
            assert(i <= CMDLEN-1);
            
            uint64_t start = (uint64_t)get_time_ms();
            
            HANDLE ev = CreateEvent(NULL, TRUE, FALSE, NULL);;
            
            start = (uint64_t)get_time_ms();
            
            // wait on whatever event we're about to overlap
            if (ov[j&0xF].hEvent)
            {
                WaitForSingleObject(ov[j&0xF].hEvent, 100);
                // FIXME handle edge case if it falls off the end of the queue even after a 100ms of waiting
                CloseHandle(ov[j&0xF].hEvent);
            }
            
            OVERLAPPED temp = {0};
            ov[j&0xF] = temp;
            ov[j&0xF].hEvent = ev;
            
            DWORD bytesWritten = 0;
            WriteFile(hidHandle, outputReport, sizeof(outputReport), &bytesWritten, &ov[j&0xF]);
            
            uint64_t end = (uint64_t)get_time_ms();
            
            printf("Output report sent (%zu) (started at %zu)\n", (uint64_t)get_time_ms(), start);
        }
        else
        {
            if (msg.message == WM_INPUT)
                ProcessRawInput(msg.lParam);

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    puts("done");

    CloseHandle(hidHandle);
    return 0;
}
