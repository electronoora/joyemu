/*
 * joyemu 
 *
 * Functions for emulating two 9-pin D-connector joystick ports.
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

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "io.h"
#include "logging.h"
#include "ports.h"


uint8_t mouse_on_port=1;
float mouse_speed=1.3;
uint8_t mouse_emulation=MOUSE_TYPE_AMIGA;

// bitmasks for mouse rotary encoder pulses and
// quadrature pulses
uint32_t mouse_x_encoder   =0x3c3c3c3c;
uint32_t mouse_x_quadrature=0xf0f0f0f0;
uint32_t mouse_y_encoder   =0x3c3c3c3c;
uint32_t mouse_y_quadrature=0xf0f0f0f0;

// accumulator for queued mouse movement
int mouse_x_accumulator=0, mouse_y_accumulator=0;

// current state of the pins in both ports
uint16_t port1_pins=0x016f, port2_pins=0x016f;

// axis direction names for debugging/logging
const char *axis_direction[2][3]={
  {"left", "center", "right"},
  {"up", "center", "down"}
};


// set the state of a joystick axis on one port
void joystick_set_axis(int port, int axis, int state) {
  uint16_t *port_pins=port ? &port2_pins : &port1_pins;
  if (state < -1 || state > 1) return;
  if (axis) {
    debug_log(LOGLEVEL_VERBOSE, "Joystick %d Y axis state %s", port+1, axis_direction[1][state+1]);
    switch (state) {
      case -1: // state -1: joystick port 1/2 disable pin 1, enable pin 2
      *port_pins&=0x01fe;
      *port_pins|=2;
      break;
      
      case  0: // state  0: joystick port 1/2 enable pin 1, enable pin 2
      *port_pins|=1;
      *port_pins|=2;
      break;
      
      case  1: // state  1: joystick port 1/2 enable pin 1, disable pin 2
      *port_pins|=1;
      *port_pins&=0x01fd;
      break;
    }
  } else {
    debug_log(LOGLEVEL_VERBOSE, "Joystick %d X axis state %s", port+1, axis_direction[0][state+1]);
    switch (state) {
      case -1: // state -1: joystick port 1/2 disable pin 3, enable pin 4
      *port_pins&=0x01fb;
      *port_pins|=8;
      break;
      
      case  0: // state  0: joystick port 1/2 enable pin 3, enable pin 4
      *port_pins|=4;
      *port_pins|=8;
      break;
      
      case  1: // state  1: joystick port 1/2 enable pin 3, disable pin 4
      *port_pins|=4;
      *port_pins&=0x01f7;
      break;
    }
  }  
}


// set joystick fire button 1 state on port
void joystick_set_fire(int port, int state) {
  uint16_t *port_pins=port ? &port2_pins : &port1_pins;
  debug_log(LOGLEVEL_VERBOSE, "Joystick %d fire button %s", port+1, state ? "down" : "up");
  *port_pins=(*port_pins&0x01df)|(((!state)&1)<<5);  // write state&1 to joystick port 1/2 pin 6
}


// set the port which the mouse is connected on
void mouse_set_port(int port) {
  mouse_on_port=port;
}


void mouse_set_emulation(int type) {
  mouse_emulation=type;
}

// rotate the horizintal encoder in the mouse for a number of bits (positive or negative)
void mouse_rotate_x_encoder(int8_t bits) {
  uint32_t e, q;
  uint16_t *port_pins=(mouse_on_port==2) ? &port2_pins : &port1_pins;
  
  if (bits < 0) {
    e=(mouse_x_encoder >> (-bits)) | (mouse_x_encoder << (32+bits));
    q=(mouse_x_quadrature >> (-bits)) | (mouse_x_quadrature << (32+bits));
  } else {
    e=(mouse_x_encoder << bits) | (mouse_x_encoder >> (32-bits));
    q=(mouse_x_quadrature << bits) | (mouse_x_quadrature >> (32-bits));
  }
  mouse_x_encoder=e;
  mouse_x_quadrature=q;
  if (mouse_emulation==MOUSE_TYPE_AMIGA) {
    *port_pins=(*port_pins&0x01fd)|((e&1)<<1); // write e&1 to joystick port 1 pin 2
    *port_pins=(*port_pins&0x01f7)|((q&1)<<3); // write q&1 to joystick port 1 pin 4
  } else {
    *port_pins=(*port_pins&0x01fd)|((e&1)<<1); // write e&1 to joystick port 1 pin 2  
    *port_pins=(*port_pins&0x01fe)|(q&1);      // write q&1 to joystick port 1 pin 1
  }
}


// rotate the vertical encoder in the mouse for a number of bits (positive or negative)
void mouse_rotate_y_encoder(int8_t bits) {
  uint32_t e=mouse_y_encoder, q=mouse_y_quadrature;
  uint16_t *port_pins=(mouse_on_port==2) ? &port2_pins : &port1_pins;

  if (bits < 0) {
    e=(mouse_y_encoder >> (-bits)) | (mouse_y_encoder << (32+bits));
    q=(mouse_y_quadrature >> (-bits)) | (mouse_y_quadrature << (32+bits));
  } else {
    e=(mouse_y_encoder << bits) | (mouse_y_encoder >> (32-bits));
    q=(mouse_y_quadrature << bits) | (mouse_y_quadrature >> (32-bits));
  }
  mouse_y_encoder=e;
  mouse_y_quadrature=q;  
  if (mouse_emulation==MOUSE_TYPE_AMIGA) {
    *port_pins=(*port_pins&0x01fe)|(e&1); // write e&1 to joystick port 1 pin 1
    *port_pins=(*port_pins&0x01fb)|((q&1)<<2); // write q&1 to joystick port 1 pin 3
  } else {
    *port_pins=(*port_pins&0x01fb)|((e&1)<<2); // write e&1 to joystick port 1 pin 3  
    *port_pins=(*port_pins&0x01f7)|((q&1)<<3); // write q&1 to joystick port 1 pin 4
  }
}


// move the mouse on an axis for the specified amount of distance units
void mouse_move(int axis, int distance) {
  float scaled_distance=mouse_speed*distance;
  int delta=round(scaled_distance);
  
  if (axis) {
    mouse_y_accumulator+=delta;
    debug_log(LOGLEVEL_DEBUG, "Mouse moved vertically %d units", distance);
  } else {
    mouse_x_accumulator+=delta;
    debug_log(LOGLEVEL_DEBUG, "Mouse moved horizontally %d units", distance);
  }
}


// set mouse left button state (1=up, 0=down)
void mouse_set_lmb(int state) {
  uint16_t *port_pins=(mouse_on_port==2) ? &port2_pins : &port1_pins;
  debug_log(LOGLEVEL_VERBOSE, "Mouse left button %s", state ? "down" : "up");
  *port_pins=(*port_pins&0x01df)|(((!state)&1)<<5); // write state&1 to joystick port 1 pin 6
}


// set mouse right button state (1=up, 0=down)
void mouse_set_rmb(int state) {
  uint16_t *port_pins=(mouse_on_port==2) ? &port2_pins : &port1_pins;
  debug_log(LOGLEVEL_VERBOSE, "Mouse right button %s", state ? "down" : "up");
  *port_pins=(*port_pins&0x00ff)|(((!state)&1)<<8); // write state&1 to joystick port 1 pin 9
}


// the thread function which performs port I/O and steps the mouse encoders
void *port_io_thread(void *params) {
  struct timeval t, last_t;
  long int dt=0;
  
  debug_log(LOGLEVEL_DEBUG, "Started port I/O thread");
  gettimeofday(&last_t, NULL);
  do {
    gettimeofday(&t, NULL);
    dt=t.tv_usec - last_t.tv_usec;
    if (dt<0) dt=1000000L-dt;
    if (dt > ENCODER_MIN_US_PER_BIT) {

      if (mouse_x_accumulator > 0) {
        mouse_rotate_x_encoder(ENCODER_BITS_PER_UNIT);
        mouse_x_accumulator--;
      } else if (mouse_x_accumulator < 0) {
        mouse_rotate_x_encoder(-ENCODER_BITS_PER_UNIT);
        mouse_x_accumulator++;
      }
      if (mouse_y_accumulator > 0) {
        mouse_rotate_y_encoder(ENCODER_BITS_PER_UNIT);
        mouse_y_accumulator--;
      } else if (mouse_y_accumulator < 0) {
        mouse_rotate_y_encoder(-ENCODER_BITS_PER_UNIT);
        mouse_y_accumulator++;
      }

      memcpy(&last_t, &t, sizeof(struct timeval));
    }
    mcp_update_port_state(port1_pins, port2_pins);
  } while (1);
}
