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
    sleep_ms(2000); 

    vend_driver_map_machine();

    sleep_ms(5000);
  vend_item("A0");
  vend_item("B3");
}
