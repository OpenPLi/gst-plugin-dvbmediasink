/*
 * GStreamer DVB Media Sink
 * 
 * Copyright 2011 <slashdev@gmx.net>
 *
 * based on code by:
 * Copyright 2006 Felix Domke <tmbinc@elitedvb.net>
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files(the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1(the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-plugin
 *
 * <refsect2>
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch -v -m audiotestsrc ! plugin ! fakesink silent=TRUE
 * </programlisting>
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/base/gstbasesink.h>

#include "common.h"
#include "gstdvbaudiosink.h"
#include "gstdvbsink-marshal.h"

GST_DEBUG_CATEGORY_STATIC(dvbaudiosink_debug);
#define GST_CAT_DEFAULT dvbaudiosink_debug

enum
{
	PROP_0,
	PROP_SYNC,
	PROP_LAST,
};

enum
{
	SIGNAL_GET_DECODER_TIME,
	LAST_SIGNAL
};

static guint gst_dvbaudiosink_signals[LAST_SIGNAL] = { 0 };

#ifdef HAVE_MP3
#define MPEGCAPS \
		"audio/mpeg, " \
		"mpegversion = (int) 1, " \
		"layer = (int) [ 1, 3 ], " \
		"parsed = (boolean) true; " \
		"audio/mpeg, " \
		"mpegversion = (int) { 2, 4 }, " \
		"profile = (string) lc, " \
		"stream-format = (string) { raw, adts, adif, loas }, " \
		"framed = (boolean) true; "
#else
#define MPEGCAPS \
		"audio/mpeg, " \
		"mpegversion = (int) 1, " \
		"layer = (int) [ 1, 2 ], " \
		"parsed = (boolean) true; "
#endif

#define AC3CAPS \
		"audio/x-ac3, " \
		"framed =(boolean) true; " \
		"audio/x-private1-ac3, " \
		"framed =(boolean) true; "

#define EAC3CAPS \
		"audio/x-eac3, " \
		"framed =(boolean) true; " \
		"audio/x-private1-eac3, " \
		"framed =(boolean) true; "

#define LPCMCAPS \
		"audio/x-private1-lpcm; "

#define DTSCAPS \
		"audio/x-dts, " \
		"framed =(boolean) true; " \
		"audio/x-private1-dts, " \
		"framed =(boolean) true; "

#define WMACAPS \
		"audio/x-wma; " \

#define AMRCAPS \
		"audio/AMR, " \
		"rate = (int) {8000, 16000}, channels = (int) 1; "

#define XRAW "audio/x-raw"
#define PCMCAPS \
		"audio/x-raw, " \
		"format = (string) { "GST_AUDIO_NE(S32)", "GST_AUDIO_NE(S24)", "GST_AUDIO_NE(S16)", S8, "GST_AUDIO_NE(U32)", "GST_AUDIO_NE(U24)", "GST_AUDIO_NE(U16)", U8 }, " \
		"layout = (string) { interleaved, non-interleaved }, " \
		"rate = (int) [ 1, " MAX_PCM_RATE " ], " "channels = (int) [ 1, 2 ]; "

static GstStaticPadTemplate sink_factory =
GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		MPEGCAPS 
		AC3CAPS
#ifdef HAVE_EAC3
		EAC3CAPS
#endif
#ifdef HAVE_DTS
		DTSCAPS
#endif
#ifdef HAVE_LPCM
		LPCMCAPS
#endif
#ifdef HAVE_WMA
		WMACAPS
#endif
#ifdef HAVE_AMR
		AMRCAPS
#endif
#ifdef HAVE_PCM
		PCMCAPS
#endif
	)
);

static void gst_dvbaudiosink_init(GstDVBAudioSink *self);

#define DEBUG_INIT \
	GST_DEBUG_CATEGORY_INIT(dvbaudiosink_debug, "dvbaudiosink", 0, "dvbaudiosink element");

static GstBaseSinkClass *parent_class = NULL;
G_DEFINE_TYPE_WITH_CODE(GstDVBAudioSink, gst_dvbaudiosink, GST_TYPE_BASE_SINK, DEBUG_INIT);

static gboolean gst_dvbaudiosink_start(GstBaseSink * sink);
static gboolean gst_dvbaudiosink_stop(GstBaseSink * sink);
static gboolean gst_dvbaudiosink_event(GstBaseSink * sink, GstEvent * event);
static GstFlowReturn gst_dvbaudiosink_render(GstBaseSink * sink, GstBuffer * buffer);
static gboolean gst_dvbaudiosink_unlock(GstBaseSink * basesink);
static gboolean gst_dvbaudiosink_unlock_stop(GstBaseSink * basesink);
static gboolean gst_dvbaudiosink_set_caps(GstBaseSink * sink, GstCaps * caps);
static GstCaps *gst_dvbaudiosink_get_caps(GstBaseSink *basesink, GstCaps *filter);
static GstStateChangeReturn gst_dvbaudiosink_change_state(GstElement * element, GstStateChange transition);
static gint64 gst_dvbaudiosink_get_decoder_time(GstDVBAudioSink *self);
static void gst_dvbaudiosink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dvbaudiosink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

/* initialize the plugin's class */
static void gst_dvbaudiosink_class_init(GstDVBAudioSinkClass *self)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(self);
	GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS(self);
	GstElementClass *element_class = GST_ELEMENT_CLASS(self);

	parent_class = g_type_class_peek_parent(self);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
	gst_element_class_set_static_metadata(element_class,
		"DVB audio sink",
		"Generic/DVBAudioSink",
		"Outputs PES into a linuxtv dvb audio device",
		"PLi team");
	gobject_class->set_property = gst_dvbaudiosink_set_property;
	gobject_class->get_property = gst_dvbaudiosink_get_property;

	g_object_class_install_property (gobject_class, PROP_SYNC,
			g_param_spec_boolean ("sync", "Sync", "Sync on the clock", FALSE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_start);
	gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_stop);
	gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_render);
	gstbasesink_class->event = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_event);
	gstbasesink_class->unlock = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_unlock);
	gstbasesink_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_unlock_stop);
	gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_set_caps);
	gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_get_caps);

	element_class->change_state = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_change_state);

	gst_dvbaudiosink_signals[SIGNAL_GET_DECODER_TIME] =
		g_signal_new("get-decoder-time",
		G_TYPE_FROM_CLASS(self),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET(GstDVBAudioSinkClass, get_decoder_time),
		NULL, NULL, gst_dvbsink_marshal_INT64__VOID, G_TYPE_INT64, 0);

	self->get_decoder_time = gst_dvbaudiosink_get_decoder_time;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void gst_dvbaudiosink_init(GstDVBAudioSink *self)
{
	self->codec_data = NULL;
	self->bypass = AUDIOTYPE_UNKNOWN;
	self->fixed_buffersize = 0;
	self->fixed_bufferduration = GST_CLOCK_TIME_NONE;
	self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
	self->aac_adts_header_valid = FALSE;
	self->pesheader_buffer = NULL;
	self->cache = NULL;
	self->playing = self->flushing = self->unlocking = self->paused = FALSE;
	self->pts_written = FALSE;
	self->lastpts = 0;
	self->timestamp_offset = 0;
	self->queue = NULL;
	self->fd = -1;
	self->unlockfd[0] = self->unlockfd[1] = -1;
	self->rate = 1.0;
	self->timestamp = GST_CLOCK_TIME_NONE;

	gst_base_sink_set_sync(GST_BASE_SINK(self), FALSE);
	gst_base_sink_set_async_enabled(GST_BASE_SINK(self), FALSE);
}

