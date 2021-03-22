# NRF52 bluetooth beacon example

## Overview

This example program uses BLE and USB CDC-ACM subsystems.

Implemented using SEGGER embedded studio (NRF Connect SDK v 1.5.0). nRF module used is NRF52840DK, with S140 softdevice.

On startup, BLE module is configured to broadcast custom advertisement packets with default GPS coordinates.
After that USB subsystem is configured as a serial (CDC-ACM) port. 
When serial port is opened, a welcome message is sent and the program waits for input from user. When receiving input from user, data is echoed back on the serial port, and while receiving input, basic checks are made to validate user input. Results of data parsing are messaged back to the user over the serial port.

## Compiling the program

* In SEGGER Embedded Studio open nrfConnect SDK project.
* * File -> Open nrfConnect SDK project.
* * Select this directory as project directory.
* * Choose board (nrf52840dk_nrf52840).
* * press OK.
* * Build project.
* Loading program to development kit.
* * Connect USB to J-Link port on development kit and power on.
* * Load S140 softdevice into target (nrfjprog --family NRF52 --program s140_nrf52_xxx_softdevice.hex --chiperase --verify)
* * Write program to dev kit.

## How to use

* Connect USB cable to the connector marked as 'nRF USB' (not the J-Link port).
* Power on the device.
* * A new COM port will appear.
* Open Putty or other terminal emulator.
* *  baudrate: 115200, data bits: 8, stop bits: 1, flow control: RTS/CTS
* If the port and serial connection settings are correct, you will see a welcome message.
* * Waiting for GPS coordinates (format: '-21.25465,-159.72921')!
* Provide new GPS coordinates (up to 5 decimal places).
* * Write coordinates (example: '59.43671,24.74283') and press enter.
* nRF module will update BLE advertisement packets with the provided coordinates.
* Check results in nRF connect mobile app.
