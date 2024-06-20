#include "vend_driver.h"

//GPIO mappings
#define BUF_GPIO 8
#define FLIPFLOP_CLR 9

#define U2_CLK 10
#define U3_CLK 11
#define U4_CLK 12

#define VEND_GPIO_MASK 0x00001FFF

bool chiller_state = false;

void vend_driver_init() {
    //Init GPIOs 0-12 (will start as input by default)
    gpio_init_mask(VEND_GPIO_MASK);

    //Set the buffer pin individually to output and pull high to disable the buffer on the control board in the vendor
    //to avoid logic clash.
    gpio_set_dir(BUF_GPIO, true);
    gpio_put(BUF_GPIO, true);

    //Now set the rest to output
    gpio_set_dir_out_masked(VEND_GPIO_MASK);
    flipflop_clear();
}

//Decide whether the triac is supposed to be enabled or disabled.
void vend_driver_set_chiller_state(bool state) {
    chiller_state = state;
    //This will cause our triac state to pushed out via the flipflops straightaway
    stop_motors();
}

void switch_to_input() {
    //Set the lower 8 GPIOs to input first of all.
    gpio_set_dir_in_masked(0xFF);
    //Enable the buffer, wait 2uSec the buffer to present its' data
    gpio_put(BUF_GPIO, false);
}

void switch_to_output() {
    //Switch back to output
    gpio_set_dir(BUF_GPIO, true);
    //Chcek the buffer is disabled
    gpio_put(BUF_GPIO, true);

    gpio_set_dir_out_masked(0xFF);
}

void flipflop_clear() {
    gpio_set_dir(BUF_GPIO, true);
    //Chcek the buffer is disabled
    gpio_put(BUF_GPIO, true);
    sleep_us(2);
    //Pull the flipflop _CLR low for 50 microseconds then release to reset
    gpio_put(FLIPFLOP_CLR, false);
    sleep_us(50);
    gpio_put(FLIPFLOP_CLR, true);
}

vend_result vend_item(char *address, bool override) {

   uint8_t buf[3];
   calculate_flipflop_data(address, buf);

   printf("Vending %c%c ", address[0], address[1]);;
    //Send the flipflop data - motors then powered.
    flipflop_output(buf);

    switch_to_input();

    //bit 0x01 goes high on even row when homed
    //bit 0x02 goes high on ODD row when homed.
    uint32_t motor_pos_gpio;
    if (address[1]%2 != 0) motor_pos_gpio = 1;
    else motor_pos_gpio = 0;

    //For drinks rows, check the NO_CAN state.
    //Also, the motor_sense_gpio is different - doesn't use the comparator.
    if (address[0] == 'E' || address[0] == 'F') {
        uint32_t no_can_gpio;
        if (address[0] == 'E') {
            //E
            motor_pos_gpio = 4;
            no_can_gpio = 5;
        } 
        else {  
            //F
            motor_pos_gpio = 6;
            no_can_gpio = 7;
        }

        //Check if there is a >1 can present
        //NB The machine is designed so it WILL NOT vend the last can...!
        sleep_us(200);
        if (!gpio_get(no_can_gpio)) {
            //No can present
            if (override) {
                printf("No can - vending anyway");
            }
            else {
                printf("No can present - aborting vend\n");
                stop_motors();
                return VEND_FAIL_NO_CAN;
            }
        }
    }

    //Common vend pathway for all rows now.
    //Motor should now be starting to move - it has up to a second to have left home.
    bool timeout = true;
    printf("Waiting for motor to leave home");
	for (int i=0; i<20; ++i) {
 	    if (!gpio_get(motor_pos_gpio)) {
		    timeout = false;
		    break;
	    }
        sleep_ms(50);
	}

	if (timeout) {
		printf("Timed out waiting for row to leave home\r\n");
        stop_motors();
		return VEND_FAIL_MOTOR_STUCK_HOME;
	}
	
	printf("Moving, waiting for max 4 seconds to return home");
	timeout = false;
	for (int i=0; i<80; ++i) {
		if (gpio_get(motor_pos_gpio)) {
			timeout = false;
			break;
		}
		sleep_ms(50);
	}
	if (timeout) {
		printf("\r\nTimed out waiting for motor to return home - jammed?\r\n");
		stop_motors();
		return VEND_FAIL_MOTOR_STUCK_NOT_HOME;
	}
	printf("Homed - Vend success\r\n"); 
	stop_motors();
    sleep_ms(250);
    //We wait 500mS before returning to allow all microswitches etc to settle.
    return VEND_SUCCESS;
 }

void stop_motors() {
    uint8_t stop_data[] = { 0x00, 0x00, 0x00 };
    flipflop_output(stop_data);
}

void flipflop_output(uint8_t *data) {
    switch_to_output();
    //Automatically add in the desired triac state to U2's data (it's bit 4)
    gpio_put_masked(0x000000FF, chiller_state ? data[0] | 0x10 : data[0]);
    gpio_put(U2_CLK, true);
    sleep_us(2);
    gpio_put(U2_CLK, false);
    sleep_us(2); 
    gpio_put_masked(0x000000FF, data[1]);
    gpio_put(U3_CLK, true);
    sleep_us(2);
    gpio_put(U3_CLK, false);
    sleep_us(2); 
    gpio_put_masked(0x000000FF, data[2]);
    gpio_put(U4_CLK, true);
    sleep_us(2);
    gpio_put(U4_CLK, false);
    sleep_us(2);    
}


