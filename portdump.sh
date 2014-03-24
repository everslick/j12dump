#!/bin/bash

PORT="/dev/ttyUSB1"

hexdump -v -e '/1  "%_ad: ""\t""    "' -e '/1    "0x%02X"' -e '/1 " = %03u "' -e '/1 " = _%_u\_\n"' $PORT
