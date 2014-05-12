kodobannin
==========

コード番人 (Kodobannin) or "code keeper" is the code repository for Hello products based on the Nordic nRF51822 chipset.

Hardware Platforms, Apps and Builds
===================================

Kodobannin has the concept of a _hardware platform_ (or _platform_), an _app_, and a _build_.

* A _hardware platform_ is a specific hardware configuration for a particular product. Technically, a hardware platform defines various constants that an app can use to access hardware functions. For example, the hardware platforms designed to run the band app must define a `IMU_SPI_MOSI` constant, which is a GPIO pin number that an app uses to talk to the accelerometer (IMU) via SPI. A hardware platform may also define any other functions or constants specific to that platform.
* An _app_ is the software that implements the features for a product.
* A _build_ is a combination of a hardware platform paired with an app.

It's easiest to understand these with examples. Some examples of hardware platforms are:

* band_EVT2: EVT-2 version of the Band hardware product (not to be confused with `band`, the name of the app).
* band_EVT3: EVT-3 version of the Band.
* EK: **E**valuation **K**it, a more colloquial name for the Nordic PCA10001 nRF51822 evaluation board.

Some examples of apps are:

* band: The firmware for the Band, which collects accelerometer and heart rate data, saves the data to storage, and transfers the data back to a phone via Bluetooth Low Energy (BLE).
* morpheus: The firmware for Morpheus, which collects environmental data (e.g. temperature and humidity) and audio, processes and stores that data, and sends the data back to Hello servers via Wi-Fi.
* hello_world: A "hello, world" app that simply blinks some LEDs on the hardware device. (Exactly which LEDs get blinked are specified by the hardware platform.)
* rtc_test: A test app to talk to an external Real-Time Clock (RTC) via I2C.

A _build_ is a combination of an app paired with a hardware platform. (An app must be paired with a hardware platform to run.) For example, `band+band_EVT3` is the band app paired with the band_EVT3 hardware platform, while `hello_world+EK` is the Hello World app designed to run on the Evaluation Kit.

Hardware platform and apps share the same namespace (because the build system uses the root directory of the code repository for both these things), so you cannot call an app the same name as a platform, and vice versa. In other words, having an app named "morpheus" and a platform named "morpheus" is invalid, but an app named "morpheus" and a platform named "morpheus_PVT" is OK. This will likely be good for your own sanity, too.

Building & Programming
======================

We use GNU make to build the firmware. To **build** a firmware binary, do:

    make app+platform

For example:

    make band+band_EVT3
	make bootloader+band_EVT3
	make hello_world+EK

Building a binary will put the resulting binary in the build/app+platform/ directory.

To **build & upload** (or **program**) some hardware, do:

    make p-app+platform  # p- prefix is for "program"

For example:

    make p-band+band_EVT3
	make p-hello_world+EK

Programming the hardware will upload the binary from the build/app+platform/ directory to the hardware via the J-Link tools. You can also program both the app and the bootloader for the platform adding `-bootloader` to the app name, e.g.

    make p-band+bootloader+band_EVT3
	make p-hello_world+bootloader+EK

To **debug** some hardware (_without_ building and uploading), do:

    make g-app+platform  # g- prefix mirrors gcc's -g debug flag

For example:

    make g-band+band_EVT3
	make g-bootloader+EK

Dependencies
============

Typing in the above make commands to build, upload/program and debug firmware should automatically download all the required dependencies.  _The Makefile is the authoritative place to check dependencies_ since that is always up to date, but here are some of them:

1. GCC for ARM. A direct link to the current version we use is https://launchpad.net/gcc-arm-embedded/4.7/4.7-2013-q3-update/+download/gcc-arm-none-eabi-4_7-2013q3-20130916-mac.tar.bz2. There are also more recent versions (e.g. GCC 4.8) that are available for other platforms (Windows, Linux), if you'd like to be enterprising.

2. The J-Link toolchain. The direct link to the current version we use is http://www.segger.com/jlink-software.html?step=1&file=JLinkMacOSX_474. (You will need to enter in the serial number on your J-Link to download it.)

3. [micro-ecc](https://github.com/kmackay/micro-ecc), for elliptic curve cryptography support.

4. Of course, Nordic's [SoftDevice and nRF51 SDK](https://www.nordicsemi.com/eng/Products/Bluetooth-R-low-energy/nRF51822-Development-Kit) libraries to provide a Bluetooth Low Energy stack.

Serial Debugging
================

The Band has serial port output. You will want the serial port set to 38400 8N1. (8 stop bits, no parity, 1 stop bit.) Plugging in a USB serial port should add a new device at `/dev/cu.usbserial-SOMETHING`.

Here are a few ways to get OS X to talk to serial ports. Take your pick of what you prefer. On pre-Mavericks (OS X 10.8 and below) machines, you'll need the FTDI serial driver from [FTDI](http://www.ftdichip.com/Drivers/VCP.htm) to make the serial port appear.

1. Run `tools/serial_cat`, which will automatically try to figure out the serial port that appeared at /dev and start echoing its contents to the console.

1. `screen /dev/cu.usbserial-* 38400`. Unless you know GNU screen voodoo, screen may or may not override your Terminal scrollback buffer, which you may or may not want.

2. `brew install minicom && minicom -D /dev/cu.usbserial-* -b 38400`. Minicom is a fairly old Unix program that was popular back in the warez^H^H^H^H^Hmodem days. You may want to disable the status line by pressing Esc -> O -> Screen and keyboard -> C.

4. If you are 31337 like Andre and like to use `cat`:

        exec 3<> /dev/cu.usbserial-*  # attach file descriptor 3 to serial port
        stty -f /dev/cu.usbserial-* speed 38400  # set serial port to 38400 baud
        cat <&3  # run cat with stdin attached to file descriptor 3
