/* 
 * Copyright (c) 2008,2009 Lucas C. Villa Real <lucasvr@gobolinux.org>
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
#include "linuxdvb.h"
#include "fsutils.h"
#include "byteops.h"
#include "backend.h"
#include "buffer.h"
#include "ts.h"

#define LINUXDVB_DEFAULT_FRONTEND_DEVICE  "/dev/dvb/adapter0/frontend0"
#define LINUXDVB_DEFAULT_DEMUX_DEVICE     "/dev/dvb/adapter0/demux0"
#define LINUXDVB_DEFAULT_DVR_DEVICE       "/dev/dvb/adapter0/dvr0"
#define LINUXDVB_DEFAULT_FREQUENCY        0
#define LINUXDVB_DEFAULT_SYMBOL_RATE      0
#define LINUXDVB_DEFAULT_QPSK_VOLTAGE     13
#define LINUXDVB_DEFAULT_QPSK_TONE        0

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,0,0)
#undef  DVB_API_VERSION
#define DVB_API_VERSION 5
#endif

struct input_parser {
	char *frontend_device;
	char *demux_device;
	char *dvr_device;
	uint32_t frequency;
	int qpsk_voltage;
	int symbol_rate;
	int qpsk_tone;
	int frontend_fd;
	int demux_fd;
	int dvr_fd;
	char *packet;
	bool packet_valid;
	uint8_t packet_size;
};

static int linuxdvb_set_frequency_v3(uint32_t frequency, struct demuxfs_data *priv);
static int linuxdvb_set_frequency_v5(uint32_t frequency, struct demuxfs_data *priv);

/**
 * Command line parsing routines.
 */
void linuxdvb_usage(void)
{
	fprintf(stderr, "\nLINUXDVB options:\n"
			"    -o frontend_device=FILE  frontend device (default=%s)\n"
			"    -o demux_device=FILE     demux device (default=%s)\n"
			"    -o dvr_device=FILE       DVR device (default=%s)\n"
			"    -o frequency=FREQ        Frequency (default=%d)\n"
			"    -o symbol_rate=RATE      Symbol rate (default=%d)\n"
			"    -o qpsk_voltage=<13|18>  QPSK voltage (default=%d)\n"
			"    -o qpsk_tone=<1|0>       QPSK tone (default=%d)\n",
			LINUXDVB_DEFAULT_FRONTEND_DEVICE,
			LINUXDVB_DEFAULT_DEMUX_DEVICE,
			LINUXDVB_DEFAULT_DVR_DEVICE,
			LINUXDVB_DEFAULT_FREQUENCY,
			LINUXDVB_DEFAULT_SYMBOL_RATE,
			LINUXDVB_DEFAULT_QPSK_VOLTAGE,
			LINUXDVB_DEFAULT_QPSK_TONE
			);
}

#define LINUXDVB_OPT(templ,offset,value) { templ, offsetof(struct input_parser, offset), value }

static struct fuse_opt linuxdvb_opts[] = {
	LINUXDVB_OPT("frontend_device=%s", frontend_device, 0),
	LINUXDVB_OPT("demux_device=%s", demux_device, 0),
	LINUXDVB_OPT("dvr_device=%s", dvr_device, 0),
	LINUXDVB_OPT("frequency=%d", frequency, 0),
	LINUXDVB_OPT("symbol_rate=%d", symbol_rate, 0),
	LINUXDVB_OPT("qpsk_voltage=%d", qpsk_voltage, 0),
	LINUXDVB_OPT("qpsk_tone=%d", qpsk_tone, 0),
	FUSE_OPT_END
};

static int linuxdvb_parse_opts(void *priv, const char *arg, int key, struct fuse_args *outargs)
{
	return 1;
}

#define TEST_AND_STRINGIFY(type) case type: return #type

