/* 
 * Copyright (c) 2008-2010, Lucas C. Villa Real <lucasvr@gobolinux.org>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of GoboLinux nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "demuxfs.h"
#include "buffer.h"
#include "crc32.h"

#define WIDTH 32
#define TOPBIT (1<<(WIDTH-1))
#define POLYNOMIAL 0x04C11DB7
#define INITIAL_REMAINDER 0xFFFFFFFF

#define TABLE_SIZE 256
static uint32_t crc32_table[TABLE_SIZE];

static void crc32_init_table()
{
	uint16_t dividend;
	uint32_t remainder;

	for (dividend=0; dividend<TABLE_SIZE; ++dividend) {
		remainder = dividend << (WIDTH - 8);
		for (uint8_t bit=8; bit>0; --bit) {
			if (remainder & TOPBIT)
				remainder = (remainder << 1) ^ POLYNOMIAL;
			else
				remainder = (remainder << 1);
		}
		crc32_table[dividend] = remainder;
	}
}

bool crc32_check(const char *buf, uint32_t len)
{
	uint32_t i, remainder = INITIAL_REMAINDER;
	static bool table_initialized = false;

	if (! table_initialized) {
		crc32_init_table();
		table_initialized = true;
	}

	for (i=0; i<len; ++i) {
		uint8_t table_idx = buf[i] ^ (remainder >> (WIDTH - 8));
		remainder = crc32_table[table_idx] ^ (remainder << 8);
	}

	return remainder ? false : true;
}
