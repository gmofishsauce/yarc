# Serial Protocol (Nano Transport Layer, “NTL”)

This document was converted from 72-column text format to markdown in June 2023. The current version of the protocol is v10.

## Overview

Communication between the host (Mac) and the “YARC downloader” (Arduino) takes place using a simple byte-oriented binary protocol, NTL. The protocol is request/response with the host initiating all activity. The protocol is session-oriented, with a session establishment phase called “synchronization” followed by a data transfer phase of indefinite length (ideally, for as long as the YARC continues running).

Once synchronization is achieved, the host sends single-byte commands. Command bytes are in the range 0xE1 - 0xFF (31 possible commands). Command bytes may be followed by 0 to N fixed arguments bytes where N is no larger than 7. The number of argument bytes is fixed by the protocol for each command byte.

The Nano collects the command and its fixed-length argument list and then acks by echoing the bitwise negation of the command back to the host. Example: after sending the valid command 0xF0 0x55, the host expects to read a single ack byte with value 0x0F. If an error occurs, the Nano sends a NAK byte in the range of 0x80 - 0x8F. The low nybble of a NAK is an error code 0x0 - 0xF. The error codes are primarily informational and are not defined by this protocol spec.

Commands can be retried by the host, but the only means of recovery from a persistent error state is to close and reopen the connection, which toggles the DTR line and resets the Nano. In fact, after connection establishment, the existing protocol code responds to all errors by resetting the Nano.

The final byte of the fixed arguments may specify a variable-length data transfer in either direction. The transfer occurs only after the Nano responds with ACK. NAK, when it occurs, always terminates command processing.

This spec originally anticipated variable-length data in various lengths. In fact, the serial line has no flow control. This limits the length of burst data to 64 bytes. All protocol commands have now been updated to transfer data in 64 byte "chunkies". A future version of this spec may remove the byte counts in favor of read and write memory commands that transfer a 64 byte chunk.

In order to support logging from the Nano to the host console, the host is expected to issue Poll (0xE9, formerly GetMsg) commands frequently.  Poll requests should be sprinkled between other commands during a multipart download. Failure to issue enough Poll commands from the host may result in log buffer overflow in the Nano, which has very limited log buffer space. Poll responses may also contain service requests from the YARC, but these are not time sensitive (fully synchronous) and will never occur during a download because a hardware interlock prevents the YARC from generating bus cycles while the Nano has control.

## Connection establishment

Arduino firmware snoops on the serial line after a reset, trying to decide if it should accept a firmware update.  For this reason the first few bytes transmitted from the host during the first few seconds after a reset may be lost.  Alternatively, if the host loses sync while the Nano is attempting to send a response, the Nano may “spew” from the host’s perspective.  The Nano never sends more than 257 bytes (one byte ack, one byte count, and up to 255 data bytes) without stopping to wait for a command. Finally, if the Nano is idle and ready to accept a command, it should respond quickly, within a millisecond. 10 or 20 milliseconds provides plenty of margin.

With these factors in mind, host connection establishment should proceed as follows. The description assumes that host software can time out I/O operations on the serial port (i.e. the host does not block indefinitely). The resolution of host timeouts can be crude, e.g. 10mS or even 16.67mS = 1/(60 Hz) is adequate.

1. If the Nano has not been reset, attempt to drain up to 257 data bytes.  If the Nano continues to transmit, report an error, reset the Nano, and proceed with step 2.

2. If the Nano has been reset (i.e.  the USB serial device has just been opened), wait several seconds (empirically, 3 seconds seems like enough).

3. Begin sending up to N sync commands, one every second or so, until an ack is received. N may be in the range of 3 to 10.

4. If no ack is received after N tries, log error, reset the Nano, and go to step 2.

5. When an ack is received, attempt to drain up to n delayed acks where n < N is the number of sync commands actually sent.  When there is a response timeout, proceed.

6. The Nano is quiet. Send a GetVersion command.  If the response is a nak or there is a response timeout, something is badly wrong so reset the Nano and go to step 2.

The host should now decide "as it sees fit" whether the Nano’s protocol version is compatible. If not, the situation is probably hopeless and the host should abort (the protocol does not specify a way for the host to update the Nano’s firmware).

Otherwise, connection is established. The host should proceed by immediately sending Poll command(s) and processing the message bodies until the Nano responds with an ack followed by a 0 count. The host must then continue to issue Poll commands frequently.

## State Model

The Nano initializes in conceptual "ACTIVE" mode, which corresponds to the YARC/Nano# bit in the Machine Control Register (MCR) being low.  This setting locks out the YARC from driving the system address and data busses and enables the Nano's drivers. PASSIVE mode similarly locks the Nano out from bus access. The hardware interlock is designed to prevent a software error from causing bus contention that could damage the drivers.

When the YARC has been prepared for execution, the host may send the RunYARC command (which puts the Nano into the PASSIVE state).  When in the PASSIVE state, commands to perform bus cycles will fail.  They may be rejected by the Nano (implementation detail). Commands to alter the clock and stop the YARC will be accepted in PASSIVE mode. Other commands should not be issued.