static const char *linuxdvb_delivery_type(int type) {
	switch (type) {
		TEST_AND_STRINGIFY(SYS_ATSC);
		TEST_AND_STRINGIFY(SYS_DVBC_ANNEX_A);
		TEST_AND_STRINGIFY(SYS_DVBC_ANNEX_B);
		TEST_AND_STRINGIFY(SYS_DVBC_ANNEX_C);
		TEST_AND_STRINGIFY(SYS_DVBH);
		TEST_AND_STRINGIFY(SYS_DVBS);
		TEST_AND_STRINGIFY(SYS_DVBS2);
		TEST_AND_STRINGIFY(SYS_DVBT);
		TEST_AND_STRINGIFY(SYS_DVBT2);
		TEST_AND_STRINGIFY(SYS_DSS);
		TEST_AND_STRINGIFY(SYS_ISDBT);
		TEST_AND_STRINGIFY(SYS_TURBO);
		default: return "UNKNOWN";
	}
}

static void linuxdvb_get_frontend_status(int status, char *buf, size_t size)
{
	char *ptr = buf;
	size_t count, n = size-1;

	memset(buf, 0, size);
	if (status & FE_HAS_SIGNAL) {
		count = snprintf(ptr, n, "HAS_SIGNAL,");
		ptr += count, n -= count;
	}
	if (status & FE_HAS_CARRIER) {
		count = snprintf(ptr, n, "HAS_CARRIER,");
		ptr += count, n -= count;
	if (status & FE_HAS_VITERBI)
		count = snprintf(ptr, n, "HAS_VITERBI,");
		ptr += count, n -= count;
	}
	if (status & FE_HAS_SYNC) {
		count = snprintf(ptr, n, "HAS_SYNC,");
		ptr += count, n -= count;
	}
	if (status & FE_HAS_LOCK) {
		count = snprintf(ptr, n, "HAS_LOCK,");
		ptr += count, n -= count;
	}
	if (status & FE_TIMEDOUT) {
		count = snprintf(ptr, n, "TIMEDOUT,");
		ptr += count, n -= count;
	}
	if (status & FE_REINIT) {
		count = snprintf(ptr, n, "REINIT,");
		ptr += count, n -= count;
	}
	if (ptr == buf)
		snprintf(ptr, n, "UNKNOWN");
	else
		ptr[strlen(ptr)-1] = '\0';
}

/**
 * linuxdvb_create_parser: backend's create() method.
 */
