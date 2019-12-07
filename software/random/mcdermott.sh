#!/bin/sh

# Clear memory
/bin/fm 0x40000000 0x00 20 0 > /dev/null
# Setup header for 512-bit chunk
/bin/pm 0x40000000 0x6162630a
/bin/pm 0x40000004 0x80000000
# /bin/pm 0x40000000 0x6162630a
# /bin/pm 0x40000004 0x80000000
# Setup tail of the 512-bit chunk
/bin/pm 0x4000003c 0x0020 > /dev/null
# Set the number of chunks
/bin/pm 0x44000014 0x1 > /dev/null
# Set the starting BRAM address
/bin/pm 0x44000018 0x0 > /dev/null
# Reset SHA Unit
/bin/pm 0x44000000 0x0 > /dev/null
# Enable SHA conversion
/bin/pm 0x44000000 0x3 > /dev/null
# Disable SHA conversion
/bin/pm 0x44000000 0x2 > /dev/null
# Dump results
/bin/dm 0x44000000 16
