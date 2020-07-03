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
#include "byteops.h"
#include "fsutils.h"
#include "xattr.h"
#include "hash.h"
#include "fifo.h"
#include "list.h"
#include "ts.h"
#include "iop.h"
#include "biop.h"
#include "tables/psi.h"
#include "dsm-cc/dsmcc.h"
#include "dsm-cc/dsi.h"
#include "dsm-cc/descriptors/descriptors.h"

void dsi_free(struct dsi_table *dsi)
{
	/* Free DSM-CC compatibility descriptors */
	dsmcc_free_compatibility_descriptors(&dsi->compatibility_descriptor);

	/* Free DSM-CC message header */
	dsmcc_free_message_header(&dsi->dsmcc_message_header);
	if (dsi->group_info_indication) {
		if (dsi->group_info_indication->dsi_group_info)
			free(dsi->group_info_indication->dsi_group_info);
		free(dsi->group_info_indication);
	}
	if (dsi->service_gateway_info) {
		iop_free_ior(dsi->service_gateway_info->iop_ior);
		free(dsi->service_gateway_info);
	}

	/* Free Private data */
//	if (dsi->private_data_bytes)
//		free(dsi->private_data_bytes);

	/* Free the dentry and its subtree */
	if (dsi->dentry && dsi->dentry->name)
		fsutils_dispose_tree(dsi->dentry);
	else if (dsi->dentry)
		/* Dentry has simply been calloc'ed */
		free(dsi->dentry);

	/* Free the dsi table structure */
	free(dsi);
}

static void dsi_create_directory(const struct ts_header *header, struct dsi_table *dsi, 
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "DSI" at the root filesystem if it doesn't exist yet */
	struct dentry *dsi_dir = CREATE_DIRECTORY(priv->root, FS_DSI_NAME);

	/* Create a directory named "<dsi_pid>" and populate it with files */
	asprintf(&dsi->dentry->name, "%#04x", header->pid);
	dsi->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(dsi_dir, dsi->dentry);

	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(dsi->dentry, dsi->version_number);

	psi_populate((void **) &dsi, *version_dentry);
}

static void dsi_create_dentries(struct dentry *parent, struct dsi_table *dsi, struct demuxfs_data *priv)
{
	struct dsmcc_message_header *msg_header = &dsi->dsmcc_message_header;
	struct dsmcc_compatibility_descriptor *cd = &dsi->compatibility_descriptor;

	dsmcc_create_message_header_dentries(msg_header, parent);
	if (dsi->private_data_length)
		dsmcc_create_compatibility_descriptor_dentries(cd, parent);

	CREATE_FILE_NUMBER(parent, dsi, private_data_length);
//	CREATE_FILE_BIN(parent, dsi, private_data_bytes, dsi->private_data_length);
}

void dsi_create_dii_symlink(const struct ts_header *header, struct dsi_table *dsi, 
		struct demuxfs_data *priv)
{
	struct iop_ior *ior = dsi && dsi->service_gateway_info ? dsi->service_gateway_info->iop_ior : NULL;
	struct biop_profile_body *pb = ior && ior->tagged_profiles ? ior->tagged_profiles->profile_body : NULL;
	struct dsmcc_tap *tap = pb && pb->connbinder.tap_count ? &pb->connbinder.taps[0] : NULL;
	char search_dir[PATH_MAX], target[PATH_MAX], subdir[PATH_MAX];

	if (tap && tap->message_selector) {
		struct dentry *dii_pid_dentry, *dii_dentry, *dsi_dentry, *transaction_dentry;
		uint32_t dii_transaction_id, dsi_transaction_id;

		snprintf(search_dir, sizeof(search_dir), "/%s/%#04x", FS_DII_NAME, header->pid);
		dii_pid_dentry = fsutils_get_dentry(priv->root, search_dir);
		if (! dii_pid_dentry) {
			/* XXX Possibly we didn't scan the DII yet */
			return;
		}

		dii_dentry = fsutils_get_current(dii_pid_dentry);
		assert(dii_dentry);

		dsi_dentry = fsutils_get_current(dsi->dentry);
		assert(dsi_dentry);

		snprintf(subdir, sizeof(subdir), "/%s/transaction_id", FS_DSMCC_MESSAGE_HEADER_DIRNAME);
		transaction_dentry = fsutils_get_dentry(dii_dentry, subdir);
		assert(transaction_dentry);

		dii_transaction_id = strtoul(transaction_dentry->contents, NULL, 16);
		dsi_transaction_id = tap->message_selector ? tap->message_selector->transaction_id : 0;
		if (dii_transaction_id != dsi_transaction_id) {
			TS_WARNING("dii_transaction_id %#x != dsi_transaction_id %#x", 
				dii_transaction_id, dsi_transaction_id);
			CREATE_SYMLINK(dsi_dentry, "DII", FS_BROKEN_SYMLINK_NAME);
		} else {
			snprintf(target, sizeof(target), "../../../%s/%#04x/%s", FS_DII_NAME, 
					header->pid, FS_CURRENT_NAME);
			CREATE_SYMLINK(dsi_dentry, "DII", target);
		}
		dsi->_linked_to_dii = true;
	}
}

