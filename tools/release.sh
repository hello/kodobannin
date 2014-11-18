#!/bin/sh
make clean \
    && make bootloader+pill_EVT2\
    && make pill+pill_EVT2\
    && make bootloader_serial+morpheus_EVT3\
    && make morpheus+morpheus_EVT3




