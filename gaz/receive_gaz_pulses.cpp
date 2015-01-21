#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include "RF24.h"

using namespace std;

const int PORT = 45888;
int sockfd; // UDP socket
struct sockaddr_in *clients[10];

const uint64_t pipe_gaz 	 = 0xF0F0F0F0F0LL;
const uint64_t pipe_ledstrip = 0xF0F0F0F0F1LL;
const uint64_t pipe_ledlamp  = 0xF0F0F0F0F2LL;
const uint64_t pipe_mailbox  = 0xF0F0F0F0F3LL;

#define PIPE_GAZ_ID 1
#define PIPE_LEDSTRIP_ID 2
#define PIPE_LEDLAMP_ID 3
#define PIPE_MAILBOX_ID 4

// CE and CSN pins On header using GPIO numbering (not pin numbers)
RF24 radio(BCM2835_SPI_CS_GPIO18, 8, BCM2835_SPI_SPEED_2MHZ);

void setup(void)
{
	//
	// Refer to RF24.h or nRF24L01 DS for settings
	radio.begin();
	radio.setPayloadSize(4);
	radio.setAutoAck(true);
	radio.setRetries(15,15);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(95);
	radio.setCRCLength(RF24_CRC_16);
	usleep(1000);

	radio.openReadingPipe(PIPE_GAZ_ID, pipe_gaz);
	radio.openReadingPipe(PIPE_LEDSTRIP_ID, pipe_ledstrip);
	radio.openReadingPipe(PIPE_LEDLAMP_ID, pipe_ledlamp);
	radio.openReadingPipe(PIPE_MAILBOX_ID, pipe_mailbox);
	usleep(1000);

	// Start Listening
	radio.startListening();

	radio.printDetails();
	usleep(1000);
}

int send_rf24_cmd(uint64_t addr, uint8_t param0, uint8_t param1, uint8_t param2, uint8_t param3)
{
	uint8_t payload[4];
	int ret = -1;
	payload[0] = param0;
	payload[1] = param1;
	payload[2] = param2;
	payload[3] = param3;

	printf("send rf24...");
	radio.stopListening();
	usleep(1000);
	radio.openWritingPipe(addr);
	radio.powerUp();
	usleep(1000);
	bool ok = radio.write(&payload[0], 4);

	if (ok) {
		printf("... successful\n");
		ret = 0;
	} else {
		printf("... could not send RF24 cmd\n");
	}
	radio.startListening();
	usleep(10000);
	return ret;
}

void led_strip_command(char *cmdbuf)
{
	char *p = cmdbuf + strlen("LEDSTRIP ");
	struct { 
		const char *cmd;
		uint8_t p0;
		uint8_t p1;
	} led_strip_commands[] = {
			{ "fullpower", 'S', 4 },
			{ "strobe", 'S', 3 },
			{ "sunrise", 'S', 1 },
			{ "sunset" , 'S', 2 },
			{ "stop" , 'S', 0 },
			{ "fast" , 'F', 0 },
			{ "query", 'L', 0 },
	};

	unsigned int i;
	for (i = 0; i < sizeof(led_strip_commands)/sizeof(led_strip_commands[0]); i++) {
		if (!strncmp(led_strip_commands[i].cmd, p, strlen(led_strip_commands[i].cmd))) {
			send_rf24_cmd(pipe_ledstrip, led_strip_commands[i].p0, led_strip_commands[i].p1, 0, 0);
			return;
		}
	}
}

void led_lamp_command(char *cmdbuf)
{
	char *p = cmdbuf + strlen("LEDLAMP ");
	int val = atoi(p);
	int retry = 3;

	if (!strncmp(p, "query", strlen("query"))) {
		while (retry-- && send_rf24_cmd(pipe_ledlamp, 'Q', 0, 0, 0)) {
			usleep(10000);
		}
	} else if (!strncmp(p, "fade", strlen("fade"))) {
		while (retry-- && send_rf24_cmd(pipe_ledlamp, 'F', 0, 0, 0)) {
			usleep(10000);
		}
	} else {
		while (retry-- && send_rf24_cmd(pipe_ledlamp, 'L', val, 0, 0)) {
			usleep(10000);
		}
	}
}

