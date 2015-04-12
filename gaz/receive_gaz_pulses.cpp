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
#include <stdarg.h>
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

int have_clients()
{
	return 1;
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


void hvprintf(const char *fmt, va_list args)
{
	char *str;
	vasprintf(&str, fmt, args);

	printf("%s", str);

	if (have_clients()) {
		send_to_clients(str, strlen(str));
	} else {
		// XXX what do we do here?
	}
	free(str);
}

// "Hub" printf
void hprintf(const char *fmt, ...) 
{
	va_list args;

	va_start(args, fmt);
	hvprintf(fmt, args);
	va_end(args);
}

int send_rf24_cmd(uint64_t addr, uint8_t param0, uint8_t param1, uint8_t param2, uint8_t param3)
{
	uint8_t payload[4];
	int ret = -1;
	payload[0] = param0;
	payload[1] = param1;
	payload[2] = param2;
	payload[3] = param3;

	hprintf("send rf24...");
	radio.stopListening();
	usleep(1000);
	radio.openWritingPipe(addr);
	radio.powerUp();
	usleep(1000);
	bool ok = radio.write(&payload[0], 4);

	if (ok) {
		hprintf("... successful\n");
		ret = 0;
	} else {
		hprintf("... could not send RF24 cmd\n");
	}
	radio.startListening();
	usleep(10000);
	return ret;
}

#define MATCHSTR(BUF,STR) !strncmp(BUF, STR, strlen(STR))

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
		if (MATCHSTR(p,led_strip_commands[i].cmd)) {
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

	if (MATCHSTR(p, "query")) {
		while (retry-- && send_rf24_cmd(pipe_ledlamp, 'Q', 0, 0, 0)) {
			usleep(10000);
		}
	} else if (MATCHSTR(p, "fade")) {
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
#define UNK  hprintf("Unknown ledstrip reply %c %c %c %c\n", p[0], p[1], p[2], p[3]);
	switch (p[0]) {
		case 'S':
			// sequence feedback
			hprintf("Ledstrip playing sequence %d, at %d seconds\n", p[1], p[2] | p[3] << 8);
			break;
		case 'F':
			// fast mode
			hprintf("Ledstrip fast mode = %d\n", p[1]);
			break;
		case 'L':
			// light level event
			hprintf("Ledstrip duty cycle %d %d %d\n", p[1], p[2], p[3]);
			break;
		default:
			UNK
	}
}

static void mailbox_command(char *cmdbuf)
{
	char *p = cmdbuf + strlen("MAILBOX ");

	if (MATCHSTR(p, "query")) {
		send_rf24_cmd(pipe_mailbox, 'Q', 0, 0, 0);
	}
}

static void mailbox_message(uint8_t *p)
{
#undef UNK
#define UNK  hprintf("Unknown mailbox message %c %c %c %c\n", p[0], p[1], p[2], p[3]);
	switch (p[0]) {
		case 'L':
			// light level
			switch (p[1]) {
				case 'N':
					hprintf("Mailbox light level %d\n", p[2] | p[3] << 8);
					break;
				default:
					UNK;
			}
			break;
		case 'I':
			if (!memcmp(p, "IRQ", 4)) {
				hprintf("Mailbox opened notification!\n");
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
#define UNK  hprintf("Unknown ledlamp reply %c %c %c %c\n", p[0], p[1], p[2], p[3]);
	switch (p[0]) {
		case 'T':
			// thermal event
			switch (p[1]) {
				case 'E':
					hprintf("Ledlamp thermal emergency, temp is %d\n", p[2] | p[3] << 8);
					break;
				case 'A':
					hprintf("Ledlamp thermal alarm, temp is %d\n", p[2] | p[3] << 8);
					break;
				case '0':
					hprintf("Ledlamp thermal stand down from alarm, temp is %d\n", p[2] | p[3] << 8);
					break;
				case 'N':
					hprintf("Ledlamp thermal notify: temp is %d\n", p[2] | p[3] << 8);
					break;
				default:
					UNK
			};
			break;
		case 'R':
			// remote command reply
			switch (p[1]) {
				case 'O':
					hprintf("Ledlamp remote reply: lamp is off\n");
					break;
				case '1':
					hprintf("Ledlamp remote reply: lamp is on, light level target %d\n", p[2] | p[3] << 8);
					break;
				default:
					UNK
			};
			break;
		case 'D':
			// light duty cycle event
			switch (p[1]) {
				case 'I':
					hprintf("Ledlamp increased power, duty cycle %d\n", p[2]);
					break;
				case 'D':
					hprintf("Ledlamp decreased power, duty cycle %d\n", p[2]);
					break;
				case 'N':
					hprintf("Ledlamp current duty cycle notify: %d\n", p[2]);
					break;
				default:
					UNK
			};
			break;
		case 'L':
			// light level event
			switch (p[1]) {
				case 'N':
					hprintf("Ledlamp current light level notify: %d\n", p[2]);
					break;
				default:
					UNK
			}
			break;

		default:
			UNK
	}
		
}

static void gas_message(uint8_t *p)
{
	char buf[1000];
	static uint16_t old_pulse = 0;
	uint16_t value;
#undef UNK
#define UNK  hprintf("Unknown gas message %c %c %c %c\n", p[0], p[1], p[2], p[3]);
	switch (p[0]) {
		case 'P':
			value = p[1] << 8 | p[2];
			old_pulse = value;
			sprintf(buf, "curl -s http://192.168.1.6:5000/update/gas/pulse/%d\n", value);
			system(buf);
			snprintf(buf, 1000, "gas/pulse/%d\n", value); 
			send_to_clients(buf, strlen(buf) + 1);
			break;
		case 'B':
			value = p[2] << 8 | p[3];
			switch (p[1]) {
				case 'L':
					hprintf("Gas meter LOW battery: %fV\n", 2*value*3.3f/1024);
					break;
				case 'N':
					hprintf("Gas meter battery level: %fV\n", 2*value*3.3f/1024);
					if (old_pulse) {
						// Send a fake pulse update: if we're getting a ping it means we haven't had a pulse  for a while
						sprintf(buf, "curl -s http://192.168.1.6:5000/update/gas/pulse/%d\n", old_pulse);
						system(buf);
						snprintf(buf, 1000, "gas/pulse/%d\n", value); 
						send_to_clients(buf, strlen(buf) + 1);
					}
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
	if (si_from.sin_family != AF_INET) {
		// Localhost comes from netlink and not UDP, why the hell?!
		return;
	}

	add_client(&si_from);

}

void loop(void)
{
	uint8_t data[4];
	uint8_t pipe = 1;
	struct pollfd input[] = {{ sockfd, POLLIN, 0 }};
	char cmdbuf[150];
	char buf[1000];

	 while (radio.available(&pipe)) {

		radio.read(data, 4);

		switch (pipe) {
			case PIPE_GAZ_ID:
				gas_message(data);
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

	if (poll(&input[0], 1, 2)) {
		read_client_command(sockfd, cmdbuf, sizeof(cmdbuf));

		if (MATCHSTR(cmdbuf, "LEDSTRIP ")) {
			led_strip_command(cmdbuf);
		} else if (MATCHSTR(cmdbuf, "LEDLAMP ")) {
			led_lamp_command(cmdbuf);
		} else if (MATCHSTR(cmdbuf, "MAILBOX ")) {
			mailbox_command(cmdbuf);
		} else if (MATCHSTR(cmdbuf, "RADIO")) {
			radio.printDetails();
		} else if (MATCHSTR(cmdbuf, "PING")) {
			hprintf("PONG\n");
		}

		cmdbuf[0] = 0;
	}
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

