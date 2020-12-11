/*
 * joyemu 
 *
 * Functions for querying USB/Bluetooth HID devices providing a Linux event device.
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
#include <glob.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include "defaults.h"
#include "logging.h"
#include "ports.h"


// sixaxis and dualshock3 have weird undocumented event codes
#define BTN_SIXAXIS_UP		292
#define BTN_SIXAXIS_RIGHT	293
#define BTN_SIXAXIS_DOWN	294
#define BTN_SIXAXIS_LEFT	295
#define BTN_SIXAXIS_TRIANGLE	300
#define BTN_SIXAXIS_CIRCLE	301
#define BTN_SIXAXIS_CROSS	302
#define BTN_SIXAXIS_SQUARE	303


// supported dpad event types
#define DPAD_TYPE_XBOX		1
#define DPAD_TYPE_GENERIC	2
#define DPAD_TYPE_SIXAXIS	3


// event device numbers set on command line
int mouse_devno=-1, joy1_devno=-1, joy2_devno=-1;

// number of gamepads found, boolean for mouse found
int gamepads_found=0, mouse_found=0;

// linux evdev structs for assigned devices
struct libevdev *dev_joysticks[MAX_JOYSTICKS]={NULL, NULL};
struct libevdev *dev_mouse = NULL;


// designate a particular event device number for a device
void input_set_mouse_device(int d) { mouse_devno=d; }
void input_set_joystick1_device(int d) { joy1_devno=d; }
void input_set_joystick2_device(int d) { joy2_devno=d; }


// return the number of joysticks connected
int input_joysticks_connected(void) {
  return gamepads_found;
}


// return nonzero if a mouse is connected
int input_mouse_connected(void) {
  return mouse_found;
}


// scan linux event devices under /dev/input and query their capabilities.
// depending on event types and codes, a device may be accepted either as
// a gamepad/joystick or a mouse. 
int input_scan_devices(int mouse_to_port, int first_joystick) {
  int fd, rc, i, devno;
  glob_t glob_result;
  struct libevdev *dev = NULL;
  int attach_to_port=(first_joystick-1)%MAX_JOYSTICKS;
  
  rc=glob("/dev/input/event*", GLOB_ERR, NULL, &glob_result);
  if (!rc) {
    // ok, what did we find?
    int i=0;
    for(i=0;i<glob_result.gl_pathc;i++) {
      sscanf(glob_result.gl_pathv[i], "/dev/input/event%d", &devno);
      debug_log(LOGLEVEL_VERBOSE, "Checking device %s, number %d", glob_result.gl_pathv[i], devno);

      fd = open(glob_result.gl_pathv[i], O_RDONLY|O_NONBLOCK);
      rc = libevdev_new_from_fd(fd, &dev);
      if (rc < 0) {
        debug_log(LOGLEVEL_ERROR, "Failed to init libevdev (%s)", strerror(-rc));
      } else {
        debug_log(LOGLEVEL_VERBOSE, "Input device name: \"%s\"", libevdev_get_name(dev));
        debug_log(LOGLEVEL_DEBUG, "Input device ID: bus %#x vendor %#x product %#x",
          libevdev_get_id_bustype(dev),
          libevdev_get_id_vendor(dev),
          libevdev_get_id_product(dev));
          
        // is this a gamepad, a mouse or neither?
        if (!libevdev_has_event_type(dev, EV_REL) ||
            !libevdev_has_event_code(dev, EV_KEY, BTN_LEFT) ||
            !libevdev_has_event_code(dev, EV_KEY, BTN_RIGHT)) {
          // not likely to be a mouse
          debug_log(LOGLEVEL_DEBUG, "Doesn't look like a mouse");
          
          int has_dpad=0, has_fire=0;
          
          // xbox controllers send EV_ABS with ABS_HAT0X and ABS_HAT0Y
          if (libevdev_has_event_type(dev, EV_ABS) &&
              libevdev_has_event_code(dev, EV_ABS, ABS_HAT0X) &&
              libevdev_has_event_code(dev, EV_ABS, ABS_HAT0Y)
             )
          {
            has_dpad=DPAD_TYPE_XBOX;
          } else {
            // generic dpad events sent as EV_KEY
            if (libevdev_has_event_type(dev, EV_KEY) &&
                libevdev_has_event_code(dev, EV_KEY, BTN_DPAD_UP) &&
                libevdev_has_event_code(dev, EV_KEY, BTN_DPAD_RIGHT) &&
                libevdev_has_event_code(dev, EV_KEY, BTN_DPAD_DOWN) &&
                libevdev_has_event_code(dev, EV_KEY, BTN_DPAD_LEFT)
               )
            {
              has_dpad=DPAD_TYPE_GENERIC;
            } else {
              // sixaxis and dualshock3 send very unstandard event codes
              if (libevdev_has_event_type(dev, EV_KEY) &&
                  libevdev_has_event_code(dev, EV_KEY, BTN_SIXAXIS_UP) &&
                  libevdev_has_event_code(dev, EV_KEY, BTN_SIXAXIS_RIGHT) &&
                  libevdev_has_event_code(dev, EV_KEY, BTN_SIXAXIS_DOWN) &&
                  libevdev_has_event_code(dev, EV_KEY, BTN_SIXAXIS_LEFT)
                 )
              {
                has_dpad=DPAD_TYPE_SIXAXIS;
              }
            }
          }
          
          if (has_dpad) {
            debug_log(LOGLEVEL_DEBUG, "Device has a dpad, event type %d", has_dpad);
            // need to find at least one fire button in addition to dpad
            if (libevdev_has_event_type(dev, EV_KEY) &&
                (
                  libevdev_has_event_code(dev, EV_KEY, BTN_NORTH) ||
                  libevdev_has_event_code(dev, EV_KEY, BTN_EAST) ||
                  libevdev_has_event_code(dev, EV_KEY, BTN_SOUTH) ||
                  libevdev_has_event_code(dev, EV_KEY, BTN_WEST) ||
                  libevdev_has_event_code(dev, EV_KEY, BTN_SIXAXIS_TRIANGLE) ||
                  libevdev_has_event_code(dev, EV_KEY, BTN_SIXAXIS_CIRCLE) ||
                  libevdev_has_event_code(dev, EV_KEY, BTN_SIXAXIS_CROSS) ||
                  libevdev_has_event_code(dev, EV_KEY, BTN_SIXAXIS_SQUARE)
                )
               )
            {
              has_fire=1;
            }
          }
          
          if (has_dpad && has_fire) {
            // device is very likely a gamepad
            if (gamepads_found < MAX_JOYSTICKS) {
              if ( (gamepads_found==0) ? (joy1_devno==-1 || joy1_devno==devno) : (joy2_devno==-1 || joy2_devno==devno) ) {
                dev_joysticks[attach_to_port]=dev;
                debug_log(LOGLEVEL_VERBOSE, "Device has capabilities to function as a joystick, assigning it to port %d", attach_to_port+1);
                gamepads_found++;
                attach_to_port=(attach_to_port+1)%MAX_JOYSTICKS;
              }
            }
          } else {
              debug_log(LOGLEVEL_DEBUG, "Doesn't look like a gamepad either");            
          }
        } else {
          // device is most probably a mouse
          if (!mouse_found) {
            if (mouse_devno==-1 || mouse_devno==devno) {
              mouse_found=1;
              dev_mouse=dev;
              debug_log(LOGLEVEL_VERBOSE, "Device has capabilities to function as a mouse, assigning it to port %d", mouse_to_port);
            } else {
              debug_log(LOGLEVEL_DEBUG, "Device %d appears to be a mouse but use designated device number %d", devno, mouse_devno);
            }
          }
        }
      }
    }
    globfree(&glob_result);
  } else return rc; // GLOB_NOSPACE, GLOB_ABORTED or GLOB_NOMATCH
  
  if (mouse_found) {
    debug_log(LOGLEVEL_INFO, "Using \"%s\" to emulate a mouse in port %d", libevdev_get_name(dev_mouse), mouse_to_port);
  }

  for(i=0;i<MAX_JOYSTICKS;i++) {
    if (!dev_joysticks[i]) continue;
    debug_log(LOGLEVEL_INFO, "Using \"%s\" to emulate a joystick in port %d", libevdev_get_name(dev_joysticks[i]), i+1);
  }  
  
  return 0;
}


// polling thread for reading pending events from devices assigned to
// joystick or mouse emulation
void *input_poll_thread(void *params) {
  int rc, i;
  struct input_event ev;

  debug_log(LOGLEVEL_DEBUG, "Started event poll thread");
  do {
    if (mouse_found) {
      rc=libevdev_next_event(dev_mouse, LIBEVDEV_READ_FLAG_NORMAL, &ev);
      if (!rc) {
        debug_log(LOGLEVEL_EXTRADEBUG, "Mouse: %s %s %d", libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);
        
        if (ev.type==EV_REL) {
          // mouse movement
          switch(ev.code) {
            case REL_X:
            mouse_move(PORT_AXIS_HORIZONTAL, ev.value);
            break;
            
            case REL_Y:
            mouse_move(PORT_AXIS_VERTICAL, ev.value);
            break;
          }
        } else if (ev.type==EV_KEY) {
          switch(ev.code) {
            case BTN_LEFT:
            mouse_set_lmb(ev.value);
            break;
            
            case BTN_RIGHT:
            mouse_set_rmb(ev.value);
            break;
          }
        }
      }
    }

    for(i=0;i<MAX_JOYSTICKS;i++) {
      if (!dev_joysticks[i]) continue;
      
      rc=libevdev_next_event(dev_joysticks[i], LIBEVDEV_READ_FLAG_NORMAL, &ev);
      if (!rc) {
        debug_log(LOGLEVEL_EXTRADEBUG, "Joystick %d: %s %s %d", i+1, libevdev_event_type_get_name(ev.type), libevdev_event_code_get_name(ev.type, ev.code), ev.value);
        
        // direction on dpad?
        if (ev.type==EV_ABS) {
          switch(ev.code) {
            case ABS_HAT0X:
            joystick_set_axis(i, PORT_AXIS_HORIZONTAL, ev.value);
            break;
            
            case ABS_HAT0Y:
            joystick_set_axis(i, PORT_AXIS_VERTICAL, ev.value);
            break;
          }
        } else if (ev.type==EV_KEY) {
          switch(ev.code) {
            case BTN_DPAD_UP:
            case BTN_SIXAXIS_UP:
            joystick_set_axis(i, PORT_AXIS_VERTICAL, PORT_AXIS_STATE_UP * ev.value);
            break;
            
            case BTN_DPAD_RIGHT:
            case BTN_SIXAXIS_RIGHT:
            joystick_set_axis(i, PORT_AXIS_HORIZONTAL, PORT_AXIS_STATE_RIGHT * ev.value);
            break;
            
            case BTN_DPAD_DOWN:
            case BTN_SIXAXIS_DOWN:
            joystick_set_axis(i, PORT_AXIS_VERTICAL, PORT_AXIS_STATE_DOWN * ev.value);
            break;
            
            case BTN_DPAD_LEFT:
            case BTN_SIXAXIS_LEFT:
            joystick_set_axis(i, PORT_AXIS_HORIZONTAL, PORT_AXIS_STATE_LEFT * ev.value);            
            break;
            
            // all face button types map to joystick button 1
            case BTN_NORTH:
            case BTN_EAST:
            case BTN_SOUTH:
            case BTN_WEST:
            case BTN_SIXAXIS_TRIANGLE:
            case BTN_SIXAXIS_CIRCLE:
            case BTN_SIXAXIS_CROSS:
            case BTN_SIXAXIS_SQUARE:
            joystick_set_fire(i, ev.value);
            break;
          }
        }
      }      
    }
  } while (1);
}
