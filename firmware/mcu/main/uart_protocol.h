#pragma once

/**
 * Parse one UART axis command (trailing \r\n already stripped).
 *
 * Expected format:  "X:90"  or  "Y:45"
 *   - axis  : 'X' or 'Y'
 *   - angle : 0-180 (inclusive)
 *
 * @param line      Null-terminated input string.
 * @param out_axis  Receives 'X' or 'Y' on success.
 * @param out_angle Receives angle value on success.
 * @return  0 on success, -1 on any parse or range error.
 */
int parse_axis_cmd(const char *line, char *out_axis, int *out_angle);
