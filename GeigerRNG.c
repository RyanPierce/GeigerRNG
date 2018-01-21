/*
	Title: Geiger Counter Random Number Generator
	Description: This is optional firmware for the mightyohm.com Geiger Counter.
		It is an attempt to produce a simple, self-contained hardware based random
		number generator for cryptographic use.
		
	Author:		Ryan Pierce
	Website:	http://www.mackenziegems.com/
	Twitter:	@RyanPierce_Chi
	Contact:	rdpierce <at> pobox <dot> com
  
	This firmware controls the ATtiny2313 AVR microcontroller on board the Geiger Counter kit.
	It is very loosely based on the default firmware written by Jeff Keyzer, MightyOhm Engineering.
	I used some of the structure and utility routines, although the code serves an entirely
	different purpose. The author's original comments are included below for completeness.
	
	Background: I intended to participate in the Zcash Powers of Tau ceremony with Andrew Miller,
	UIUC. The SNARK computation requires input of bytes of random entropy. Pulse timing from a
	Geiger counter and a radioactive source provides an excellent source of entropy. I had been
	aware of John Walker's HotBits project (https://www.fourmilab.ch/hotbits/) from over two decades
	ago, however it used a PC to capture timing from a Geiger tube. With the widespread availability of 
	microcontrollers and open source toolchains, I intended to construct a small, self-contained
	device incorporating all the Geiger counter circuitry and a microcontroller to perform all timing
	and random number generation. This has an added security benefit: the code is small, easy to audit,
	and, as it runs on a microcontroller, has no operating system that may be subject to vulnerabilities.
	
	Andrew and I used this code successfully on January 20, 2018 to perform Powers of Tau contribution #41.
	
	For the hardware platform, I selected the MightyOhm Geiger Counter kit (available at
	http://mightyohm.com/geiger ). I purchased the kit without a tube, as well as the laser cut case. The
	kit is designed for a Russian SBM-20 Geiger tube, which is readily available, or some smaller variants.
	I constructed the kit without modifications. However, because I wished to use a Russian SI-22G tube that
	I already owned, which is much larger than a SBM-20, I had to locate the tube externally. I could insert
	the anode end into the clip, with the body of the tube extending outside the case to the left. I then used 
	an alligator clip jumper cable to connect the cathode to the negative clip inside the case. See the picture 
	at https://twitter.com/RyanPierce_Chi/status/954044819856347137 for an illustration. This is liable to
	fall apart if bumped, so we had to handle this carefully. For random number generation purposes, the SBM-20 
	should be perfectly fine, and it has the added benefit that it can be located securely inside the case.
	
	The random number generation works by timing Geiger pulses from a decaying radioactive source (in addition
	to the natural and manmade background radiation surrounding us that also generates pulses in the Geiger tube) 
	in microseconds. Radioactive decay is governed by quantum mechanics, and the timing of any individual event
	should be entirely unpredictable. Additionally, Geiger tubes only detect some of the gammas or betas that pass
	through them, depending on the detector efficiency, so a pulse isn't guaranteed for every decay. Geiger tubes 
	also become insensitive to radiation for a brief period of time after a pulse occurs; this is known as dead
	time. These factors also add uncertainty to the measurement process.
	
	It must be noted that the timing of intervals between two consecutive pulses is not random. The Geiger tube
	will, for a given configuration, produce a certain number of counts per second on average. One would expect
	some type of probability distribution of these intervals, which could lead to biases if they were used directly
	as random numbers. Rather, the method used by HotBits is to compare the relative times of two consecutive pairs
	of pulses. If T4 - T3 > T2 - T1, we generate a 1. Otherwise, we generate a 0.
	
	Because the source is decaying, one might expect the time between consecutive pulses to increase. This could
	lead to a bias, with more 1's than 0's. This code uses the same method HotBits uses, which is to flip every 
	other bit. This technique does not increase the entropy, but by flipping half of the bits, it should remove a 
	bias of 1's or 0's if the measured data contains such a bias. It should be noted that with a source that has a 
	30 year half life, any such bias should be negligible, but I'm correcting for it anyway.
	
	The ATtiny2313 used in this kit is quite limited: 2k program storage and 128 bytes RAM. I originally intended
	to write code from scratch using the Arduino environment, which does support this chip. There isn't enough
	space for a bootloader, so one must program the chip using the 3x2 programming header. I used a SparkFun
	programmer (available at https://www.sparkfun.com/products/9825 ).
	
	Unfortunately, as soon as I added the Arduino Serial library, I realized that I was rapidly eating into
	the 128 bytes of RAM. Instead, I decided to abandon the Arduino environment and modify the original MightyOhm
	firmware, written in C, and use WinAVR (including the AVR-GCC compiler and avrdude to program the microcontroller 
	and set the fuses) as my development environment.
	
	The code uses a hardware interrupt, triggered by the falling edge of the Geiger pulse signal, to record
	the time in microseconds. 4 events = 1 bit. After 8 bits (32 events) are collected, the ISR ceases to collect
	timing data, and the main code loop proceeds to output the byte, as two hex characters, via the TTL serial
	interface. It then flashes the LED and, if BEEP is defined, sounds the kit's piezo buzzer, for 10 milliseconds.
	During this time, the code ignores any new Geiger events.
	
	The kit contains a 6 pin FTDI header. I used this cable (https://www.adafruit.com/product/70) to connect it to a 
	USB port. This cable provides 5V power (unused by the Geiger kit) but only outputs 3V logic levels, which is 
	compatible with the kit.
	
	After RAND_CHARS (defined below as 64 bytes) of randomness are output (as 128 hex characters), the code outputs
	a CRLF.
	
	The code operates in two modes: if CONTINUOUS is defined, it will contine outputting random data indefinitely.
	Or if not defined (which is what we used for Powers of Tau), it remains idle until the pushbutton is pressed.
	Then, it outputs RAND_CHARS bytes of entropy, a CRLF, and remains idle until the button is pressed again. This
	simulates keyboard input of entropy.

	Andrew Miller used the Dieharder randomness test suite on approx. 500 kb of entropy I generated using this code,
	and according to him, it passed.
	
	Still, I see some areas for improvement:
	
	* Timing bug. The code measures microseconds by using a 16 bit timer. The kit uses an 8 MHz external clock,
	so the timer uses a /8 divisor. When it incremets to 1000, it rolls over and triggers a timer interrupt. The timer
	ISR increments a uint32_t counter of milliseconds. To measure time in microseconds, the code takes 
	milliseconds * 1000 + the timer, and stores this as a uint32_t.
	
	This method isn't always reliable. While debugging the code, I sometimes noticed intervals where T2 < T1 or T4 < T3.
	This only happened when both event times had the same number of milliseconds, and the second event had 0 or 1 microseconds. I believe what happened was that the Geiger pulse ISR executed and recorded the time after a
	millisecond rollover but before the timer ISR could increment the milliseconds counter. I corrected for this by 
	adding 1000 to the second event time when T2 < T1 or T4 < T3, which makes it greater than the first.
	
	However, in hindsight, I realize now that this can be happening for any of the four times recorded. I think there
	has to be a better way to measure microseconds, and I'm open to suggestions.
	
	* Microsecond timer overflow. A 32 bit counter will overflow every 72 minutes. I haven't verified whether the
	comparison code will function properly in this edge case. At most, it introduces a potential non-random bit once
	every 72 minutes, which seems acceptable. Still, I'd like to verify that in this edge case, times are compared
	correctly.

	* Possible comparison bias. Right now, (T4 - T3) > (T2 - T1) generates a 1. The < case generates a 0. But the =
	case generates a 0, too. I believe HotBits identified this as a source of bias in favor of 0's, and so it will
	throw out the data if the two intervals are equal and collect more data. I'd like to study this and see if I can
	observe a bias, and consider incorporating the HotBits behavior as a future enhancement. Even so, by flipping 
	every other bit, this should hide any small bias towards 0's.
	
	* Button bounce. When interfacing buttons with microcontrollers, there is usually some electrical bounce when one presses and releases the button. I'm using a hardware interrupt to detect the button press and start collecting
	data. The ISR will likely be called several times whenever the button is pressed or released. This shouldn't
	interfere with the function of the program; the ISR returns immediately if counting is in progress, and a button
	press should last less than the time needed to capture 64 bytes. However, I'm concerned that these unnecessary
	interrupts while counting may introduce timing errors. It should be easy to disable the hardware interrupt 
	after the initial press and re-enable it once the data is collected.
	
	Change log:
	
	1/21/18 1.00: Initial release. The code is identical to what we used for Powers of Tau #41 with the
	exception of these comments.

		Copyright 2018 Ryan Pierce
 
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
	Title: Geiger Counter with Serial Data Reporting
	Description: This is the firmware for the mightyohm.com Geiger Counter.
		There is more information at http://mightyohm.com/geiger 
		
	Author:		Jeff Keyzer
	Company:	MightyOhm Engineering
	Website:	http://mightyohm.com/
	Contact:	jeff <at> mightyohm.com
  
	This firmware controls the ATtiny2313 AVR microcontroller on board the Geiger Counter kit.
	
	When an impulse from the GM tube is detected, the firmware flashes the LED and produces a short
	beep on the piezo speaker.  It also outputs an active-high pulse (default 100us) on the PULSE pin.
	
	A pushbutton on the PCB can be used to mute the beep.
	
	A running average of the detected counts per second (CPS), counts per minute (CPM), and equivalent dose
	(uSv/hr) is output on the serial port once per second. The dose is based on information collected from 
	the web, and may not be accurate.
	
	The serial port is configured for BAUD baud, 8-N-1 (default 9600).
	
	The data is reported in comma separated value (CSV) format:
	CPS, #####, CPM, #####, uSv/hr, ###.##, SLOW|FAST|INST
	
	There are three modes.  Normally, the sample period is LONG_PERIOD (default 60 seconds). This is SLOW averaging mode.
	If the last five measured counts exceed a preset threshold, the sample period switches to SHORT_PERIOD seconds (default 5 seconds).
	This is FAST mode, and is more responsive but less accurate. Finally, if CPS > 255, we report CPS*60 and switch to
	INST mode, since we can't store data in the (8-bit) sample buffer.  This behavior could be customized to suit a particular
	logging application.
	
	The largest CPS value that can be displayed is 65535, but the largest value that can be stored in the sample buffer is 255.
	
	***** WARNING *****
	This Geiger Counter kit is for EDUCATIONAL PURPOSES ONLY.  Don't even think about using it to monitor radiation in
	life-threatening situations, or in any environment where you may expose yourself to dangerous levels of radiation.
	Don't rely on the collected data to be an accurate measure of radiation exposure! Be safe!
	
	
	Change log:
	8/4/11 1.00: Initial release for Chaos Camp 2011!


		Copyright 2011 Jeff Keyzer, MightyOhm Engineering
 
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Includes
#include <avr/io.h>			// this contains the AVR IO port definitions
#include <avr/interrupt.h>	// interrupt service routines
#include <avr/pgmspace.h>	// tools used to store variables in program memory
#include <avr/sleep.h>		// sleep mode utilities
#include <util/delay.h>		// some convenient delay functions
#include <stdlib.h>			// some handy functions like utoa()

// Defines
#define	F_CPU			8000000	// AVR clock speed in Hz
#define	BAUD			9600	// Serial BAUD rate
#define SER_BUFF_LEN	11		// Serial buffer length
#define RAND_CHARS		64		// Define here the number of bytes to generate per request

// If defined, this will enable the piezo buzzer and make beeps.
// Beeps occur only after a full byte is collected. (This equals 32 Geiger counts.)
#define BEEP

// If defined, counting is continuous. We collect RAND_CHARS bytes, output a Carriage Return,
// and then we continue.
#undef CONTINUOUS

// Function prototypes
void uart_putchar(char c);			// send a character to the serial port
void uart_putstring(char *buffer);		// send a null-terminated string in SRAM to the serial port
void uart_putstring_P(char *buffer);	// send a null-terminated string in PROGMEM to the serial port

void sendreport(void);	// log data over the serial port

// Global variables
volatile uint32_t t1;
volatile uint32_t t2;
volatile uint32_t t3;
volatile uint8_t rand_byte;
#define MODE_OFF 0
#define MODE_COUNTING 1
#define MODE_DONE 2
volatile uint8_t mode;
volatile uint32_t milliseconds;
volatile uint8_t rand_mask;
char serbuf[SER_BUFF_LEN];	// serial buffer
uint16_t byte_count;

// Interrupt service routines

//	Pin change interrupt for pin INT0
//	This interrupt is called on the falling edge of a GM pulse.
ISR(INT0_vect)
{
	uint16_t micros;
	uint32_t event;
	// First, capture the timer ASAP
	micros = TCNT1;
	// Now, exit if we're not in counting mode
	if (mode != MODE_COUNTING)
		return;
	event = milliseconds * 1000 + micros;
	if (t1 == 0L) {
		t1 = event;
	} else if (t2 == 0L) {
		t2 = event;
		// Rare edge case: If TCNT1 is small, then the timer
		// interrupt may not have executed yet! And T2 < T1
		// which should never happen.
		if (t2 < t1) {
			t2 += 1000L;
		}
	} else if (t3 == 0L) {
		t3 = event;
	} else {
		// We've got all the data we need!
		// Check for the same edge case
		if (event < t3) {
			event += 1000L;
		}
		// Make the determination of the bit
		if ((event - t3) > (t2 - t1))
			rand_byte ^= rand_mask;
		// Reset the times
		t1 = 0L;
		t2 = 0L;
		t3 = 0L;
		// Update the mask
		if (rand_mask != 0x80) {
			// We don't have 8 bits yet, rotate the mask
			rand_mask <<= 1;
		} else {
			// We have 8 bits, we're ready to report this via serial
			
			// Flipping the comparison criteria with every other bit is done so that if there
			// is any bias towards pulse timing increasing over time (say, due to half life of
			// the source as it decays), then it won't result in a bias in the number of 1s or 0s.
			// This doesn't add any antropy, just helps to correct the balance of 1s nd 0s. 
			// In practice, this should be utterly inconsequential of sources with multi-year half
			// lives, but it's onw line of code to correct this.
			rand_byte ^= 0xaa;
			
			// Reset the mask and let the main program take this.
			rand_mask = 0x01;
			mode = MODE_DONE;
		}
	}
}

//	Pin change interrupt for pin INT1 (pushbutton)
//	If the user pushes the button, this interrupt is executed.
//	We need to be careful about switch bounce, which will make the interrupt
//	execute multiple times if we're not careful.
//
//  In this case, if we aren't in continuous mode, the push button will
//  trigger the counting mode. De-bouncing the button is not an issue because
//  it is highly unlikely that the operation will complete while the button
//  is bouncing due to a press.
ISR(INT1_vect)
{
#ifndef CONTINUOUS
	if (mode != MODE_OFF)
		return;
	mode = MODE_COUNTING; // Move into counting mode
#endif	
}

/*	Timer1 compare interrupt 
 *	This interrupt is called every time TCNT1 reaches OCR1A and is reset back to 0 (CTC mode).
 *  Timer1 is setup so this happens once a millisecond.
 */
