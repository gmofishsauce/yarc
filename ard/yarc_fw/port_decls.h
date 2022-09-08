// Copyright (c) Jeff Berkowitz 2021. All rights reserved.
// Symbol prefix: port, PORT

// These are the declarations for port access by the rest
// of the firmware. The impl is in port_task.h.

#pragma once

// Public interface to the write-only 8-bit Display Register (DR)
// After freezeDisplay, the DR will not change until the Nano is reset.

void setDisplay(byte b);
void freezeDisplay(byte b);
