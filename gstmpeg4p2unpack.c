/*
 *Unpacking routines are based on https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/mpeg4_unpack_bframes_bsf.c
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <gst/gst.h>

#include "gstmpeg4p2unpack.h"

static unsigned int mpeg4p2_find_startcode(const uint8_t *buf, int buf_size, int *pos)
{
	unsigned int startcode = 0xFF;

	for (; *pos < buf_size;)
	{
		startcode = ((startcode << 8) | buf[*pos]) & 0xFFFFFFFF;
		*pos +=1;
		if ((startcode & 0xFFFFFF00) != 0x100)
			continue;  /* no startcode */
		return startcode;
	}

	return 0;
}

/* determine the position of the packed marker in the userdata,
 * the number of VOPs and the position of the second VOP */
static void mpeg4p2_scan_buffer(const uint8_t *buf, int buf_size, int *pos_p, int *nb_vop, int *pos_vop2)
{
	unsigned int startcode;
	int pos, i;

	for (pos = 0; pos < buf_size;)
	{
		startcode = mpeg4p2_find_startcode(buf, buf_size, &pos);

		if (startcode == MPEG4P2_USER_DATA_STARTCODE && pos_p)
		{
			/* check if the (DivX) userdata string ends with 'p' (packed) */
			for (i = 0; i < 255 && pos + i + 1 < buf_size; i++)
			{
				if (buf[pos + i] == 'p' && buf[pos + i + 1] == '\0')
				{
					*pos_p = pos + i;
					break;
				}
			}
		}
		else if (startcode == MPEG4P2_VOP_STARTCODE && nb_vop)
		{
			*nb_vop += 1;
			if (*nb_vop == 2 && pos_vop2)
			{
				*pos_vop2 = pos - 4; /* subtract 4 bytes startcode */
			}
		}
	}
}

GST_DEBUG_CATEGORY_STATIC(mpeg4p2unpack_debug);
#define GST_CAT_DEFAULT (mpeg4p2unpack_debug)


static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) 4, " "systemstream = (boolean) false; "
        "video/x-divx, " "divxversion = (int) [ 4, 5 ]")
    );


static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) 4, " "unpacked = (boolean) true, " "systemstream = (boolean) false; ")
    );

static gboolean gst_mpeg4p2unpack_sink_event(GstPad * pad, GstObject *parent, GstEvent * event);
static GstFlowReturn gst_mpeg4p2unpack_chain(GstPad *pad, GstObject *parent, GstBuffer *buf);
static GstFlowReturn gst_mpeg4p2unpack_handle_frame(GstMpeg4P2Unpack *self, GstBuffer *buffer);
static GstStateChangeReturn gst_mpeg4p2unpack_change_state (GstElement * element, GstStateChange transition);

static GstElementClass *parent_class = NULL;

static void gst_mpeg4p2unpack_base_init(gpointer g_class)
{
	GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

	gst_element_class_add_pad_template (element_class,
			gst_static_pad_template_get (&sink_template));
	gst_element_class_add_pad_template (element_class,
			gst_static_pad_template_get (&src_template));
	gst_element_class_set_static_metadata(element_class, "Mpeg4Part2 video unpacker",
			"Codec/Parser/Video",
			"Unpacks Mpeg4Part2 video streams",
			"mx3L");
}

static void gst_mpeg4p2unpack_class_init(GstMpeg4P2UnpackClass *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);
	gstelement_class->change_state = gst_mpeg4p2unpack_change_state;
}

static void gst_mpeg4p2unpack_init(GstMpeg4P2Unpack *self, GstMpeg4P2UnpackClass * g_class)
{
	GstElement *element = GST_ELEMENT(self);

	self->b_frame = NULL;
	gint i;
	for(i=0; i < MPEG4P2_MAX_B_FRAMES_COUNT; i++)
		self->b_frames[i] = NULL;
	self->b_frames_count = 0;
	self->passthrough = FALSE;
	self->first_ip_frame_written = FALSE;
	self->second_ip_frame = NULL;
	self->buffer_duration = GST_CLOCK_TIME_NONE;

	self->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
	self->srcpad = gst_pad_new_from_static_template (&src_template, "src");
	gst_pad_set_chain_function (self->sinkpad, GST_DEBUG_FUNCPTR (gst_mpeg4p2unpack_chain));
	gst_pad_set_event_function (self->sinkpad, GST_DEBUG_FUNCPTR (gst_mpeg4p2unpack_sink_event));
	gst_element_add_pad(element, self->sinkpad);
	gst_element_add_pad(element, self->srcpad);
}

