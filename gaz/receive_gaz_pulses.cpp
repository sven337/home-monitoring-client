/* 
 *
 *  Filename : rpi-hub.cpp
 *
 *  This program makes the RPi as a hub listening to all six pipes from the remote 
 *  sensor nodes ( usually Arduino  or RPi ) and will return the packet back to the 
 *  sensor on pipe0 so that the sender can calculate the round trip delays
 *  when the payload matches.
 *  
 *  Refer to RF24/examples/rpi_hub_arduino/ for the corresponding Arduino sketches 
 * to work with this code.
 *  
 *  CE is connected to GPIO25
 *  CSN is connected to GPIO8 
 *
 *  Refer to RPi docs for GPIO numbers
 *
 *  Author : Stanley Seow
 *  e-mail : stanleyseow@gmail.com
 *  date   : 4th Apr 2013
 *
 */

#include <cstdlib>
#include <iostream>
#include "RF24.h"

using namespace std;

static enum { EMONCMS, DOMAH } OUTPUT_MODE;

// Radio pipe addresses for the 2 nodes to communicate.
// First pipe is for writing, 2nd, 3rd, 4th, 5th & 6th is for reading...
// Pipe0 in bytes is "serv1" for mirf compatibility
const uint64_t pipes[6] = { 0xF0F0F0F0F1LL, 0xF0F0F0F0F0LL, 0xF0F0F0F0A1LL, 0xF0F0F0F0A2LL, 0xF0F0F0F0A3, 0xF0F0F0F0A4 };

// CE and CSN pins On header using GPIO numbering (not pin numbers)
RF24 radio("/dev/spidev0.0",8000000,18);  // Setup for GPIO 25 CSN


void setup(void)
{
	//
	// Refer to RF24.h or nRF24L01 DS for settings
	radio.begin();
//	radio.enableDynamicPayloads();
	radio.setPayloadSize(4);
	radio.setAutoAck(false);
	radio.setRetries(15,15);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(95);
	radio.setCRCLength(RF24_CRC_16);

//	radio.openWritingPipe(pipes[0]);
	radio.openReadingPipe(1,pipes[1]);

	//
	// Dump the configuration of the rf unit for debugging
	//

	// Start Listening
	radio.startListening();

	radio.printDetails();
	usleep(1000);
}

void loop(void)
{
	char receivePayload[33];
	uint8_t pipe = 1;
	

	 while ( radio.available( &pipe ) ) {

		radio.read( receivePayload, 4);

		// Display it on screen
		switch (OUTPUT_MODE) {
			case EMONCMS:
				printf("appart.GAZ_PULSE:%d\n",*((uint16_t *)receivePayload));
				break;
			case DOMAH:
				printf("gas/pulse/%d\n",*((uint16_t *)receivePayload)); 
				break;
		}
		radio.flush_tx();
//		radio.flush_rx();


	}

	usleep(1000);
}


int main(int argc, char** argv) 
{
    setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	if (argc == 2 && !strcmp(argv[1], "--emoncms"))
		OUTPUT_MODE = EMONCMS;
	else OUTPUT_MODE = DOMAH;
	setup();
	while(1)
		loop();
	
	return 0;
}

