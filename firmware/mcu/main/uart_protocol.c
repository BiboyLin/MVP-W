#include "uart_protocol.h"
#include <stdio.h>

int parse_axis_cmd(const char *line, char *out_axis, int *out_angle)
{
    if (!line || !out_axis || !out_angle) return -1;

    char axis;
    int  angle;

    /* sscanf enforces the literal ':' separator */
    if (sscanf(line, "%c:%d", &axis, &angle) != 2) return -1;
    if (axis != 'X' && axis != 'Y')                return -1;
    if (angle < 0 || angle > 180)                  return -1;

    *out_axis  = axis;
    *out_angle = angle;
    return 0;
}
