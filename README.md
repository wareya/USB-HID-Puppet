# USB HID Puppet

This repository has:
- An Arduino IDE project implementing a host-controllable USB HID input device.
- A test program to run on a host Window machine to verify that it works and demonstrate how to connect to it and talk to it. The test program makes a virtual mouse that spins the cursor in circles for a few seconds.

Compile and upload the Arduino IDE project to a microcontroller platform like the Raspberry Pi Pico 2, keeping it connected. Then compile and run test.c on a windows machine (link: `-lhid -lsetupapi -lwinmm`).

You can use this to simulate arbitrary input devices entirely from user-privilege-level software, without any drivers or admin or root privileges, using nothing but a cheap toy USB microcontroller platform like the Raspberry Pi Pico 2 or anything similar.

Video: Controlling a USB device that sends inputs to the PC, from userspace code. Example that just moves the mouse cursor in a circle with different axis orientations.

https://github.com/user-attachments/assets/855c75d8-5e8f-429c-9649-2b97fad7b384

This has four purposes:

1) It's a good accessibility tool. You can use this technique to translate from one input device to any other input device (e.g. keyboard -> controller, or controller -> mouse), without needing a driver, on any platform that supports raw HID messages, which are essential to all kinds of mundane user-level software like controlling the LEDs or settings on a mouse.
2) You can automate debugging or unit test tasks that require real-looking user input.
3) I need to keep certain pieces of functionality out of https://github.com/wareya/vmulti (e.g. relative mouse, analog joystick support) so that it doesn't get used by badware developers and blacklisted by anticheat/DRM software, but those pieces of functionality would still be Useful, and it felt morally wrong to axe them without creating an alternative. This project is that alternative.
4) It demonstrates that it's silly and bizarre that Windows doesn't let any non-driver software, not even with admin privileges, synthesize HID events. If you have even just rudimentary user-level USB HID hardware access, and a 3~5 dollar USB thingy, it becomes trivial. The only security concern is whether you have physical access to the machine, but if you have admin privileges, that security concern is long gone.