## Design Rationale

The serial line between host and Nano is assumed to be highly reliable because modern "serial interfaces" are really USB connections.  As a result, this protocol is more like a message-oriented session protocol than a link layer.  If a traditional serial line is to be employed, it will be necessary to wrap this protocol in a link layer and/or transport suitable for noisy serial lines. There are a lot of those to choose from, so no more will be said about it.

The YARC toolchain is written in Go and runs on the Mac (it could run on Windows or Linux because of Go’s strong portability story).  The front end of the toolchain is an assembler which generates an absolute binary image. The back end of the toolchain can read image files and download them to the YARC.

The serial line is currently configured to run at 115kbps (changing this requires recompiling both the host software and Arduino firmware). The line is not flow-controlled. Both ends of the line contain sufficient hardware and driver-level buffering to allow protocol command carrying up to 64 data bytes to sent or received reliably. Larger bursts result in lost data. This was not known when the protocol was designed. The protocol now supports 64 byte counts only. A future update to this document may remove the redundant count bytes.

## Protocol commands

All commands are ack’d (or nak’d). The ack (nak) byte is assumed and so not counted as a result or response byte in the description below. In general, the host implementation decides whether an error should result in a Nano reset and session recreation or an attempt to continue.

##### Reserved, do not use - 0xE0

This command should not be used because its ack pattern (bitwise negation of the command) conflicts with a valid ASCII character (space).  The response is nak.

##### Get Machine Control Register - 0xE1
No arguments bytes
<br>
1 result byte

Get the value of the Machine Control Register (MCR) for the YARC.  The MCR is an 8-bit register that controls several YARC features, including the hardware clock and the YARC/NANO bit that arbitrates bus ownership between the Nano and the YARC.

##### Run COST tests - 0xE2
##### Stop COST tests - 0xE3
No argument bytes
<br>
No result byte

These commands stop and start the Continuous Self Test (COST). The COST should be stopped before trying to alter any state.

##### Clock Control - 0xE4
1 argument byte
<br>
1 result byte

YARC clock pulses may be generated by the YARC's hardware clock or by the Nano. The clock stops in the high state, so a single pulse always produces a falling edge followed by a rising edge. The hardware clock runs at 1MHz or more and is sometimes called the "fast clock". The Nano-controlled software "slow clock" produces assymetrical pulses with long "high" times (10 to 100 microseconds) and "low" times of less than 500ns. Clock commands are in the
argument byte as follows:

0 - stop the clock.
<br>
1 .. 127 - generate 1 to 127 pulses of the software (slow) clock
<br>
255 (0xFF) - start the fast clock
<br>
128 (0x80) - start the slow clock free-running


The MCR value (not the Bus Input Register BIR) is returned.

Note: the obsolete command 0xF4 Perform Bus Cycle is retained, for now, with the same functionality as 0xE4 0x01.

##### WriteMem - 0xE5
3 argument bytes
<br>
64 data bytes
<br>
No result byte

The first two argument bytes specify a main memory address in the range 0 .. 0x77C0, MSB first. The third byte specifies the count, which must be 64. The 64 bytes of data that follow are written to memory at the address. 

##### ReadMem - 0xE6
3 argument bytes
<br>
No data bytes
<br>
64 result bytes

The first two argument bytes specify a main memory address in the range 0 .. 0x77C0, MSB first. The third byte specifies the count, which must be 64. 64 bytes of data are read from the address and transmitted to the host.

##### RunYARC - 0xE7
No argument bytes
<br>
No result bytes

The Nano disables both clocks and sets the YARC/Nano# bit in the MCR high to enable YARC bus cycles. This command does not start the clock. When the Nano is passive, it cannot initiate bus cycles. As result, commands to read and write data will fail.

##### StopYARC - 0xE8
No argument bytes
<br>
No result bytes

The Nano stops the YARC clock and sets the YARC/Nano# bit in the MCR to low.

##### Poll - 0xE9
No arguments bytes
<br>
1 to 255 results bytes

The host issues this command periodically (and often) to read service requests from the Nano. The Nano responds with a standard ack followed by a single byte which is a byte count (0 - 255). If the byte count is 0, the command is complete. Otherwise the host must read bytecount bytes (the “body”) from the connection. Interpretation of the body is not defined by this specification.

Errors are returned only for cases like loss of synchronization or serious failures in the YARC or Nano software. See the note to the next command (Service Response) for the anticipated use of the two commands.

##### Service Response - 0xEA
1 arguments byte
<br>
0 to 255 data bytes
<br>
No result byte

The first argument byte is interpreted as an unsigned byte count between 0 and 255. The count byte is followed by the counted number of data bytes (the “body”). Body bytes have the same restrictions as specified for the POLL command (0xE9), above. Body content is not defined by this specification. Service responses with a byte count of 0 are expensive no-ops and should be avoided.

Note: The Poll and Service Response commands are expected to be used to implement both logging for the Nano and a primitive system call mechanism for the YARC. The first byte of the body may be used to encode the request, with one value set aside for log message requests from the Nano. These log message requests require no response other than the transport layer acknowledgement, i.e. they are “fire and forget” from the Nano’s perspective.

