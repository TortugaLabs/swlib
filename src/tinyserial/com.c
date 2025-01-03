/*
 * Building: cc -o com com.c
 * Usage   : ./com /dev/device [speed]
 * Example : ./com /dev/ttyS0 [115200]
 * Keys    : Ctrl-A - exit, Ctrl-X - display control lines status
 * Darcs   : darcs get http://tinyserial.sf.net/
 * Homepage: http://tinyserial.sourceforge.net
 * Version : 2009-03-05
 *
 * Ivan Tikhonov, http://www.brokestream.com, kefeer@brokestream.com
 * Patches by Jim Kou, Henry Nestler, Jon Miner, Alan Horstmann
 *
 */


/* Copyright (C) 2007 Ivan Tikhonov

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Ivan Tikhonov, kefeer@brokestream.com

*/

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include <sys/signal.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

int transfer_byte(int from, int to, int is_control);

typedef struct {char *name; int flag; } speed_spec;
int akey = ('a' & 0x1f);	// Control + A
int xkey = ('x' & 0x1f);	// Control + X
#define UNDEFINED_KEY	256

char *printable(int ascii) {
  switch (ascii) {
    case 0: return "Control+@";
    case 8: return "Backspace";
    case 9: return "Tab";
    case 13: return "Enter";
    case 27: return "Escape";
    case UNDEFINED_KEY: return "(Disabled)";
  }
  if (1 <= ascii && ascii <= 31) {
    static char control_str[] = "Control+#";
    control_str[sizeof(control_str)-2] = ascii | 0x40;
    return control_str;
  }
  if (isprint(ascii)) {
    static char cb[2];
    cb[0] = ascii;
    cb[1] = 0;
    return cb;
  }
  return "???";
}

void print_status(int fd) {
	int status;
	unsigned int arg;
	status = ioctl(fd, TIOCMGET, &arg);
	if (status == -1) {
	  perror("ioctl");
	  return;
	}
	fprintf(stderr, "[STATUS]: ");
	if(arg & TIOCM_RTS) fprintf(stderr, "RTS ");
	if(arg & TIOCM_CTS) fprintf(stderr, "CTS ");
	if(arg & TIOCM_DSR) fprintf(stderr, "DSR ");
	if(arg & TIOCM_CAR) fprintf(stderr, "DCD ");
	if(arg & TIOCM_DTR) fprintf(stderr, "DTR ");
	if(arg & TIOCM_RNG) fprintf(stderr, "RI ");
	fprintf(stderr, "\r\n");
}

int parse_key(char *inp, int defval) {
  if (inp[0] == 0) return defval;
  if (inp[0] == '-') return UNDEFINED_KEY;
  return atoi(inp);
}

int main(int argc, char *argv[])
{
	int comfd;
	struct termios oldtio, newtio;       //place for old and new port settings for serial port
	struct termios oldkey, newkey;       //place tor old and new port settings for keyboard teletype
	char *devicename;
	int need_exit = 0;
	speed_spec speeds[] =
	{
		{"1200", B1200},
		{"2400", B2400},
		{"4800", B4800},
		{"9600", B9600},
		{"19200", B19200},
		{"38400", B38400},
		{"57600", B57600},
		{"115200", B115200},
		{NULL, 0}
	};
	char *argv0;
	int speed = B115200;	// Default speed

	argv0 = argv[0];
	while (argc > 1) {
	  if (argv[1][0] == '-' && argv[1][1] == 'a') {
	    akey = parse_key(argv[1]+2, 'a' & 0x1f);
	  } else if (argv[1][0] == '-' && argv[1][1] == 'x') {
	    xkey = parse_key(argv[1]+2, 'x' & 0x1f);
	  } else {
	    break;
	  }
	  --argc;
	  ++argv;
	}
	if(argc < 2) {
		fprintf(stderr, "example: %s [options] /dev/ttyS0 [115200]\n", argv0);
		fprintf(stderr, "  Speeds: 1200 2400 4800 9600 19200 38400 57600 115200\n");
		fprintf(stderr, "\nOptions:\n");
		fprintf(stderr, "- -a : set exit key to default\n");
		fprintf(stderr, "- -a- : Disable exit key\n");
		fprintf(stderr, "- -aNN : Where NN is an integer, set exit key to the ascii value NN\n");
		fprintf(stderr, "- -x : set status key to default\n");
		fprintf(stderr, "- -x- : Disable statujs key\n");
		fprintf(stderr, "- -xNN : Where NN is an integer, set status key to the ascii value NN\n");
		exit(1);
	}
	devicename = argv[1];

	comfd = open(devicename, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (comfd < 0)
	{
		perror(devicename);
		exit(-1);
	}


	if(argc > 2) {
		speed_spec *s;
		for(s = speeds; s->name; s++) {
			if(strcmp(s->name, argv[2]) == 0) {
				speed = s->flag;
				fprintf(stderr, "setting speed %s\n", s->name);
				break;
			}
		}
	}

	fprintf(stderr, "%s exit, ", printable(akey));
	fprintf(stderr, "%s modem lines status\n", printable(xkey));

	tcgetattr(STDIN_FILENO,&oldkey);
	newkey.c_cflag = B9600 | CRTSCTS | CS8 | CLOCAL | CREAD;
	newkey.c_iflag = IGNPAR;
	newkey.c_oflag = 0;
	newkey.c_lflag = 0;
	newkey.c_cc[VMIN]=1;
	newkey.c_cc[VTIME]=0;
	tcflush(STDIN_FILENO, TCIFLUSH);
	tcsetattr(STDIN_FILENO,TCSANOW,&newkey);

	tcgetattr(comfd,&oldtio); // save current port settings
	newtio.c_cflag = speed | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	newtio.c_cc[VMIN]=1;
	newtio.c_cc[VTIME]=0;
	tcflush(comfd, TCIFLUSH);
	tcsetattr(comfd,TCSANOW,&newtio);

	print_status(comfd);

	while(!need_exit) {
		fd_set fds;
		int ret;

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		FD_SET(comfd, &fds);


		ret = select(comfd+1, &fds, NULL, NULL, NULL);
		if(ret == -1) {
			perror("select");
		} else if (ret > 0) {
			if(FD_ISSET(STDIN_FILENO, &fds)) {
				need_exit = transfer_byte(STDIN_FILENO, comfd, 1);
			}
			if(FD_ISSET(comfd, &fds)) {
				need_exit = transfer_byte(comfd, STDIN_FILENO, 0);
			}
		}
	}

	tcsetattr(comfd,TCSANOW,&oldtio);
	tcsetattr(STDIN_FILENO,TCSANOW,&oldkey);
	close(comfd);

	return 0;
}


int transfer_byte(int from, int to, int is_control) {
	char c;
	int ret;
	do {
		ret = read(from, &c, 1);
	} while (ret < 0 && errno == EINTR);
	if(ret == 1) {
		if(is_control) {
			if(c == akey) {
				return -1;
			} else if(c == xkey) {
				print_status(to);
				return 0;
			}
		}
		while(write(to, &c, 1) == -1) {
			if(errno!=EAGAIN && errno!=EINTR) { perror("write failed"); break; }
		}
	} else {
		fprintf(stderr, "\nnothing to read. probably port disconnected.\n");
		return -2;
	}
	return 0;
}


