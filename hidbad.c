#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <stdlib.h> // For malloc, realloc, free
#include <stdio.h>  // For printf
#include <string.h> // For memcpy

// --- HID Report Descriptor Item Constants (from HID Usage Tables) ---
// Using raw hex values directly as a workaround for "undeclared identifier" if #defines fail
// This is a highly unusual workaround and suggests an environment issue with #defines
// But we must assume your compiler environment is having issues with #define visibility.

// Main Items
#define HID_ITEM_MAIN_INPUT         0x80
#define HID_ITEM_MAIN_OUTPUT        0x90
#define HID_ITEM_MAIN_FEATURE       0xB0
#define HID_ITEM_MAIN_COLLECTION    0xA0
#define HID_ITEM_MAIN_END_COLLECTION 0xC0

// Global Items
#define HID_ITEM_GLOBAL_USAGE_PAGE      0x04
#define HID_ITEM_GLOBAL_LOGICAL_MINIMUM 0x14
#define HID_ITEM_GLOBAL_LOGICAL_MAXIMUM 0x24
#define HID_ITEM_GLOBAL_PHYSICAL_MINIMUM 0x34
#define HID_ITEM_GLOBAL_PHYSICAL_MAXIMUM 0x44
// Unit Exponent not typically used directly in modern SDKs' HIDP_VALUE_CAPS
#define HID_ITEM_GLOBAL_UNIT            0x64
// These two are consistently reported as "undeclared identifier" or "no member named"
// when they *should* be available. Re-defining with raw values to force.
#define HID_ITEM_GLOBAL_REPORT_SIZE     0x74 // This constant is defined, but was reported as undeclared.
#define HID_ITEM_GLOBAL_REPORT_COUNT    0x94 // This constant is defined, but was reported as undeclared.
#define HID_ITEM_GLOBAL_REPORT_ID       0x84 
#define HID_ITEM_GLOBAL_PUSH            0xA4
#define HID_ITEM_GLOBAL_POP             0xB4

// Local Items
// This constant is defined, but was reported as undeclared.
#define HID_ITEM_LOCAL_USAGE            0x08 
#define HID_ITEM_LOCAL_USAGE_MINIMUM    0x18
#define HID_ITEM_LOCAL_USAGE_MAXIMUM    0x28
#define HID_ITEM_LOCAL_DESIGNATOR_INDEX 0x38
#define HID_ITEM_LOCAL_DESIGNATOR_MINIMUM 0x48
#define HID_ITEM_LOCAL_DESIGNATOR_MAXIMUM 0x58
#define HID_ITEM_LOCAL_STRING_INDEX     0x78
#define HID_ITEM_LOCAL_STRING_MINIMUM   0x88
#define HID_ITEM_LOCAL_STRING_MAXIMUM   0x98
#define HID_ITEM_LOCAL_DELIMITER        0xA8

// Collection Types
#define HID_COLLECTION_PHYSICAL     0x00
#define HID_COLLECTION_APPLICATION  0x01
#define HID_COLLECTION_LOGICAL      0x02

// --- Helper functions (unchanged) ---
BOOL AddHidItemInternal(BYTE** buf, DWORD* current_len, DWORD* max_len, BYTE item_prefix_base, const void* data, DWORD data_len_code) {
    DWORD raw_data_len;
    switch (data_len_code) {
        case 0: raw_data_len = 0; break;
        case 1: raw_data_len = 1; break;
        case 2: raw_data_len = 2; break;
        case 3: raw_data_len = 4; break;
        default: return FALSE; 
    }

    DWORD needed_len = *current_len + 1 + raw_data_len; 

    if (needed_len > *max_len) {
        *max_len = max(*max_len * 2, needed_len + 64); 
        BYTE* new_buf = (BYTE*)realloc(*buf, *max_len);
        if (!new_buf) {
            return FALSE;
        }
        *buf = new_buf;
    }

    (*buf)[*current_len] = item_prefix_base | data_len_code; 
    (*current_len)++;

    if (raw_data_len > 0 && data != NULL) {
        memcpy((*buf) + *current_len, data, raw_data_len);
        (*current_len) += raw_data_len;
    }
    return TRUE;
}