static void gst_dvbaudiosink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK (object);

	switch (prop_id)
	{
	/* sink should only work with sync turned off, ignore all attempts to change it */
	case PROP_SYNC:
		GST_DEBUG_OBJECT(self, "ignoring attempt to change 'sync' to '%d'", g_value_get_boolean(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void gst_dvbaudiosink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK (object);

	switch (prop_id)
	{
	case PROP_SYNC:
		g_value_set_boolean(value, gst_base_sink_get_sync(GST_BASE_SINK(object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gint64 gst_dvbaudiosink_get_decoder_time(GstDVBAudioSink *self)
{
	gint64 cur = 0;
	if (self->fd < 0 || !self->playing || !self->pts_written) return GST_CLOCK_TIME_NONE;

	ioctl(self->fd, AUDIO_GET_PTS, &cur);
	if (cur)
	{
		self->lastpts = cur;
	}
	else
	{
		cur = self->lastpts;
	}
	cur *= 11111;

	return cur - self->timestamp_offset;
}

static gboolean gst_dvbaudiosink_unlock(GstBaseSink *basesink)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);
	self->unlocking = TRUE;
	/* wakeup the poll */
	write(self->unlockfd[1], "\x01", 1);
	GST_DEBUG_OBJECT(basesink, "unlock");
	return TRUE;
}

static gboolean gst_dvbaudiosink_unlock_stop(GstBaseSink *basesink)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);
	self->unlocking = FALSE;
	GST_DEBUG_OBJECT(basesink, "unlock_stop");
	return TRUE;
}

#ifdef HAVE_DTSDOWNMIX
static gboolean get_downmix_setting()
{
	FILE *f;
	char buffer[32] = {0};
	f = fopen("/proc/stb/audio/ac3", "r");
	if (f)
	{
		fread(buffer, sizeof(buffer), 1, f);
		fclose(f);
	}
	return !strncmp(buffer, "downmix", 7);
}
#endif

static GstCaps *gst_dvbaudiosink_get_caps(GstBaseSink *basesink, GstCaps *filter)
{
	GstCaps *caps = gst_caps_from_string(
		MPEGCAPS 
		AC3CAPS
#ifdef HAVE_EAC3
		EAC3CAPS
#endif
#ifdef HAVE_LPCM
		LPCMCAPS
#endif
#ifdef HAVE_WMA
		WMACAPS
#endif
#ifdef HAVE_AMR
		AMRCAPS
#endif
#ifdef HAVE_PCM
		PCMCAPS
#endif
	);

#ifdef HAVE_DTS
# ifdef HAVE_DTSDOWNMIX
	if (!get_downmix_setting())
	{
		gst_caps_append(caps, gst_caps_from_string(DTSCAPS));
	}
# else
	gst_caps_append(caps, gst_caps_from_string(DTSCAPS));
# endif
#endif

	if (filter)
	{
		GstCaps *intersection = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
		gst_caps_unref(caps);
		caps = intersection;
	}
	return caps;
}

static gboolean gst_dvbaudiosink_set_caps(GstBaseSink *basesink, GstCaps *caps)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);
	GstStructure *structure = gst_caps_get_structure(caps, 0);
	const char *type = gst_structure_get_name(structure);
	t_audio_type bypass = AUDIOTYPE_UNKNOWN;

	self->skip = 0;
	self->aac_adts_header_valid = FALSE;
	self->fixed_buffersize = 0;
	self->fixed_bufferduration = GST_CLOCK_TIME_NONE;
	self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;

	if (self->codec_data)
	{
		gst_buffer_unref(self->codec_data);
		self->codec_data = NULL;
	}

	if (!strcmp(type, "audio/mpeg"))
	{
		gint mpegversion;
		gst_structure_get_int(structure, "mpegversion", &mpegversion);
		switch (mpegversion)
		{
			case 1:
			{
				gint layer;
				gst_structure_get_int(structure, "layer", &layer);
				if (layer == 3)
				{
					bypass = AUDIOTYPE_MP3;
				}
				else
				{
					bypass = AUDIOTYPE_MPEG;
				}
				GST_INFO_OBJECT(self, "MIMETYPE %s version %d layer %d", type, mpegversion, layer);
				break;
			}
			case 2:
			case 4:
			{
				const gchar *stream_type = gst_structure_get_string(structure, "stream-type");
				if (!stream_type)
				{
					stream_type = gst_structure_get_string(structure, "stream-format");
				}
				if (stream_type && !strcmp(stream_type, "adts"))
				{
					GST_INFO_OBJECT(self, "MIMETYPE %s version %d(AAC-ADTS)", type, mpegversion);
				}
				else
				{
					const GValue *codec_data = gst_structure_get_value(structure, "codec_data");
					GST_INFO_OBJECT(self, "MIMETYPE %s version %d(AAC-RAW)", type, mpegversion);
					if (codec_data)
					{
						guint8 h[2];
						gst_buffer_extract(gst_value_get_buffer(codec_data), 0, h, sizeof(h));
						guint8 obj_type =h[0] >> 3;
						guint8 rate_idx =((h[0] & 0x7) << 1) |((h[1] & 0x80) >> 7);
						guint8 channels =(h[1] & 0x78) >> 3;
						GST_INFO_OBJECT(self, "have codec data -> obj_type = %d, rate_idx = %d, channels = %d\n",
							obj_type, rate_idx, channels);
						/* Sync point over a full byte */
						self->aac_adts_header[0] = 0xFF;
						/* Sync point continued over first 4 bits + static 4 bits
						 *(ID, layer, protection)*/
						self->aac_adts_header[1] = 0xF1;
						if (mpegversion == 2)
							self->aac_adts_header[1] |= 8;
						/* Object type over first 2 bits */
						self->aac_adts_header[2] = (obj_type - 1) << 6;
						/* rate index over next 4 bits */
						self->aac_adts_header[2] |= rate_idx << 2;
						/* channels over last 2 bits */
						self->aac_adts_header[2] |= (channels & 0x4) >> 2;
						/* channels continued over next 2 bits + 4 bits at zero */
						self->aac_adts_header[3] = (channels & 0x3) << 6;
						self->aac_adts_header_valid = TRUE;
					}
					else
					{
						gint rate, channels, rate_idx = 0, obj_type = 1; // hardcoded yet.. hopefully this works every time ;)
						GST_INFO_OBJECT(self, "no codec data");
						if (gst_structure_get_int(structure, "rate", &rate) && gst_structure_get_int(structure, "channels", &channels))
						{
							guint samplingrates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0 };
							do
							{
								if (samplingrates[rate_idx] == rate) break;
								++rate_idx;
							} while (samplingrates[rate_idx]);
							if (samplingrates[rate_idx])
							{
								GST_INFO_OBJECT(self, "mpegversion %d, channels %d, rate %d, rate_idx %d\n", mpegversion, channels, rate, rate_idx);
								/* Sync point over a full byte */
								self->aac_adts_header[0] = 0xFF;
								/* Sync point continued over first 4 bits + static 4 bits
								 *(ID, layer, protection)*/
								self->aac_adts_header[1] = 0xF1;
								if (mpegversion == 2) self->aac_adts_header[1] |= 8;
								/* Object type over first 2 bits */
								self->aac_adts_header[2] = obj_type << 6;
								/* rate index over next 4 bits */
								self->aac_adts_header[2] |= rate_idx << 2;
								/* channels over last 2 bits */
								self->aac_adts_header[2] |=(channels & 0x4) >> 2;
								/* channels continued over next 2 bits + 4 bits at zero */
								self->aac_adts_header[3] =(channels & 0x3) << 6;
								self->aac_adts_header_valid = TRUE;
							}
						}
					}
				}
				bypass = AUDIOTYPE_AAC_PLUS; // always use AAC+ ADTS yet..
				break;
			}
			default:
				GST_ELEMENT_ERROR(self, STREAM, FORMAT,(NULL),("unhandled mpeg version %i", mpegversion));
				break;
		}
	}
	else if (!strcmp(type, "audio/x-ac3"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		bypass = AUDIOTYPE_AC3;
	}
	else if (!strcmp(type, "audio/x-eac3"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		bypass = AUDIOTYPE_AC3_PLUS;
	}
	else if (!strcmp(type, "audio/x-private1-dts"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s(DVD Audio - 2 byte skipping)",type);
		bypass = AUDIOTYPE_DTS;
		self->skip = 2;
	}
	else if (!strcmp(type, "audio/x-private1-ac3"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s(DVD Audio - 2 byte skipping)",type);
		bypass = AUDIOTYPE_AC3;
		self->skip = 2;
	}
	else if (!strcmp(type, "audio/x-private1-eac3"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s(DVD Audio - 2 byte skipping)",type);
		bypass = AUDIOTYPE_AC3_PLUS;
		self->skip = 2;
	}
	else if (!strcmp(type, "audio/x-private1-lpcm"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s(DVD Audio)",type);
		bypass = AUDIOTYPE_LPCM;
	}
	else if (!strcmp(type, "audio/x-dts"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		bypass = AUDIOTYPE_DTS;
	}
	else if (!strcmp(type, "audio/x-wma"))
	{
		const GValue *codec_data = gst_structure_get_value(structure, "codec_data");
		gint wmaversion, bitrate, depth, rate, channels, block_align;
		gst_structure_get_int(structure, "wmaversion", &wmaversion);
		gst_structure_get_int(structure, "bitrate", &bitrate);
		gst_structure_get_int(structure, "depth", &depth);
		gst_structure_get_int(structure, "rate", &rate);
		gst_structure_get_int(structure, "channels", &channels);
		gst_structure_get_int(structure, "block_align", &block_align);
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		bypass = (wmaversion > 2) ? AUDIOTYPE_WMA_PRO : AUDIOTYPE_WMA;
		if (codec_data)
		{
			guint8 *data;
			guint8 *codec_data_pointer;
			gint codec_data_size;
			gint codecid = 0x160 + wmaversion - 1;
			GstMapInfo map, codecdatamap;
			gst_buffer_map(gst_value_get_buffer(codec_data), &codecdatamap, GST_MAP_READ);
			codec_data_pointer = codecdatamap.data;
			codec_data_size = codecdatamap.size;
			self->codec_data = gst_buffer_new_and_alloc(18 + codec_data_size);
			gst_buffer_map(self->codec_data, &map, GST_MAP_WRITE);
			data = map.data;
			/* codec tag */
			*(data++) = codecid & 0xff;
			*(data++) = (codecid >> 8) & 0xff;
			/* channels */
			*(data++) = channels & 0xff;
			*(data++) = (channels >> 8) & 0xff;
			/* sample rate */
			*(data++) = rate & 0xff;
			*(data++) = (rate >> 8) & 0xff;
			*(data++) = (rate >> 16) & 0xff;
			*(data++) = (rate >> 24) & 0xff;
			/* byte rate */
			bitrate /= 8;
			*(data++) = bitrate & 0xff;
			*(data++) = (bitrate >> 8) & 0xff;
			*(data++) = (bitrate >> 16) & 0xff;
			*(data++) = (bitrate >> 24) & 0xff;
			/* block align */
			*(data++) = block_align & 0xff;
			*(data++) = (block_align >> 8) & 0xff;
			/* word size */
			*(data++) = depth & 0xff;
			*(data++) = (depth >> 8) & 0xff;
			/* codec data size */
			*(data++) = codec_data_size & 0xff;
			*(data++) = (codec_data_size >> 8) & 0xff;
			memcpy(data, codec_data_pointer, codec_data_size);
			gst_buffer_unmap(self->codec_data, &map);
			gst_buffer_unmap(gst_value_get_buffer(codec_data), &codecdatamap);
		}
	}
	else if (!strcmp(type, "audio/AMR"))
	{
		const GValue *codec_data = gst_structure_get_value(structure, "codec_data");
		if (codec_data)
		{
			self->codec_data = gst_buffer_copy(gst_value_get_buffer(codec_data));
		}
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		bypass = AUDIOTYPE_AMR;
	}
	else if (!strcmp(type, XRAW))
	{
		guint8 *data;
		gint size;
		gint format = 0x01;
		const gchar *formatstring = NULL;
		gint width = 0, depth = 0, rate = 0, channels, block_align, byterate;
		self->codec_data = gst_buffer_new_and_alloc(18);
		GstMapInfo map;
		gst_buffer_map(self->codec_data, &map, GST_MAP_WRITE);
		data = map.data;
		size = map.size;
		formatstring = gst_structure_get_string(structure, "format");
		if (formatstring)
		{
			if (!strncmp(&formatstring[1], "32", 2))
			{
				width = depth = 32;
			}
			else if (!strncmp(&formatstring[1], "24", 2))
			{
				width = depth = 24;
			}
			else if (!strncmp(&formatstring[1], "16", 2))
			{
				width = depth = 16;
			}
			else if (!strncmp(&formatstring[1], "8", 1))
			{
				width = depth = 8;
			}
		}
		gst_structure_get_int(structure, "rate", &rate);
		gst_structure_get_int(structure, "channels", &channels);
		byterate = channels * rate * width / 8;
		block_align = channels * width / 8;
		memset(data, 0, size);
		/* format tag */
		*(data++) = format & 0xff;
		*(data++) = (format >> 8) & 0xff;
		/* channels */
		*(data++) = channels & 0xff;
		*(data++) = (channels >> 8) & 0xff;
		/* sample rate */
		*(data++) = rate & 0xff;
		*(data++) = (rate >> 8) & 0xff;
		*(data++) = (rate >> 16) & 0xff;
		*(data++) = (rate >> 24) & 0xff;
		/* byte rate */
		*(data++) = byterate & 0xff;
		*(data++) = (byterate >> 8) & 0xff;
		*(data++) = (byterate >> 16) & 0xff;
		*(data++) = (byterate >> 24) & 0xff;
		/* block align */
		*(data++) = block_align & 0xff;
		*(data++) = (block_align >> 8) & 0xff;
		/* word size */
		*(data++) = depth & 0xff;
		*(data++) = (depth >> 8) & 0xff;
		self->fixed_buffersize = rate * 30 / 1000;
		self->fixed_buffersize *= channels * depth / 8;
		self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
		self->fixed_bufferduration = GST_SECOND * (GstClockTime)self->fixed_buffersize / (GstClockTime)byterate;
		GST_INFO_OBJECT(self, "MIMETYPE %s", type);
		bypass = AUDIOTYPE_RAW;
		gst_buffer_unmap(self->codec_data, &map);
	}
	else
	{
		GST_ELEMENT_ERROR(self, STREAM, TYPE_NOT_FOUND,(NULL),("unimplemented stream type %s", type));
		return FALSE;
	}

	GST_INFO_OBJECT(self, "setting dvb mode 0x%02x\n", bypass);

	if (self->playing)
	{
		if (self->fd >= 0) ioctl(self->fd, AUDIO_STOP, 0);
		self->playing = FALSE;
	}
	if (self->fd < 0 || ioctl(self->fd, AUDIO_SET_BYPASS_MODE, bypass) < 0)
	{
		GST_ELEMENT_ERROR(self, STREAM, TYPE_NOT_FOUND,(NULL),("hardware decoder can't be set to bypass mode type %s", type));
		return FALSE;
	}
	if (self->fd >= 0) ioctl(self->fd, AUDIO_PLAY);
	self->playing = TRUE;

	self->bypass = bypass;
	return TRUE;
}

static gboolean gst_dvbaudiosink_event(GstBaseSink *sink, GstEvent *event)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(sink);
	GST_DEBUG_OBJECT(self, "EVENT %s", gst_event_type_get_name(GST_EVENT_TYPE(event)));
	gboolean ret = TRUE;

	switch (GST_EVENT_TYPE(event))
	{
	case GST_EVENT_FLUSH_START:
		self->flushing = TRUE;
		/* wakeup the poll */
		write(self->unlockfd[1], "\x01", 1);
		break;
	case GST_EVENT_FLUSH_STOP:
		if (self->fd >= 0) ioctl(self->fd, AUDIO_CLEAR_BUFFER);
		GST_OBJECT_LOCK(self);
		while (self->queue)
		{
			queue_pop(&self->queue);
		}
		self->flushing = FALSE;
		self->timestamp = GST_CLOCK_TIME_NONE;
		self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
		if (self->cache)
		{
			gst_buffer_unref(self->cache);
			self->cache = NULL;
		}
		GST_OBJECT_UNLOCK(self);
		break;
	case GST_EVENT_EOS:
	{
		struct pollfd pfd[2];
		pfd[0].fd = self->unlockfd[0];
		pfd[0].events = POLLIN;
		pfd[1].fd = self->fd;
		pfd[1].events = POLLIN;

		GST_BASE_SINK_PREROLL_UNLOCK(sink);
		while (1)
		{
			int retval = poll(pfd, 2, 250);
			if (retval < 0)
			{
				perror("poll in EVENT_EOS");
				ret = FALSE;
				break;
			}

			if (pfd[0].revents & POLLIN)
			{
				GST_DEBUG_OBJECT(self, "wait EOS aborted!!\n");
				ret = FALSE;
				break;
			}

			if (pfd[1].revents & POLLIN)
			{
				GST_DEBUG_OBJECT(self, "got buffer empty from driver!\n");
				break;
			}

			if (sink->flushing)
			{
				GST_DEBUG_OBJECT(self, "wait EOS flushing!!\n");
				ret = FALSE;
				break;
			}
		}
		GST_BASE_SINK_PREROLL_LOCK(sink);
		break;
	}
	case GST_EVENT_SEGMENT:
	{
		const GstSegment *segment;
		GstFormat format;
		gdouble rate;
		guint64 start, end, pos;
		gst_event_parse_segment(event, &segment);
		format = segment->format;
		rate = segment->rate;
		start = segment->start;
		end = segment->stop;
		pos = segment->position;
		GST_DEBUG_OBJECT(self, "GST_EVENT_NEWSEGMENT rate=%f %d\n", rate, format);
		
		if (format == GST_FORMAT_TIME)
		{
			self->timestamp_offset = start - pos;
			if (rate != self->rate)
			{
				int video_fd = open("/dev/dvb/adapter0/video0", O_RDWR);
				if (video_fd >= 0)
				{
					int skip = 0, repeat = 0;
					if (rate > 1.0)
					{
						skip = (int)rate;
					}
					else if (rate < 1.0)
					{
						repeat = 1.0 / rate;
					}
					ioctl(video_fd, VIDEO_SLOWMOTION, repeat);
					ioctl(video_fd, VIDEO_FAST_FORWARD, skip);
					ioctl(video_fd, VIDEO_CONTINUE);
					close(video_fd);
					video_fd = -1;
				}
				self->rate = rate;
			}
		}
		break;
	}
	default:
		break;
	}
	if (ret) ret = GST_BASE_SINK_CLASS(parent_class)->event(sink, event);
	else gst_event_unref(event);

	return ret;
}

static int audio_write(GstDVBAudioSink *self, GstBuffer *buffer, size_t start, size_t end)
{
	size_t written = start;
	size_t len = end;
	struct pollfd pfd[2];
	guint8 *data;
	int retval = 0;
	GstMapInfo map;
	gst_buffer_map(buffer, &map, GST_MAP_READ);
	data = map.data;

	pfd[0].fd = self->unlockfd[0];
	pfd[0].events = POLLIN;
	pfd[1].fd = self->fd;
	pfd[1].events = POLLOUT;

	do
	{
		if (self->flushing)
		{
			GST_DEBUG_OBJECT(self, "flushing, skip %d bytes", len - written);
			break;
		}
		else if (self->paused || self->unlocking)
		{
			GST_OBJECT_LOCK(self);
			queue_push(&self->queue, buffer, written, end);
			GST_OBJECT_UNLOCK(self);
			GST_DEBUG_OBJECT(self, "pushed %d bytes to queue", len - written);
			break;
		}
		else
		{
			GST_LOG_OBJECT(self, "going into poll, have %d bytes to write", len - written);
		}
		if (poll(pfd, 2, -1) < 0)
		{
			if (errno == EINTR) continue;
			retval = -1;
			break;
		}
		if (pfd[0].revents & POLLIN)
		{
			/* read all stop commands */
			while (1)
			{
				gchar command;
				int res = read(self->unlockfd[0], &command, 1);
				if (res < 0)
				{
					GST_DEBUG_OBJECT(self, "no more commands");
					/* no more commands */
					break;
				}
			}
			continue;
		}
		if (pfd[1].revents & POLLOUT)
		{
			size_t queuestart, queueend;
			GstBuffer *queuebuffer;
			GST_OBJECT_LOCK(self);
			if (queue_front(&self->queue, &queuebuffer, &queuestart, &queueend) >= 0)
			{
				guint8 *queuedata;
				GstMapInfo queuemap;
				gst_buffer_map(queuebuffer, &queuemap, GST_MAP_READ);
				queuedata = queuemap.data;
				int wr = write(self->fd, queuedata + queuestart, queueend - queuestart);
				gst_buffer_unmap(queuebuffer, &queuemap);
				if (wr < 0)
				{
					switch(errno)
					{
						case EINTR:
						case EAGAIN:
							break;
						default:
							GST_OBJECT_UNLOCK(self);
							retval = -3;
							break;
					}
					if (retval < 0) break;
				}
				else if (wr >= queueend - queuestart)
				{
					queue_pop(&self->queue);
					GST_DEBUG_OBJECT(self, "written %d queue bytes... pop entry", wr);
				}
				else
				{
					self->queue->start += wr;
					GST_DEBUG_OBJECT(self, "written %d queue bytes... update offset", wr);
				}
				GST_OBJECT_UNLOCK(self);
				continue;
			}
			GST_OBJECT_UNLOCK(self);
			int wr = write(self->fd, data + written, len - written);
			if (wr < 0)
			{
				switch(errno)
				{
					case EINTR:
					case EAGAIN:
						continue;
					default:
						retval = -3;
						break;
				}
				if (retval < 0) break;
			}
			written += wr;
		}
	} while (written < len);

	gst_buffer_unmap(buffer, &map);
	return retval;
}

GstFlowReturn gst_dvbaudiosink_push_buffer(GstDVBAudioSink *self, GstBuffer *buffer)
{
	guint8 *pes_header;
	gsize pes_header_len = 0;
	gsize size;
	guint8 *data, *original_data;
	guint8 *codec_data = NULL;
	gsize codec_data_size = 0;
	GstClockTime timestamp = self->timestamp;
	GstClockTime duration = GST_BUFFER_DURATION(buffer);
	GstMapInfo map, pesheadermap, codecdatamap;
	gst_buffer_map(buffer, &map, GST_MAP_READ);
	original_data = data = map.data;
	size = map.size;
	gst_buffer_map(self->pesheader_buffer, &pesheadermap, GST_MAP_WRITE);
	pes_header = pesheadermap.data;
	if (self->codec_data)
	{
		gst_buffer_map(self->codec_data, &codecdatamap, GST_MAP_READ);
		codec_data = codecdatamap.data;
		codec_data_size = codecdatamap.size;
	}
	/* 
	 * Some audioformats have incorrect timestamps, 
	 * so if we have both a timestamp and a duration, 
	 * keep extrapolating from the first timestamp instead
	 */
	if (timestamp == GST_CLOCK_TIME_NONE)
	{
		timestamp = GST_BUFFER_PTS(buffer);
		if (timestamp != GST_CLOCK_TIME_NONE && duration != GST_CLOCK_TIME_NONE)
		{
			self->timestamp = timestamp + duration;
		}
	}
	else
	{
		if (duration != GST_CLOCK_TIME_NONE)
		{
			self->timestamp += duration;
		}
		else
		{
			timestamp = GST_BUFFER_PTS(buffer);
			self->timestamp = GST_CLOCK_TIME_NONE;
		}
	}

	pes_header[0] = 0;
	pes_header[1] = 0;
	pes_header[2] = 1;
	pes_header[3] = 0xc0;

	pes_header[6] = 0x81;
	pes_header[7] = 0; /* no pts */
	pes_header[8] = 0;
	pes_header_len = 9;

	if (self->bypass == AUDIOTYPE_DTS)
	{
		int pos = 0;
		while ((pos + 4) <= size)
		{
			/* check for DTS-HD */
			if (!strcmp((char*)(data + pos), "\x64\x58\x20\x25"))
			{
				size = pos;
				break;
			}
			++pos;
		}
	}

	if (timestamp != GST_CLOCK_TIME_NONE)
	{
		pes_header[7] = 0x80; /* pts */
		pes_header[8] = 5; /* pts size */
		pes_header_len += 5;
		pes_set_pts(timestamp, pes_header);
	}

	if (self->aac_adts_header_valid)
	{
		size_t payload_len = size + 7;
		self->aac_adts_header[3] &= 0xC0;
		/* frame size over last 2 bits */
		self->aac_adts_header[3] |= (payload_len & 0x1800) >> 11;
		/* frame size continued over full byte */
		self->aac_adts_header[4] = (payload_len & 0x1FF8) >> 3;
		/* frame size continued first 3 bits */
		self->aac_adts_header[5] = (payload_len & 7) << 5;
		/* buffer fullness(0x7FF for VBR) over 5 last bits */
		self->aac_adts_header[5] |= 0x1F;
		/* buffer fullness(0x7FF for VBR) continued over 6 first bits + 2 zeros for
		 * number of raw data blocks */
		self->aac_adts_header[6] = 0xFC;
		memcpy(pes_header + pes_header_len, self->aac_adts_header, 7);
		pes_header_len += 7;
	}

	if (self->bypass == AUDIOTYPE_LPCM && (data[0] < 0xa0 || data[0] > 0xaf))
	{
		/*
		 * gstmpegdemux removes the streamid and the number of frames
		 * for certain lpcm streams, so we need to reconstruct them.
		 * Fortunately, the number of frames is ignored.
		 */
		pes_header[pes_header_len++] = 0xa0;
		pes_header[pes_header_len++] = 0x01;
	}
	else if (self->bypass == AUDIOTYPE_WMA || self->bypass == AUDIOTYPE_WMA_PRO)
	{
		if (self->codec_data)
		{
			size_t payload_len = size;
			pes_header[pes_header_len++] = (payload_len >> 24) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 16) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 8) & 0xff;
			pes_header[pes_header_len++] = payload_len & 0xff;
			memcpy(&pes_header[pes_header_len], codec_data, codec_data_size);
			pes_header_len += codec_data_size;
		}
	}
	else if (self->bypass == AUDIOTYPE_AMR)
	{
		if (self->codec_data && codec_data_size >= 17)
		{
			size_t payload_len = size + 17;
			pes_header[pes_header_len++] = (payload_len >> 24) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 16) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 8) & 0xff;
			pes_header[pes_header_len++] = payload_len & 0xff;
			memcpy(&pes_header[pes_header_len], codec_data + 8, 9);
			pes_header_len += 9;
		}
	}
	else if (self->bypass == AUDIOTYPE_RAW)
	{
		if (self->codec_data && codec_data_size >= 18)
		{
			size_t payload_len = size;
			pes_header[pes_header_len++] = (payload_len >> 24) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 16) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 8) & 0xff;
			pes_header[pes_header_len++] = payload_len & 0xff;
			memcpy(&pes_header[pes_header_len], codec_data, codec_data_size);
			pes_header_len += codec_data_size;
		}
	}

	pes_set_payload_size(size + pes_header_len - 6, pes_header);

	if (audio_write(self, self->pesheader_buffer, 0, pes_header_len) < 0) goto error;
	if (audio_write(self, buffer, data - original_data, data - original_data + size) < 0) goto error;
	if (timestamp != GST_CLOCK_TIME_NONE)
	{
		self->pts_written = TRUE;
	}
	gst_buffer_unmap(self->pesheader_buffer, &pesheadermap);
	if (self->codec_data)
	{
		gst_buffer_unmap(self->codec_data, &codecdatamap);
	}
	gst_buffer_unmap(buffer, &map);
	return GST_FLOW_OK;
error:
	gst_buffer_unmap(self->pesheader_buffer, &pesheadermap);
	if (self->codec_data)
	{
		gst_buffer_unmap(self->codec_data, &codecdatamap);
	}
	gst_buffer_unmap(buffer, &map);
	{
		GST_ELEMENT_ERROR(self, RESOURCE, READ,(NULL),
				("audio write: %s", g_strerror(errno)));
		GST_WARNING_OBJECT(self, "Audio write error");
		return GST_FLOW_ERROR;
	}
}

