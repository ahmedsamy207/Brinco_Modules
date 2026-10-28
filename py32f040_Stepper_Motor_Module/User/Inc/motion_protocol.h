/** @file motion_protocol.h
 * Strict ASCII command dispatch. Caller supplies a complete NUL-terminated line
 * and at least160bytes reply storage; response is always one CRLF ASCII line.
 */
#ifndef MOTION_PROTOCOL_H
#define MOTION_PROTOCOL_H
void MotionProtocol_Execute(const char *line,char *reply);
#endif
