#!/bin/sh
make clean \
    && make bootloader+pill_PVT1\
    && make pill+pill_PVT1\
    && make bootloader_serial+morpheus_PVT1\
    && make morpheus+morpheus_PVT1




