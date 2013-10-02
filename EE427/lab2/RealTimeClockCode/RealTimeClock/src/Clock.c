#include "xgpio.h"          // Provides access to PB GPIO driver.
#include <stdio.h>          // xil_printf and so forth.
#include "platform.h"       // Enables caching and other system stuff.
#include "mb_interface.h"   // provides the microblaze interrupt enables, etc.
#include "xintc_l.h"        // Provides handy macros for the interrupt controller.

#define hour_btn 	0X8
#define min_btn  	0X1
#define	sec_btn	 	0X2
#define	up_btn	 	0X10
#define	down_btn	0X4
XGpio gpLED;  // This is a handle for the LED GPIO block.
XGpio gpPB;   // This is a handle for the push-button GPIO block.

int currentButtonState;
int second_timer;
int debounce_timer;
int hours;
int minutes;
int seconds;
int incremented;
int auto_increment_timer;

// This is invoked in response to a timer interrupt.
// It does 2 things: 1) debounce switches, and 2) advances the time.
void timer_interrupt_handler() {
	second_timer++;
	if(second_timer == 99){
		if(currentButtonState == 0){
			seconds++;
			update_time();
		}
		second_timer = 0;
	}

	if(auto_increment_timer > 0){
		auto_increment_timer--;
		if(auto_increment_timer == 0){
			if(currentButtonState){
				incremented = 0;
				read_buttons();
				auto_increment_timer = 49;
			}
		}
	}

	//Switch debouncing
	if(debounce_timer > 0){
		debounce_timer--;
		if(debounce_timer ==0){
			if(currentButtonState != 0){
				read_buttons();
			}

			if(!(currentButtonState & (down_btn + up_btn))){
				incremented = 0;
			}
		}
	}
}

//Checks buttons to see if user has pressed the up or down button along with a time-setting
//button.  Updates the corresponding hours, minutes, or seconds value and then uses change_time
//to update the clock.
void read_buttons(){
	if(incremented == 0){
		if(currentButtonState & up_btn){
			if(currentButtonState & hour_btn){
				hours++;
			}
			if(currentButtonState & min_btn){
				minutes++;
			}
			if(currentButtonState & sec_btn){
				seconds++;
			}

			change_time();
		}

		else if(currentButtonState & down_btn){
			if(currentButtonState & hour_btn){
				hours--;
			}
			if(currentButtonState & min_btn){
				minutes--;
			}
			if(currentButtonState & sec_btn){
				seconds--;
			}

				change_time();
		}
		incremented = 1;
	}
}

void print_time(){
	xil_printf("\b\b\b\b\b\b\b\b\b");
	xil_printf("%02d:%02d:%02d", hours, minutes,seconds);
}

//Called when the user changes the time manually.
void change_time(){
	if(seconds == 60) {
		seconds = 0;
	} else if (seconds < 0){
		seconds = 59;
	}

	if(minutes == 60){
		minutes = 0;
	} else if (minutes < 0){
		minutes = 59;
	}

	if(hours == 24){
		hours = 0;
	} else if (hours < 0){
		hours = 23;
	}

	print_time();
}

//Called when the clock needs to update the time during normal operation.
void update_time() {
	if(seconds == 60) {
		minutes++;
		seconds = 0;
	}

	if(minutes == 60){
		hours++;
		minutes = 0;
	}

	if(hours == 24){
		hours = 0;
	}
	print_time();
}

// This is invoked each time there is a change in the button state (result of a push or a bounce).
void pb_interrupt_handler() {
  // Clear the GPIO interrupt.
  XGpio_InterruptGlobalDisable(&gpPB);                // Turn off all PB interrupts for now.
  currentButtonState = XGpio_DiscreteRead(&gpPB, 1);  // Get the current state of the buttons.
  // You need to do something here.
  XGpio_InterruptClear(&gpPB, 0xFFFFFFFF);            // Ack the PB interrupt.

  debounce_timer = 3;
  auto_increment_timer = 99;

  XGpio_InterruptGlobalEnable(&gpPB);                 // Re-enable PB interrupts.
}

// Main interrupt handler, queries the interrupt controller to see what peripheral
// fired the interrupt and then dispatches the corresponding interrupt handler.
// This routine acks the interrupt at the controller level but the peripheral
// interrupt must be ack'd by the dispatched interrupt handler.
void interrupt_handler_dispatcher(void* ptr) {
	int intc_status = XIntc_GetIntrStatus(XPAR_INTC_0_BASEADDR);
	// Check the FIT interrupt first.
	if (intc_status & XPAR_FIT_TIMER_0_INTERRUPT_MASK){
		XIntc_AckIntr(XPAR_INTC_0_BASEADDR, XPAR_FIT_TIMER_0_INTERRUPT_MASK);
		timer_interrupt_handler();
	}
	// Check the push buttons.
	if (intc_status & XPAR_PUSH_BUTTONS_5BITS_IP2INTC_IRPT_MASK){
		XIntc_AckIntr(XPAR_INTC_0_BASEADDR, XPAR_PUSH_BUTTONS_5BITS_IP2INTC_IRPT_MASK);
		pb_interrupt_handler();
	}
}

int main (void) {
    init_platform();
    // Initialize the GPIO peripherals.
    int success;

    //Initialize variables
    second_timer = 0;
    debounce_timer = 0;
    hours = 0;
    minutes = 0;
    seconds = 0;
    incremented = 0;
    auto_increment_timer = 0;

    print("It's a clock\n\r");
    success = XGpio_Initialize(&gpPB, XPAR_PUSH_BUTTONS_5BITS_DEVICE_ID);
    // Set the push button peripheral to be inputs.
    XGpio_SetDataDirection(&gpPB, 1, 0x0000001F);
    // Enable the global GPIO interrupt for push buttons.
    XGpio_InterruptGlobalEnable(&gpPB);
    // Enable all interrupts in the push button peripheral.
    XGpio_InterruptEnable(&gpPB, 0xFFFFFFFF);

    microblaze_register_handler(interrupt_handler_dispatcher, NULL);
    XIntc_EnableIntr(XPAR_INTC_0_BASEADDR,
    		(XPAR_FIT_TIMER_0_INTERRUPT_MASK | XPAR_PUSH_BUTTONS_5BITS_IP2INTC_IRPT_MASK));
    XIntc_MasterEnable(XPAR_INTC_0_BASEADDR);
    microblaze_enable_interrupts();

    while(1);  // Program never ends.

    cleanup_platform();

    return 0;
}