static GstFlowReturn gst_dvbaudiosink_render(GstBaseSink *sink, GstBuffer *buffer)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(sink);
	GstBuffer *disposebuffer = NULL;
	GstFlowReturn retval = GST_FLOW_OK;
	GstClockTime duration = GST_BUFFER_DURATION(buffer);
	gsize buffersize;
	buffersize = gst_buffer_get_size(buffer);
	GstClockTime timestamp = GST_BUFFER_PTS(buffer);

	if (self->bypass <= AUDIOTYPE_UNKNOWN)
	{
		GST_ELEMENT_ERROR(self, STREAM, FORMAT,(NULL), ("hardware decoder not setup (no caps in pipeline?)"));
		return GST_FLOW_ERROR;
	}

	if (self->fd < 0) return GST_FLOW_ERROR;

	if (GST_BUFFER_IS_DISCONT(buffer)) 
	{
		if (self->cache) 
		{
			gst_buffer_unref(self->cache);
			self->cache = NULL;
		}
		self->timestamp = GST_CLOCK_TIME_NONE;
		self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
	}

	disposebuffer = buffer;
	/* grab an additional ref, because we need to return the buffer with the same refcount as we got it */
	gst_buffer_ref(buffer);

	if (self->skip)
	{
		GstBuffer *newbuffer;
		newbuffer = gst_buffer_copy_region(buffer, GST_BUFFER_COPY_ALL, self->skip, buffersize - self->skip);
		GST_BUFFER_PTS(newbuffer) = timestamp;
		GST_BUFFER_DURATION(newbuffer) = duration;
		if (disposebuffer) gst_buffer_unref(disposebuffer);
		buffer = disposebuffer = newbuffer;
		buffersize = gst_buffer_get_size(buffer);
	}

	if (self->cache)
	{
		/* join unrefs both buffers */
		buffer = gst_buffer_append(self->cache, buffer);
		buffersize = gst_buffer_get_size(buffer);
		GST_BUFFER_PTS(buffer) = timestamp;
		GST_BUFFER_DURATION(buffer) = duration;
		disposebuffer = buffer;
		self->cache = NULL;
	}

	if (buffer)
	{
		if (self->fixed_buffersize)
		{
			if (self->fixed_buffertimestamp == GST_CLOCK_TIME_NONE)
			{
				self->fixed_buffertimestamp = timestamp;
			}
			if (buffersize < self->fixed_buffersize)
			{
				self->cache = gst_buffer_copy(buffer);
				retval = GST_FLOW_OK;
			}
			else if (buffersize > self->fixed_buffersize)
			{
				int index = 0;
				while (index <= buffersize - self->fixed_buffersize)
				{
					GstBuffer *block;
					block = gst_buffer_copy_region(buffer, GST_BUFFER_COPY_ALL, index, self->fixed_buffersize);
					/* only the first buffer needs the correct timestamp, next buffer timestamps will be ignored (and extrapolated) */
					GST_BUFFER_PTS(block) = self->fixed_buffertimestamp;
					GST_BUFFER_DURATION(block) = self->fixed_bufferduration;
					self->fixed_buffertimestamp += self->fixed_bufferduration;
					gst_dvbaudiosink_push_buffer(self, block);
					gst_buffer_unref(block);
					index += self->fixed_buffersize;
				}
				if (index < buffersize)
				{
					self->cache = gst_buffer_copy_region(buffer, GST_BUFFER_COPY_ALL, index, buffersize - index);
				}
				retval = GST_FLOW_OK;
			}
			else
			{
				/* could still be the original buffer, make sure we can write metadata */
				if (!gst_buffer_is_writable(buffer))
				{
					GstBuffer *tmpbuf = gst_buffer_copy(buffer);
					GST_BUFFER_DURATION(tmpbuf) = self->fixed_bufferduration;
					retval = gst_dvbaudiosink_push_buffer(self, tmpbuf);
					gst_buffer_unref(tmpbuf);
				}
				else
				{
					GST_BUFFER_DURATION(buffer) = self->fixed_bufferduration;
					retval = gst_dvbaudiosink_push_buffer(self, buffer);
				}
			}
		}
		else
		{
			retval = gst_dvbaudiosink_push_buffer(self, buffer);
		}
	}

	if (disposebuffer) gst_buffer_unref(disposebuffer);
	return retval;
}