int dsi_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct dsi_table *dsi, *current_dsi = NULL;
	
	if (payload_len < 20) {
		dprintf("payload is too small (%d)", payload_len);
		return 0;
	}
	
	dsi = (struct dsi_table *) calloc(1, sizeof(struct dsi_table));
	assert(dsi);
	dsi->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(dsi->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) dsi, payload, payload_len);
	if (ret < 0) {
		dprintf("error parsing PSI packet");
		dsi_free(dsi);
		return 0;
	}

	/** DSM-CC Message Header */
	struct dsmcc_message_header *msg_header = &dsi->dsmcc_message_header;
	int len = dsmcc_parse_message_header(msg_header, &payload[8]);
	int j = 8 + len;
	
	/** 
	 * At this point we know for sure that this is a DSI table.
	 *
	 * However, since the dentry inode is generated based on the PID and
	 * table_id, that will certainly match with any DII tables previously
	 * added to the hash table, as the DSI can arrive in the same PID.
	 *
	 * For that reason we need to modify the inode number, and we
	 * do that by setting the 1st bit from the 7th byte, which is not
	 * used by the TS_PACKET_HASH_KEY macro.
	 * */ 
	dsi->dentry->inode = TS_PACKET_HASH_KEY(header, dsi) | 0x1000000;

	current_dsi = hashtable_get(priv->psi_tables, dsi->dentry->inode);
	if (! dsi->current_next_indicator || (current_dsi && current_dsi->version_number == dsi->version_number)) {
		dsi_free(dsi);
		if (current_dsi && ! current_dsi->_linked_to_dii)
			dsi_create_dii_symlink(header, current_dsi, priv);
		return 0;
	}

	TS_INFO("DSI parser: pid=%#x, table_id=%#x, dsi->version_number=%#x, transaction_nr=%#x", 
			header->pid, dsi->table_id, dsi->version_number, msg_header->transaction_id & ~0x80000000);

	/* Create filesystem entries for this table */
	struct dentry *version_dentry = NULL;
	dsi_create_directory(header, dsi, &version_dentry, priv);
	dsi_create_dentries(version_dentry, dsi, priv);

	/** Parse DSI bits */

	/* server_id must contain 20 entries filled up with 0xff */
	memcpy(dsi->server_id, &payload[j], 20);
	CREATE_FILE_BIN(version_dentry, dsi, server_id, 20);
	j += 20;

	/** DSM-CC Compatibility Descriptor. There must be no entries in this loop. */
	struct dsmcc_compatibility_descriptor *cd = &dsi->compatibility_descriptor;
	cd->compatibility_descriptor_length = CONVERT_TO_16(payload[j], payload[j+1]);
	if (cd->compatibility_descriptor_length)
		TS_WARNING("DSM-CC compatibility descriptor has length != 0 (%d) (payload[%d],[%d] = %d,%d), payload_len=%d",
			cd->compatibility_descriptor_length, j, j+1, payload[j], payload[j+1], payload_len);
	CREATE_FILE_NUMBER(version_dentry, cd, compatibility_descriptor_length);

	/* Private data (Group Info Indication) */
	dsi->private_data_length = CONVERT_TO_16(payload[j+2], payload[j+3]);
	if ((payload_len-j-8) != dsi->private_data_length)
		TS_WARNING("DSI private_data_length=%d, end of data=%d", 
			dsi->private_data_length, payload_len-j-8);
	CREATE_FILE_NUMBER(version_dentry, dsi, private_data_length);

	/* Force data length to be within the payload boundaries */
	dsi->private_data_length = payload_len-j-8;
	j += 4;

	/* 
	 * The private data can hold two different data sets, depending on
	 * which carousel we are parsing.
	 * 
	 * For Data Carousels it holds a GroupInfoByte structure followed by
	 * DSM-CC Carousel Descriptors.
	 * For Object Carousels it holds a BIOP::ServiceGatewayInformation.
	 * 
	 * We use the same trick employed by DVBSnoop here: we look for a
	 * BIOP U-U object type_id "srg\0" and "DSM:" strings to know how
	 * to parse the private data.
	 */

	/* Prefetch only */
	uint32_t idx = j + 4;
	uint32_t type_id = CONVERT_TO_32(payload[idx], payload[idx+1], payload[idx+2], payload[idx+3]);

	if (type_id != 0x73726700 && type_id != 0x53443a4d) {
		/* Data Carousel: GroupInfoByte + DSM-CC CarouselDescriptors */
		struct dentry *gii_dentry = CREATE_DIRECTORY(version_dentry, FS_DSMCC_GROUP_INFO_INDICATION_DIRNAME);
		struct dsi_group_info_indication *gii;

		dsi->group_info_indication = calloc(1, sizeof(struct dsi_group_info_indication));
		gii = dsi->group_info_indication;
		gii->number_of_groups = CONVERT_TO_16(payload[j], payload[j+1]);
		CREATE_FILE_NUMBER(gii_dentry, gii, number_of_groups);
		j += 2;

		if (gii->number_of_groups) {
			uint16_t i;
			gii->dsi_group_info = calloc(gii->number_of_groups, sizeof(struct dsi_group_info));
			for (i=0; i<gii->number_of_groups; ++i) {
				struct dentry *group_dentry = CREATE_DIRECTORY(gii_dentry, "GroupInfo_%02d", i+1);
				struct dsi_group_info *group_info = &gii->dsi_group_info[i];
				int len;
				TS_INFO("parsing group %d/%d", i+1, gii->number_of_groups);
				group_info->group_id = CONVERT_TO_32(payload[j], payload[j+1],
						payload[j+2], payload[j+3]);
				group_info->group_size = CONVERT_TO_32(payload[j+4], payload[j+5],
						payload[j+6], payload[j+7]);
				CREATE_FILE_NUMBER(group_dentry, group_info, group_id);
				CREATE_FILE_NUMBER(group_dentry, group_info, group_size);

				// GroupCompatibility()
				len = dsmcc_parse_compatibility_descriptors(&group_info->group_compatibility,
						&payload[j+8]);
				TS_INFO("parsing group %d/%d: id=%d, size=%d, len=%d", i+1, gii->number_of_groups, group_info->group_id, group_info->group_size, len);
				dsmcc_create_compatibility_descriptor_dentries(&group_info->group_compatibility,
						group_dentry);
				j += 8 + len;
			}
		}

		gii->private_data_length = CONVERT_TO_16(payload[j], payload[j+1]);
		dprintf("private_data_length=%d", gii->private_data_length);

		// XXX: CarouselDescriptors
	
	} else {
		/* Object Carousel: BIOP::ServiceGatewayInformation */
		struct dentry *sgi_dentry = CREATE_DIRECTORY(version_dentry, FS_BIOP_SERVICE_GATEWAY_INFORMATION_DIRNAME);
		struct iop_ior *ior = calloc(1, sizeof(struct iop_ior));

		/* Parse IOP::IOR() */
		dsi->service_gateway_info = calloc(1, sizeof(struct dsi_service_gateway_info));
		dsi->service_gateway_info->iop_ior = ior;
		j += iop_parse_ior(ior, &payload[j], payload_len-j);
		iop_create_ior_dentries(sgi_dentry, ior);

		if (ior->tagged_profiles_count) {
			/* Check if DSI transaction_id matches DII transaction_id and create a symlink accordingly */
			dsi_create_dii_symlink(header, dsi, priv);
		}
		
		/* Remaining Service Gateway Info members */
		struct dsi_service_gateway_info *sgi = dsi->service_gateway_info;
		sgi->download_taps_count = payload[j];
		CREATE_FILE_NUMBER(sgi_dentry, sgi, download_taps_count);
		if (sgi->download_taps_count) {
			sgi->download_taps = calloc(sgi->download_taps_count, sizeof(char));
			memcpy(sgi->download_taps, &payload[j+1], sgi->download_taps_count);
			CREATE_FILE_BIN(sgi_dentry, sgi, download_taps, sgi->download_taps_count);
		}
		j += 1 + sgi->download_taps_count;

		sgi->service_context_list_count = payload[j++];
		CREATE_FILE_NUMBER(sgi_dentry, sgi, service_context_list_count);
		if (sgi->service_context_list_count) {
			/* Parse context id  and data length */
			uint32_t context_id = CONVERT_TO_16(payload[j], payload[j+1]);
			uint8_t context_data_length = payload[j+2];
		//	sgi->service_context_list = calloc(context_data_length, sizeof(char));
		//	memcpy(sgi->service_context_list, &payload[j+3], context_data_length);
		//	CREATE_FILE_BIN(sgi_dentry, sgi, service_context_list, sgi->service_context_list_count);
			j += 2 + context_data_length;
		}
		
		sgi->user_info_length = CONVERT_TO_16(payload[j], payload[j+1]);
		CREATE_FILE_NUMBER(sgi_dentry, sgi, user_info_length);
		if (sgi->user_info_length) {
			sgi->user_info = calloc(sgi->user_info_length, sizeof(char));
			memcpy(sgi->user_info, &payload[j+2], sgi->user_info_length);
			CREATE_FILE_BIN(sgi_dentry, sgi, user_info, sgi->user_info_length);
		}
		j += 2 + sgi->user_info_length;
	}
	if (current_dsi) {
		fsutils_migrate_children(current_dsi->dentry, dsi->dentry);
		hashtable_del(priv->psi_tables, current_dsi->dentry->inode);
	}
	hashtable_add(priv->psi_tables, dsi->dentry->inode, dsi, (hashtable_free_function_t) dsi_free);

	return 0;
}