void vend_driver_map_machine() {
    //Go through each of the motor row and column combinations, and pulse the motor driver so briefly nothing moves
    //Read the comparator sense lines to work out if the motor is present, and if it is homed.
    //That way, we can map out what trays/motors are installed and available in our machine.
    switch_to_output();

    for (int i=0; i<12; ++i) {
        //12 row drive options - A-F *2 (odd/even)
        if (i<8) {
            //U4
            gpio_put_masked(0xFF, 0x01 << i);
            //Clock pulse U4
            gpio_put(U4_CLK, true);
            sleep_us(2);
            gpio_put(U4_CLK, false);

            gpio_put_masked(0xFF, 0x00);
            //Clock pulse U2
            gpio_put(U2_CLK, true);
            sleep_us(2);
            gpio_put(U2_CLK, false);
        }
        else {
            //Rows E-F are on U2
            gpio_put_masked(0xFF, 0x01 <<i-8);

            //Clock pulse U2
            gpio_put(U2_CLK, true);
            sleep_us(2);
            gpio_put(U2_CLK, false);

            gpio_put_masked(0xFF, 0x00);  
            gpio_put(U4_CLK, true);
            sleep_us(2);
            gpio_put(U4_CLK, false);
        }

        for (int j=0; j<5; ++j) {
            //There are five column drives
            //Pulse U3.

            gpio_put_masked(0xFF, 0x01<<j);
            gpio_put(U3_CLK, true);
            sleep_us(2);
            gpio_put(U3_CLK, false);

            switch_to_input();  
            //need about 200uSec to get a sensible reading from the buffer
            sleep_us(200);
            uint8_t result = gpio_get_all() & 0xFF;
            switch_to_output();

            //Unpulse U3 so nothing goes round!
            gpio_put_masked(0xFF, 0x00);
            gpio_put(U3_CLK, true);
            sleep_us(2);
            gpio_put(U3_CLK, false);

            //Now interpret the result.
            //bit 0x01 goes high on even row when homed
            //bit 0x02 goes high on ODD row when homed.
            bool present, homed, no_can;
            if (i < 8) {
                if (i%2 != 0) {
                    //Odd row
                    present = result & 0x01;
                    homed = result & 0x02;
                }
                else {
                    present = result & 0x02;
                    homed = result & 0x01;
                }
            }
            else if (i == 8 || i ==9 ) {
                present = true;
                homed = result & (0x01 << 4);
                no_can = result & (0x01 << 5);

            }
            else if (i == 10|| i == 11) {
                present = true;
                homed = result & (0x01 << 6);
                no_can =  result & (0x01 << 7);

            }
            //Work out what address we've checked
            char row = i/2 + 'A';
            char col = j*2 + i%2 + '0';
          
          if (present) { 
            printf("%c%c - PRESENT - ", row, col);
            if (!homed) {
                printf("NOT homed ");
            }
            else printf("Homed ");

            if (row == 'E' || row == 'F') { 
                printf("0x%2x - ", result&0xFF);
                if (!no_can ) printf("NO CAN");
                else printf("CAN");
                printf("\n");
            }
            else printf("\n");
          }
        }
    }
}


void calculate_flipflop_data(char *address_to_vend, uint8_t *buf) {
    buf[0] = buf[1] = buf[2] = 0x00;
    //Calculate column to drive
    //The column drivers are as follows (U3):
    //0x01 - Cols 0,1
    //0x02 - Cols 2,3
    //0x04 - Cols 4,5
    //0x08 - Cols 6,7
    //0x10 - Cols 8,9

    buf[1] |= 0x01 << ((address_to_vend[1]-'0') / 2);

    //Configure row drive.
    //Rows are:
    //U4:
    //0x01 - Row A Even
    //0x02 - Row A Odd
    //0x04 - Row B Even
    //0x08 - Row B Odd
    //0x10 - Row C Even
    //0x20 - Row C Odd
    //0x40 - Row D Even
    //0x80 - Row D Odd
    
    //U2:
    //0x01 - Row E Even
    //0x02 - Row E Odd
    //0x04 - Row F Even
    //0x08 - Row F Odd

    //U3:
    //0x20 - Gum and mint row drive (no odd/even) 

    uint8_t bit_offset = 0;

    //if it's an odd row, add one to the bit offset to push it to the _ODD_ROW_DRV
    if (address_to_vend[1]%2 != 0) bit_offset += 1;

    if (address_to_vend[0] <='D') {
        //A-D are on  U4
        bit_offset += (address_to_vend[0] - 'A') * 2;
        buf[2] |= (0x01 << bit_offset);
    }
    else {
        //E and F are on U2
        bit_offset += (address_to_vend[0] - 'E') * 2;
        buf[0] |= (0x01 << bit_offset);
    }
}




