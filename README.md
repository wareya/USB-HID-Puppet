# USB HID Puppet

This repository has:
- An Arduino IDE project implementing a host-controllable USB HID input device.
- A test program to run on a host Window machine to verify that it works and demonstrate how to connect to it and talk to it.

Compile and upload the Arduino IDE project to a microcontroller platform like the Raspberry Pi Pico 2. Then compile and run test.c on a windows machine (link: `-lhid -lsetupapi -lwinmm`).

You can use this to simulate arbitrary input devices entirely from user-privilege-level software, without any drivers or admin or root privileges, using nothing but a cheap toy USB microcontroller platform like the Raspberry Pi Pico 2 or anything similar.

This has three purposes:

1) It's a good accessibility tool. You can use this technique to translate from one input device to any other input device without needing a driver, on any platform that supports raw HID messages, which are essential to all kinds of mundane user-level software like controlling the LEDs or settings on a mouse.
2) You can automate debugging or unit test tasks that require real-looking user input.
3) It demonstrates that it's silly and bizarre that Windows doesn't let any non-driver software, not even with admin privileges, synthesize HID events. If you have even just rudimentary user-level USB HID hardware access, and a 3 dollar USB thingy, it becomes trivial. The only security concern is whether you have hardware access, but if you have admin privileges, that security concern is long gone.
