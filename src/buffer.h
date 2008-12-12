#ifndef __buffer_h
#define __buffer_h

#define BUFFER_MAX_SIZE 4096

struct buffer {
	char *data;
	size_t max_size;
	size_t current_size;
};

struct buffer *buffer_create(size_t max_size);
void buffer_destroy(struct buffer *buffer);
int buffer_append(struct buffer *buffer, const char *buf, size_t size, bool pes);
void buffer_reset_size(struct buffer *buffer);
bool buffer_contains_full_psi_section(struct buffer *buffer);
bool buffer_contains_full_pes_section(struct buffer *buffer);

#endif /* __buffer_h */
