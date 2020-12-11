/*
 * joyemu 
 *
 * Functions for logging and debugging
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

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "logging.h"


// logging verbosity level
int log_verbosity;

// prefix characters for log entries
const char *errlevel_prefix=".-+*!";


// set the logging verbosity
void debug_set_verbosity(int v) {
  log_verbosity=v;
}


// print a debugging line or discard it if below verbosity level
void debug_log(int level, const char *fmt, ...) {
  if (level < log_verbosity) return;
  char p=errlevel_prefix[level];
  struct timeval t;
  char fmtbuf[1024];

  gettimeofday(&t, NULL);
  va_list argptr;
  va_start(argptr, fmt);
  vsnprintf((char*)&fmtbuf, 1024, fmt, argptr);
  va_end(argptr);

  fprintf(stderr, "[%10d.%010d] %c%c %s\n", (long)(t.tv_sec), (long)(t.tv_usec), p, p, fmtbuf);
}
