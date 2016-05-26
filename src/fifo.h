#ifndef __fifo_h
#define __fifo_h

struct fifo;

/**
 * fifo_init - Initializes a new FIFO
 *
 * Returns a pointer to the newly allocated FIFO or NULL on error.
 */
struct fifo *fifo_init();

/**
 * fifo_destroy - Destroys a FIFO and all resources allocated by it.
 *
 * @fifo: the FIFO.
 */
void fifo_destroy(struct fifo *fifo);

/**
 * fifo_get_default_size - Returns the default size for when creating a new dentry.
 */
size_t fifo_get_default_size();

/**
 * fifo_get_type - Returns the default type which will be assigned to the dentry.
 */
int fifo_get_type();

/**
 * fifo_set_path - Configures the path to the FIFO in the real filesystem
 *
 * @fifo: the FIFO.
 * @path: NULL terminator path to the FIFO, starting from the filesystem's real 
 * root directory.
 *
 * Returns 0 on success or a negative value on error.
 */
int fifo_set_path(struct fifo *fifo, char *path);

/**
 * fifo_get_path - Get the path to the FIFO in the real filesystem
 *
 * @fifo: the FIFO.
 *
 * Returns a pointer to the path or NULL on error.
 */
const char *fifo_get_path(struct fifo *fifo);

/**
 * fifo_flush - Removes remaining elements stored in a FIFO
 *
 * @fifo: the FIFO.
 */
void fifo_flush(struct fifo *fifo);

/**
 * fifo_is_open - Tells if a FIFO is open
 * 
 * @fifo: the FIFO.
 *
 * Returns true if the FIFO is open or false if it's not.
 */
bool fifo_is_open(struct fifo *fifo);

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