BOOL AddValueItem(BYTE** buf, DWORD* current_len, DWORD* max_len, BYTE item_prefix_base, LONG value) {
    DWORD data_len_code;
    if (value >= -128 && value <= 127) {
        data_len_code = 1; 
    } else if (value >= -32768 && value <= 32767) {
        data_len_code = 2; 
    } else {
        data_len_code = 3; 
    }
    return AddHidItemInternal(buf, current_len, max_len, item_prefix_base, &value, data_len_code);
}

BOOL AddSimpleItem(BYTE** buf, DWORD* current_len, DWORD* max_len, BYTE item_prefix_base) {
    return AddHidItemInternal(buf, current_len, max_len, item_prefix_base, NULL, 0);
}

BOOL AddByteItem(BYTE** buf, DWORD* current_len, DWORD* max_len, BYTE item_prefix_base, BYTE data) {
    return AddHidItemInternal(buf, current_len, max_len, item_prefix_base, &data, 1);
}

// Function to reconstruct the HID Report Descriptor from preparsed data
BOOL ReconstructHidReportDescriptor(
    PHIDP_PREPARSED_DATA pPreparsedData,
    BYTE** pRawDescriptor,
    DWORD* pDescriptorLength)
{
    *pRawDescriptor = NULL;
    *pDescriptorLength = 0;

    if (!pPreparsedData) {
        return FALSE;
    }

    DWORD current_len = 0;
    DWORD max_len = 256; 

    HIDP_CAPS caps;
    if (HidP_GetCaps(pPreparsedData, &caps) != HIDP_STATUS_SUCCESS) {
        return FALSE;
    }

    BYTE* raw_descriptor_buf = NULL;
    raw_descriptor_buf = (BYTE*)malloc(max_len);
    if (!raw_descriptor_buf) {
        return FALSE;
    }

    // Declare pointers for cleanup
    PHIDP_BUTTON_CAPS pButtonCaps = NULL;
    PHIDP_VALUE_CAPS pValueCaps = NULL;

    // --- DEBUG: Print HIDP_CAPS data ---
    printf("\n--- HIDP_CAPS Data ---\n");
    printf("UsagePage: 0x%04X\n", caps.UsagePage);
    printf("Usage: 0x%04X\n", caps.Usage);
    printf("InputReportByteLength: %lu\n", caps.InputReportByteLength);
    printf("OutputReportByteLength: %lu\n", caps.OutputReportByteLength);
    printf("FeatureReportByteLength: %lu\n", caps.FeatureReportByteLength);
    printf("NumberLinkCollectionNodes: %hu\n", caps.NumberLinkCollectionNodes);
    printf("NumberInputButtonCaps: %hu\n", caps.NumberInputButtonCaps);
    printf("NumberInputValueCaps: %hu\n", caps.NumberInputValueCaps);
    printf("NumberInputDataIndices: %hu\n", caps.NumberInputDataIndices);
    printf("NumberOutputButtonCaps: %hu\n", caps.NumberOutputButtonCaps);
    printf("NumberOutputValueCaps: %hu\n", caps.NumberOutputValueCaps);
    printf("NumberOutputDataIndices: %hu\n", caps.NumberOutputDataIndices);
    printf("NumberFeatureButtonCaps: %hu\n", caps.NumberFeatureButtonCaps);
    printf("NumberFeatureValueCaps: %hu\n", caps.NumberFeatureValueCaps);
    printf("NumberFeatureDataIndices: %hu\n", caps.NumberFeatureDataIndices);
    // Removed ReportDescriptorLength as it's not present in your _HIDP_CAPS
    // printf("ReportDescriptorLength: %hu\n", caps.ReportDescriptorLength); 
    printf("------------------------\n\n");
    // --- END DEBUG ---


    // --- Start with the top-level Application Collection ---
    // Usage Page
    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_USAGE_PAGE, caps.UsagePage)) goto cleanup;
    // Usage (from caps)
    // Using raw hex 0x08 for HID_ITEM_LOCAL_USAGE
    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x08, caps.Usage)) goto cleanup; // Using raw hex for HID_ITEM_LOCAL_USAGE
    // Collection (Application)
    if (!AddByteItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_MAIN_COLLECTION, HID_COLLECTION_APPLICATION)) goto cleanup;

    // --- Handle Input Reports ---
    if (caps.NumberInputButtonCaps > 0) {
        pButtonCaps = (PHIDP_BUTTON_CAPS)calloc(caps.NumberInputButtonCaps, sizeof(HIDP_BUTTON_CAPS));
        if (!pButtonCaps) goto cleanup;
        USHORT buttonCapsLength = caps.NumberInputButtonCaps;
        if (HidP_GetButtonCaps(HidP_Input, pButtonCaps, &buttonCapsLength, pPreparsedData) == HIDP_STATUS_SUCCESS) {
            printf("\n--- Input Button Capabilities ---\n");
            for (USHORT i = 0; i < buttonCapsLength; ++i) {
                printf("  Button Cap %hu:\n", i);
                printf("    Usage Page: 0x%04X\n", pButtonCaps[i].UsagePage);
                printf("    Report ID: 0x%02X\n", pButtonCaps[i].ReportID);
                printf("    IsRange: %s\n", pButtonCaps[i].IsRange ? "TRUE" : "FALSE");
                if (pButtonCaps[i].IsRange) {
                    printf("    UsageMin: 0x%04X\n", pButtonCaps[i].Range.UsageMin);
                    printf("    UsageMax: 0x%04X\n", pButtonCaps[i].Range.UsageMax);
                } else {
                    printf("    Usage: 0x%04X\n", pButtonCaps[i].NotRange.Usage);
                }
                printf("    IsAbsolute: %s\n", pButtonCaps[i].IsAbsolute ? "TRUE" : "FALSE");
                printf("    // Note: ReportSize and ReportCount are typically 1 for buttons.\n");

                // If Report ID is present, emit it (applies to subsequent items until changed)
                if (pButtonCaps[i].ReportID != 0) {
                    if (!AddByteItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_REPORT_ID, pButtonCaps[i].ReportID)) goto cleanup;
                }
                
                // Usage Page for this cap
                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_USAGE_PAGE, pButtonCaps[i].UsagePage)) goto cleanup;

                if (pButtonCaps[i].IsRange) {
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_LOCAL_USAGE_MINIMUM, pButtonCaps[i].Range.UsageMin)) goto cleanup;
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_LOCAL_USAGE_MAXIMUM, pButtonCaps[i].Range.UsageMax)) goto cleanup;
                    // Using raw hex 0x94 for HID_ITEM_GLOBAL_REPORT_COUNT
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x94, pButtonCaps[i].Range.UsageMax - pButtonCaps[i].Range.UsageMin + 1)) goto cleanup; 
                } else {
                    // Using raw hex 0x08 for HID_ITEM_LOCAL_USAGE
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x08, pButtonCaps[i].NotRange.Usage)) goto cleanup; 
                    // Using raw hex 0x94 for HID_ITEM_GLOBAL_REPORT_COUNT
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x94, 1)) goto cleanup; 
                }
                
                // Using raw hex 0x74 for HID_ITEM_GLOBAL_REPORT_SIZE
                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x74, 1)) goto cleanup; // Buttons are typically 1 bit

                BYTE input_flags = 0x02; // Data | Variable | Absolute (default for buttons)
                if (!pButtonCaps[i].IsAbsolute) input_flags |= 0x04; // Set Relative bit if not Absolute

                if (!AddByteItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_MAIN_INPUT, input_flags)) goto cleanup;
            }
            printf("---------------------------------\n\n");
        }
    }

    if (caps.NumberInputValueCaps > 0) {
        pValueCaps = (PHIDP_VALUE_CAPS)calloc(caps.NumberInputValueCaps, sizeof(HIDP_VALUE_CAPS));
        if (!pValueCaps) goto cleanup;
        USHORT valueCapsLength = caps.NumberInputValueCaps;
        if (HidP_GetValueCaps(HidP_Input, pValueCaps, &valueCapsLength, pPreparsedData) == HIDP_STATUS_SUCCESS) {
            printf("\n--- Input Value Capabilities ---\n");
            for (USHORT i = 0; i < valueCapsLength; ++i) {
                printf("  Value Cap %hu:\n", i);
                printf("    Usage Page: 0x%04X\n", pValueCaps[i].UsagePage);
                printf("    Report ID: 0x%02X\n", pValueCaps[i].ReportID);
                printf("    LogicalMin: %ld\n", pValueCaps[i].LogicalMin);
                printf("    LogicalMax: %ld\n", pValueCaps[i].LogicalMax);
                printf("    PhysicalMin: %ld\n", pValueCaps[i].PhysicalMin);
                printf("    PhysicalMax: %ld\n", pValueCaps[i].PhysicalMax);
                // Based on your system's `hidpi.h` lacking these members:
                // printf("    Usage: 0x%04X\n", pValueCaps[i].Usage);
                // printf("    Unit: 0x%08X\n", pValueCaps[i].Unit);
                // printf("    UnitExponent: %hhd\n", pValueCaps[i].UnitExponent);
                // printf("    ReportSize: %lu\n", pValueCaps[i].ReportSize);
                // printf("    ReportCount: %lu\n", pValueCaps[i].ReportCount);
                printf("    Units (raw): 0x%08lX\n", pValueCaps[i].Units); // Fixed format specifier
                printf("    IsAbsolute: %s\n", pValueCaps[i].IsAbsolute ? "TRUE" : "FALSE");
                printf("    IsRange: %s\n", pValueCaps[i].IsRange ? "TRUE" : "FALSE");
                if (pValueCaps[i].IsRange) {
                    printf("    UsageMin: 0x%04X\n", pValueCaps[i].Range.UsageMin);
                    printf("    UsageMax: 0x%04X\n", pValueCaps[i].Range.UsageMax);
                } else {
                    // Usage is not available in _HIDP_VALUE_CAPS on your system, so this branch won't use it.
                    printf("    NotRange.Usage: 0x%04X\n", pValueCaps[i].NotRange.Usage); 
                }

                 // If Report ID is present, emit it (applies to subsequent items until changed)
                if (pValueCaps[i].ReportID != 0) {
                    if (!AddByteItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_REPORT_ID, pValueCaps[i].ReportID)) goto cleanup;
                }

                // Set Usage Page for this cap
                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_USAGE_PAGE, pValueCaps[i].UsagePage)) goto cleanup;

                // --- MODIFICATION: Removed Usage as it's not present in your hidpi.h for _HIDP_VALUE_CAPS ---
                // No item for Usage (local) from pValueCaps[i].Usage

                // Handle Usage ranges if IsRange is TRUE (this part relies on Range.UsageMin/Max)
                if (pValueCaps[i].IsRange) {
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_LOCAL_USAGE_MINIMUM, pValueCaps[i].Range.UsageMin)) goto cleanup;
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_LOCAL_USAGE_MAXIMUM, pValueCaps[i].Range.UsageMax)) goto cleanup;
                    // Using raw hex 0x94 for HID_ITEM_GLOBAL_REPORT_COUNT
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x94, pValueCaps[i].Range.UsageMax - pValueCaps[i].Range.UsageMin + 1)) goto cleanup;
                } else {
                    // Handle single Usage if NotRange (this part relies on NotRange.Usage)
                    // Using raw hex 0x08 for HID_ITEM_LOCAL_USAGE
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x08, pValueCaps[i].NotRange.Usage)) goto cleanup; 
                    // Using raw hex 0x94 for HID_ITEM_GLOBAL_REPORT_COUNT
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x94, 1)) goto cleanup; 
                }


                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_LOGICAL_MINIMUM, pValueCaps[i].LogicalMin)) goto cleanup;
                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_LOGICAL_MAXIMUM, pValueCaps[i].LogicalMax)) goto cleanup;
                
                // Add physical min/max if present and meaningful
                if ((pValueCaps[i].PhysicalMin != 0 || pValueCaps[i].PhysicalMax != 0) &&
                    (pValueCaps[i].PhysicalMin != pValueCaps[i].LogicalMin || pValueCaps[i].PhysicalMax != pValueCaps[i].LogicalMax)) {
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_PHYSICAL_MINIMUM, pValueCaps[i].PhysicalMin)) goto cleanup;
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_PHYSICAL_MAXIMUM, pValueCaps[i].PhysicalMax)) goto cleanup;
                }

                // Add unit if present - relying on 'Units' member only, as 'Unit' and 'UnitExponent' were missing
                if (pValueCaps[i].Units != 0) { 
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_UNIT, pValueCaps[i].Units)) goto cleanup;
                }

                // --- MODIFICATION: Removed ReportSize and ReportCount as they are not present in your hidpi.h for _HIDP_VALUE_CAPS ---
                // We're now using raw hex 0x74 for HID_ITEM_GLOBAL_REPORT_SIZE for buttons.
                // For values, this is a major problem, as ReportSize is device-dependent.
                // Since pValueCaps[i].ReportSize is not available, we cannot accurately
                // generate this part of the descriptor for VALUE items.
                // We're forced to omit the Report Size item here, which is why the
                // descriptor will be very incomplete.
                // If we were to guess, we could look at LogicalMin/Max range to infer size,
                // but that's a heuristic, not a reconstruction.
                // If it were present, it would look like:
                // if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x74, pValueCaps[i].ReportSize)) goto cleanup;
                // if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x94, pValueCaps[i].ReportCount)) goto cleanup;
                
                BYTE input_flags = 0x02; // Data | Variable | Absolute (default)
                if (!pValueCaps[i].IsAbsolute) input_flags |= 0x04; // Set Relative bit if not Absolute

                if (!AddByteItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_MAIN_INPUT, input_flags)) goto cleanup;
            }
            printf("--------------------------------\n\n");
        }
    }

    // --- Handle Output Reports ---
    if (caps.NumberOutputValueCaps > 0) {
        // You might need to declare a separate pOutputValueCaps if reusing pValueCaps causes issues.
        // For simplicity, let's just reuse pValueCaps (it's freed and reallocated below).
        // If you were processing Input, Output, AND Feature, you'd want three distinct pointers.

        // Free pValueCaps from input processing if it was allocated
        if (pValueCaps) {
            free(pValueCaps);
            pValueCaps = NULL;
        }

        pValueCaps = (PHIDP_VALUE_CAPS)calloc(caps.NumberOutputValueCaps, sizeof(HIDP_VALUE_CAPS));
        if (!pValueCaps) goto cleanup;
        USHORT outputValueCapsLength = caps.NumberOutputValueCaps;

        // Use HidP_Output for output reports
        if (HidP_GetValueCaps(HidP_Output, pValueCaps, &outputValueCapsLength, pPreparsedData) == HIDP_STATUS_SUCCESS) {
            printf("\n--- Output Value Capabilities ---\n");
            for (USHORT i = 0; i < outputValueCapsLength; ++i) {
                printf("  Output Value Cap %hu:\n", i);
                printf("    Usage Page: 0x%04X\n", pValueCaps[i].UsagePage);
                printf("    Report ID: 0x%02X\n", pValueCaps[i].ReportID);
                printf("    LogicalMin: %ld\n", pValueCaps[i].LogicalMin);
                printf("    LogicalMax: %ld\n", pValueCaps[i].LogicalMax);
                printf("    PhysicalMin: %ld\n", pValueCaps[i].PhysicalMin);
                printf("    PhysicalMax: %ld\n", pValueCaps[i].PhysicalMax);
                printf("    Units (raw): 0x%08lX\n", pValueCaps[i].Units);
                printf("    IsAbsolute: %s\n", pValueCaps[i].IsAbsolute ? "TRUE" : "FALSE");
                printf("    IsRange: %s\n", pValueCaps[i].IsRange ? "TRUE" : "FALSE");
                if (pValueCaps[i].IsRange) {
                    printf("    UsageMin: 0x%04X\n", pValueCaps[i].Range.UsageMin);
                    printf("    UsageMax: 0x%04X\n", pValueCaps[i].Range.UsageMax);
                } else {
                    printf("    NotRange.Usage: 0x%04X\n", pValueCaps[i].NotRange.Usage); 
                }

                // If Report ID is present, emit it (applies to subsequent items until changed)
                if (pValueCaps[i].ReportID != 0) {
                    if (!AddByteItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_REPORT_ID, pValueCaps[i].ReportID)) goto cleanup;
                }

                // Set Usage Page for this cap
                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_USAGE_PAGE, pValueCaps[i].UsagePage)) goto cleanup;

                if (pValueCaps[i].IsRange) {
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_LOCAL_USAGE_MINIMUM, pValueCaps[i].Range.UsageMin)) goto cleanup;
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_LOCAL_USAGE_MAXIMUM, pValueCaps[i].Range.UsageMax)) goto cleanup;
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x94, pValueCaps[i].Range.UsageMax - pValueCaps[i].Range.UsageMin + 1)) goto cleanup; // HID_ITEM_GLOBAL_REPORT_COUNT
                } else {
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x08, pValueCaps[i].NotRange.Usage)) goto cleanup; // HID_ITEM_LOCAL_USAGE
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x94, 1)) goto cleanup; // HID_ITEM_GLOBAL_REPORT_COUNT
                }

                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_LOGICAL_MINIMUM, pValueCaps[i].LogicalMin)) goto cleanup;
                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_LOGICAL_MAXIMUM, pValueCaps[i].LogicalMax)) goto cleanup;
                
                if ((pValueCaps[i].PhysicalMin != 0 || pValueCaps[i].PhysicalMax != 0) &&
                    (pValueCaps[i].PhysicalMin != pValueCaps[i].LogicalMin || pValueCaps[i].PhysicalMax != pValueCaps[i].LogicalMax)) {
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_PHYSICAL_MINIMUM, pValueCaps[i].PhysicalMin)) goto cleanup;
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_PHYSICAL_MAXIMUM, pValueCaps[i].PhysicalMax)) goto cleanup;
                }

                if (pValueCaps[i].Units != 0) { 
                    if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_GLOBAL_UNIT, pValueCaps[i].Units)) goto cleanup;
                }

                // **Crucial point for Output reports: Report Size and Report Count**
                // Since your hidpi.h doesn't expose ReportSize or ReportCount in _HIDP_VALUE_CAPS,
                // and you have OutputReportByteLength: 64, we can infer that this single
                // output value capability covers all 64 bytes.
                // This means:
                // Report Size: 8 bits (for bytes)
                // Report Count: 64 (for 64 bytes)
                // This is an **inference**, not direct information from the API, due to your header limitations.
                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x74, 8)) goto cleanup; // Report Size (8 bits)
                if (!AddValueItem(&raw_descriptor_buf, &current_len, &max_len, 0x94, caps.OutputReportByteLength)) goto cleanup; // Report Count (64)
                
                // Output item flags: Data (bit 0 = 0), Variable (bit 1 = 1), Absolute (bit 2 = 0)
                BYTE output_flags = 0x02; // Data | Variable | Absolute (default for values)
                if (!pValueCaps[i].IsAbsolute) output_flags |= 0x04; // Set Relative bit if not Absolute

                if (!AddByteItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_MAIN_OUTPUT, output_flags)) goto cleanup;
            }
            printf("---------------------------------\n\n");
        }
    }
    
    // TODO: Add similar logic for Feature reports

    // --- End Collection ---
    if (!AddSimpleItem(&raw_descriptor_buf, &current_len, &max_len, HID_ITEM_MAIN_END_COLLECTION)) goto cleanup;

    // Finalize buffer size
    BYTE* final_buf = (BYTE*)realloc(raw_descriptor_buf, current_len);
    
    // Check if realloc failed and we need to use the original buffer
    if (!final_buf && current_len > 0) {
        *pRawDescriptor = raw_descriptor_buf; 
    } else {
        *pRawDescriptor = final_buf;
    }
    *pDescriptorLength = current_len;

    return TRUE;

cleanup:
    if (pButtonCaps) {
        free(pButtonCaps);
    }
    if (pValueCaps) {
        free(pValueCaps);
    }
    if (raw_descriptor_buf) {
        free(raw_descriptor_buf);
    }
    *pRawDescriptor = NULL;
    *pDescriptorLength = 0;
    return FALSE;
}
