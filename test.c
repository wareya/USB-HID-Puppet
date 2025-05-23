#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>

/////
// Windows example.
// See the comment above hid_report_descriptor_alt
// -lhid -lsetupapi -lwinmm
/////

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
            
            BYTE outputReport[CMDLEN];
            memset(outputReport, 0, CMDLEN);
            outputReport[0] = 0x7E; // Output report.
            
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

BOOL register_raw_input(HANDLE hwnd)
{
    RAWINPUTDEVICE rid;
    
    rid.usUsagePage = 0xFF; // our device's usage page
    rid.usUsage = 0x00; // along with RIDEV_PAGEONLY: get all usages
    rid.dwFlags = RIDEV_INPUTSINK | RIDEV_PAGEONLY;
    rid.hwndTarget = hwnd;
    
    return RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
}

void handle_input(MSG msg)
{
    HRAWINPUT input = (HRAWINPUT)msg.lParam;
    UINT size;
    GetRawInputData(input, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
    RAWINPUT * rawinput = (RAWINPUT *)malloc(size);
    if (!rawinput)
        return; // OOM
    if (GetRawInputData(input, RID_INPUT, rawinput, &size, sizeof(RAWINPUTHEADER)) != size)
        return free(rawinput), (void)0; // unknown error. message might be malformed?
    if (rawinput->header.dwType != RIM_TYPEHID)
        return free(rawinput), (void)0; // not ours, or at least we don't care about it
    
    if (rawinput->data.hid.bRawData[0] != 0x7F)
        return free(rawinput), (void)0; // not a return message
    if (rawinput->data.hid.bRawData[1] == CMDIN_OKYESNO)
        return free(rawinput), (void)0; // OKYESNO, not something this care about here
    
    printf("Raw HID input received: size=%lu\n", rawinput->data.hid.dwSizeHid);
    printf("Received:\n");
    
    for (DWORD i = 0; i < rawinput->data.hid.dwSizeHid; i++)
        printf("%02X ", rawinput->data.hid.bRawData[i]);
    puts("");
    for (DWORD i = 0; i < rawinput->data.hid.dwSizeHid; i++)
        printf(" %c ", isprint(rawinput->data.hid.bRawData[i]) ? rawinput->data.hid.bRawData[i] : '.');
    puts("");
    fflush(stdout);
    
    free(rawinput);
}

/////
// Swap the X and Y usage lines and the clockwiseness of the input test will change.
// That's proof that the descriptor override works.
/////

static uint8_t const hid_report_descriptor_alt[] = {
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

double get_time_ms(void) {
    return GetTickCount();
}

int main(void)
{
    /////
    // Hi-res timing, no stdout buffering.
    /////
    
    timeBeginPeriod(1);
    setvbuf(stdout, NULL, _IOFBF, 4096);
    
    /////
    // Find our device.
    /////
    
    HANDLE hidHandle = get_my_hid_device();
    if (hidHandle == INVALID_HANDLE_VALUE)
    {
        puts("Failed to open HID device");
        return 1;
    }
    puts("Found our device!");
    
    HidD_SetNumInputBuffers(hidHandle, 512);
    
    /////
    // Build an invisible background window and hook raw input up to it.
    // We need this to get return messages back from the device.
    /////
    
    HINSTANCE inst = GetModuleHandle(NULL);
    
    WNDCLASS wnd;
    memset(&wnd, 0, sizeof(wnd));
    wnd.lpfnWndProc = DefWindowProc;
    wnd.hInstance = inst;
    wnd.lpszClassName = "DummyRawInputWindowClass";
    RegisterClass(&wnd);
    
    HWND hwnd = CreateWindow(
        "DummyRawInputWindowClass",
        "Window Title",
        0, 0, 0, 0, 0,
        NULL, NULL, inst, NULL
    );
    if (!hwnd || !register_raw_input(hwnd))
    {
        puts("Failed to set up raw input");
        printf("%lu\n", GetLastError());
        return 1;
    }
    
    MSG msg; // For return messages.
    
    /////
    // Write the "Hello, world!" output and wait for the response.
    /////
    
    BYTE outputReport[CMDLEN] = {0};
    outputReport[0] = 0x7E;
    outputReport[1] = CMD_REQ_HELLOWORLD;

    // NOTE: HidD_SetOutputReport blocks until a USB round trip completes, maybe multiple?
    // So it isn't appropriate for sending input reports at 1000hz.
    // When we get around to input reports later, we use WriteFile with OVERLAP instead.
    if (!HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport)))
        printf("HidD_SetOutputReport failed: %lu\n", GetLastError());
    else
        printf("Output report sent (asked for hello-world)\n");
    
    Sleep(1);
    
    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) && GetMessage(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_INPUT)
            handle_input(msg);
    }
    
    /////
    // Change the device descriptor and reacquire the device.
    /////
    
    outputReport[1] = CMD_START_DESC;
    HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport));
    
    outputReport[1] = CMD_APPEND_DESC;
    outputReport[2] = sizeof(hid_report_descriptor_alt);
    for (size_t i = 0; i < sizeof(hid_report_descriptor_alt); i++)
        outputReport[i+3] = hid_report_descriptor_alt[i];
    HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport));
    
    outputReport[1] = CMD_FINALIZE_DESC;
    HidD_SetOutputReport(hidHandle, outputReport, sizeof(outputReport));
    
    puts("Waiting for reenumeration...");
    fflush(stdout);
    
    Sleep(100);
    hidHandle = get_my_hid_device();
    // The device might not actually be fully reconnected yet, so we have to wait in a loop.
    // This usually takes ~500ms, but sometimes it takes a second or two.
    // TODO: Fail out after ~10 seconds.
    while (hidHandle == INVALID_HANDLE_VALUE)
    {
        printf("Trying again...\n");
        fflush(stdout);
        Sleep(100);
        hidHandle = get_my_hid_device();
    }
    
    /////
    // Start sending input commands to the reconfigured device.
    /////
    
    puts("Starting test...");
    fflush(stdout);
    
    // Just waiting on the most recent sent event causes events to be limited to like, 500hz for some reason?
    // So we need to have a ring buffer of dispatched events and only block when the oldest one hasn't finished yet.
    OVERLAPPED ov[16];
    memset(ov, 0, sizeof(ov));
    
    int j = 0;
    while (j < 3000)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_INPUT)
                handle_input(msg);
        }
        
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
        
        printf("Output report sent (%zu) (started at %zu)\n", end, start);
        fflush(stdout);
    }
    
}
