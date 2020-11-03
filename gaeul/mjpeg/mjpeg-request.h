/**
 *  Copyright 2020 SK Telecom Co., Ltd.
 *    Author: Jeongseok Kim <jeongseok.kim@sk.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef __GAEUL_MJPEG_REQUEST_H__
#define __GAEUL_MJPEG_REQUEST_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GAEUL_TYPE_MJPEG_REQUEST    (gaeul_mjpeg_requet_get_type())

typedef struct _GaeulMjpegRequest
{
  gchar *uid;
  gchar *rid;

  guint protocol_latency;
  guint demux_latency;

  gint width;
  gint height;
  gint fps;

  guint flip;
  guint orientation;

  /*< private >*/
  gint refcount;
} GaeulMjpegRequest;

GType               gaeul_mjpeg_request_get_type (void);

GaeulMjpegRequest  *gaeul_mjpeg_request_new    (const gchar *uid, const gchar *rid,
                                                guint protocol_latency,
                                                guint demux_latency,
                                                gint width, gint height, gint fps,
                                                guint flip, guint orientation);

GaeulMjpegRequest  *gaeul_mjpeg_request_ref    (GaeulMjpegRequest *self);

void                gaeul_mjpeg_request_unref  (GaeulMjpegRequest *self);

guint               gaeul_mjpeg_request_hash   (GaeulMjpegRequest *self);

guint               gaeul_mjpeg_request_parameter_hash
                                               (GaeulMjpegRequest *self);

gboolean            gaeul_mjpeg_request_equal  (GaeulMjpegRequest *r1, GaeulMjpegRequest *r2);



G_DEFINE_AUTOPTR_CLEANUP_FUNC                  (GaeulMjpegRequest, gaeul_mjpeg_request_unref)

G_END_DECLS

#endif //__GAEUL_MJPEG_REQUEST_H__