ISR(TIMER1_COMPA_vect)
{
	++milliseconds;
}

// Functions

// Send a character to the UART
void uart_putchar(char c)
{
	if (c == '\n') uart_putchar('\r');	// Windows-style CRLF
  
	loop_until_bit_is_set(UCSRA, UDRE);	// wait until UART is ready to accept a new character
	UDR = c;							// send 1 character
}

// Send a string in SRAM to the UART
void uart_putstring(char *buffer)	
{	
	// start sending characters over the serial port until we reach the end of the string
	while (*buffer != '\0') {	// are we at the end of the string yet?
		uart_putchar(*buffer);	// send the contents
		buffer++;				// advance to next char in buffer
	}
}

// Send a string in PROGMEM to the UART
void uart_putstring_P(char *buffer)	
{	
	// start sending characters over the serial port until we reach the end of the string
	while (pgm_read_byte(buffer) != '\0')	// are we done yet?
		uart_putchar(pgm_read_byte(buffer++));	// read byte from PROGMEM and send it
}

// log data over the serial port
void sendreport(void)
{
	ultoa(rand_byte, serbuf, 16);		// radix 16	
	// Add a leading 0 if this is a single hex digit
	if (serbuf[1] == 0) {
		serbuf[2] = 0;
		serbuf[1] = serbuf[0];
		serbuf[0] = '0';
	}
	uart_putstring(serbuf);
}

