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
#include <iostream>
#include <iomanip>

/* Include register maps */
#include "registers/swiftnap.h"

/* Instances */
#define NAP ((swiftnap_t *)0x43C00000)
#define NAP_FE ((nt1065_frontend_t *)0x43C10000)

#include "nap_linux.h"

#if 0
#include "obfuscated_string.h"
#else
#define OBF(X) X
#endif

int main(int argc, char* argv[]) {

	(void)argc;
	(void)argv;

	int pmem = open(OBF("/dev/mem"), O_RDWR | O_SYNC);
	swiftnap_t* nap = (swiftnap_t*) mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, pmem, (off_t)NAP);

  std::cout << OBF("NAP register test...") << std::endl << std::endl;
  std::cout << std::setfill('0') << std::setw(2) << std::hex;

  std::cout << OBF("NAP status register: ") << nap->CONTROL << std::endl;
  std::cout << OBF("NAP version register: ") << nap->VERSION << std::endl;
}
