/*
 * Copyright (C) 2014 PHYTEC America, LLC
 * All rights reserved.
 * Author: Russell Robinson <rrobinson@phytec.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#define KSP5012_FPGA_PORT "/dev/ttyO3"

int set_interface_attribs (int fd, int speed, int parity)
{
	int ret = 0;
        struct termios tty;

        memset(&tty, 0, sizeof tty);

	ret = tcgetattr(fd, &tty);
	if (0 != ret)
        {
		fprintf(stderr, "fpga_ksp5012 (attribs): tcgetattr failed\n");
		goto exit;
        }

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag =(tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;         // disable break processing
	tty.c_lflag = 0;                // no signaling chars, no echo,
	// no canonical processing
	tty.c_oflag = 0;                // no remapping, no delays
	tty.c_cc[VMIN]  = 0;            // read doesn't block
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
					// enable reading
	tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	ret = tcsetattr(fd, TCSANOW, &tty);
	if (0 != ret)
		fprintf(stderr, "fpga_ksp5012 (attribs): tcsetattr failed\n");

exit:
        return ret;
}

void set_blocking (int fd, int should_block)
{
	struct termios tty;
	memset(&tty, 0, sizeof tty);
	if (tcgetattr(fd, &tty) != 0) {
		fprintf(stderr, "fpga_ksp5012 (blocking): tcgetattr failed\n");
		return;
	}

	tty.c_cc[VMIN]  = should_block ? 1 : 0;
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		fprintf(stderr, "fpga_ksp5012 (blocking): tcsetattr failed\n");
	}
}

void usage(const char* prog)
{
	printf("Usage: %s [i2c_bus_id] \n"
		"  i2c_bus_id    This is ignored if FPGA version < 3\n"
		"                Defaults to 1 for FPGA version >= 3\n"
		"                The valid values are 1 or 4.\n", prog);
}

int main(int argc, char** argv)
{
	int busId, fd, numChar, fpgaVer;
	char buf[7];

	if (argc > 2) {
		usage(argv[0]);
		return -E2BIG;
	} else if (argc == 2) {
		busId = strtol(argv[1], NULL, 10);
		if ((busId != 4) && (busId != 1)) {
			usage(argv[0]);
			return -EINVAL;
		}
	} else {
		busId = 1;
	}

	fd = open(KSP5012_FPGA_PORT, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "%s: error opening %s\n", argv[0],
				KSP5012_FPGA_PORT);
		return -ENODEV;
	}

	/* 115200 baud, 8n1 */
	if (set_interface_attribs(fd, B115200, 0) < 0) {
		fprintf(stderr, "%s: failed to set attribs\n", argv[0]);
	}
	/* no blocking */
	set_blocking(fd, 0);

	/* command to read from FPGA register 1 */
	write(fd, "*r1", 3);

	/* sleep to allow time for write and response */
	usleep((3 + 7) * 100);

	/* handle FPGA response */
	numChar = read(fd, buf, sizeof(buf));
	if (numChar < 1) {
		fprintf(stderr, "%s: unable to read register 1\n", argv[0]);
		return -EINVAL;
	}

	printf("%s: %d char register output: %s\n", argv[0], numChar, buf);
	if (!strncmp("ERR", &buf[4], 3)) {
		fprintf(stderr, "%s: register 1 returned ERR\n", argv[0]);
		return -EINVAL;
	} else {
		fpgaVer = (strtol(&buf[4], NULL, 16) >> 7);
		if (fpgaVer >= 3) {
			if (busId == 4) {
				system("/system/bin/insmod "
					"/system/lib/modules/board-omap4ksp5012-version.ko "
					"bus_id=4");
			} else {
				system("/system/bin/insmod "
					"/system/lib/modules/board-omap4ksp5012-version.ko "
					"bus_id=1");
			}
		} else {
			busId = 3;
			system("/system/bin/insmod "
				"/system/lib/modules/board-omap4ksp5012-version.ko "
				"bus_id=3");
		}
	}

	printf("%s: the FPGA version is %d and the "
		"i2c modules were loaded on bus %d\n", argv[0], fpgaVer, busId);

	printf("running: /system/bin/busybox hwclock -s");
	system("/system/bin/busybox hwclock -s");

	return 0;
}

