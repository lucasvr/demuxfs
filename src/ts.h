#ifndef __ts_h
#define __ts_h

#define TS_WARNING(x...) do { dprintf(x) } while(0)
#define TS_ERROR(x...) do { dprintf(x) } while(0)

#define TS_SYNC_BYTE          0x47
#define TS_PACKET_SIZE        188
#define TS_MAX_SECTION_LENGTH 0x03FD
#define TS_LAST_TABLE_ID      0xBF

/* Known PIDs */
#define TS_PAT_PID    0x00
#define TS_CAT_PID    0x01
#define TS_NIT_PID    0x10
#define TS_SDT_PID    0x11
#define TS_BAT_PID    0x11
#define TS_EIT1_PID   0x12
#define TS_EIT2_PID   0x26
#define TS_EIT3_PID   0x27
#define TS_RST_PID    0x13
#define TS_TDT_PID    0x14
#define TS_DCT_PID    0x17
#define TS_DIT_PID    0x1E
#define TS_SIT_PID    0x1F
#define TS_PCAT_PID   0x22
#define TS_SDTT1_PID  0x23
#define TS_SDTT2_PID  0x28
#define TS_BIT_PID    0x24
#define TS_NBIT_PID   0x25
#define TS_LDT_PID    0x25
#define TS_CDT_PID    0x29
#define TS_NULL_PID   0x1FFF

/* The hash key generator to the private hash table */
#define TS_PACKET_HASH_KEY(ts_header,packet_header) \
	(((ts_header)->pid << 8) | ((struct psi_common_header*)(packet_header))->table_id)

struct ts_status {
    uint32_t packet_error_count;
};

/**
 * Transport stream header
 */
struct ts_header {
    uint8_t sync_byte;
    uint16_t transport_error_indicator:1;
    uint16_t payload_unit_start_indicator:1;
    uint16_t transport_priority:1;
    uint16_t pid:13;
    uint8_t transport_scrambling_control:2;
    uint8_t adaptation_field:2;
    uint8_t continuity_counter:4;
} __attribute__((__packed__));

struct adaptation_field {
    uint8_t length;
    uint8_t discontinuity_indicator:1;
} __attribute__((__packed__));

#define TS_SECTION_LENGTH(header) (((((char*)header)[1] << 8) | (((char*)header)[2])) & 0x0fff)
#define TS_PAYLOAD_LENGTH(header) (TS_SECTION_LENGTH(header)+sizeof(uint8_t)+sizeof(uint16_t))

/* Forward declaration */
struct psi_common_header;

typedef int (*parse_function_t)(const struct ts_header *header, const void *vpayload, uint8_t payload_len, 
		struct demuxfs_data *priv);

/**
 * Function prototypes
 */
int ts_parse_packet(const struct ts_header *header, const void *payload, struct demuxfs_data *priv);

/* Debug only */
void ts_dump_header(const struct ts_header *header);
void ts_dump_psi_header(struct psi_common_header *header);

#endif /* __ts_h */