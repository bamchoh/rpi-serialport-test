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

#define SERIAL_PORT "/dev/ttyAMA0"

int setBaudrate(char *rate)
{
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

void print_time(struct timespec t) {
	printf("now:%10ld.%09ld\n", t.tv_sec, t.tv_nsec);
}

void print_duration(struct timespec s, struct timespec e) {
	if(e.tv_nsec < s.tv_nsec) {
		printf("%10ld.%09ld\n", e.tv_sec - s.tv_sec - 1, e.tv_nsec + 1000000000 - s.tv_nsec);
	} else {
		printf("%10ld.%09ld\n", e.tv_sec - s.tv_sec, e.tv_nsec - s.tv_nsec);
	}
}

int main(int argc, char *argv[])
{
	if(argc < 2){
		printf("too few arguments\n");
		return -1;
	}

	unsigned char msg[] = "serial port open...\n";
	unsigned char buf[255];
	int fd;
	struct termios tio;
	int baudRate = setBaudrate(argv[1]);
	int i;
	int len;

	FILE *fp;
	char sdata[1024];

	fp = fopen("text.txt", "r");

	if(fp==NULL) {
		printf("File was not found\n");
		return -1;
	}

	while((fgets(sdata, 1024, fp)) != NULL) { }

	// fd = open(SERIAL_PORT, O_RDWR|O_NOCTTY);
	fd = open(SERIAL_PORT, O_RDWR);
	if (fd < 0) {
		printf("open error\n");
		return -1;
	}

	tio.c_cflag |= CREAD;
	tio.c_cflag |= CLOCAL;
	tio.c_cflag &= ~CSIZE;
	tio.c_cflag |= CS8;
	tio.c_cflag &= ~CSTOPB;
	tio.c_cflag &= ~PARENB;
	tio.c_cflag &= ~PARODD;
	tio.c_cflag |= CRTSCTS;

	// tio.c_iflag &= ~IUTF8;
	// tio.c_iflag &= ~ICRNL;
	// tio.c_iflag &= ~INLCR;

	// tio.c_oflag &= ~OCRNL;
	// tio.c_oflag &= ~ONLRET;
	// tio.c_oflag &= ~NLDLY;
	// tio.c_oflag |= NL0;
	// tio.c_oflag &= ~CRDLY;
	// tio.c_oflag |= CR0;
	// tio.c_oflag &= ~TABDLY;
	// tio.c_oflag |= TAB0;
	// tio.c_oflag &= ~BSDLY;
	// tio.c_oflag |= BS0;
	// tio.c_oflag &= ~VTDLY;
	// tio.c_oflag |= VT0;
	// tio.c_oflag &= ~FFDLY;
	// tio.c_oflag |= FF0;

	cfsetispeed(&tio, baudRate);
	cfsetospeed(&tio, baudRate);

	// non canonical mode setting
	tio.c_lflag = 0;
	tio.c_cc[VTIME] = 0;
	tio.c_cc[VMIN] = 1;

	ioctl(fd, TCSETS, &tio);

	// send break signal for 100 ms(?)
	// tcsendbreak(fd, 100);
	// tcdrain(fd);

	// write 2 bytes
	for(i = 0; i < strlen(sdata); i++) {
		printf("%02X ", sdata[i]);
	}
	printf("\n");
	write(fd, sdata, strlen(sdata));
	tcdrain(fd);

	// waiting for receive any data
	struct timespec start_time, end_time;
	while (1) {
		clock_gettime(CLOCK_REALTIME, &start_time);
		len = read(fd, buf, sizeof(buf));
		clock_gettime(CLOCK_REALTIME, &end_time);
		print_time(start_time);
		print_time(end_time);
		print_duration(start_time, end_time);
		printf("\n");
		fflush(stdout);
		if (0 < len) {
			// check break detection count
			struct serial_icounter_struct icount;
			ioctl(fd, TIOCGICOUNT, &icount);
			printf("brk: %d\n", icount.brk);
			icount.brk = 0;

			// print received data
			for(i = 0; i < len; i++) {
				if(i != 0) {
					// if((i % 8) == 0) {
					// 	printf("\n");
					// } else {
						printf(":");
					// }
				}
				printf("%02X", buf[i]);
			}
			printf("\n");
		}
	}
	close(fd);
	fclose(fp);
	return 0;
}
