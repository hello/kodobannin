target remote localhost:2331
set remote memory-write-packet-size 1024
set remote memory-write-packet-size fixed
# monitor reg r13 = (0x20000000)
# monitor reg pc = (0x20000004)
monitor flash device = nRF51822_xxAA
