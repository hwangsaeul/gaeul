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

#include "config.h"

#include "gaeul.h"
#include "source-application.h"

#include <gaeguli/gaeguli.h>

/* *INDENT-OFF* */
#if !GLIB_CHECK_VERSION(2,57,1)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GEnumClass, g_type_class_unref)
#endif
/* *INDENT-ON* */

typedef struct _GaeguliNest
{
  gint refcount;

  GaeguliPipeline *pipeline;
  GaeguliFifoTransmit *transmit;

  guint transmit_id;
} GaeguliNest;

static GaeguliNest *
gaeguli_nest_new (GaeguliPipeline * pipeline, GaeguliFifoTransmit * transmit)
{
  GaeguliNest *nest = g_new0 (GaeguliNest, 1);

  nest->refcount = 1;
  nest->pipeline = g_object_ref (pipeline);
  nest->transmit = g_object_ref (transmit);

  return nest;
}

static GaeguliNest *
gaeguli_nest_ref (GaeguliNest * nest)
{
  g_return_val_if_fail (nest != NULL, NULL);
  g_return_val_if_fail (nest->pipeline != NULL, NULL);
  g_return_val_if_fail (nest->transmit != NULL, NULL);

  g_atomic_int_inc (&nest->refcount);

  return nest;
}

static void
gaeguli_nest_start (GaeguliNest * nest, GaeguliVideoCodec codec,
    GaeguliVideoResolution resolution, guint fps, guint bitrates)
{
  if (nest->transmit_id == 0) {
    const gchar *fifo = gaeguli_fifo_transmit_get_fifo (nest->transmit);
    nest->transmit_id =
        gaeguli_pipeline_add_fifo_target_full (nest->pipeline, codec,
        resolution, fps, bitrates, fifo, NULL);
  }
}

static void
gaeguli_nest_stop (GaeguliNest * nest)
{
  if (nest->transmit_id > 0) {
    gaeguli_pipeline_remove_target (nest->pipeline, nest->transmit_id, NULL);
    gaeguli_fifo_transmit_stop (nest->transmit, nest->transmit_id, NULL);
    nest->transmit_id = 0;
  }
}

static void
gaeguli_nest_unref (GaeguliNest * nest)
{
  g_return_if_fail (nest != NULL);
  g_return_if_fail (nest->pipeline != NULL);
  g_return_if_fail (nest->transmit != NULL);

  if (g_atomic_int_dec_and_test (&nest->refcount)) {

    gaeguli_nest_stop (nest);

    g_clear_object (&nest->pipeline);
    g_clear_object (&nest->transmit);
    g_free (nest);
  }
}

/* *INDENT-OFF* */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GaeguliNest, gaeguli_nest_unref)
/* *INDENT-ON* */

typedef enum
{
  PROP_TMPDIR = 1,

  /*< private > */

  PROP_LAST
} _GaeulSourceApplicationProperty;

static GParamSpec *properties[PROP_LAST] = { NULL };

struct _GaeulSourceApplication
{
  GaeulApplication parent;

  GSettings *settings;
  gchar *tmpdir;

  GList *gaegulis;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulSourceApplication, gaeul_source_application, GAEUL_TYPE_APPLICATION)
/* *INDENT-ON* */

static void
gaeul_source_application_startup (GApplication * app)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);
  g_autofree gchar *source_conf_dir = NULL;
  const gchar *source_conf_file = NULL;
  g_autoptr (GDir) confd = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GaeguliFifoTransmit) transmit = NULL;

  g_debug ("startup");

  g_clear_object (&self->settings);
  self->settings = gaeul_gsettings_new (GAEUL_SOURCE_APPLICATION_SCHEMA_ID,
      gaeul_application_get_config_path (GAEUL_APPLICATION (self))
      );

  g_settings_bind (self->settings, "uid", self, "uid", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "tmpdir", self, "tmpdir",
      G_SETTINGS_BIND_DEFAULT);

  source_conf_dir = g_settings_get_string (self->settings, "source-conf-dir");
  confd = g_dir_open (source_conf_dir, 0, &error);

  transmit = gaeguli_fifo_transmit_new_full (self->tmpdir);

  while ((source_conf_file = g_dir_read_name (confd)) != NULL) {

    g_autoptr (GaeguliNest) nest = NULL;
    g_autoptr (GaeguliPipeline) pipeline = NULL;


    g_autofree gchar *name = NULL;
    g_autofree gchar *device = NULL;

    g_autofree gchar *target_uri = NULL;
    g_autofree gchar *target_host = NULL;
    g_autofree gchar *target_mode = NULL;
    guint target_port = 0;

    g_autofree gchar *sconf_path =
        g_build_filename (source_conf_dir, source_conf_file, NULL);
    g_autoptr (GSettings) ssettings =
        gaeul_gsettings_new (GAEUL_SOURCE_APPLICATION_SCHEMA_ID ".Node",
        sconf_path);

    name = g_settings_get_string (ssettings, "name");
    device = g_settings_get_string (ssettings, "device");
    target_uri = g_settings_get_string (ssettings, "target-uri");

    if (!gaeul_parse_srt_uri (target_uri, &target_host, &target_port,
            &target_mode)) {
      continue;
    }



    /* TODO: do not hard code parameters */
    pipeline =
        gaeguli_pipeline_new_full (GAEGULI_VIDEO_SOURCE_VIDEOTESTSRC, device,
        GAEGULI_ENCODING_METHOD_GENERAL);

    nest = gaeguli_nest_new (pipeline, transmit);

    gaeguli_nest_start (nest, GAEGULI_VIDEO_CODEC_H264,
        GAEGULI_VIDEO_RESOLUTION_640X480, 15, 2000000);
    self->gaegulis = g_list_prepend (self->gaegulis, gaeguli_nest_ref (nest));
  }

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->startup (app);
}

static void
gaeul_source_application_activate (GApplication * app)
{
  g_debug ("activate");

  // TBD

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->activate (app);
}

static void
gaeul_source_application_shutdown (GApplication * app)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);

  g_debug ("shutdown");

  g_list_free_full (self->gaegulis, (GDestroyNotify) gaeguli_nest_unref);

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->shutdown (app);
}

static void
gaeul_source_application_dispose (GObject * object)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  g_clear_object (&self->settings);
  g_clear_pointer (&self->tmpdir, g_free);

  G_OBJECT_CLASS (gaeul_source_application_parent_class)->dispose (object);
}

static void
gaeul_source_application_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  switch (prop_id) {
    case PROP_TMPDIR:
      g_value_set_string (value, self->tmpdir);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_source_application_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  switch (prop_id) {
    case PROP_TMPDIR:
      g_free (self->tmpdir);
      self->tmpdir = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_source_application_class_init (GaeulSourceApplicationClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = gaeul_source_application_dispose;
  object_class->set_property = gaeul_source_application_set_property;
  object_class->get_property = gaeul_source_application_get_property;

  properties[PROP_TMPDIR] =
      g_param_spec_string ("tmpdir", "tmpdir", "tmpdir", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties),
      properties);

  app_class->startup = gaeul_source_application_startup;
  app_class->activate = gaeul_source_application_activate;
  app_class->shutdown = gaeul_source_application_shutdown;
}

static void
gaeul_source_application_init (GaeulSourceApplication * self)
{
}
