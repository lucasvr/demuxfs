#ifndef __buffer_h
#define __buffer_h

#define MAX_SECTION_SIZE 4096
#define MAX_PACKET_SIZE  0xffff

struct buffer {
	char *data;
	uint16_t pid;
	size_t max_size;
	size_t current_size;
	uint8_t continuity_counter;
	bool holds_pes_data;
	bool pes_unbounded_data;
};

struct buffer *buffer_create(uint16_t pid, size_t max_size, bool pes_data);
void buffer_destroy(struct buffer *buffer);
int  buffer_append(struct buffer *buffer, const char *buf, size_t size);
int  buffer_get_max_size(struct buffer *buffer);
int  buffer_get_current_size(struct buffer *buffer);
void buffer_reset_size(struct buffer *buffer);
bool buffer_is_unbounded(struct buffer *buffer);
bool buffer_contains_full_psi_section(struct buffer *buffer);
bool buffer_contains_full_pes_section(struct buffer *buffer);

#endif /* __buffer_h */
