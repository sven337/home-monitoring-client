#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "teleinfo.h"

int main(int argc, char **argv)
{
	int fd = 0;
	fd = teleinfo_open("/dev/ttyAMA0");
	if (!fd) {
		fprintf(stderr, "Failed to open serial port\n");
		exit(1);
	}

	setvbuf(stdout, NULL, _IONBF, 0);
	char buf[1024];
	while (1) {
		int nb = read(fd, &buf[0], 1024);
		if (!nb) {
			fprintf(stderr, "Unable to read from serial port\n");
			exit(1);
		}
		fwrite(&buf[0], nb, 1, stdout);
	}
	teleinfo_close(fd);
}
