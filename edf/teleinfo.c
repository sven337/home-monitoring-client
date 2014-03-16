/*-
 * Copyright (C) 2010 Romuald Conty
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* This file is based on teleinfuse. 
 */
#include "teleinfo.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <termios.h>
#include <errno.h>

int teleinfo_open(const char *port)
{
	struct termios teleinfo_serial_attr;
	int fd;

	if ((fd = open(port, O_RDONLY | O_NOCTTY)) == -1) {
		syslog(LOG_ERR, "Erreur ouverture du port serie %s !", port);
		return 0;
	}

	tcgetattr(fd, &teleinfo_serial_attr);

	cfsetispeed(&teleinfo_serial_attr, B1200);
	cfsetospeed(&teleinfo_serial_attr, B1200);

	teleinfo_serial_attr.c_cflag |= (CLOCAL | CREAD);

	// Format série "7E1"
	teleinfo_serial_attr.c_cflag |= PARENB;	// Active 7 bits de donnees avec parite pair.
	teleinfo_serial_attr.c_cflag &= ~PARODD;
	teleinfo_serial_attr.c_cflag &= ~CSTOPB;
	teleinfo_serial_attr.c_cflag &= ~CSIZE;
	teleinfo_serial_attr.c_cflag |= CS7;
//     teleinfo_serial_attr.c_cflag &= ~CRTSCTS ;                           // Désactive control de flux matériel. (pas compatible POSIX)

	teleinfo_serial_attr.c_iflag |= (INPCK | ISTRIP);
	teleinfo_serial_attr.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);	// Désactive control de flux logiciel, conversion 0xOD en 0x0A.

	teleinfo_serial_attr.c_oflag &= ~OPOST;	// Pas de mode de sortie particulier (mode raw).

	teleinfo_serial_attr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);	// Mode non-canonique (mode raw) sans echo.

	teleinfo_serial_attr.c_cc[VTIME] = 80;	// time-out à ~8s.
	teleinfo_serial_attr.c_cc[VMIN] = 0;	// 1 car. attendu.

	tcflush(fd, TCIFLUSH);	// Efface les données reçues mais non lues.
	tcsetattr(fd, TCSANOW, &teleinfo_serial_attr);	// Sauvegarde des nouveaux parametres
	return fd;
}

void teleinfo_close(int fd)
{
	close(fd);
}

#ifdef DEBUG
#include "time.h"
void dbg_dump(const char *buf, size_t n)
{
	time_t now = time(NULL);
	char filename[32];
	snprintf(filename, 31, "/tmp/teleinfo-dump-%d", (int)now);
	syslog(LOG_INFO, "dumping buffer to %s (%d bytes)", filename, n);
	FILE *file = fopen(filename, "wb");
	if (fwrite(buf, 1, n, file) != n)
		syslog(LOG_INFO, "unable to write dump file");
	fclose(file);
}
#endif