System service requests from the YARC, however, will require responses.  These responses must be asynchronous. Example: the YARC may issue a console readline request that is read from YARC memory by the Nano and later received by the host via POLL. The host then prompts the user for input. Many seconds later, the user finishes entering a line; in the meantime, multiple unrelated NTL interactions may have occurred over the serial link. The host then transmits a Service Response to the Nano containing the user’s line. Nano software must of course be able to associate the line with the original console readline request, but this association is unknown to NTL.

##### Debug - 0xEB
7 argument bytes
<br>
64 result bytes

The command and 7 bytes of arguments are passed to the Nano. The Nano performs an operation and returns 64 bytes (always). The operation is specified by the first argument byte. The operations and result values are not formally specified in the protocol. Command byte 1 stops the YARC and returns the 64 bytes at 0x7700 in main memory.

##### GetVersion - 0xEE
No argument bytes
<br>
1 result byte

The Nano returns its protocol version. The protocol version is a single byte containing a binary integer between 1 and 255. This specification does not dictate how the host determines if the firmware’s version is “compatible” or how the host should respond if not.

##### Sync - 0xEF
No argument bytes
<br>
No result bytes

The Nano acks the 0xEF (i.e. sends a 0x10 byte). The host should check for a 0x8x error response, but no such errors are currently defined for Sync. No other actions are taken, so no state is altered either in the Nano or the YARC. This "ping-like" command is used in connection setup.

##### Set Address Register High - 0xF0
1 argument byte
<br>
No result bytes

The command is followed by a single byte which the Nano stores in the upper byte of the address bus register (ARH, Address Register High).

**Set Address Register Low - 0xF1**
<br>
**Set Data Register High - 0xF2**
<br>
**Set Data Register Low - 0xF3**

1 argument byte
<br>
No result bytes

Similarly for the other three bus registers (ARL, DRH, DRL).

Note: the direction of the transfer (write YARC memory / read YARC memory) is determined by the high order bit of the address register.  The host constructs the content of this byte, and the Nano is not aware of it.  For write cycles, the Data Register Low (byte) is transferred into YARC memory. For reads, the Data Register Low is ignored and the data byte specified by the Address Registers is transferred from YARC memory Bus Input Register. The Bus Input Register is returned in all cases and should always reflect the value written or read.

##### Single Clock - 0xF4
No argument bytes.
<br>
1 result byte

This command causes the Nano to toggle the slow clock once, triggering a cycle on the YARC’s 16-bus. The four bus registers provide the high and low bytes. This command does not alter the content of the bus registers.

##### Get Bus Input Register - 0xF5
No argument bytes
<br>
One result byte

The Nano sends the content of the Bus Input Register to the host followed by the ack byte. The Bus Input Register is not altered.

##### Write Slice - 0xF6
3 argument bytes
<br>
0 to 64 data bytes
<br>
No result byte

The first argument byte specifies the high byte of an opcode (in 0x80..0xFF). The second specifies the slice (in 0..3). The third specifies the number of data bytes (in 0..64). The third argument byte is followed by the counted number of data bytes. The data is written to microcode memory for the given opcode and slice.

##### Read Slice - 0xF7
3 argument bytes
<br>
1 fixed result byte
<br>
0 to 64 data bytes

The first argument byte specifies the high byte of an opcode (in 0x80..0xFF). The second argument specifies the slice (in 0..3).  The third argument specifies the count (in 0..64). The Nano first echoes the count and then reads that number of bytes from the slice and transmits them to the host.

##### Write K - 0xFB
4 argument bytes
<br>
No result bytes

All four K registers are updated with the four argument bytes. The most significant (K[3]) is transmitted first and least last.  K[3] contains bits 31:24 of the microcode pipeline register while K[0] contains 7:0.

The K (microcode pipeline) register is not readable, so no read capability is supported by the protocol.

##### SetMCR - 0xFC
1 argument byte
<br>
No result byte

The argument byte is immediately transferred to the MCR. This dangerous command represents a change in philosophy. Use with care.

##### WriteALU - 0xFD
3 argument bytes
<br>
0 .. 128 data bytes (update: must be 64)
<br>
No result byte

The first two argument bytes specify the write address in 0..1FFF, high byte first. The third byte is an unsigned byte count between 0 and 128.  The arguments are followed by count number of bytes.  The bytes are written to ALU memory at the given address. The behavior of a write outside the 8k ALU RAM address space is undefined.  Note: the hardware supports only parallel writes to the three ALU RAMs.

##### ReadALU - 0xFE
4 argument bytes
<br>
1 fixed response byte
<br>
0 to 128 result bytes (update: must be 64)

The first two argument bytes specify the read address in 0..1FFF, high byte first. The third byte is RAM identifier in 0..2. The fourth byte is a count of bytes to read. The fixed response byte specifies the length of the response; normally it echoes the last fixed argument byte. The bytes are read from the specified ALU RAM and returned to the host.  The behavior of a read outside the 8k ALU RAM address space or from a RAM ID not in 0..2 is undefined.
