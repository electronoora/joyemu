/*
 * joyemu 
 *
 * Functions for interfacing with an MCP23017 I/O extender connected on I2C bus.
 *
 *
 * Copyright (c) 2017 Noora Halme
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list
 *    of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 *    list of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include <asm/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"

// i2c device file descriptor
int i2c_dev=0;

// last joystick port pin states written to I/O board
uint16_t last_port1=0, last_port2=0;


// get an I2C bus file descriptor and acquire access to board address
int open_i2c(char *device, uint16_t base_addr) {
  int h=open(device, O_RDWR);
  if (h < 0) {
    debug_log(LOGLEVEL_ERROR, "I2C: failed to open bus device %s, errno %d", device, errno);
    return 0;
  }
  if (ioctl(h, I2C_SLAVE, base_addr) < 0) {
    debug_log(LOGLEVEL_ERROR, "I2C: failed to acquire slave access to 0x%03x, errno %d", base_addr, errno);
    return 0;
  }
  debug_log(LOGLEVEL_DEBUG, "I2C: opened bus device %s and acquired access to slave at 0x%03x", device, base_addr);
  return h;
}


// write a byte to a register on the opened I2C device
int write_i2c(uint8_t regno, uint8_t data)
{
  if (i2c_dev) {
    int rc=i2c_smbus_write_byte_data(i2c_dev, regno, data);
    if (rc == -1) {
      debug_log(LOGLEVEL_ERROR, "I2C: writing byte to register 0x%02x failed with errno %d", regno, errno);
      return -1;
    }
    debug_log(LOGLEVEL_EXTRADEBUG, "I2C: wrote 0x%02x to register 0x%02x", data, regno);
    return 0;
  } else return -1;
}


// read a bute from the opened I2C device
int read_i2c(uint8_t regno, uint8_t *data)
{
  if (i2c_dev) {
    int rc=i2c_smbus_read_byte_data(i2c_dev, regno);
    if (rc == -1) {
      debug_log(LOGLEVEL_ERROR, "I2C: reading byte from register 0x%02x failed with errno %d", regno, errno);
      return -1;
    }
    debug_log(LOGLEVEL_EXTRADEBUG, "I2C: read 0x%02x from register 0x%02x", data, regno);
    return 0;
  } else return -1;
}


// set IODIRA/IODIRB register on the MCP23017
int mcp_set_iodir(uint8_t bank, uint8_t iodir) // set io direction register
{
  return write_i2c(0x00+bank, iodir); // 1=input, 0=output
}


// set GPPUA/GPPUB register on the MCP23017
int mcp_set_gppu(uint8_t bank, uint8_t gppu) // set pull-up resistor config
{
  return write_i2c(0x0c+bank, gppu); // 1=pull-up enabled
}


// write to GPIOA/GPIOB registers on the MCP23017
int mcp_write_gpio(uint8_t bank, uint8_t data)
{
  return write_i2c(0x12+bank, data);
}


// read GPIOA/GPIOB register from the MCP23017
unsigned char mcp_read_gpio(uint8_t bank) {
  uint8_t gpio;
  read_i2c(0x12+bank, &gpio);
  return gpio;
}


// write the joystick port pin states to the GPIO pins
void mcp_update_port_state(uint16_t port1_pins, uint16_t port2_pins) {
  uint8_t p=0;

  if (last_port1 != port1_pins) {
    debug_log(LOGLEVEL_DEBUG, "Port 1 pins [ %1d %1d %1d %1d %1d %1d %1d %1d %1d ]",
      port1_pins>>8, (port1_pins>>7)&1, (port1_pins>>6)&1, (port1_pins>>5)&1, (port1_pins>>4)&1,
      (port1_pins>>3)&1, (port1_pins>>2)&1, (port1_pins>>1)&1, port1_pins&1);
    p=(port1_pins&0x00f) | ((port1_pins&0x020)>>1) | ((port1_pins&0x100)>>3);
    mcp_write_gpio(0, p);
    last_port1=port1_pins;
  }
  
  if (last_port2 != port2_pins) {
    debug_log(LOGLEVEL_DEBUG, "Port 2 pins [ %1d %1d %1d %1d %1d %1d %1d %1d %1d ]",
      port2_pins>>8, (port2_pins>>7)&1, (port2_pins>>6)&1, (port2_pins>>5)&1, (port2_pins>>4)&1,
      (port2_pins>>3)&1, (port2_pins>>2)&1, (port2_pins>>1)&1, port2_pins&1);
    p=(port2_pins&0x00f) | ((port2_pins&0x020)>>1) | ((port2_pins&0x100)>>3);
    mcp_write_gpio(1, p);
    last_port2=port2_pins;
  }
}


// initialize the MCP23017 to required state
int mcp_initialize(uint8_t bus, uint16_t addr) // i2c bus number
{
  char dev[512];

  // open the I2C device  
  snprintf((char*)&dev, 511, "/dev/i2c-%d", bus);
  i2c_dev=open_i2c(dev, addr);
  if (i2c_dev) {

    // reset IOCON to set BANK=0. if already 0, the write goes to GPINTENB and has no effect
    write_i2c(0x05, 0x00);
  
    mcp_set_iodir(0, 0x00); // set all pins on GPIOA and GPIOB
    mcp_set_iodir(1, 0x00); // to output
  
    write_i2c(0x04, 0x00);  // disable interrupt on all pins by setting
    write_i2c(0x05, 0x00);  // all bits in GPINTENA and GPINTENB low
    
    return i2c_dev;
  } else {
    return 0;
  }
}
