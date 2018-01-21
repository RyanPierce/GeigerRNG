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

	Dieharder tests
	====
	As a sanity check we ran a standard battery of statistical tests call [Dieharder](https://webhome.phy.duke.edu/~rgb/General/dieharder.php).
	
	We generated a 500 kb sample of output from the geiger counter, captured as putty.log. To convert to the format dieharder reads, we ran a conversion script puttylog2dieharder.py. Finally we ran the dieharder tests with
	```
	dieharder -a -k2 -Y 1 -f geigersamples.input > geigersampes.output
	```
	
	The output file is attached, and shows that all tests were passed.
	
	Areas for improvement
	=====
	
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