GType gst_mpeg4p2unpack_get_type(void)
{
	static GType mpeg4p2unpack_type = 0;

	if (!mpeg4p2unpack_type)
	{
		static const GTypeInfo mpeg4p2unpack_info =
		{
			sizeof (GstMpeg4P2UnpackClass),
			(GBaseInitFunc) gst_mpeg4p2unpack_base_init,
			NULL, (GClassInitFunc) gst_mpeg4p2unpack_class_init,
			NULL,
			NULL,
			sizeof (GstMpeg4P2Unpack),
			0,
			(GInstanceInitFunc) gst_mpeg4p2unpack_init,
		};

		mpeg4p2unpack_type =
				g_type_register_static (GST_TYPE_ELEMENT, "GstMpeg4P2Unpack", &mpeg4p2unpack_info, 0);

		GST_DEBUG_CATEGORY_INIT(mpeg4p2unpack_debug, "mpeg4p2unpack", 0, "MPEG4-Part2 video unpacker");
	}
	return mpeg4p2unpack_type;
}

static gboolean gst_mpeg4p2unpack_sink_event(GstPad * pad, GstObject *parent, GstEvent * event)
{
	GstMpeg4P2Unpack *self = GST_MPEG4P2UNPACK(gst_pad_get_parent(pad));
	gboolean ret = FALSE;

	GST_LOG_OBJECT(self, "%s event", GST_EVENT_TYPE_NAME(event));

	switch (GST_EVENT_TYPE (event))
	{
		case GST_EVENT_CAPS:
		{
			GstCaps *caps, *srccaps;
			gst_event_parse_caps(event, &caps);
			GST_DEBUG_OBJECT(self, "sinkcaps = %s", gst_caps_to_string(caps));

			GstStructure *structure = gst_structure_copy(gst_caps_get_structure (caps, 0));
			gint numerator, denominator;
			if (gst_structure_get_fraction (structure, "framerate", &numerator, &denominator))
			{
				self->buffer_duration = 1000 / ((double)numerator * 1000 / denominator) * GST_SECOND;
			}

			srccaps = gst_caps_new_empty();
			gst_structure_set_name(structure, "video/mpeg");
			srccaps = gst_caps_merge_structure(srccaps, structure);
			gst_caps_set_simple (srccaps, "mpegversion", G_TYPE_INT, 4, NULL);
			gst_caps_set_simple (srccaps, "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
			gst_caps_set_simple (srccaps, "unpacked", G_TYPE_BOOLEAN, TRUE, NULL);

			GST_DEBUG_OBJECT(self, "srccaps = %s", gst_caps_to_string(srccaps));
			ret = gst_pad_set_caps(self->srcpad, srccaps);
			gst_caps_unref(srccaps);
			gst_event_unref(event);
		}
			break;
		case GST_EVENT_FLUSH_STOP:
		{
			guint i;
			for (i=0; i < MPEG4P2_MAX_B_FRAMES_COUNT; i++)
			{
				if (self->b_frames[i])
				{
					gst_buffer_unref(self->b_frames[i]);
					self->b_frames[i] = NULL;
				}
			}
			if (self->second_ip_frame)
			{
				gst_buffer_unref(self->second_ip_frame);
				self->second_ip_frame = NULL;
			}
			if (self->b_frame)
			{
				gst_buffer_unref(self->b_frame);
				self->b_frame = NULL;
			}
			self->b_frames_count = 0;
			self->first_ip_frame_written = FALSE;
			ret = gst_pad_push_event(self->srcpad, event);
		}
			break;
		default:
			ret = gst_pad_push_event(self->srcpad, event);
			break;
	}
	return ret;
}

static GstFlowReturn gst_mpeg4p2unpack_chain(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
	guint8 *data;
	gsize data_len;
	GstMapInfo buffermap;
	GstFlowReturn ret = GST_FLOW_OK;
	GstMpeg4P2Unpack *self = GST_MPEG4P2UNPACK(GST_PAD_PARENT(pad));

	if (self->buffer_duration == GST_CLOCK_TIME_NONE)
	{
		if (!GST_BUFFER_DURATION_IS_VALID(buffer))
		{
			GST_WARNING_OBJECT(self, "Cannot retrieve buffer duration, dropping");
			gst_buffer_unref(buffer);
			return GST_FLOW_OK;
		}
		self->buffer_duration = GST_BUFFER_DURATION(buffer);
	}

	gst_buffer_map(buffer, &buffermap, GST_MAP_READ);
	data = buffermap.data;
	data_len = buffermap.size;

	int pos_p = -1, nb_vop = 0, pos_vop2 = -1;
	mpeg4p2_scan_buffer(data, data_len, &pos_p, &nb_vop, &pos_vop2);
	// GST_LOG_OBJECT(self, "pos_p=%d, num_vop=%d, pos_vop2=%d", pos_p, nb_vop, pos_vop2);

	/* if we don't have userdata we can unmap buffer */
	if (pos_p < 0)
	{
		gst_buffer_unmap(buffer, &buffermap);
		data = NULL;
	}

	if (pos_vop2 >= 0)
	{
		if (self->b_frame)
		{
			GST_WARNING_OBJECT(self, "Missing one N-VOP packet, discarding one B-frame");
			gst_buffer_unref(self->b_frame);
			self->b_frame = NULL;
		}
		// GST_LOG_OBJECT(self, "Storing B-Frame of packed PB-Frame");
		self->b_frame = gst_buffer_copy_region(buffer, GST_BUFFER_COPY_ALL, pos_vop2, data_len - pos_vop2);
		GST_BUFFER_DTS(self->b_frame) = GST_BUFFER_DTS(buffer) + self->buffer_duration;
	}

	if (nb_vop > 2)
	{
		GST_WARNING_OBJECT(self, "Found %d VOP headers in one packet, only unpacking one.", nb_vop);
	}

	if (nb_vop == 1 && self->b_frame)
	{
		// GST_LOG_OBJECT(self, "Push previous B-Frame");
		ret = gst_mpeg4p2unpack_handle_frame(self, self->b_frame);
		if (data_len <= MPEG4P2_MAX_NVOP_SIZE)
		{
			// GST_LOG_OBJECT(self, "Skipping N-VOP");
			self->b_frame = NULL;
			gst_buffer_unref(buffer);
		}
		else
		{
			// GST_LOG_OBJECT(self, "Store B-Frame");
			GST_BUFFER_DTS(buffer) = GST_BUFFER_DTS(self->b_frame) + self->buffer_duration;
			self->b_frame = buffer;
		}
	}
	else if (nb_vop >= 2)
	{
		// GST_LOG_OBJECT(self, "Push P-frame of packed PB-Frame");
		GstBuffer *p_frame = gst_buffer_copy_region(buffer, GST_BUFFER_COPY_ALL, 0, pos_vop2);
		ret = gst_mpeg4p2unpack_handle_frame(self, p_frame);
		gst_buffer_unref(buffer);
	}
	else if (pos_p >= 0)
	{
		// GST_LOG_OBJECT(self, "Updating DivX userdata (replacing trailing 'p')");
		gst_buffer_unmap(buffer, &buffermap);
		gst_buffer_map(buffer, &buffermap, GST_MAP_WRITE);
		data = buffermap.data;
		data[pos_p] = 'n';
		gst_buffer_unmap(buffer, &buffermap);
		data = NULL;
		ret = gst_mpeg4p2unpack_handle_frame(self, buffer);
	}
	else
	{
		ret = gst_mpeg4p2unpack_handle_frame(self, buffer);
	}
	return ret;
}

/* computes PTS from DTS, for better ilustration see:
 * https://software.intel.com/sites/default/files/pts-dts_shift_explain.gif */
static GstFlowReturn gst_mpeg4p2unpack_handle_frame(GstMpeg4P2Unpack *self, GstBuffer *buffer)
{
	/* matroska container know only about PTS */
	if(self->passthrough || !GST_BUFFER_DTS_IS_VALID(buffer))
	{
		return gst_pad_push(self->srcpad, buffer);
	}

	guint8 *data;
	gsize data_len = 0;
	GstMapInfo buffermap;
	GstFlowReturn ret = GST_FLOW_OK;

	gst_buffer_map(buffer, &buffermap, GST_MAP_READ);
	data = buffermap.data;
	data_len = buffermap.size;

	unsigned int pos = 0;
	while (pos < data_len)
	{
		if (memcmp(&data[pos], "\x00\x00\x01\xb6", 4))
		{
			pos++;
			continue;
		}
		pos += 4;

		// .X. - means pushed X-frame
		// <X> - means stored X-frame
		// [X] - means current X-frame

		switch ((data[pos] & 0xC0) >> 6)
		{
			case 0: // I-Frame
			case 1: // P-Frame
				// . . < > [P] -> PTS(P) = DTS(P), push P
				if (!self->first_ip_frame_written)
				{
					// GST_LOG_OBJECT(self, "First (IP)-Frame pushed");
					self->first_ip_frame_written = TRUE;
					GST_BUFFER_PTS(buffer) = GST_BUFFER_DTS(buffer) + self->buffer_duration;
					goto push_buffer;
				}
				// .P0. < > [P1] -> store P1
				else if (!self->second_ip_frame)
				{
					// GST_LOG_OBJECT(self, "Store second (IP)-Frame (1)");
					self->second_ip_frame = buffer;
					goto done;
				}
				else
				{
					// GST_LOG_OBJECT(self, "B-Frames count in between (IP)-Frames = %d", self->b_frames_count);
					if (!self->b_frames_count)
					{
						// .P0. <P1> [P2] ->  PTS(P1) = DTS(P1), push P1, store [P2]
						GST_BUFFER_PTS(self->second_ip_frame) = GST_BUFFER_DTS(self->second_ip_frame) + self->buffer_duration;
						ret = gst_pad_push(self->srcpad, self->second_ip_frame);
						self->second_ip_frame = buffer;
						// GST_LOG_OBJECT(self, "Store second (IP)-Frame (2)");
						goto done;
					}
					else
					{
						// .P0. <P1,B1,B2..Bx>[P2] -> PTS(P1) = DTS(Bx), PTS(B1) = DTS(P1), PTS(By) = DTS(By-1), push P1 B1..Bx, store P2

						// (DTS)IPB1B2B3 -> (PTS)IB1B2B3P

						// (PTS)P = (DTS)B2
						GST_BUFFER_PTS(self->second_ip_frame) = GST_BUFFER_DTS(self->b_frames[self->b_frames_count-1]) + self->buffer_duration;
						// (PTS)B1 = (DTS)P
						GST_BUFFER_PTS(self->b_frames[0]) = GST_BUFFER_DTS(self->second_ip_frame) + self->buffer_duration;
						guint i;
						for (i=1; i < self->b_frames_count; i++)
						{
							// (PTS)B2 = (DTS)B1
							// (PTS)B3 = (DTS)B2
							// ..
							GST_BUFFER_PTS(self->b_frames[i]) = GST_BUFFER_DTS(self->b_frames[i-1]) + self->buffer_duration;
						}
						ret = gst_pad_push(self->srcpad, self->second_ip_frame);
						self->second_ip_frame = NULL;
						if (ret != GST_FLOW_OK)
						{
							GST_DEBUG_OBJECT(self, "Error when pushing buffer (1)");
							goto drop_buffer;
						}
						for (i=0; i < self->b_frames_count; i++)
						{
							ret = gst_pad_push(self->srcpad, self->b_frames[i]);
							self->b_frames[i] = NULL;
							if (ret != GST_FLOW_OK)
							{
								GST_DEBUG_OBJECT(self, "Error when pushing buffer (2)");
								goto drop_buffer;
							}
						}
						self->b_frames_count = 0;
						self->second_ip_frame = buffer;
						// GST_LOG_OBJECT(self, "Store second (IP)-Frame (3)");
						goto done;
					}
				}
				break;
			case 3: // S-Frame
				break;
			case 2: // B-Frame
				if (!self->second_ip_frame)
				{
					GST_INFO_OBJECT(self, "Cannot predict B-Frame without surrounding I/P-Frames, dropping...");
					goto drop_buffer;
				}
				if (GST_BUFFER_PTS (buffer) != GST_CLOCK_TIME_NONE)
				{
					GST_INFO_OBJECT(self, "We have B frames with PTS timestamps set! setting passthrough mode");
					ret = gst_pad_push(self->srcpad, self->second_ip_frame);
					self->second_ip_frame = NULL;
					if (ret != GST_FLOW_OK)
					{
						GST_DEBUG_OBJECT(self, "Error when pushing buffer (3)");
						goto drop_buffer;
					}
					self->passthrough = TRUE;
					goto push_buffer;
				}
				else if(self->b_frames_count == MPEG4P2_MAX_B_FRAMES_COUNT)
				{
					GST_ERROR_OBJECT(self, "Oops max B-frames count = %d, reached", MPEG4P2_MAX_B_FRAMES_COUNT);
					ret = GST_FLOW_ERROR;
					goto drop_buffer;
				}
				else
				{
					// GST_LOG_OBJECT(self, "Store B-Frame [%d]", self->b_frames_count);
					self->b_frames[self->b_frames_count++] = buffer;
					goto done;
				}
				break;
			case 4: // N-Frame
			default:
				g_warning("unhandled divx5/xvid frame type %d\n", (data[pos] & 0xC0) >> 6);
				break;
		}
	}
push_buffer:
	gst_buffer_unmap(buffer, &buffermap);
	return gst_pad_push(self->srcpad, buffer);
drop_buffer:
	gst_buffer_unmap(buffer, &buffermap);
	gst_buffer_unref(buffer);
	return ret;
done:
	gst_buffer_unmap(buffer, &buffermap);
	return ret;
}

static GstStateChangeReturn gst_mpeg4p2unpack_change_state(GstElement * element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstMpeg4P2Unpack *self = GST_MPEG4P2UNPACK(element);

	switch (transition)
	{
		case GST_STATE_CHANGE_NULL_TO_READY:
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
			break;
		default:
			break;
	}

	ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

	switch (transition)
	{
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
		{
			guint i;
			for (i=0; i < MPEG4P2_MAX_B_FRAMES_COUNT; i++)
			{
				if (self->b_frames[i])
				{
					gst_buffer_unref(self->b_frames[i]);
					self->b_frames[i] = NULL;
				}
			}
			if (self->second_ip_frame)
			{
				gst_buffer_unref(self->second_ip_frame);
				self->second_ip_frame = NULL;
			}
			if (self->b_frame)
			{
				gst_buffer_unref(self->b_frame);
				self->b_frame = NULL;
			}
			self->b_frames_count = 0;
			self->first_ip_frame_written = FALSE;
			self->passthrough = FALSE;
		}
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			break;
		default:
			break;
	}

	return ret;
}

static gboolean plugin_init (GstPlugin * plugin)
{
	/* we need higher rank than mpeg4videoparse */
	if (!gst_element_register (plugin, "mpeg4p2unpack", GST_RANK_PRIMARY+2,
					GST_TYPE_MPEG4P2UNPACK))
		return FALSE;
	return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		mpeg4p2unpack,
		"MPEG4-Part2 video unpacker",
		plugin_init, VERSION, "LGPL", "GStreamer", "http://gstreamer.net/");
