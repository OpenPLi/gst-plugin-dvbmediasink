#ifndef __GST_MPEG4P2UNPACK_H__
#define __GST_MPEG4P2UNPACK_H__

G_BEGIN_DECLS

#define GST_TYPE_MPEG4P2UNPACK \
  (gst_mpeg4p2unpack_get_type())
#define GST_MPEG4P2UNPACK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPEG4P2UNPACK,GstMpeg4P2Unpack))
#define GST_MPEG4P2UNPACK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPEG4P2UNPACK,GstMpeg4P2UnpackClass))
#define GST_IS_MPEG4P2UNPACK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPEG4P2UNPACK))
#define GST_IS_MPEG4P2UNPACK_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPEG4P2UNPACK))

#define MPEG4P2_MAX_NVOP_SIZE        8
/* FIXME find out what what is the MAX B frames count, current
 * value is just based on observations */
#define MPEG4P2_MAX_B_FRAMES_COUNT   5
#define MPEG4P2_VOP_STARTCODE        0x1B6
#define MPEG4P2_USER_DATA_STARTCODE  0x1B2

typedef struct _GstMpeg4P2Unpack GstMpeg4P2Unpack;
typedef struct _GstMpeg4P2UnpackClass GstMpeg4P2UnpackClass;

struct _GstMpeg4P2Unpack
{
	GstElement element;

	/* pads */
	GstPad *sinkpad, *srcpad;

	/* unpacking mpeg4p2 */
	GstBuffer *b_frame;

	/* computing PTS from DTS for mpeg4p2 */
	gint b_frames_count;
	gboolean first_ip_frame_written, passthrough;
	GstBuffer *b_frames[MPEG4P2_MAX_B_FRAMES_COUNT];
	GstBuffer *second_ip_frame;
	GstClockTime buffer_duration;
};

struct _GstMpeg4P2UnpackClass
{
	GstElementClass parent_class;
};

G_END_DECLS

#endif /* __GST_MPEG4P2UNPACK_H__ */