void ledstrip_reply(uint8_t *p)
{
#define UNK  printf("Unknown ledstrip reply %c %c %c %c\n", p[0], p[1], p[2], p[3]);
	switch (p[0]) {
		case 'S':
			// sequence feedback
			printf("Ledstrip playing sequence %d, at %d seconds\n", p[1], p[2] | p[3] << 8);
			break;
		case 'F':
			// fast mode
			printf("Ledstrip fast mode = %d\n", p[1]);
			break;
		case 'L':
			// light level event
			printf("Ledstrip duty cycle %d %d %d\n", p[1], p[2], p[3]);
			break;
		default:
			UNK
	}
}

static void mailbox_command(char *cmdbuf)
{
	char *p = cmdbuf + strlen("MAILBOX ");

	if (!strncmp(p, "query", strlen("query"))) {
		send_rf24_cmd(pipe_mailbox, 'Q', 0, 0, 0);
	}
}

static void mailbox_message(uint8_t *p)
{
#undef UNK
#define UNK  printf("Unknown mailbox message %c %c %c %c\n", p[0], p[1], p[2], p[3]);
	switch (p[0]) {
		case 'L':
			// light level
			switch (p[1]) {
				case 'N':
					printf("Mailbox light level %d\n", p[2] | p[3] << 8);
					break;
				default:
					UNK;
			}
			break;
		case 'I':
			if (!memcmp(p, "IRQ", 4)) {
				printf("Mailbox opened notification!\n");
				system("date");
				system("/root/home-monitoring-client/mailbox/received_mail.sh");
			} else {
				UNK;
			}
			break;
		default:
			UNK;
	}
}

void ledlamp_reply(uint8_t *p)
{
#undef UNK
#define UNK  printf("Unknown ledlamp reply %c %c %c %c\n", p[0], p[1], p[2], p[3]);
	switch (p[0]) {
		case 'T':
			// thermal event
			switch (p[1]) {
				case 'E':
					printf("Ledlamp thermal emergency, temp is %d\n", p[2] | p[3] << 8);
					break;
				case 'A':
					printf("Ledlamp thermal alarm, temp is %d\n", p[2] | p[3] << 8);
					break;
				case '0':
					printf("Ledlamp thermal stand down from alarm, temp is %d\n", p[2] | p[3] << 8);
					break;
				case 'N':
					printf("Ledlamp thermal notify: temp is %d\n", p[2] | p[3] << 8);
					break;
				default:
					UNK
			};
			break;
		case 'R':
			// remote command reply
			switch (p[1]) {
				case 'O':
					printf("Ledlamp remote reply: lamp is off\n");
					break;
				case '1':
					printf("Ledlamp remote reply: lamp is on, light level target %d\n", p[2] | p[3] << 8);
					break;
				default:
					UNK
			};
			break;
		case 'D':
			// light duty cycle event
			switch (p[1]) {
				case 'I':
					printf("Ledlamp increased power, duty cycle %d\n", p[2]);
					break;
				case 'D':
					printf("Ledlamp decreased power, duty cycle %d\n", p[2]);
					break;
				case 'N':
					printf("Ledlamp current duty cycle notify: %d\n", p[2]);
					break;
				default:
					UNK
			};
			break;
		case 'L':
			// light level event
			switch (p[1]) {
				case 'N':
					printf("Ledlamp current light level notify: %d\n", p[2]);
					break;
				default:
					UNK
			}
			break;

		default:
			UNK
	}
		
}

int find_client(unsigned long addr)
{
	unsigned int i;
	for (i = 0; i < sizeof(clients)/sizeof(clients[0]); i++) {
		if (!clients[i]) {
			continue;
		} 

		if (clients[i]->sin_addr.s_addr == addr) {
			return i;
		}
	}

	return -1;
}

