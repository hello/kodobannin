#!/bin/sh
make clean \
    && make bootloader+pill_DVT1\
    && make pill+pill_DVT1\
    && make bootloader_serial+morpheus_DVT1\
    && make morpheus+morpheus_DVT1




