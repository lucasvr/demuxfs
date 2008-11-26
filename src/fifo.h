#ifndef __fifo_h
#define __fifo_h

struct fifo {
	struct list_head list;
	uint32_t num_elements;
	uint32_t max_elements;
	pthread_mutex_t head_mutex;
};

struct fifo_element {
	struct list_head list;
	char *data;
	char *read_ptr;
	uint32_t size;
};

/**
 * fifo_init - Initializes a new FIFO
 *
 * @max_elements: how many elements to keep in the FIFO at most
 *
 * Returns a pointer to the newly allocated FIFO or NULL on error.
 */
struct fifo *fifo_init(uint32_t max_elements);

/**
 * fifo_destroy - Destroys a FIFO and all resources allocated by it.
 *
 * @fifo: the FIFO.
 */
void fifo_destroy(struct fifo *fifo);

/**
 * fifo_empty - Tells if a FIFO is empty
 *
 * @fifo: the FIFO.
 *
 * Returns true if the FIFO is empty or false if it's not.
 */
bool fifo_empty(struct fifo *fifo);

/**
 * fifo_read - Consumes data from a FIFO
 *
 * @fifo: the FIFO we want to read from
 * @buf: buffer where the FIFO data will be copied to
 * @size: @buf size
 *
 * Returns the number of data read, or 0 if no data is
 * available. The caller is responsible from calling this
 * function as many times as required to fulfill its buffer.
 */
size_t fifo_read(struct fifo *fifo, char *buf, size_t size);

/**
 * fifo_append - appends data to the FIFO.
 *
 * @fifo: the FIFO which will receive the data.
 * @data: data that's being appended to the FIFO.
 * @size: @data length.
 *
 * Returns 0 on success or a negative value on error.
 */
int fifo_append(struct fifo *fifo, const char *data, uint32_t size);

#endif /* __fifo_h */
