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

#include "source-application.h"

/* *INDENT-OFF* */
#if !GLIB_CHECK_VERSION(2,57,1)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GEnumClass, g_type_class_unref)
#endif
/* *INDENT-ON* */

typedef enum
{
  PROP_ID = 1,
  /*< private > */

  PROP_LAST
} _GaeulSourceApplicationProperty;

static GParamSpec *properties[PROP_LAST] = { NULL };

struct _GaeulSourceApplication
{
  GaeulApplication parent;

  gchar *id;

  GSettings *settings;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulSourceApplication, gaeul_source_application, GAEUL_TYPE_APPLICATION)
/* *INDENT-ON* */

static void
gaeul_source_application_activate (GApplication * app)
{
  g_debug ("activate");

  // TBD
}

static void
gaeul_source_application_shutdown (GApplication * app)
{
  g_debug ("shutdown");

  // TBD

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->shutdown (app);
}

static void
gaeul_source_application_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  switch (prop_id) {
    case PROP_ID:
      g_value_set_string (value, self->id);
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
    case PROP_ID:
      g_free (self->id);
      self->id = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_source_application_dispose (GObject * object)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  g_clear_pointer (&self->id, g_free);

  G_OBJECT_CLASS (gaeul_source_application_parent_class)->dispose (object);
}

static void
gaeul_source_application_class_init (GaeulSourceApplicationClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->get_property = gaeul_source_application_get_property;
  object_class->set_property = gaeul_source_application_set_property;
  object_class->dispose = gaeul_source_application_dispose;

  properties[PROP_ID] =
      g_param_spec_string ("id", "id", "id", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties),
      properties);

  app_class->activate = gaeul_source_application_activate;
  app_class->shutdown = gaeul_source_application_shutdown;
}

static void
gaeul_source_application_init (GaeulSourceApplication * self)
{

}
