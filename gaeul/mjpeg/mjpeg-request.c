#include "mjpeg/mjpeg-request.h"

/* *INDENT-OFF* */
G_DEFINE_BOXED_TYPE(GaeulMjpegRequest, gaeul_mjpeg_request, gaeul_mjpeg_request_ref, gaeul_mjpeg_request_unref)
/* *INDENT-ON* */

GaeulMjpegRequest *
gaeul_mjpeg_request_new (const gchar * uid, const gchar * rid,
    guint protocol_latency, guint demux_latency, gint width, gint height,
    gint fps, guint flip, guint orientation)
{
  GaeulMjpegRequest *r = g_new0 (GaeulMjpegRequest, 1);

  r->refcount = 1;

  r->uid = g_strdup (uid);
  r->rid = g_strdup (rid);

  r->protocol_latency = protocol_latency;
  r->demux_latency = demux_latency;

  r->width = width;
  r->height = height;
  r->fps = fps;

  r->flip = flip;
  r->orientation = orientation;

  return r;
}

GaeulMjpegRequest *
gaeul_mjpeg_request_ref (GaeulMjpegRequest * self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_int_inc (&self->refcount);

  return self;
}

void
gaeul_mjpeg_request_unref (GaeulMjpegRequest * self)
{
  g_return_if_fail (self != NULL);

  if (g_atomic_int_dec_and_test (&self->refcount)) {
    g_free (self->uid);
    g_free (self->rid);
    g_free (self);
  }
}

guint
gaeul_mjpeg_request_hash (GaeulMjpegRequest * self)
{
  g_autofree gchar *str = NULL;

  str = g_strdup_printf ("%s%s"
      "%" G_GUINT32_FORMAT "%" G_GUINT32_FORMAT
      "%" G_GINT32_FORMAT "%" G_GINT32_FORMAT "%" G_GINT32_FORMAT
      "%" G_GUINT32_FORMAT "%" G_GUINT32_FORMAT,
      self->uid, self->rid, self->protocol_latency, self->demux_latency,
      self->width, self->height, self->fps, self->flip, self->orientation);

  return g_str_hash (str);
}

gboolean
gaeul_mjpeg_request_equal (GaeulMjpegRequest * r1, GaeulMjpegRequest * r2)
{
  g_return_val_if_fail (r1 != NULL, FALSE);
  g_return_val_if_fail (r2 != NULL, FALSE);

  return
      (g_strcmp0 (r1->uid, r2->uid) == 0) && (g_strcmp0 (r1->rid, r2->rid) == 0)
      && (r1->protocol_latency == r2->protocol_latency)
      && (r1->demux_latency == r2->demux_latency) && (r1->height == r2->height)
      && (r1->width == r2->width) && (r1->fps == r2->fps)
      && (r1->flip == r2->flip) && (r1->orientation == r2->orientation);
}

guint
gaeul_mjpeg_request_parameter_hash (GaeulMjpegRequest * self)
{
  g_autofree gchar *str = NULL;
  str =
      g_strdup_printf ("%u/%u/%d/%d/%d/%u/%u", self->protocol_latency,
      self->demux_latency, self->width, self->height, self->fps,
      self->flip, self->orientation);
  return g_str_hash (str);
}
