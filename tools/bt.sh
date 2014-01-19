sudo kextunload -b com.apple.iokit.BroadcomBluetoothHostControllerUSBTransport
sudo kextunload -b com.apple.iokit.IOBluetoothSerialManager
sudo kextunload -b com.apple.iokit.IOBluetoothUSBDFU
sudo kextunload -b com.apple.iokit.IOBluetoothHostControllerUSBTransport
sudo kextunload -b com.apple.iokit.IOBluetoothFamily

sudo kextload -b com.apple.iokit.IOBluetoothFamily
sudo kextload -b com.apple.iokit.IOBluetoothHostControllerUSBTransport
sudo kextload -b com.apple.iokit.IOBluetoothUSBDFU
sudo kextload -b com.apple.iokit.IOBluetoothSerialManager
sudo kextload -b com.apple.iokit.BroadcomBluetoothHostControllerUSBTransport
