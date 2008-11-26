/* 
 * Copyright (c) 2008, Lucas C. Villa Real <lucasvr@gobolinux.org>
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

static struct fifo_element *get_head(struct fifo *fifo);

struct fifo *fifo_init(uint32_t max_elements)
{
	struct fifo *fifo = (struct fifo *) malloc(sizeof(struct fifo));
	if (! fifo)
		return NULL;
	INIT_LIST_HEAD(&fifo->list);
	fifo->max_elements = max_elements;
	fifo->num_elements = 0;
	pthread_mutex_init(&fifo->head_mutex, NULL);
	return fifo;
}

void fifo_destroy(struct fifo *fifo)
{
	struct fifo_element *entry, *aux;

	pthread_mutex_lock(&fifo->head_mutex);
	list_for_each_entry_safe(entry, aux, &fifo->list, list) {
		list_del(&entry->list);
		free(entry->data);
		free(entry);
	}
	pthread_mutex_unlock(&fifo->head_mutex);
	pthread_mutex_destroy(&fifo->head_mutex);
}

bool fifo_empty(struct fifo *fifo)
{
	bool empty;
	pthread_mutex_lock(&fifo->head_mutex);
	empty = fifo->num_elements == 0;
	pthread_mutex_unlock(&fifo->head_mutex);
	return empty;
}

size_t fifo_read(struct fifo *fifo, char *buf, size_t size)
{
	struct fifo_element *head;
	size_t to_read, copied = 0;
	bool read_everything;

	head = get_head(fifo);
	if (! head)
		return 0;

	read_everything = size > head->size;
	to_read = read_everything ? head->size : size;
	
	/* 
	 * Lock-free. We are guaranteed by the filesystem to have 
	 * at most one reader consuming data from special files.
	 */
	memcpy(buf, head->read_ptr, to_read);
	copied = to_read;

	pthread_mutex_lock(&fifo->head_mutex);
	if (! read_everything) {
		/* There's room left in this buffer element */
		head->read_ptr += copied;
		head->size -= copied;
	} else {
		/* This buffer element has been consumed completely */
		list_del(&head->list);
		free(head->data);
		free(head);
		fifo->num_elements--;
	}
	pthread_mutex_unlock(&fifo->head_mutex);

	return copied;
}

int fifo_append(struct fifo *fifo, const char *data, uint32_t size)
{
	struct fifo_element *element;
	
	pthread_mutex_lock(&fifo->head_mutex);
	if (fifo->num_elements+1 > fifo->max_elements) {
		pthread_mutex_unlock(&fifo->head_mutex);
		return -ENOBUFS;
	}
	fifo->num_elements++;
	pthread_mutex_unlock(&fifo->head_mutex);

	element = (struct fifo_element *) calloc(1, sizeof(struct fifo_element));
	if (! element)
		return -ENOMEM;

	element->data = (char *) malloc(sizeof(char) * size);
	if (! element->data) {
		free(element);
		return -ENOMEM;
	}

	memcpy(element->data, data, size);
	element->size = size;
	element->read_ptr = element->data;

	/* Need to get a lock, as it's possible that the list is empty by now. */
	pthread_mutex_lock(&fifo->head_mutex);
	list_add_tail(&element->list, &fifo->list);
	pthread_mutex_unlock(&fifo->head_mutex);

	return 0;
}

/**
 * get_head - Returns the element in the head of the FIFO
 *
 * @fifo: the FIFO
 *
 * Returns a pointer to the head, or NULL if the list is empty.
 */
static struct fifo_element *get_head(struct fifo *fifo)
{
	struct fifo_element *element = NULL;

	pthread_mutex_lock(&fifo->head_mutex);
	if (list_empty(&fifo->list))
		goto out_unlock;
	list_for_each_entry(element, &fifo->list, list)
		/* Get the head */
		break;
out_unlock:
	pthread_mutex_unlock(&fifo->head_mutex);

	return element;
}

