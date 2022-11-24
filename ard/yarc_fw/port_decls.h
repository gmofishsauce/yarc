// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
// Symbol prefix: port, PORT

// These are the declarations for port access by the rest
// of the firmware. The impl is in port_task.h.

#pragma once

// Public interface to the write-only 8-bit Display Register (DR)

void setDisplay(byte b);

// Public functions exposed by the port task

bool publicWriteByteToK(byte kReg, byte kVal);
