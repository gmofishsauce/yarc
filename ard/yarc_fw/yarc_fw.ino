
// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
// Downloader firmware for Yet Another Retro Computer - YARC
//
// The Arduino IDE has peculiar rules about multiple file programs.
// Separate compilation is only allowed for "library modules", which
// are less convenient to develop than "ordinary" firmware.
//
// But the IDE allows explicit include files, so the best compromise
// is to put most of the code in .h files and include them here. So
// if we were naive, all symbols would be equally visible throughout
// the code. We could live with this, but it would be annoying. We
// implement a (slightly) better solution.
//
// We have two kinds of files: _decls files which contain only forward
// declarations of functions intended to be public and _task files
// containing code. Each task file may define a C++11 namespace
// conventionally named TaskPrivate, e.g. SerialPrivate, etc., for
// its private functions and data. The task's public methods must
// explicitly reference the private namespace, which is annoying,
// but so must any other task that wants to breaks the visibility
// rules, and the compiler enforces that. It's a sufficient answer.
//
// Since the declaration files are just forwards, their order doesn't
// matter; and once they are all included first, the order of tasks
// doesn't matter either. So in theory there are no ordering
// dependencies except putting all the declaration includes before
// all the task includes.
//
// There is a tiny "task executor" for the "tasks". But don't be fooled;
// the task abstraction is just a way of structuring the main loop.
// There are bo trixie preemption schemes, no interrupt code, nothing
// like that. Just a main loop that runs through all the task bodies
// as quickly as possible. In fact, "tasks" don't need to have bodies
// or even init functions; the logging "task" is just a couple of public
// functions and some private data.
//
// We do concede to including the task runner last so we don't have
// to forward-declare all the task init and body functions. We could
// instead have a registration scheme, but this approach allows us to
// put the task table in ROM (see task_runner.h). We make similar use
// of PROGMEM for data tables in several of the task modules.

#include "task_decls.h"
#include "port_decls.h"
#include "display_decls.h"
#include "led_decls.h"
#include "log_decls.h"

#include "heartbeat_task.h"
#include "serial_task.h"
#include "port_task.h"
#include "display_task.h"
#include "led_task.h"
#include "log_task.h"

#include "task_runner.h"

void setup() {
  InitTasks();
}

// Main body - delegate to task runner
void loop() {
  for (;;) {
    RunTasks();
  }
}