// Flashes the LED and makes a beep
//
// Note that while we're in this routine, the ISR is ignoring counts.
// Throwing out data is fine as long as we're not in a counting cycle.
// We don't want to have delays when the four events are collected to 
// determine a random bit.

void beep(void) {
	PORTB |= _BV(PB4);	// turn on the LED

#ifdef BEEP	
	TCCR0A |= _BV(COM0A0);	// enable OCR0A output on pin PB2
	TCCR0B |= _BV(CS01);	// set prescaler to clk/8 (1Mhz) or 1us/count
	OCR0A = 160;	// 160 = toggle OCR0A every 160ms, period = 320us, freq= 3.125kHz
#endif
		
	// 10ms delay gives a nice short flash and 'click' on the piezo
	_delay_ms(10);	
			
	PORTB &= ~(_BV(PB4));	// turn off the LED
#ifdef BEEP	
		TCCR0B = 0;				// disable Timer0 since we're no longer using it
	TCCR0A &= ~(_BV(COM0A0));	// disconnect OCR0A from Timer0, this avoids occasional HVPS whine after beep
#endif
}

// Start of main program
int main(void)
{	
	// Configure the UART	
	// Set baud rate generator based on F_CPU
	UBRRH = (unsigned char)(F_CPU/(16UL*BAUD)-1)>>8;
	UBRRL = (unsigned char)(F_CPU/(16UL*BAUD)-1);
	
	// Enable USART transmitter and receiver
	UCSRB = (1<<RXEN) | (1<<TXEN);
	
	// Set up AVR IO ports
	DDRB = _BV(PB4) | _BV(PB2);  // set pins connected to LED and piezo as outputs
	DDRD = _BV(PD6);	// configure PULSE output
	PORTD |= _BV(PD3);	// enable internal pull up resistor on pin connected to button
	
	// Initialize state
	t1 = 0L;
	t2 = 0L;
	t3 = 0L;
	rand_byte = 0;
	rand_mask = 0x01;
	byte_count = 0;
	mode = MODE_OFF;
	
	// Set up external interrupts	
	// INT0 is triggered by a GM impulse
	// INT1 is triggered by pushing the button
	MCUCR |= _BV(ISC01);	// Config interrupts on falling edge of INT0
	GIMSK |= _BV(INT0);		// Enable external interrupts on pin INT0
#ifndef CONTINUOUS
	MCUCR |= _BV(ISC11);	// Config interrupts on falling edge of INT1
	GIMSK |= _BV(INT1);	// Enable external interrupts on pin INT1
#endif
	
	// Configure the Timers		
	// Set up Timer0 for tone generation
	// Toggle OC0A (pin PB2) on compare match and set timer to CTC mode
	TCCR0A = (0<<COM0A1) | (1<<COM0A0) | (0<<WGM02) |  (1<<WGM01) | (0<<WGM00);
	TCCR0B = 0;	// stop Timer0 (no sound)

	// Set up Timer1 for 1 millisecond interrupts
	TCCR1B = _BV(WGM12) | _BV(CS11);  // CTC mode, prescaler = 8 (1us ticks)
	OCR1A = 1000;	// 1 us * 1000 = 1 ms
	TIMSK = _BV(OCIE1A);  // Timer1 overflow interrupt enable
	
	sei();	// Enable interrupts
	
#ifdef CONTINUOUS
	// If we are counting continuously, then start now
	mode = MODE_COUNTING; // Tell the interrupt to start counting
#endif

	while(1) {	// loop forever
		
		// Configure AVR for sleep, this saves a couple mA when idle
		set_sleep_mode(SLEEP_MODE_IDLE);	// CPU will go to sleep but peripherals keep running
		sleep_enable();		// enable sleep
		sleep_cpu();		// put the core to sleep
		
		// Zzzzzzz...	CPU is sleeping!
		// Execution will resume here when the CPU wakes up.
		
		sleep_disable();	// disable sleep so we don't accidentally go to sleep
		
		while (mode == MODE_DONE) {
			sendreport();
			beep();
			t1 = 0L;
			t2 = 0L;
			t3 = 0L;
			rand_byte = 0;
			++byte_count;
			if (byte_count == RAND_CHARS) {
				uart_putchar('\n');	
				byte_count = 0;
#ifdef CONTINUOUS
				mode = MODE_COUNTING;
#else
				mode = MODE_OFF;
#endif
			} else {
				mode = MODE_COUNTING;
			}
		}
	
	}	
	return 0;	// never reached
}