int linuxdvb_create_parser(struct fuse_args *args, struct demuxfs_data *priv)
{
	struct input_parser *p = calloc(1, sizeof(struct input_parser));
	assert(p);

	int ret = fuse_opt_parse(args, p, linuxdvb_opts, linuxdvb_parse_opts);
	if (ret < 0) {
		free(p);
		return -1;
	}

	/* Validate options */
	if (! p->dvr_device)
		p->dvr_device = LINUXDVB_DEFAULT_DVR_DEVICE;

	if (! p->demux_device)
		p->demux_device = LINUXDVB_DEFAULT_DEMUX_DEVICE;

	if (! p->frontend_device)
		p->frontend_device = LINUXDVB_DEFAULT_FRONTEND_DEVICE;

	if (! p->symbol_rate)
		p->symbol_rate = LINUXDVB_DEFAULT_SYMBOL_RATE;

	if (! p->qpsk_voltage)
		p->qpsk_voltage = LINUXDVB_DEFAULT_QPSK_VOLTAGE;

	if (! p->qpsk_tone)
		p->qpsk_tone = LINUXDVB_DEFAULT_QPSK_TONE;

	if (p->qpsk_voltage != 13 && p->qpsk_voltage != 18) {
		fprintf(stderr, "Invalid value '%d' for qpsk_voltage\n", p->qpsk_voltage);
		ret = -EINVAL;
		goto out_free;
	}

	if (p->qpsk_tone != 0 && p->qpsk_tone != 1) {
		fprintf(stderr, "Invalid value '%d' for qpsk_tone\n", p->qpsk_tone);
		ret = -EINVAL;
		goto out_free;
	}

	if (p->frequency < 1000000)
		p->frequency *=1000;

	/* Open the frontend device if a non-zero frequency has been set */
	if (p->frequency) {
		p->frontend_fd = open(p->frontend_device, O_RDWR);
		if (p->frontend_fd < 0) {
			perror(p->frontend_device);
			ret = -errno;
			goto out_free;
		}
	} else
		p->frontend_fd = -1;

	/* Configure the DVR device to read raw TS packets */
	p->dvr_fd = open(p->dvr_device, O_RDONLY);
	if (p->dvr_fd < 0) {
		perror(p->dvr_device);
		ret = -errno;
		goto out_free;
	}

	/* Configure the Demux device to route packets from the frontend to the DVR */
	p->demux_fd = open(p->demux_device, O_RDWR|O_NONBLOCK);
	if (p->demux_fd < 0) {
		perror(p->demux_device);
		ret = -errno;
		goto out_free;
	}

	struct dmx_pes_filter_params pes_filter;
	memset(&pes_filter, 0, sizeof(pes_filter));
	pes_filter.pid      = 0x2000;
	pes_filter.input    = DMX_IN_FRONTEND;
	pes_filter.output   = DMX_OUT_TS_TAP;
	pes_filter.pes_type = DMX_PES_OTHER;
	pes_filter.flags    = DMX_IMMEDIATE_START;

	ret = ioctl(p->demux_fd, DMX_SET_PES_FILTER, &pes_filter);
	if (ret < 0) {
		perror("DMX_SET_PES_FILTER");
		goto out_free;
	}
	
	/* Configure the packet size */
	p->packet_size = 188;
	p->packet = (char *) malloc(p->packet_size);

	/* Propagate user options back to the caller */
	priv->options.packet_size = p->packet_size;
	priv->options.packet_error_correction_bytes = p->packet_size - 188;
	priv->options.frequency = p->frequency;

	priv->parser = p;
	return 0;

out_free:
	if (p) {
		if (p->frontend_fd >= 0)
			close(p->frontend_fd);
		if (p->demux_fd >= 0)
			close(p->demux_fd);
		if (p->dvr_fd >= 0)
			close(p->dvr_fd);
		free(p);
	}
	return ret;
}

/**
 * linuxdvb_destroy_parser: backend's destroy() method.
 */
int linuxdvb_destroy_parser(struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	int ret;

	ret = ioctl(p->demux_fd, DMX_STOP);
	if (ret < 0)
		perror("DMX_STOP");

	close(p->demux_fd);
	close(p->dvr_fd);
	free(p->packet);
	free(p);
	return 0;
}