static gboolean gst_dvbaudiosink_start(GstBaseSink * basesink)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);

	GST_DEBUG_OBJECT(self, "start");

	if (socketpair(PF_UNIX, SOCK_STREAM, 0, self->unlockfd) < 0)
	{
		perror("socketpair");
		goto error;
	}

	fcntl(self->unlockfd[0], F_SETFL, O_NONBLOCK);
	fcntl(self->unlockfd[1], F_SETFL, O_NONBLOCK);

	self->pesheader_buffer = gst_buffer_new_and_alloc(256);

	self->fd = open("/dev/dvb/adapter0/audio0", O_RDWR | O_NONBLOCK);

	self->pts_written = FALSE;
	self->lastpts = 0;

	return TRUE;
error:
	{
		GST_ELEMENT_ERROR(self, RESOURCE, OPEN_READ_WRITE,(NULL),
				GST_ERROR_SYSTEM);
		return FALSE;
	}
}

static gboolean gst_dvbaudiosink_stop(GstBaseSink * basesink)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);

	GST_DEBUG_OBJECT(self, "stop");

	if (self->fd >= 0)
	{
		if (self->playing)
		{
			ioctl(self->fd, AUDIO_STOP);
			self->playing = FALSE;
		}
		ioctl(self->fd, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX);

		if (self->rate != 1.0)
		{
			int video_fd = open("/dev/dvb/adapter0/video0", O_RDWR);
			if (video_fd >= 0)
			{
				ioctl(video_fd, VIDEO_SLOWMOTION, 0);
				ioctl(video_fd, VIDEO_FAST_FORWARD, 0);
				close(video_fd);
			}
			self->rate = 1.0;
		}
		close(self->fd);
		self->fd = -1;
	}

	if (self->codec_data)
	{
		gst_buffer_unref(self->codec_data);
		self->codec_data = NULL;
	}

	if (self->pesheader_buffer)
	{
		gst_buffer_unref(self->pesheader_buffer);
		self->pesheader_buffer = NULL;
	}

	if (self->cache)
	{
		gst_buffer_unref(self->cache);
		self->cache = NULL;
	}

	while (self->queue)
	{
		queue_pop(&self->queue);
	}

	/* close write end first */
	if (self->unlockfd[1] >= 0)
	{
		close(self->unlockfd[1]);
		self->unlockfd[1] = -1;
	}
	if (self->unlockfd[0] >= 0)
	{
		close(self->unlockfd[0]);
		self->unlockfd[0] = -1;
	}
	return TRUE;
}

