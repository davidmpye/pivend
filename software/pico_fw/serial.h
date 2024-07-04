#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdlib.h>
#include <tusb.h>
#include "pico/stdlib.h"
#include <stdio.h>


//This function handles the USB-Serial UART comms between the Pico board and the main controller board
typedef enum serial_cmd {
    CMD_ERR,
    CMD_VEND,
    CMD_STATUS,
    CMD_MAP_MACHINE,
    CMD_HOME_ROW,
    CMD_TEMP,
    CMD_SET_TEMP,
} serial_cmd;

//A parsed USB-serial command, broken up into space separated arguments by strtok, with a newline at the end.
typedef struct command_struct {
    serial_cmd cmd;
    char *args[10];
} command_struct;

unsigned char *serial_process_input();
command_struct serial_parse_line(char*);

#endif