#define DECLARE_PROPERTY(varname,command) \
	struct dtv_properties varname; \
	struct dtv_property varname ## _ ## command; \
	memset(&varname, 0, sizeof(varname)); \
	memset(&varname ## _ ## command, 0, sizeof(varname ## _ ## command)); \
	varname.num = 1; \
	varname.props = &varname ## _ ## command; \
	varname.props[0].cmd = command

#define DECLARE_PROPERTIES(varname,n) \
	int varname ## _count = n; \
	struct dtv_properties varname; \
	struct dtv_property varname ## _props[varname ## _count]; \
	memset(&varname, 0, sizeof(varname)); \
	memset(varname ## _props, 0, sizeof(varname ## _props)); \
	varname.num = varname ## _count; \
	varname.props = varname ## _props

#define PUSH_PROPERTY(varname,idx,command,value) do { \
	int varname ## _count = idx; \
	varname.props[varname ## _count].cmd = command; \
	varname.props[varname ## _count].u.data = value; \
} while(0)

#define UPDATE_PROPERTY(varname,command,value) do { \
	for (int p=0; p<varname.num; ++p) { \
		if (varname.props[p].cmd == command) { \
			varname.props[p].u.data = value; \
			break; \
		} \
	} \
} while(0)

/**
 * linuxdvb_set_frequency: backend's set_frequency() method.
 */
int linuxdvb_set_frequency(uint32_t frequency, struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	int ret;

	if (p->frontend_fd < 0)
		return 0;

	DECLARE_PROPERTY(properties, DTV_API_VERSION);
	ret = ioctl(p->frontend_fd, FE_GET_PROPERTY, &properties);
	if (ret == 0 && (properties.props[0].u.data & 0x500) == 0x500) {
		fprintf(stderr, "APIv5\n");
		return linuxdvb_set_frequency_v5(frequency, priv);
	} else {
		fprintf(stderr, "APIv3\n");
		return linuxdvb_set_frequency_v3(frequency, priv);
	}
}

static int linuxdvb_set_frequency_v3(uint32_t frequency, struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	struct dvb_frontend_parameters params;
	struct dvb_frontend_event event;
	struct dvb_frontend_info info;
	fe_sec_voltage_t voltage;
	fe_sec_tone_mode_t tone;
	struct pollfd pollfd;
	int ret;

	memset(&info, 0, sizeof(info));
	ret = ioctl(p->frontend_fd, FE_GET_INFO, &info);
	if (ret < 0) {
		perror("FE_GET_INFO");
		return -errno;
	}

	params.frequency = p->frequency;
	params.inversion = INVERSION_AUTO;

	switch (info.type) {
		case FE_OFDM:
			params.u.ofdm.bandwidth = BANDWIDTH_AUTO;
			params.u.ofdm.code_rate_HP = FEC_AUTO;
			params.u.ofdm.code_rate_LP = FEC_AUTO;
			params.u.ofdm.constellation = QAM_AUTO;
			params.u.ofdm.transmission_mode = TRANSMISSION_MODE_AUTO;
			params.u.ofdm.guard_interval = GUARD_INTERVAL_AUTO;
			params.u.ofdm.hierarchy_information = HIERARCHY_AUTO;
			break;
		case FE_QPSK:
			params.u.qpsk.symbol_rate = p->symbol_rate;
			params.u.qpsk.fec_inner = FEC_AUTO;
			voltage = p->qpsk_voltage == 13 ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
			tone = p->qpsk_tone == 0 ? SEC_TONE_OFF : SEC_TONE_ON;

			ret = ioctl(p->frontend_fd, FE_SET_VOLTAGE, voltage);
			if (ret < 0) {
				perror("FE_SET_VOLTAGE");
				return -errno;
			}
			ret = ioctl(p->frontend_fd, FE_SET_TONE, tone);
			if (ret < 0) {
				perror("FE_SET_TONE");
				return -errno;
			}
			break;
		case FE_QAM:
			params.inversion = INVERSION_OFF;
			params.u.qam.symbol_rate = p->symbol_rate;
			params.u.qam.fec_inner = FEC_AUTO;
			params.u.qam.modulation = QAM_AUTO;
			break;
		case FE_ATSC:
			/* Not tested */
			break;
		default:
			fprintf(stderr, "Unknown frontend type '%#x'\n", info.type);
			return -ECANCELED;
	}

	fprintf(stdout, "Setting frequency to %d...\n", p->frequency);
	ret = ioctl(p->frontend_fd, FE_SET_FRONTEND, &params);
	if (ret < 0) {
		perror("FE_SET_FRONTEND");
		return -errno;
	}

	pollfd.fd = p->frontend_fd;
	pollfd.events = POLLIN;

	event.status = 0;
	while (! (event.status & FE_TIMEDOUT) && ! (event.status & FE_HAS_LOCK)) {
		ret = poll(&pollfd, 1, 10000);
		if (ret < 0) {
			perror("poll");
			return -errno;
		}
		if (pollfd.revents & POLLIN) {
			ret = ioctl(p->frontend_fd, FE_GET_EVENT, &event);
			if (ret < 0 && errno == EOVERFLOW)
				continue;
			else if (ret < 0) {
				perror("FE_GET_EVENT");
				return -errno;
			}
		}
		fprintf(stdout, "Waiting to get a lock on the frontend...\n");
	}

	if (event.status & FE_HAS_LOCK) {
		fprintf(stderr, "Tuner successfully set to frequency %d\n", p->frequency);
		return 0;
	} else {
		fprintf(stderr, "Timed out tuning to frequency %d\n", p->frequency);
		return -ETIMEDOUT;
	}
}

static int linuxdvb_set_frequency_v5(uint32_t frequency, struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	fe_sec_voltage_t voltage;
	fe_sec_tone_mode_t tone;
	int ret;
	
	DECLARE_PROPERTY(curr_prop, DTV_DELIVERY_SYSTEM);
	ret = ioctl(p->frontend_fd, FE_GET_PROPERTY, &curr_prop);
	if (ret < 0) {
		perror("FE_GET_PROPERTY");
		return -errno;
	}
	int delivery = curr_prop.props[0].u.data;

	int idx = 0;
	DECLARE_PROPERTIES(new_prop, 64);
	PUSH_PROPERTY(new_prop, idx++, DTV_CLEAR,             0);
	PUSH_PROPERTY(new_prop, idx++, DTV_DELIVERY_SYSTEM,   delivery);
	PUSH_PROPERTY(new_prop, idx++, DTV_FREQUENCY,         p->frequency);
	PUSH_PROPERTY(new_prop, idx++, DTV_BANDWIDTH_HZ,      BANDWIDTH_AUTO);
	PUSH_PROPERTY(new_prop, idx++, DTV_INVERSION,         INVERSION_AUTO);
	PUSH_PROPERTY(new_prop, idx++, DTV_GUARD_INTERVAL,    GUARD_INTERVAL_AUTO);
	PUSH_PROPERTY(new_prop, idx++, DTV_TRANSMISSION_MODE, TRANSMISSION_MODE_AUTO);

	switch (delivery) {
		// OFDM delivery type
		case SYS_ISDBT:
			// Supported transmission modes: 8k, 4k, 2k.
			// Supported bandwidths: 6MHz
			UPDATE_PROPERTY(new_prop,      DTV_BANDWIDTH_HZ,                   6000000);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_SOUND_BROADCASTING,       0);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_PARTIAL_RECEPTION,        0);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYER_ENABLED,            7); // 0x1:LayerA, 0x2:LayerB, 0x4:LayerC
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_SB_SUBCHANNEL_ID,         0); // applies if SOUND_BROADCASTING=1
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_SB_SEGMENT_IDX,           0); // applies if SOUND_BROADCASTING=1
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_SB_SEGMENT_COUNT,         0); // applies if SOUND_BROADCASTING=1
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERA_FEC,               FEC_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERA_MODULATION,        QAM_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERA_SEGMENT_COUNT,     0);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERA_TIME_INTERLEAVING, 0);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERB_FEC,               FEC_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERB_MODULATION,        QAM_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERB_SEGMENT_COUNT,     0);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERB_TIME_INTERLEAVING, 0);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERC_FEC,               FEC_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERC_MODULATION,        QAM_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERC_SEGMENT_COUNT,     0);
			PUSH_PROPERTY(new_prop, idx++, DTV_ISDBT_LAYERC_TIME_INTERLEAVING, 0);
			break;
		case SYS_DVBT:
		case SYS_DVBT2:
		case SYS_DVBH:
			PUSH_PROPERTY(new_prop, idx++, DTV_MODULATION,   QAM_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_SYMBOL_RATE,  p->symbol_rate);
			PUSH_PROPERTY(new_prop, idx++, DTV_HIERARCHY,    HIERARCHY_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_ROLLOFF,      ROLLOFF_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_PILOT,        PILOT_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_INNER_FEC,    FEC_AUTO);
			break;

		// QPSK delivery type
		case SYS_DSS:
		case SYS_DVBS:
		case SYS_DVBS2:
		case SYS_TURBO:
			voltage = p->qpsk_voltage == 13 ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18;
			tone = p->qpsk_tone == 0 ? SEC_TONE_OFF : SEC_TONE_ON;
			PUSH_PROPERTY(new_prop, idx++, DTV_VOLTAGE,      voltage);
			PUSH_PROPERTY(new_prop, idx++, DTV_TONE,         tone);
			PUSH_PROPERTY(new_prop, idx++, DTV_MODULATION,   QAM_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_SYMBOL_RATE,  p->symbol_rate);
			PUSH_PROPERTY(new_prop, idx++, DTV_HIERARCHY,    HIERARCHY_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_ROLLOFF,      ROLLOFF_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_PILOT,        PILOT_AUTO);
			PUSH_PROPERTY(new_prop, idx++, DTV_INNER_FEC,    FEC_AUTO);
			break;

		// QAM delivery type
		case SYS_DVBC_ANNEX_A:
		case SYS_DVBC_ANNEX_C:
			UPDATE_PROPERTY(new_prop,      DTV_INVERSION,    INVERSION_OFF);
			break;
		// ATSC delivery type
		case SYS_ATSC:
		case SYS_DVBC_ANNEX_B:
			/* Not tested */
			break;
		default:
			fprintf(stderr, "Unknown frontend type '%#x'\n", delivery);
			return -ECANCELED;
	}

	PUSH_PROPERTY(new_prop, idx++, DTV_TUNE, 0);

	fprintf(stdout, "%s: setting frequency to %d...\n", linuxdvb_delivery_type(delivery), p->frequency);
	new_prop.num = idx;
	ret = ioctl(p->frontend_fd, FE_SET_PROPERTY, &new_prop);
	if (ret < 0) {
		perror("FE_SET_PROPERTY");
		return -errno;
	}

	fe_status_t status;
	char fe_status[256]; 
	while (true) {
		ret = ioctl(p->frontend_fd, FE_READ_STATUS, &status);
		if (ret < 0) {
			perror("FE_READ_STATUS");
			return -errno;
		}
		if (status & FE_HAS_LOCK) {
			fprintf(stderr, "Tuner successfully set to frequency %d\n", p->frequency);
			return 0;
		}
		linuxdvb_get_frontend_status(status, fe_status, sizeof(fe_status));
		fprintf(stdout, "Waiting to get a lock on the frontend... (status=%s)\n", fe_status);
		usleep(1000000);
	}

	fprintf(stderr, "Timed out tuning to frequency %d\n", p->frequency);
	return -ETIMEDOUT;
}

