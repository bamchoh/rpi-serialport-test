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
#include <malloc.h>

#define SERIAL_PORT "/dev/ttyAMA0"
#define UIOGRXIS 0x80000001
#define SIZEMAX 100
#define NUMMAX 100

typedef struct log {
	int size;
	int num;
	struct timespec st;
	struct timespec et;
} log; 

int check_serial_error(int fd, struct serial_icounter_struct prev, struct serial_icounter_struct now) {
	if(prev.parity != now.parity || prev.frame != now.frame || prev.overrun != now.overrun)
		return -1;

	return 0;
}

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

struct timespec calc_duration(FILE *fp, struct timespec s, struct timespec e) {
	struct timespec tmp;
	if(e.tv_nsec < s.tv_nsec) {
		tmp.tv_sec = e.tv_sec - s.tv_sec - 1;
		tmp.tv_nsec = e.tv_nsec + 1000000000 - s.tv_nsec;
	} else {
		tmp.tv_sec = e.tv_sec - s.tv_sec;
		tmp.tv_nsec = e.tv_nsec - s.tv_nsec;
	}
	return tmp;
}

int main(int argc, char *argv[])
{
	unsigned char msg[] = "serial port open...\n";
	unsigned char buf[255];
	unsigned char rxbuf[4086] = {0};
	int fd;
	struct termios tio = {0};
	int i;
	int len;
	FILE *fp;
	char sdata[1024];
	struct timeval tv;
	int n;
	unsigned long rxtocnt = 0;

	int opt;
	char *b_optarg = 0;
	char *p_optarg = 0;
	char *s_optarg = 0;
	char *l_optarg = 0;
	char *f_optarg = 0;
	opterr = 0;
	while ((opt = getopt(argc, argv, "b:p:s:l:f:")) != -1) {
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
			case 'f':
				f_optarg = optarg;
				break;
			default:
				break;
		}
	}

	if(f_optarg == 0) {
		fp = stdout;
	} else {
		fp = fopen(f_optarg, "w");
		if(fp == NULL) {
			fprintf(stderr, "Any error happened in opening result file. (%d) \n", fp);
			return -2;
		}
	}

	fd = open(SERIAL_PORT, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open error\n");
		return -1;
	}

	tio.c_cflag |= CREAD;
	tio.c_cflag |= CLOCAL;
	tio.c_cflag |= CRTSCTS;
	tio.c_cflag |= setBaudrate(b_optarg);
	tio.c_cflag |= setParity(p_optarg);
	tio.c_cflag |= setStopBits(b_optarg);
	tio.c_cflag |= setLength(l_optarg);

	// non canonical mode setting
	tio.c_lflag = 0;
	tio.c_cc[VTIME] = 0;
	tio.c_cc[VMIN] = 1;

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &tio);

	// send break signal for 100 ms(?)
	// tcsendbreak(fd, 100);
	// tcdrain(fd);

	// init rxtocnt
	unsigned long prev_rxtocnt;
	ioctl(fd, UIOGRXIS, &rxtocnt);
	prev_rxtocnt = rxtocnt;

	// init rxtotal
	struct serial_icounter_struct prev_icount, now_icount;
	__u32 rxtotal = 0;
	ioctl(fd, TIOCGICOUNT, &prev_icount);
	rxtotal = prev_icount.rx;

	// write 2 bytes
	// fp = fopen("text.txt", "r");
	// if(fp==NULL) {
	// 	printf("File was not found\n");
	// } else {
	// 	while((fgets(sdata, 1024, fp)) != NULL) { }
	// 	for(i = 0; i < strlen(sdata); i++) {
	// 		printf("%02X ", sdata[i]);
	// 	}
	// 	printf("\n");
	// 	write(fd, sdata, strlen(sdata));
	// 	tcdrain(fd);
	// }

	log l[SIZEMAX][NUMMAX];

	// waiting for receive any data
	struct timespec start_time, end_time, mid_time;
	int total = 0;
	int endcnt = 0;
	clock_gettime(CLOCK_REALTIME, &start_time);
	for(int size = 1; size < SIZEMAX; size++) {
		for(int num = 0; num < NUMMAX; num++) {
			write(fd, sdata, size);
			while(1) {
				len = read(fd, buf, sizeof(buf));
				if (len <= 0) {
					fprintf(stderr, "read error!!\n");
					break;
				}

				ioctl(fd, UIOGRXIS, &rxtocnt);

				for(i = 0; i < len;i++) {
					rxbuf[total+i] = buf[i];
				}
				// check break detection count
				ioctl(fd, TIOCGICOUNT, &now_icount);
				if(check_serial_error(fd, prev_icount, now_icount))
					printf("serial error happens!!\n");

				prev_icount = now_icount;
				total += len;

				if (prev_rxtocnt != rxtocnt && total == (prev_icount.rx - rxtotal)) {

					clock_gettime(CLOCK_REALTIME, &end_time);

					l[size][num].size = size;
					l[size][num].num = num;
					l[size][num].st = start_time;
					l[size][num].et = end_time;

					start_time.tv_sec = end_time.tv_sec;
					start_time.tv_nsec = end_time.tv_nsec;

					// print received data
					// for(i = 0; i < total; i++) {
					// 	if(i != 0) {
					// 		printf(":");
					// 	}
					// 	printf("%02X", rxbuf[i]);
					// }

					rxtotal = prev_icount.rx;
					prev_rxtocnt = rxtocnt;
					memset(rxbuf, 0, total);
					total = 0;
					break;
				}
			}
		}
	}

	for(int size = 1;size < SIZEMAX;size++) {
		for(int num = 0; num < NUMMAX; num++) {
			struct timespec calc;
			calc = calc_duration(fp, l[size][num].st, l[size][num].et);
			fprintf(fp, "size=%3d\tnum=%3d\t%ld.%09ld",
					l[size][num].size,
					l[size][num].num,
					calc.tv_sec,
					calc.tv_nsec);

			fprintf(fp, "\n");
			fflush(fp);
		}
	}

	close(fd);
	fclose(fp);
	return 0;
}