static GstStateChangeReturn gst_dvbaudiosink_change_state(GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(element);

	switch(transition)
	{
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_DEBUG_OBJECT(self,"GST_STATE_CHANGE_NULL_TO_READY");
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_DEBUG_OBJECT(self,"GST_STATE_CHANGE_READY_TO_PAUSED");
		self->paused = TRUE;

		if (self->fd >= 0)
		{
			ioctl(self->fd, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY);
			ioctl(self->fd, AUDIO_PAUSE);
		}
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_DEBUG_OBJECT(self,"GST_STATE_CHANGE_PAUSED_TO_PLAYING");
		if (self->fd >= 0) ioctl(self->fd, AUDIO_CONTINUE);
		self->paused = FALSE;
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

	switch(transition)
	{
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_DEBUG_OBJECT(self,"GST_STATE_CHANGE_PLAYING_TO_PAUSED");
		self->paused = TRUE;
		if (self->fd >= 0) ioctl(self->fd, AUDIO_PAUSE);
		/* wakeup the poll */
		write(self->unlockfd[1], "\x01", 1);
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_DEBUG_OBJECT(self,"GST_STATE_CHANGE_PAUSED_TO_READY");
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_DEBUG_OBJECT(self,"GST_STATE_CHANGE_READY_TO_NULL");
		break;
	default:
		break;
	}

	return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 *
 * exchange the string 'plugin' with your elemnt name
 */
static gboolean plugin_init(GstPlugin *plugin)
{
	return gst_element_register(plugin, "dvbaudiosink",
						 GST_RANK_PRIMARY + 1,
						 GST_TYPE_DVBAUDIOSINK);
}

/* this is the structure that gstreamer looks for to register plugins
 *
 * exchange the strings 'plugin' and 'Template plugin' with you plugin name and
 * description
 */
GST_PLUGIN_DEFINE(
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	dvbaudiosink,
	"DVB Audio Output",
	plugin_init,
	VERSION,
	"LGPL",
	"GStreamer",
	"http://gstreamer.net/"
)
