#include "vend_driver.h"

//GPIO mappings for the vend driver board.
#define BUF_GPIO 8
#define FLIPFLOP_CLR 9

#define U2_CLK 10
#define U3_CLK 11
#define U4_CLK 12

#define VEND_GPIO_MASK 0x00001FFF

//The triac for the compressor for the fridge is bit 0x10 on U2 - it's defined in flipflop_output

//Prototypes
void switch_to_input(void);
void switch_to_output(void);
void flipflop_clear(void);
void stop_motors(void);
void calculate_flipflop_data(char *addr, uint8_t *buf);
void flipflop_output(uint8_t *buf);

//Internal states
char str_errors[][MAX_ERR_STR_LENGTH] = { 
    "Success",
    "Motor not home at start of cycle",
    "Item drop not detected",
    "Invalid address",
    "Motor jammed in home position",
    "Motor jammed in not-home position",
    "Less than two cans present",
    "Unknown failure"
};

bool chiller_on = false;

//Functions start here
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
void vend_driver_set_chiller_on(bool on) {
    chiller_on = on;
    //This will cause our triac state to pushed out via the flipflops straightaway
    stop_motors();
}

void switch_to_input() {
    //Set the lower 8 GPIOs to input first of all.
    gpio_set_dir_in_masked(0xFF);
    //Enable the buffer, wait 2uSec the buffer to present its' data
    gpio_put(BUF_GPIO, false);
    sleep_us(150);
}

void switch_to_output() {
    //Switch back to output
    gpio_set_dir(BUF_GPIO, true);
    //Chcek the buffer is disabled
    gpio_put(BUF_GPIO, true);
    sleep_us(150);
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
    //Sanity check the address
    if (strlen(address) != 2 || address[0] < 'A' || address[0] > 'G' || address[1] < '0' || address[1] > '9')
        return VEND_FAIL_INVALID_ADDRESS;
        
    //Calculate the flipflop data for the address.
    uint8_t buf[3];
    calculate_flipflop_data(address, buf);

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
        if (!gpio_get(no_can_gpio)) {
            //No can present
            if (!override) {
                stop_motors();
                return VEND_FAIL_NO_CAN;
            }
        }
    }

    if ((!gpio_get(motor_pos_gpio)) && !override) {
        //If this row is not home, and override is not set, then we should not vend it.
        stop_motors();
        return VEND_FAIL_NOT_HOME;
    }

    //Common vend pathway for all rows now.
    //Motor should now be starting to move - it has up to a second to have left home.
    bool timeout = true;
	for (int i=0; i<20; ++i) {
 	    if (!gpio_get(motor_pos_gpio)) {
		    timeout = false;
		    break;
	    }
        sleep_ms(50);
	}

	if (timeout) {
        stop_motors();
		return VEND_FAIL_MOTOR_STUCK_HOME;
	}
	
    //We wait an additional four seconds for the motor to have completed its' cycle and returned home.
	timeout = false;
	for (int i=0; i<80; ++i) {
		if (gpio_get(motor_pos_gpio)) {
			timeout = false;
			break;
		}
		sleep_ms(50);
	}
	if (timeout) {
		stop_motors();
		return VEND_FAIL_MOTOR_STUCK_NOT_HOME;
	}
	stop_motors();
    return VEND_SUCCESS;
}

void stop_motors() {
    uint8_t stop_data[] = { 0x00, 0x00, 0x00 };
    flipflop_output(stop_data);
}

void flipflop_output(uint8_t *data) {
    //Clock the data out to the flipflops in order
    switch_to_output();

    uint32_t clock_pins[] = {
        U2_CLK,
        U3_CLK, 
        U4_CLK,
    };

    if (chiller_on) {
        //Automatically add in the desired triac state to U2's data (it's bit 4)
        data[0] |= 0x10;
    }

    for (int i=0; i<3; ++i) {
        gpio_put_masked(0xFF, data[i]);
        //Toggle the clock pins
        gpio_put(clock_pins[i], true);
        sleep_us(2);
        gpio_put(clock_pins[i], false);
        sleep_us(2);
    }
}

void vend_driver_map_machine() {
    //Go through each of the motor row and column combinations, and pulse the motor driver so briefly nothing moves
    //Read the comparator sense lines to work out if the motor is present, and if it is homed.
    //That way, we can map out what trays/motors are installed and available in our machine.
    switch_to_output();

    for (int i=0; i<12; ++i) {
        uint8_t buf[3];

        if (i<8)
            buf[2] = 0x01 <<i;
        else 
            buf[0] = 0x01 << i-8;

        for (int j=0; j<5; ++j) {
            //There are five column drives to map (0-1, 2-3, 4-5, 6-7, 8-9)
	        buf[1] = 0x01 << j;

            //Send the flipflop data.
            flipflop_output(buf);

            switch_to_input();

            uint8_t result = gpio_get_all() & 0xFF;

            switch_to_output();
            //Clear U3 to take away the +V drive so the motors don't actually go round!
            buf[1] = 0x00;
            flipflop_output(buf);

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
    //0x20 - Row G (Even?) Gum and mint row drive
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

char *vend_driver_strerror(vend_result err) {
    return str_errors[err];
}