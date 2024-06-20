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

int main() {
    stdio_init_all();

    vend_driver_init();

    //Await USB host
    while (!tud_connected()) {

    }
    //Start with chiller off
    vend_driver_set_chiller_state(false);
    vend_driver_map_machine();

}
