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
#include "ce2110.h"
#include "ts.h"

struct input_parser {
    GstElement *bin;
    GstBus     *bin_bus;
    guint       bin_bus_id;
    GstElement *input_src;
    GstPad     *psi_srcpad;
    struct ts_status ts_status;
};

static gboolean ce2110_bus_callback(GstBus *bus, GstMessage *msg, gpointer user_data)
{
    GstBuffer *buf;
    GError *error = NULL;
    gchar *error_msg = NULL;
    const GValue *info_value = NULL;
    const GstStructure *info_struct = NULL;
    const struct ts_header *header = NULL;
    const char *payload = NULL;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_WARNING:
            gst_message_parse_warning(msg, &error, &error_msg);
            g_error_free(error);
            g_warning("GST_MESSAGE_WARNING: %s", error_msg);
            g_free(error_msg);
            break;
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &error, &error_msg);
            g_error_free(error);
            g_warning("GST_MESSAGE_ERROR: %s", error_msg);
            g_free(error_msg);
            break;
        case GST_MESSAGE_EOS:
            g_message("GST_MESSAGE_EOS");
            break;
        case GST_MESSAGE_PSI_PACKET:
            info_struct = (GstStructure *) gst_message_get_structure(msg);
            info_value = (GValue *) gst_structure_get_value(info_struct, "psi-packet");
            buf = (GstBuffer *) g_value_get_int(info_value);
            header = (struct ts_header *) GST_BUFFER_DATA(buf);
            payload = (char *) GST_BUFFER_DATA(buf) + sizeof(struct ts_header);
            ts_parse_packet(header, payload);
            gst_buffer_unref(buf);
            break;
        case GST_MESSAGE_TAG:
            g_message("GST_MESSAGE_TAG");
            break;
        case GST_MESSAGE_APPLICATION:
            g_message("GST_MESSAGE_APPLICATION");
            break;
        default:
            break;
    }
    return TRUE;
}

int ce2110_create_parser(struct fuse_args *args, struct demuxfs_data *priv)
{
    struct input_parser *p = calloc(1, sizeof(struct input_parser));
    g_assert(p);

    /* Create a Gstreamer bin */
    gst_init(NULL, NULL);
    p->bin = gst_pipeline_new("Pipeline");
    g_assert(p->bin);
    p->bin_bus = gst_pipeline_get_bus(GST_PIPELINE(p->bin));
    g_assert(p->bin_bus);
    p->bin_bus_id = gst_bus_add_watch_full(p->bin_bus, MAX_PRIORITY, ce2110_bus_callback, p, NULL);

    /* Configure tuner to parallel mode */
    p->input_src = gst_element_factory_make("smd_tsd_source", NULL);
    g_assert(p->input_src);
    g_object_set(G_OBJECT(p->input_src), "mode", TRUE, NULL);

    /* Request smd_tsd_source to forward PSI packets to the pipeline message bus */
    p->psi_srcpad = gst_element_get_request_pad(p->input_src, "psi_1");
    g_assert(p->psi_srcpad);
    g_object_set(G_OBJECT(p->psi_srcpad), 
            "psi_output_mode_bus", TRUE, 
            "lspid", "0,0,0",
            NULL);

    /* Attach the input element to the bin */
    gst_bin_add(GST_BIN(p->bin), p->input_src);

    /* Start the pipeline */
    gst_element_set_state(p->bin, GST_STATE_PLAYING);

	priv->parser = p;
    return 0;
}

int ce2110_destroy_parser(struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
    gst_element_set_state(p->bin, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(p->bin), p->input_src);
    g_object_unref(p->psi_srcpad);
    g_source_remove(p->bin_bus_id);
    g_object_unref(p->bin_bus);
    g_object_unref(p->bin);
    gst_deinit();
    free(p);
    return 0;
}

int ce2110_read_packet(struct demuxfs_data *priv) 
{
    g_main_context_iteration(NULL, TRUE);
    return 0;
}

int ce2110_process_packet(struct demuxfs_data *priv)
{
    /* Packet processing happens asynchronously throughout pipeline messages */
    return 0;
}

bool ce2110_keep_alive(struct demuxfs_data *priv)
{
    return true;
}

struct backend_ops ce2110_backend_ops = {
    .create = ce2110_create_parser,
    .destroy = ce2110_destroy_parser,
    .read = ce2110_read_packet,
    .process = ce2110_process_packet,
    .keep_alive = ce2110_keep_alive,
};
