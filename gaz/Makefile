# The recommended compiler flags for the Raspberry Pi
CCFLAGS=-Wall -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s

PROGRAMS = receive_gaz_pulses
SOURCES = ${PROGRAMS:=.cpp}

all: ${PROGRAMS}

${PROGRAMS}: ${SOURCES}
#	g++ ${CCFLAGS} -Wall -L../librf24/  -lrf24 $@.cpp -o $@
	g++ ${CCFLAGS} -g -I./librf24 -L/usr/local/lib  -lrf24-bcm $@.cpp -o $@

clean:
	rm -rf $(PROGRAMS)

