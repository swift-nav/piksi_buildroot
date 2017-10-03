/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Engineering <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */


#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "nap_linux.h"

int main(int argc, char* argv[]) {

	(void)argc;
	(void)argv;

	int pmem = open("/dev/mem", O_RDWR | O_SYNC);
	swiftnap_t* nap = (swiftnap_t*) mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, pmem, (off_t)NAP);

	printf("NAP status register: %08x\n", nap->CONTROL);
	printf("NAP version register: %08x\n", nap->VERSION);
}
