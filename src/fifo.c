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
#include "fifo.h"
#include "ts.h"

struct fifo {
	pthread_mutex_t mutex;
	bool flushed;
	char *path;
	int fd;
};

struct fifo *fifo_init()
{
	struct fifo *fifo = (struct fifo *) calloc(1, sizeof(struct fifo));
	if (fifo) {
		pthread_mutex_init(&fifo->mutex, NULL);
		fifo->flushed = true;
		fifo->path = NULL;
		fifo->fd = -1;
	}
	return fifo;
}

void fifo_destroy(struct fifo *fifo)
{
	if (fifo) {
		pthread_mutex_destroy(&fifo->mutex);
		if (fifo->path)
			free(fifo->path);
		if (fifo->fd > 0)
			close(fifo->fd);
		free(fifo);
	}
}

size_t fifo_get_default_size()
{
	return 0;
}

int fifo_get_type()
{
	return S_IFIFO;
}

bool fifo_is_open(struct fifo *fifo)
{
	if (fifo->fd < 0)
		fifo->fd = open(fifo->path, O_WRONLY | O_NONBLOCK);
	return fifo->fd >= 0;
}

int fifo_set_path(struct fifo *fifo, char *path)
{
	fifo->path = strdup(path);
	return 0;
}

const char *fifo_get_path(struct fifo *fifo)
{
	return fifo->path;
}

int fifo_append(struct fifo *fifo, const char *data, uint32_t size)
{
	int err, ret;
	
	do {
		ret = write(fifo->fd, data, size);
		err = ret < 0 ? errno : 0;

		pthread_mutex_lock(&fifo->mutex);
		if (ret > 0)
			fifo->flushed = false;
		else if (err != EAGAIN) {
			close(fifo->fd);
			fifo->flushed = true;
			fifo->fd = -1;
		}
		pthread_mutex_unlock(&fifo->mutex);
	} while (err == EAGAIN);

	return 0;
}
