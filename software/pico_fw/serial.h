#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdlib.h>
#include <tusb.h>
#include "pico/stdlib.h"
#include <stdio.h>

typedef enum serial_cmd {
    CMD_ERR,
    CMD_VEND,
    CMD_STATUS,
    CMD_MAP_MACHINE,
    CMD_HOME_ROW,
} serial_cmd;


typedef struct command_struct {
    serial_cmd cmd;
    char *args[10];
} command_struct;


unsigned char *serial_readline();
command_struct serial_parse_line(char*);
#endif
