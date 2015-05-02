# Platform name  cc2420DK, firefly2_1, firefly2_2, micaZ
PLATFORM = firefly3


# Target file name (without extension).
TARGET = main

# Set the Port that you programmer is connected to 
##PROGRAMMING_PORT = /dev/ttyUSB1 # programmer connected to serial device
#PROGRAMMING_PORT = /dev/tty.usbserial-AM017Y3E
#PROGRAMMING_PORT = /dev/tty.usbserial-AM017Y3D
PROGRAMMING_PORT = /dev/tty.usbserial-AE00BUMF
#PROGRAMMING_PORT = /dev/tty.usbserial-AM017Y4H
#PROGRAMMING_PORT = /dev/tty.usbserial-AE00BULD
# Set this such that the nano-RK directory is the base path
ROOT_DIR = ../../..

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


# Add extra includes files. 
# For example:
EXTRAINCDIRS =
EXTRAINCDIRS += $(ROOT_DIR)/src/net/flash


#  This is where the final compile and download happens
include $(ROOT_DIR)/include/platform/$(PLATFORM)/common.mk
