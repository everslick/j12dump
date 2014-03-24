#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include <stropts.h>
#include <asm/termios.h>

#define SERIAL_OK           0 ///< no error
#define SERIAL_ERR         -1 ///< unknown error
#define SERIAL_ERR_OPEN    -2 ///< error while opening the serial port
#define SERIAL_ERR_READ    -3 ///< reading from port failed
#define SERIAL_ERR_WRITE   -4 ///< could not write to port
#define SERIAL_ERR_INIT    -5 ///< parameter mismatch error
#define SERIAL_ERR_TIMEOUT -6 ///< read did not complete in time

static int fd = -1;

static struct termios2 settings;

static unsigned char exp[2][2];
static unsigned char btn[12];

int tcdrain(int fd);
int tcflush(int fd, int queue_selector);

int
SerialOpen(const char *device) {
	// open port
	if ((fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
		return (SERIAL_ERR_OPEN);
	}

	// read current termios
	if (ioctl(fd, TCGETS2, &settings)) {
		return (SERIAL_ERR_OPEN);
	}

	return (SERIAL_OK);
}

void
SerialClose(void) {
	close(fd);
}

int
SerialFlush(void) {
	if (fd < 0) return (SERIAL_ERR);

	tcflush(fd, TCIOFLUSH);

	return (SERIAL_OK);
}

int
SerialDrain(void) {
	if (fd < 0) return (SERIAL_ERR);

	tcdrain(fd);

	return (SERIAL_OK);
}

int
SerialSetTimeout(int ms) {
	if (ms < 0) {
		settings.c_cc[VMIN]  = 0;
		settings.c_cc[VTIME] = 0;
	} else {
		settings.c_cc[VMIN]  = (ms) ? 0 : 1;
		settings.c_cc[VTIME] = (ms + 99) / 100; // recalculate from ms to ds
	}

	if (ioctl(fd, TCSETS2, &settings)) {
		return (SERIAL_ERR_INIT);
	}

	return (SERIAL_OK);
}

int
SerialInit(int baud, const char *format, int rtscts) {
	tcflag_t cflags = settings.c_cflag;
	speed_t speed = 0;

	if (fd < 0) return (SERIAL_ERR_INIT);

	if ((!format) || (strlen(format) != 3)) {
		return (SERIAL_ERR_INIT);
	}

	// we need those anyway
	cflags |= CLOCAL | CREAD;

	// datasize
	switch (format[0]) {
		case '5': cflags |= CS5; break;
		case '6': cflags |= CS6; break;
		case '7': cflags |= CS7; break;
		case '8': cflags |= CS8; break;
		default: return (SERIAL_ERR_INIT);
	}

	// parity
	switch (format[1]) {
		case 'N': break;                         // no parity
		case 'O': cflags |= PARODD; // no break! // odd parity
		case 'E': cflags |= PARENB; break;       // even parity
		default: return (SERIAL_ERR_INIT);
	}

	// stopbit
	switch (format[2]) {
		case '1': break;                   // 1 stopbit
		case '2': cflags |= CSTOPB; break; // 2 stopbits
		default: return (SERIAL_ERR_INIT);
	}

	// baudrate
	switch (baud) {
		case 115200: cflags |= B115200; break;
		case  57600: cflags |= B57600;  break;
		case  38400: cflags |= B38400;  break;
		case  19200: cflags |= B19200;  break;
		case   9600: cflags |= B9600;   break;
		case   4800: cflags |= B4800;   break;
		case   2400: cflags |= B2400;   break;
		case    300: cflags |= B300;    break;
		default:
			cflags &= ~CBAUD;
			cflags |= BOTHER;
			speed   = baud;
		break;
	}

	// handshake
	if (rtscts) {
		cflags |= CRTSCTS; // hardware handshake
	}

	// set port parameters
	settings.c_cflag = cflags;
	settings.c_iflag = IGNPAR;
	settings.c_ispeed = speed;
	settings.c_ospeed = speed;
	settings.c_cc[VMIN]  = 0;
	settings.c_cc[VTIME] = 0;

	// this finally initializes the driver
	if (ioctl(fd, TCSETS2, &settings)) {
		return (SERIAL_ERR_INIT);
	}

	// read back settings, so we know what baudrate really was set
	if (ioctl(fd, TCGETS2, &settings)) {
		return (SERIAL_ERR_INIT);
	}

	SerialFlush();

	return (SERIAL_OK);
}

int
SerialSendBuffer(const void *buf, unsigned int len) {
	int written;

	if (fd < 0) return (SERIAL_ERR_WRITE);

	while (len > 0) {
		written = write(fd, buf, len);
		if (written < 0) {
			if (errno == EINTR) continue;
			return (SERIAL_ERR_WRITE);
		}
		len -= written;
		buf = (char *)buf + written;
	}

	return (SERIAL_OK);
}

int
SerialSendByte(unsigned char b) {
	return (SerialSendBuffer(&b, 1));
}

int
SerialReceiveBuffer(void *buf, unsigned int *len, int timeout) {
	unsigned int length = 0;
	int received;

	if (fd < 0) return (SERIAL_ERR_READ);

	SerialSetTimeout(timeout);

	while (*len > 0) {
		received = read(fd, buf, *len);

		if (received > 0) length += received;

		if (received < 0) {
			if (errno == EINTR) continue;

			*len = length;

			return (SERIAL_ERR_READ);
		} else if (received == 0) {
			*len = length;

			if (timeout < 0) return (SERIAL_OK);

			return (SERIAL_ERR_TIMEOUT);
		}

		*len -= received;
		buf = (char *)buf + received;
	}

	*len = length;

	return (SERIAL_OK);
}

int
SerialReceiveByte(unsigned char *c, int timeout) {
	unsigned int len = 1;

	return (SerialReceiveBuffer(c, &len, timeout));
}

void
ReadPedal(int pedal) {
	unsigned int len = 2;

	int err = SerialReceiveBuffer(exp[pedal], &len, 0);

	if (err) {
		printf("\rError %i while reading pedal %i", err, pedal);
		fflush(stdout); sleep(1);
	}
}

void
ReadButton(int button) {
	if (button == 0x0F) {
		memset(btn, 0, sizeof (btn));
	} else {
		btn[button] = 1;
	}
}

int
main(int argc, char **argv) {
	const char *port = "/dev/ttyUSB1";
	unsigned char cmd;
	int i, err;

	if (argc >= 2) port = argv[1];

	if (SerialOpen(port)) {
		printf("Can't open serial device\n");

		return (-1);
	}

	if (SerialInit(10416, "8N1", 0)) {
		printf("Can't initialize serial device\n");

		return (-1);
	}

	printf("initialized %s with %i bps\n\n", port, settings.c_ospeed);

	memset(btn, 0, sizeof (btn));
	memset(exp, 0, sizeof (exp));

	while (1) {
		// timeout: 0 .. infinite, -1 .. non blocking
		err = SerialReceiveByte(&cmd, 1);

		if (err) {
			printf("\rError %i while reading command    ", err);
			fflush(stdout); sleep(1);
		}

		switch (cmd & 0xF0) {
			case 0xE0:  ReadPedal(cmd & 0x01); break;
			case 0xF0: ReadButton(cmd & 0x0F); break;
		}

		printf("\r");
		printf("CMD:%02X ", cmd);
		printf("Exp1:%02X%02X ", exp[0][0], exp[0][1]);
		printf("Exp2:%02X%02X ", exp[1][0], exp[1][1]);
		printf("Button:");

		for (i=0; i<12; i++) {
			btn[i] ? printf("*") : printf(" ");
		}

		fflush(stdout); usleep(10000);
	}

	SerialClose();	

	return (0);
}
