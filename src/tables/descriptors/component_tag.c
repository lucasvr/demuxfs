/* 
 * Copyright (c) 2008-2018, Lucas C. Villa Real <lucasvr@gobolinux.org>
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

bool component_is_video(uint8_t component_tag, bool *is_primary)
{
	*is_primary = component_tag == 0x00 || /* full-seg */
				  component_tag == 0x81;   /* one-seg */
	return component_tag <= 0x0f ||                        /* full-seg */
		   component_tag == 0x81 || component_tag == 0x82; /* one-seg */
}

bool component_is_audio(uint8_t component_tag, bool *is_primary)
{
	*is_primary = component_tag == 0x10 ||                          /* full-seg */
				  component_tag == 0x83 || component_tag == 0x85 || /* one-seg */
				  component_tag == 0x90;                            /* one-seg */
	return (component_tag >= 0x10 && component_tag <= 0x2f) ||      /* full-seg */
		   (component_tag >= 0x83 && component_tag <= 0x86) ||      /* one-seg */
		   (component_tag == 0x90 || component_tag == 0x91);        /* one-seg */
}

bool component_is_caption(uint8_t component_tag, bool *is_primary)
{
	*is_primary = component_tag == 0x30 || /* full-seg */
				  component_tag == 0x87;   /* one-seg */
	return (component_tag >= 0x30 && component_tag <= 0x37) || /* full-seg */
		   (component_tag == 0x87);                            /* one-seg */
}

bool component_is_superimposed(uint8_t component_tag, bool *is_primary)
{
	*is_primary = component_tag == 0x38 || /* full-seg */
		          component_tag == 0x88;   /* one-seg */
	return (component_tag >= 0x38 && component_tag <= 0x3f) || /* full-seg */
		   (component_tag == 0x88);                            /* one-seg */
}

bool component_is_object_carousel(uint8_t component_tag, bool *is_primary)
{
	*is_primary = component_tag == 0x40;
	return component_tag >= 0x40 && component_tag <= 0x6f;
}

bool component_is_event_message(uint8_t component_tag)
{
	return component_tag >= 0x70 && component_tag <= 0x7f;
}

bool component_is_data_carousel(uint8_t component_tag, bool *is_primary)
{
	*is_primary = component_tag == 0x80;
	return component_tag == 0x80;
}

bool component_is_one_seg(uint8_t component_tag)
{
	return component_tag >= 0x80 && component_tag <= 0x8f;
}