int add_client(struct sockaddr_in *si)
{
	unsigned int i;
	int index = find_client(si->sin_addr.s_addr);
	if (index != -1)
		// Client already registered, nothing to do
		return 1;

	for (i = 0; i < sizeof(clients)/sizeof(clients[0]); i++) {
		if (!clients[i]) {
			// Found a free slot
			clients[i] = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
			memcpy(clients[i], si, sizeof(struct sockaddr_in));
			return 0;
		}
	}
	return 0;
}

void read_client_command(int fd, char *buf, int sz) 
{
	struct sockaddr_in si_from;
	int addr_len = sizeof(struct sockaddr_in);
	int len = 0;
	errno = 0;
	len = recvfrom(fd, buf, sz, 0, (struct sockaddr *)&si_from, (socklen_t *)&addr_len);

	if (len == -1) {
		perror("recvfrom");
		return;
	}
	buf[len] = 0;

	printf("Got client command %s\n", buf);
}

void send_to_clients(const char *buf, int len)
{
	unsigned int i;
	for (i = 0; i < sizeof(clients)/sizeof(clients[0]); i++) {
		if (clients[i]) {
			if (sendto(sockfd, buf, len, 0, (struct sockaddr *)clients[i], sizeof(struct sockaddr_in)) == -1) {
				perror("sendto");
				return;
			}
		}
	}
}

void loop(void)
{
	uint8_t data[4];
	uint8_t pipe = 1;
	struct pollfd input[] = {{ sockfd, POLLIN, 0 }};
	char cmdbuf[150];
	char buf[1000];
	char gas_cmd[400];

	 while (radio.available(&pipe)) {

		radio.read(data, 4);

		switch (pipe) {
			case PIPE_GAZ_ID:
				sprintf(gas_cmd, "curl -s http://192.168.1.6:5000/update/gas/pulse/%d\n", *((uint16_t *)data));
				system(gas_cmd);
				snprintf(buf, 1000, "gas/pulse/%d\n",*((uint16_t *)data)); 
				send_to_clients(buf, strlen(buf) + 1);
				break;
			case PIPE_LEDSTRIP_ID:
				ledstrip_reply(data);
				break;
			case PIPE_LEDLAMP_ID:
				ledlamp_reply(data);
				break;
			case PIPE_MAILBOX_ID:
				mailbox_message(data);
				break;
			default:
				fprintf(stderr, "Received message on unknown pipe id %d: %c %c %c %c\n", pipe, data[0], data[1], data[2], data[3]);
				snprintf(buf, 1000, "Received message on unknown pipe id %d: %c %c %c %c\n", pipe, data[0], data[1], data[2], data[3]);
				send_to_clients(buf, strlen(buf) + 1);
		}
	}

	if (poll(&input[0], 1, 1)) {
		read_client_command(sockfd, cmdbuf, sizeof(cmdbuf));

		if (!strncmp(cmdbuf, "LEDSTRIP ", strlen("LEDSTRIP "))) {
			led_strip_command(cmdbuf);
		} else if (!strncmp(cmdbuf, "LEDLAMP ", strlen("LEDLAMP "))) {
			led_lamp_command(cmdbuf);
		} else if (!strncmp(cmdbuf, "MAILBOX ", strlen("MAILBOX "))) {
			mailbox_command(cmdbuf);
		} else if (!strncmp(cmdbuf, "RADIO", strlen("RADIO"))) {
			radio.printDetails();
		}

		cmdbuf[0] = 0;
	}

	usleep(1000);
}

int create_socket(void)
{
	struct sockaddr_in si_me;

	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1) {
		perror("socket");
		exit(1);
	}

	memset(&si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sockfd, (struct sockaddr *)&si_me, sizeof(si_me)) == -1) {
		perror("bind");
		exit(1);
	}

	return 0;
}

int main(int argc, char** argv) 
{
	create_socket();
    setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	setup();
	while(1)
		loop();
	
	return 0;
}

