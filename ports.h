/*
 * joyemu 
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

#ifndef _PORTS_H_
#define _PORTS_H_

// constans for joystick axes
#define PORT_AXIS_HORIZONTAL	0
#define PORT_AXIS_VERTICAL	1

// all possible states for joystick axes
#define	PORT_AXIS_STATE_UP	-1
#define PORT_AXIS_STATE_CENTER	0
#define PORT_AXIS_STATE_DOWN	1
#define PORT_AXIS_STATE_LEFT	-1
#define PORT_AXIS_STATE_RIGHT	1

// bits to rotate per move unit and minimum microseconds to hold a bit
#define ENCODER_BITS_PER_UNIT   7
#define ENCODER_MIN_US_PER_BIT  2

// mouse emulation type
#define MOUSE_TYPE_AMIGA	0
#define MOUSE_TYPE_ATARI_ST	1

void joystick_set_axis(int port, int axis, int state);
void joystick_set_fire(int port, int state);

void mouse_set_port(int port);
void mouse_set_emulation(int type);

void mouse_rotate_x_encoder(int8_t bits);
void mouse_rotate_y_encoder(int8_t bits);

void mouse_move(int axis, int distance);
void mouse_set_lmb(int state);
void mouse_set_rmb(int state);

void *port_io_thread(void *params);

#endif