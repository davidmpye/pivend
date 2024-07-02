#include "serial.h"
#include <string.h>
#include <stdio.h>




//FIXED BUFFER
#define CMD_BUFLEN 32
char buf[CMD_BUFLEN];

unsigned char *serial_readline() {

    unsigned int chars_read = 0;
    while (chars_read < CMD_BUFLEN) {
        
        buf[chars_read++] = getchar();
        if (buf[chars_read-1] == '\r') {
            buf[chars_read-1] = 0x00; //Null terminate it.
            return buf;
        }
    }
    //You get an empty string.
    buf[0] = 0x00;
    return buf;
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