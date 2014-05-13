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
#include <unistd.h>
#include <poll.h>
#include "RF24.h"

using namespace std;

static enum { EMONCMS, DOMAH } OUTPUT_MODE;

// Radio pipe addresses for the 2 nodes to communicate.
// First pipe is for writing, 2nd, 3rd, 4th, 5th & 6th is for reading...
const uint64_t pipes[6] = { 0xF0F0F0F0F1LL, 0xF0F0F0F0F0LL, 0xF0F0F0F0F2LL, 0xF0F0F0F0A2LL, 0xF0F0F0F0A3, 0xF0F0F0F0A4 };

const uint64_t pipe_gaz 	 = 0xF0F0F0F0F0LL;
const uint64_t pipe_ledstrip = 0xF0F0F0F0F1LL;
const uint64_t pipe_ledlamp  = 0xF0F0F0F0F2LL;
#define PIPE_GAZ_ID 1
#define PIPE_LEDSTRIP_ID 2
#define PIPE_LEDLAMP_ID 3

// CE and CSN pins On header using GPIO numbering (not pin numbers)
//RF24 radio("/dev/spidev0.0",8000000,18);  // Setup for GPIO 25 CSN
//RF24 radio("/dev/spidev0.0",8000000,18);  // Setup for GPIO 25 CSN
RF24 radio(8, 18, BCM2835_SPI_SPEED_4MHZ);

void setup(void)
{
	//
	// Refer to RF24.h or nRF24L01 DS for settings
	radio.begin();
//	radio.enableDynamicPayloads();
	radio.setPayloadSize(4);
	radio.setAutoAck(true);
	radio.setRetries(15,15);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(95);
//	radio.setCRCLength(RF24_CRC_16);

	radio.openReadingPipe(PIPE_GAZ_ID, pipe_gaz);
	radio.openReadingPipe(PIPE_LEDSTRIP_ID, pipe_ledstrip);
	radio.openReadingPipe(PIPE_LEDLAMP_ID, pipe_ledlamp);

	// Start Listening
	radio.startListening();

	radio.printDetails();
	usleep(1000);
}

void send_rf24_cmd(uint64_t addr, uint8_t param)
{
	uint8_t payload[4];
	payload[0] = param;
	payload[1] = param;
	payload[2] = param;
	payload[3] = param;

	radio.stopListening();
	usleep(10000);
	radio.openWritingPipe(addr);
	usleep(10000);
	if (radio.write(&payload[0], 4)) {
		fprintf(stderr, "Send successful\n");
	} else {
		fprintf(stderr, "Could not send RF24 cmd\n");
		radio.printDetails();
		setup();
		radio.powerDown();
		usleep(1000);
		radio.powerUp();
	}
	usleep(10000);
	radio.startListening();
}

void led_strip_command(char *cmdbuf)
{
	char *p = cmdbuf + strlen("LEDSTRIP ");
	struct { 
		const char *cmd;
		int param;
	} led_strip_commands[] = {
			{ "strobe", 3 },
			{ "sunrise", 1 },
			{ "sunset" , 2 },
			{ "stop" , 0 },
	};

	unsigned int i;
	for (i = 0; i < sizeof(led_strip_commands)/sizeof(led_strip_commands[0]); i++) {
		if (!strncmp(led_strip_commands[i].cmd, p, strlen(led_strip_commands[i].cmd))) {
			send_rf24_cmd(pipes[0], led_strip_commands[i].param);
			return;
		}
	}
}

void led_lamp_command(char *cmdbuf)
{
	char *p = cmdbuf + strlen("LEDLAMP ");
	int val = atoi(p);
	send_rf24_cmd(pipes[2], val);
}

void loop(void)
{
	char data[33];
	uint8_t pipe = 1;
	struct pollfd input = { 0, POLLIN, 0 };
	char cmdbuf[150];

	 while (radio.available(&pipe)) {

		radio.read(data, 4);

		switch (pipe) {
			case PIPE_GAZ_ID:
				// Display it on screen
				switch (OUTPUT_MODE) {
					case EMONCMS:
						printf("appart.GAZ_PULSE:%d\n",*((uint16_t *)data));
						break;
					case DOMAH:
						printf("gas/pulse/%d\n",*((uint16_t *)data)); 
						break;
				}
				break;
			case PIPE_LEDSTRIP_ID:
				printf("Ledstrip says %c %c %dc %c\n", data[0], data[1], data[2], data[3]);
				break;
			case PIPE_LEDLAMP_ID:
				printf("Ledstrip says %c %c %dc %c\n", data[0], data[1], data[2], data[3]);
				break;
			default:
				printf("Received message on unknown pipe id %d: %c %c %c %c\n", data[0], data[1], data[2], data[3]);
		}
	}

	if (poll(&input, 1, 1)) {
		// Data to read on stdin
		fgets(cmdbuf, sizeof(cmdbuf), stdin);

		if (!strncmp(cmdbuf, "LEDSTRIP ", strlen("LEDSTRIP "))) {
			led_strip_command(cmdbuf);
		} else if (!strncmp(cmdbuf, "LEDLAMP ", strlen("LEDLAMP "))) {
			led_lamp_command(cmdbuf);
		}
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

