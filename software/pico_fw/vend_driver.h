#ifndef __VEND__DRIVER__

#define __VEND__DRIVER__

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define MAX_ERR_STR_LENGTH 40

//Result codes for an attempt to vend
typedef enum {
    VEND_SUCCESS,                   //Successful vend
    VEND_FAIL_NOT_HOME,             //The desired row was NOT home at the start of the vend cycle - ?jammed
    VEND_FAIL_NODROP,               //Not implemented yet 
    VEND_FAIL_INVALID_ADDRESS,      //No such address in machine
    VEND_FAIL_MOTOR_STUCK_HOME,     //Motor stuck in home pos
    VEND_FAIL_MOTOR_STUCK_NOT_HOME, //Motor stuck, not in home pos
    VEND_FAIL_NO_CAN,               //You tried to vend a can row, but it has only 1 or no cans, and it won't vend the last one!
    VEND_FAIL_UNKNOWN,              // ¯\_(ツ)_/¯
} vend_result;

char *vend_driver_strerror(vend_result);

//Initialise the vend driver
void vend_driver_init();

//*will* return a machine map, listing which rows/columns are present, and what the state of those motors are.
void vend_driver_map_machine();

//Attempt to vend the item specified at address row (e.g. "A2")
//Note: If you attempt to vend a previously stuck row, it will attempt to vend each time.  It's up to you to record
//if a motor was jammed, and decide whether to "Select another item" or try to vend.
//It *wont* try to vend a can if it thinks there aren't any cans, and will instead return VEND_FAIL_NO_CAN.
//If you want it to try to vend an empty can-row, eg as a test vend, then set override to true.
vend_result vend_item(char *, bool);

//Set whether you want the triac to be on or off to activate the compressor
void set_chiller_state(bool);



#endif