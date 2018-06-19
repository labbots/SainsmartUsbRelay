## Sainsmart USB Relay ##
----------
This project provides a simple driver to use sainsmart 4/8 channel USB relay based on FTDI chip in Linux system. This application is based on C program.

Installation
============
This software is based on C program and requires gcc to compile the program from source.

 - **Installing dependencies**
  This program requires FTDI library to interact with the relay and it can be installed using the following command in Debian based system.

        sudo apt-get install libftdi1 libftdi-dev libusb-dev

 - **Compiling the code**

        git clone https://github.com/labbots/SainsmartUsbRelay.git
        cd sainsmart-usb-relay
        make
        sudo make install
    
  The "make install" command copies the binary to /usr/local/bin. So the command can be utilized anywhere from the system.

 - The following works for both a Raspberry Pi (Debian Wheezy) and Ubuntu 16.04, getting ordinary users (e.g. ‘pi’ on the RPi) access to the FTDI device without needing root permissions:

 Create a file /etc/udev/rules.d/99-libftdi.rules. You will need sudo access to create this file.
Put the following in the file:

        SUBSYSTEMS=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", GROUP="dialout", MODE="0660"
        SUBSYSTEMS=="usb", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6014", GROUP="dialout", MODE="0660"
 Some FTDI devices may use other USB PIDs. You could try removing the match on idProduct altogether, just matching on the FTDI vendor ID. Or Use lsusb or similar to determine the exact values to use (or try checking dmesg output on device insertion / removal).
 
Usage
============
This project provides a simple command to interact with the USB relay.

To turn on the USB relay

    sudo sainsmart --on RELAY_NUMBER

For example to turn on the relay 1 we can use

    sudo sainsmart --on 1
To run on all the relays

    sudo sainsmart --on all

To turn off the USB relay 1

    sudo sainsmart --off 1
To run on all the relays

    sudo sainsmart --off all

To get the status of the relays

    sudo sainsmart --status

To get more help information

    sudo sainsmart --help

Notes
============
The Sainsmart card uses the FTDI FT245RL chip. This chip is controlled directly through the open source libFTDI library. No Kernel driver is needed. However on most Linux distributions, the ftdi_sio serial driver is automatically loaded when the FT245RL chip is detected. In order to grant the sainsmart software access to the card, the default driver needs to be unloaded:

    rmmod ftdi_sio
To prevent automatic loading of the driver, add the following line to /etc/modprobe.d/blacklist.conf:

    blacklist ftdi_sio
Both 4 and 8 channel versions are supported. However, there seems to be no way to automatically detect which version of the card is used. Therefore the number of relay channles can be configured in the configuration file. 
