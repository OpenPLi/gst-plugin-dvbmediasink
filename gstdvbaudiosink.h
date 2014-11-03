/*
 * GStreamer DVB Media Sink
 * based on code by:
 * Copyright 2006 Felix Domke <tmbinc@elitedvb.net>
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
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
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_DVBAUDIOSINK_H__
#define __GST_DVBAUDIOSINK_H__

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_DVBAUDIOSINK \
  (gst_dvbaudiosink_get_type())
#define GST_DVBAUDIOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DVBAUDIOSINK,GstDVBAudioSink))
#define GST_DVBAUDIOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DVBAUDIOSINK,GstDVBAudioSinkClass))
#define GST_DVBAUDIOSINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_DVBAUDIOSINK,GstDVBAudioSinkClass))
#define GST_IS_DVBAUDIOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DVBAUDIOSINK))
#define GST_IS_DVBAUDIOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DVBAUDIOSINK))

typedef struct _GstDVBAudioSink		GstDVBAudioSink;
typedef struct _GstDVBAudioSinkClass	GstDVBAudioSinkClass;
typedef struct _GstDVBAudioSinkPrivate	GstDVBAudioSinkPrivate;

typedef enum {
	AUDIOTYPE_UNKNOWN = -1,
	AUDIOTYPE_AC3 = 0,
	AUDIOTYPE_MPEG = 1,
	AUDIOTYPE_DTS = 2,
	AUDIOTYPE_LPCM = 6,
	AUDIOTYPE_AAC = 8,
	AUDIOTYPE_AAC_HE = 9,
	AUDIOTYPE_MP3 = 0xa,
	AUDIOTYPE_AAC_PLUS = 0xb,
	AUDIOTYPE_DTS_HD = 0x10,
	AUDIOTYPE_WMA = 0x20,
	AUDIOTYPE_WMA_PRO = 0x21,
	AUDIOTYPE_AC3_PLUS = 0x22,
	AUDIOTYPE_AMR = 0x23,
	AUDIOTYPE_RAW = 0x30
} t_audio_type;

struct _GstDVBAudioSink
{
	GstBaseSink element;
	guint8 aac_adts_header[7];
	gboolean aac_adts_header_valid;

	GstBuffer *pesheader_buffer;
	GstBuffer *codec_data;
	GstBuffer *cache;

	int fd;
	int unlockfd[2];

	int skip;
	int bypass;
	int fixed_buffersize;
	GstClockTime fixed_buffertimestamp;
	GstClockTime fixed_bufferduration;

	GstClockTime timestamp;
	gdouble rate;
	gboolean playing, paused, flushing, unlocking;
	gboolean pts_written;
	gint64 lastpts;
	gint64 timestamp_offset;

	queue_entry_t *queue;
};

struct _GstDVBAudioSinkClass
{
	GstBaseSinkClass parent_class;
	gint64 (*get_decoder_time) (GstDVBAudioSink *sink);
};

GType gst_dvbaudiosink_get_type (void);

GstFlowReturn gst_dvbaudiosink_push_buffer(GstDVBAudioSink *self, GstBuffer *buffer);

G_END_DECLS

#endif /* __GST_DVBAUDIOSINK_H__ */
