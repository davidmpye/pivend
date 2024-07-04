/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
  *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/stdlib.h"
#include "hardware/uart.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "tusb.h"

#include "vend_driver.h"
#include "serial.h"

float temperature;


int main() {
    stdio_init_all();

    vend_driver_init();

    //Await USB host
    while (!tud_connected()) {

    }

    sleep_ms(3000);
    //Start with chiller off
    vend_driver_set_chiller_on(false);
    
    while (true) {
        char *line = serial_process_input();
        if (line == NULL) continue;
        
        command_struct s =  serial_parse_line(line);

        switch (s.cmd) {
            case CMD_ERR:
                printf("Error - valid commands: VEND <row>, STATUS <row>, HOME <row>, MAP_MACHINE, SET_TEMP <temp>, TEMP\n");
                break;
            case CMD_VEND:
                if (s.args[0]!=NULL) {
                    printf("%s\n", vend_driver_strerror(vend_item(s.args[0], false)));
                }
                else {
                    printf("Error, no item address specified\n");
                }
                break;
            case CMD_HOME_ROW:
                //Given that we can only drive motors forward, HOME just does a vend with the 'override not home' option set. Might vend an item, might not.
                if (s.args[0]!=NULL) {
                    printf("%s\n", vend_driver_strerror(vend_item(s.args[0], true)));
                }
                break;
            case CMD_MAP_MACHINE:
                vend_driver_map_machine();
                break;
            case CMD_STATUS:
                printf("status\n");
                break;
        }
    }
}
