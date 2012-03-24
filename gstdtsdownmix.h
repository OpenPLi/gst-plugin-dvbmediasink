#ifndef __GST_DTSDOWNMIX_H__
#define __GST_DTSDOWNMIX_H__

#include <dca.h>

G_BEGIN_DECLS

#define GST_TYPE_DTSDOWNMIX \
  (gst_dtsdownmix_get_type())
#define GST_DTSDOWNMIX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DTSDOWNMIX,GstDtsDownmix))
#define GST_DTSDOWNMIX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DTSDOWNMIX,GstDtsDownmixClass))
#define GST_IS_DTSDOWNMIX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DTSDOWNMIX))
#define GST_IS_DTSDOWNMIX_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DTSDOWNMIX))

typedef struct _GstDtsDownmix GstDtsDownmix;
typedef struct _GstDtsDownmixClass GstDtsDownmixClass;

struct _GstDtsDownmix 
{
	GstElement element;

	/* pads */
	GstPad *sinkpad, *srcpad;

	GstSegment segment;
	gboolean sent_segment;

	char stcmode[32];

	/* stream properties */
	unsigned char dtsheader[4];
	gint bit_rate;
	gint sample_rate;
	gint stream_channels;
	gint using_channels;
	gint sample_width;
	gint framelength;

	/* decoding properties */
	sample_t *samples;
	dca_state_t *state;

	GstClockTime timestamp;

	/* Data left over from the previous buffer */
	GstBuffer *cache;
};

struct _GstDtsDownmixClass
{
	GstElementClass parent_class;
};

G_END_DECLS

#endif /* __GST_DTSDOWNMIX_H__ */
