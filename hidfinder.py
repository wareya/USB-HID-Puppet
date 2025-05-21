#!python
import sys
import binascii

# Common HID Report Descriptor tags
COMMON_TAGS = {
    0x05,  # Usage Page
    0x09,  # Usage
    0xA1,  # Collection
    0xC0,  # End Collection
    0x15,  # Logical Minimum
    0x25,  # Logical Maximum
    0x75,  # Report Size
    0x95,  # Report Count
    0x81,  # Input
    0x91,  # Output
    0x85,  # Report ID
}

START_TAG = 0x05  # Usage Page usually starts a descriptor

# Check if a byte sequence could be a HID Report Descriptor
def is_possible_hid_descriptor(data):
    score = 0
    for b in data:
        if b in COMMON_TAGS:
            score += 1
    return score >= len(data) * 0.2  # Heuristic: 20% known tags

def hex_dump(data, start_offset):
    hex_str = ' '.join(f'{b:02X}' for b in data)
    print(f"[Offset 0x{start_offset:X}]\n{hex_str}\n")

def search_hid_descriptors(filename, min_len=32, max_len=256):
    with open(filename, 'rb') as f:
        blob = f.read()

    print(f"Scanning {filename} for HID Report Descriptor candidates...")
    offset = 0
    while offset < len(blob) - min_len:
        if blob[offset] == START_TAG:
            depth = 0
            for end in range(offset, min(offset + max_len, len(blob))):
                tag = blob[end]
                if tag == 0xA1:  # Collection
                    depth += 1
                elif tag == 0xC0:  # End Collection
                    depth -= 1
                    if depth <= 0 and (end - offset + 1) >= min_len:
                        candidate = blob[offset:end + 1]
                        if is_possible_hid_descriptor(candidate):
                            hex_dump(candidate, offset)
                            offset = end + 1
                            break
            else:
                offset += 1
        else:
            offset += 1

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <binary file>")
        sys.exit(1)

    search_hid_descriptors(sys.argv[1])