// Trame Teleinfo
// STX 1 char (0x02)
// [ MESSAGE_0 ] 25 char max
// ... [ MESSAGE_N ]
// ETX 1 char (0x03)
#define STX '\x02'
#define ETX '\x03'
#define EOT '\x04'
#define LF  '\x0a'
#define CR  '\x0d'
int teleinfo_read_frame_ext(const int fd, char *const buffer,
			    const size_t buflen, int *error_counter)
{
	char *p = buffer;
	size_t s = 0;
	char c;
	enum state { INIT, FRAME_BEGIN, FRAME_END, MSG_BEGIN, MSG_END };
	enum state current_state = INIT;
	int error_count = 0;
	int bytes_in_init_mode = 0;

	do {
		int res = read(fd, &c, 1);
		if (!res) {
			syslog(LOG_ERR, "unable to read from source\n");
			return EIO;
		}
		switch (c) {
		case STX:
			if (current_state != INIT) {
#ifdef DEBUG
				syslog(LOG_INFO,
				       "new STX detected but not expected, resetting frame begin");
				if (s < buflen) {
					*p++ = c;
					s++;
				} else {
					return EMSGSIZE;
				}
				dbg_dump(buffer, s);
#endif
				error_count++;
			}
			current_state = FRAME_BEGIN;
			p = buffer;
			s = 0;
//         if (s<buflen) { *p++ = c; s++; } else { return EMSGSIZE; }
			break;
		case LF:
			if (current_state != INIT) {
				if ((current_state != FRAME_BEGIN)
				    && (current_state != MSG_END)) {
#ifdef DEBUG
					syslog(LOG_INFO,
					       "LF detected but not expected, frame is invalid");
					if (s < buflen) {
						*p++ = c;
						s++;
					} else {
						return EMSGSIZE;
					}
					dbg_dump(buffer, s);
#endif
					error_count++;
					current_state = INIT;
				} else {
					current_state = MSG_BEGIN;
					if (s < buflen) {
						*p++ = c;
						s++;
					} else {
						return EMSGSIZE;
					}
				}
			}	// else do nothing: simply skip the char
			break;
		case CR:
			if (current_state != INIT) {
				if (current_state != MSG_BEGIN) {
#ifdef DEBUG
					syslog(LOG_INFO,
					       "CR detected but not expected, frame is invalid");
					if (s < buflen) {
						*p++ = c;
						s++;
					} else {
						return EMSGSIZE;
					}
					dbg_dump(buffer, s);
#endif
					error_count++;
					current_state = INIT;
				} else {
					current_state = MSG_END;
					if (s < buflen) {
						*p++ = c;
						s++;
					} else {
						return EMSGSIZE;
					}
				}
			}	// else do nothing: simply skip the char
			break;
		case ETX:
			if (current_state != INIT) {
				if (current_state != MSG_END) {
#ifdef DEBUG
					syslog(LOG_INFO,
					       "ETX detected but not expected, frame is invalid");
					if (s < buflen) {
						*p++ = c;
						s++;
					} else {
						return EMSGSIZE;
					}
					dbg_dump(buffer, s);
#endif
					error_count++;
					current_state = INIT;
				} else {
					current_state = FRAME_END;
//             if (s<buflen) { *p++ = c; s++; } else { return EMSGSIZE; }
				}
			}	// else do nothing: simply skip the char
			break;
		case EOT:
			syslog(LOG_INFO,
			       "frame have been interrupted by EOT, resetting frame");
			current_state = INIT;
			break;
		default:
			switch (current_state) {
			case INIT:
				// STX have not been detected yet, so we skip char
				break;
			case FRAME_BEGIN:
#ifdef DEBUG
				syslog(LOG_INFO,
				       "STX should be followed by LF, frame is invalid");
				if (s < buflen) {
					*p++ = c;
					s++;
				} else {
					return EMSGSIZE;
				}
				dbg_dump(buffer, s);
#endif
				current_state = INIT;
				error_count++;
				break;
			case FRAME_END:
				// We should not be here !
				break;
			case MSG_BEGIN:
				// Message content
				if (s < buflen) {
					*p++ = c;
					s++;
				} else {
					return EMSGSIZE;
				}
				break;
			case MSG_END:
#ifdef DEBUG
				syslog(LOG_INFO,
				       "CR should be followed by ETX or LF, frame is invalid");
				if (s < buflen) {
					*p++ = c;
					s++;
				} else {
					return EMSGSIZE;
				}
				dbg_dump(buffer, s);
#endif
				current_state = INIT;
				error_count++;
				break;
			}
		}
		if (current_state == INIT)
			bytes_in_init_mode++;
	} while ((current_state != FRAME_END) && (error_count < 10)
		 && (bytes_in_init_mode < TI_FRAME_LENGTH_MAX * 2));
	if (error_counter != NULL) {
		*error_counter = error_count;
	}
	if (current_state == FRAME_END) {
		return 0;
	} else {
		syslog(LOG_INFO, "too many error while reading, giving up");
		return EBADMSG;
	}
}

int teleinfo_checksum(char *message)
{
	const char *orig_message = message;
	const char *message_oel = strchr(message, 0x0d);	// Mémorise le pointer de fin de ligne
	unsigned char sum = 0;	// Somme des codes ASCII du message

	message++;
	while ((*message != '\0') && (message != (message_oel - 2))) {	// Tant qu'on est pas au checksum (avec SP precédent)
		sum += *message;
		message++;
	}
	sum = (sum & 0x3F) + 0x20;
	message++;		// On passe le SP
	if (sum == *message) {
		return 1;	// Return 1 si checkum ok.*
	}
#ifdef DEBUG
	syslog(LOG_INFO,
	       "wrong checksum: 0x%02x should be 0x%02x, for %s, message_oel is %s",
	       *message, sum, orig_message, message_oel);
#endif
	return 0;
}

int teleinfo_decode(const char *frame, teleinfo_data dataset[],
		    size_t * datasetlen)
{
	char *message_oel;
	char *message = (char *)frame;
	char label[20];
	char value[20];
	int wrong_checksum_count = 0;
	size_t data_count = 0;
	*datasetlen = 0;

	while ((message_oel = strchr(message, 0x0d))) {
		if (1 == teleinfo_checksum(message)) {
			message++;	// On passe le LF de début de ligne

			sscanf(message, "%s %s *", label, value);
			// TODO: Check if lenght(label) > 8
			strncpy(dataset[data_count].label, label, 8);
			dataset[data_count].label[8] = '\0';
			// TODO: Check  if lenght(value) > 12
			strncpy(dataset[data_count].value, value, 12);
			dataset[data_count].value[12] = '\0';
			data_count++;

		} else {
			// Erreur de checksum
			wrong_checksum_count++;
			if (wrong_checksum_count >= 3) {
				return EBADMSG;
			}
		}
		message = message_oel;	// On se place sur la fin de ligne
		message++;	// On passe le CR de fin de ligne
	}
	*datasetlen = data_count;
	return 0;
}
