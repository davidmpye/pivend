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
    sleep_us(2);
}

void switch_to_output() {
    //Switch back to output
    gpio_set_dir(BUF_GPIO, true);
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

vend_result vend_item(char *address) {

   uint8_t buf[3];
   calculate_flipflop_data(address, buf);

   printf("Flip flop data: ");
   for (int i=0; i<3; ++i) {
        printf("0x%02X", buf[i]);
   }
   printf("\n");
   printf("Vending");
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

        //Check if there is a can - if not, abort vend.
        //NB The machine is set up so it WILL NOT vend the last can...!
        if (!gpio_get(no_can_gpio)) {
            printf("NO CAN - abort\n");
            stop_motors();
            return false;
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
    return VEND_SUCCESS;
 }

void stop_motors() {
    uint8_t stop_data[] = { 0x00, 0x00, 0x00 };
    flipflop_output(stop_data);
}

void flipflop_output(uint8_t *data) {
    switch_to_output();
    //Automatically add in the desired triac state to U2's data (it's bit 4)
    gpio_put_masked(0x000000FF, chiller_state ? data[0] : data[0] | 0x10 );
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
    gpio_set_dir_out_masked(0xFF);

    for (int i=0; i<12; ++i) {
        //12 row drive options - A-F *2 (odd/even)
        if (i<=7) {
            //U4
            gpio_put_masked(0xFF, 0x01 << i);
            //Clock pulse U4
            gpio_put(U4_CLK, true);
            sleep_us(2);
            gpio_put(U4_CLK, false);
        }
        else {
            //U2
            gpio_put_masked(0xFF, 0x00 <<i-8);
            //Clock pulse U2
            gpio_put(U2_CLK, true);
            sleep_us(2);
            gpio_put(U2_CLK, false);
        }

        for (int j=0; j<5; ++j) {
            //There are five column drives
            //Pulse U3.
            gpio_put_masked(0xFF, 0x00<<j);
            gpio_put(U3_CLK, true);
            sleep_us(2);
            gpio_put(U3_CLK, false);

            switch_to_input();
            uint8_t result = gpio_get_all() & 0xFF;
            switch_to_output();

            //Unpulse U3.
            gpio_put_masked(0xFF, 0x00);
            gpio_put(U3_CLK, true);
            sleep_us(2);
            gpio_put(U3_CLK, false);

            //Now interpret the result.
            //bit 0x01 goes high on even row when homed
            //bit 0x02 goes high on ODD row when homed.
            bool present, homed;
            if (i < 9) {
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
            else {
                //FIXME CANS


            }
            //Work out what address we've checked
            char row = i/2 + 'A';
            char col = j*2 + i%2 + '0';
            printf("%c%c", row, col);
            printf(" - ");
            if (!present) {
                printf ("NOT present\n");
            }
            else {
                printf("Present, ");
                if (!homed) {
                    printf("NOT");
                }
                printf("Homed\n");
            }
        }
    }
}


void calculate_flipflop_data(char *address_to_vend, uint8_t *buf) {
    buf[0] = buf[1] = buf[2] = 0x00;
   /* if (cooling_on) {
        buf[0] = 0x10;
    }*/
    //Calculate column to drive
    //The column drivers are as follows (U3):
    //0x01 - Cols 0,1
    //0x02 - Cols 2,3
    //0x04 - Cols 4,5
    //0x08 - Cols 6,7
    //0x10 - Cols 8,9

    //set the drive state for the column
    if (address_to_vend[0] == 'E' || address_to_vend[0]== 'F') {
        //For some reason, the cans are labelled 'F0, F1, F2 F3' but drive as F0, F2, F4, F6.
        buf[1] |= 0x01 << ((address_to_vend[1]-'0'));
    }   
    else {
        //Standard drive.
        buf[1] |= 0x01 << ((address_to_vend[1]-'0') / 2);
    }
    
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
    if (address_to_vend[0] <='D') {
        //A-D are on  U4
        bit_offset = (address_to_vend[0] - 'A') * 2;
        //if it's an odd row, add one to the bit offset to push it to the _ODD_ROW_DRV
        if (address_to_vend[1]%2 != 0) bit_offset += 1;
        buf[2] |= (0x01 << bit_offset);
    }
    else {
        //E and F are on U2
        bit_offset = (address_to_vend[0] - 'E') * 2;
        //if it's an odd row, add one to the bit offset to push it to the _ODD_ROW_DRV
        if (address_to_vend[1]%2 != 0) bit_offset += 1;
        buf[0] |= (0x01 << bit_offset);
    }
}




