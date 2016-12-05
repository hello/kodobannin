#!/bin/sh
s3cmd put build/pill+pillx_DVT1.bin s3://hello-firmware/kodobannin/mobile/pill.bin &&
s3cmd setacl s3://hello-firmware/kodobannin/mobile/pill.bin --acl-public
