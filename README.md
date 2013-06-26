kodobannin
==========

コード番人 or "code keeper" is the bootloader for Hello products based on the nRF51822 chipset

Design
======

The primary goals of kodobannin are to be small, secure and efficient. Booting the firmware and device recovery are secondary goals... ;)

For more in-depth design notes, please refer to [the wiki](http://wiki.sayhello.com/software/firmware/bootloader)

External libraries
==================

Kodobannin uses [micro-ecc](https://github.com/kmackay/micro-ecc) to provide eliptic curve cryptography support and Nordic's [SoftDevice and nRF51 SDK](https://www.nordicsemi.com/eng/Products/Bluetooth-R-low-energy/nRF51822-Development-Kit) libraries to provide a BlueTooth Low Energy stack.
