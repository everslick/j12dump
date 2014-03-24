#!/bin/bash

PORT="/dev/ttyUSB1"

stty -F $PORT 38400

setserial $PORT spd_cust
setserial $PORT divisor 2304