/**
 * linuxdvb_read_parser: backend's read() method.
 */
int linuxdvb_read_packet(struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	ssize_t n = read(p->dvr_fd, p->packet, p->packet_size);
	if (n <= 0) {
		p->packet_valid = false;
		return 0;
	}
	p->packet_valid = true;
	return 0;
}

/**
 * linuxdvb_process_parser: backend's process() method.
 */
int linuxdvb_process_packet(struct ts_header *header, void **payload, struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	if (! p->packet_valid)
		return -EINVAL;
	else if (! header || ! payload)
		return -EINVAL;

	*payload = (void *) &p->packet[4];

	header->sync_byte                    =  p->packet[0];
	header->transport_error_indicator    = (p->packet[1] >> 7) & 0x01;
	header->payload_unit_start_indicator = (p->packet[1] >> 6) & 0x01;
	header->transport_priority           = (p->packet[1] >> 5) & 0x01;
	header->pid                          = CONVERT_TO_16(p->packet[1], p->packet[2]) & 0x1fff;
	header->transport_scrambling_control = (p->packet[3] >> 6) & 0x03;
	header->adaptation_field             = (p->packet[3] >> 4) & 0x03;
	header->continuity_counter           = (p->packet[3]) & 0x0f;

	return 0;
}

/**
 * linuxdvb_keep_alive: backend's keep_alive() method.
 */
bool linuxdvb_keep_alive(struct demuxfs_data *priv)
{
	return true;
}

struct backend_ops linuxdvb_backend_ops = {
	.create        = linuxdvb_create_parser,
	.destroy       = linuxdvb_destroy_parser,
	.set_frequency = linuxdvb_set_frequency,
	.read          = linuxdvb_read_packet,
	.process       = linuxdvb_process_packet,
	.keep_alive    = linuxdvb_keep_alive,
	.usage         = linuxdvb_usage,
};

struct backend_ops *backend_get_ops(void)
{
	return &linuxdvb_backend_ops;
}
