#include "serial.h"
#include <string.h>
#include <stdio.h>




//FIXED BUFFER
#define CMD_BUFLEN 32

unsigned char *serial_process_input() {
    static char buf[CMD_BUFLEN];
    static unsigned int index = 0;

    char c = getchar_timeout_us(1000);
    if (c == PICO_ERROR_TIMEOUT) {
        //No chars ready
        return NULL;
    }

    buf[index++] = c;
    
    if (index==CMD_BUFLEN-1) {
        //We've hit the end of the buffer - this shouldn't happen.
        //Return NULL.
        index = 0;
        return NULL;
    }

    if (c == '\r') { 
        //Null terminate the string and return it.
        buf[index] = 0x00;
        index = 0;
        return buf;
    }

    return NULL; //no string (yet)
}   

command_struct serial_parse_line(char *line) {

    struct command_struct c;
    c.cmd = CMD_ERR;

    char *ptr = strtok(line, " ");

    //ptr should point to a cmd.
    if (ptr) {
        if (!strcmp(ptr, "VEND")) {
            c.cmd = CMD_VEND;
        }
        else if (!strcmp(ptr, "STATUS")) {
            c.cmd = CMD_STATUS;
        }
        else if (!strcmp(ptr, "MAP_MACHINE")) {
            c.cmd = CMD_MAP_MACHINE;
        }
        else if (!strcmp(ptr, "HOME")) {
            c.cmd = CMD_HOME_ROW;
        }
        //else command remains CMD_ERR.
    }

    for (int i=0; ptr != NULL && i<10; i++) {
        //Build an array of arguments
        ptr = strtok(NULL, " ");
        c.args[i] = ptr;
    }

    return c;
}