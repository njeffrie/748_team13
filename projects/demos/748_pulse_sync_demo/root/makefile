# Platform name  cc2420DK, firefly, micaZ
#PLATFORM = firefly2_2
PLATFORM = firefly3
#PLATFORM = firefly2_3


# Target file name (without extension).
TARGET = main

# Set the Port that you programmer is connected to 
PROGRAMMING_PORT = /dev/ttyUSB0 # Default FireFly port 
#PROGRAMMING_PORT = /dev/tty.usbserial-AE00BULD # Default micaZ port 
#PROGRAMMING_PORT = /dev/tty.usbserial-AE00BUMF # Default micaZ port 
#PROGRAMMING_PORT = /dev/tty.usbserial-AM017Y3D
#PROGRAMMING_PORT = /dev/tty.usbserial-AM017Y4H

# Set this such that the nano-RK directory is the base path
ROOT_DIR = ../../../..

# Set platform specific defines 
# The following will be defined based on the PLATFORM variable:
# PROG_TYPE  (e.g. avrdude, or uisp)
# MCU (e.g. atmega32, atmega128, atmega1281) 
# RADIO (e.g. cc2420)
include $(ROOT_DIR)/include/platform.mk


SRC = $(TARGET).c

# Add extra source files. 
# For example:
SRC += $(ROOT_DIR)/src/net/flash/flash.c
SRC += $(ROOT_DIR)/src/net/pulse_sync/pulse_sync.c

# Add extra includes files. 
# For example:
EXTRAINCDIRS =
EXTRAINCDIRS += $(ROOT_DIR)/src/net/flash
EXTRAINCDIRS += $(ROOT_DIR)/src/net/pulse_sync


#  This is where the final compile and download happens
include $(ROOT_DIR)/include/platform/$(PLATFORM)/common.mk
