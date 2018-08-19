#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <linux/serial.h>
#include <time.h>
#include <unistd.h>

#define SERIAL_PORT "/dev/ttyAMA0"

tcflag_t setBaudrate(char *rate)
{
	if(rate == 0)
		return B9600;

	if(strcmp(rate, "2400") == 0)
		return B2400;

	if(strcmp(rate, "4800") == 0)
		return B4800;

	if(strcmp(rate, "9600") == 0)
		return B9600;

	if(strcmp(rate, "19200") == 0)
		return B19200;

	if(strcmp(rate, "38400") == 0)
		return B38400;

	if(strcmp(rate, "57600") == 0)
		return B57600;

	if(strcmp(rate, "115200") == 0)
		return B115200;

	return B9600;
}

tcflag_t setParity(char *parity) {
	if(parity == 0)
		return 0;

	if(strcmp(parity, "e") == 0)
		return PARENB;

	if(strcmp(parity, "o") == 0)
		return PARENB | PARODD;

	return 0;
}

tcflag_t setStopBits(char *bit) {
	if(bit == 0)
		return 0;

	if(strcmp(bit, "2") == 0)
		return CSTOPB;

	return 0;
}

tcflag_t setLength(char *len) {
	if(len == 0)
		return CS8;

	if(strcmp(len, "7") == 0)
		return CS7;

	return CS8;
}

void print_time(char *name, struct timespec t) {
	printf("%s:%10ld.%09ld ", name, t.tv_sec, t.tv_nsec);
}

void print_duration(struct timespec s, struct timespec e) {
	if(e.tv_nsec < s.tv_nsec) {
		printf("(%ld.%09ld)", e.tv_sec - s.tv_sec - 1, e.tv_nsec + 1000000000 - s.tv_nsec);
	} else {
		printf("(%ld.%09ld)", e.tv_sec - s.tv_sec, e.tv_nsec - s.tv_nsec);
	}
}

int main(int argc, char *argv[])
{
	unsigned char msg[] = "serial port open...\n";
	unsigned char buf[255];
	int fd;
	struct termios tio = {0};
	int i;
	int len;
	FILE *fp;
	char sdata[1024];
	fd_set fds, readfds;
	struct timeval tv;
	int n;

	int opt;
	char *b_optarg;
	char *p_optarg;
	char *s_optarg;
	char *l_optarg;
	opterr = 0;
	while ((opt = getopt(argc, argv, "b:p:s:l:")) != -1) {
		switch(opt) {
			case 'b':
				b_optarg = optarg;
				break;
			case 'p':
				p_optarg = optarg;
				break;
			case 's':
				s_optarg = optarg;
				break;
			case 'l':
				l_optarg = optarg;
				break;
			default:
				break;
		}
	}

	// fd = open(SERIAL_PORT, O_RDWR|O_NOCTTY);
	fd = open(SERIAL_PORT, O_RDWR);
	if (fd < 0) {
		printf("open error\n");
		return -1;
	}

	tio.c_cflag |= CREAD;
	tio.c_cflag |= CLOCAL;
	tio.c_cflag |= CRTSCTS;
	tio.c_cflag |= setBaudrate(b_optarg);
	tio.c_cflag |= setParity(p_optarg);
	tio.c_cflag |= setStopBits(b_optarg);
	tio.c_cflag |= setLength(l_optarg);

	tio.c_iflag &= ~IUTF8;
	tio.c_iflag &= ~ICRNL;
	tio.c_iflag &= ~INLCR;

	tio.c_oflag &= ~OCRNL;
	tio.c_oflag &= ~ONLRET;
	tio.c_oflag &= ~NLDLY;
	tio.c_oflag |= NL0;
	tio.c_oflag &= ~CRDLY;
	tio.c_oflag |= CR0;
	tio.c_oflag &= ~TABDLY;
	tio.c_oflag |= TAB0;
	tio.c_oflag &= ~BSDLY;
	tio.c_oflag |= BS0;
	tio.c_oflag &= ~VTDLY;
	tio.c_oflag |= VT0;
	tio.c_oflag &= ~FFDLY;
	tio.c_oflag |= FF0;

	// non canonical mode setting
	tio.c_lflag = 0;
	tio.c_cc[VTIME] = 0;
	tio.c_cc[VMIN] = 1;

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &tio);

	// send break signal for 100 ms(?)
	// tcsendbreak(fd, 100);
	// tcdrain(fd);

	// write 2 bytes
	fp = fopen("text.txt", "r");
	if(fp==NULL) {
		printf("File was not found\n");
	} else {
		while((fgets(sdata, 1024, fp)) != NULL) { }
		for(i = 0; i < strlen(sdata); i++) {
			printf("%02X ", sdata[i]);
		}
		printf("\n");
		write(fd, sdata, strlen(sdata));
		tcdrain(fd);
	}

	// select init
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	// waiting for receive any data
	struct timespec start_time, end_time;
	__u32 prx;
	struct serial_icounter_struct icount;
	ioctl(fd, TIOCGICOUNT, &icount);
	prx = icount.rx;
	int delta, total = 0;
	while (1) {
		clock_gettime(CLOCK_REALTIME, &start_time);

		len = read(fd, buf, sizeof(buf));

		if (0 < len) {
			// check break detection count
			ioctl(fd, TIOCGICOUNT, &icount);
			printf("brk: %d,", icount.brk);
			delta = icount.rx - prx;
			total += delta;
			printf(" rx: %d\n", icount.rx - prx);
			prx = icount.rx;

			if (delta < 8) {
				printf("end\n");
				fflush(stdout);
				total = 0;
			} else {
				tv.tv_sec = 0;
				tv.tv_usec = 1000;

				n = select(fd+1, &readfds, NULL, NULL, &tv);

				if (n == 0) {
					printf("end\n");
					fflush(stdout);
					total = 0;
				}
			}

			// print received data
			// for(i = 0; i < len; i++) {
			// 	if(i != 0) {
			// 		// if((i % 8) == 0) {
			// 		// 	printf("\n");
			// 		// } else {
			// 		printf(":");
			// 		// }
			// 	}
			// 	printf("%02X", buf[i]);
			// }
			// printf("\n");
		}

		clock_gettime(CLOCK_REALTIME, &end_time);
		print_time("s", start_time);
		print_time("e", end_time);
		print_duration(start_time, end_time);
		printf("\n");
		fflush(stdout);
	}
	close(fd);
	fclose(fp);
	return 0;
}
