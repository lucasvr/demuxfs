#ifndef __buffer_h
#define __buffer_h

struct buffer {
	char *data;
	size_t max_size;
	size_t current_size;
};

struct buffer *buffer_create(size_t max_size);
void buffer_destroy(struct buffer *buffer);
int buffer_append(struct buffer *buffer, const char *buf, size_t size);

#endif /* __buffer_h */
