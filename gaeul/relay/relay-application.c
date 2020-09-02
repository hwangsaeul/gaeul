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
#include "gaeul/relay/relay-application.h"

#include <hwangsae/hwangsae.h>

typedef enum
{
  PROP_SINK_PORT = 1,
  PROP_SOURCE_PORT,
  PROP_EXTERNAL_IP,

  /*< private > */
  PROP_LAST
} GaeulRelayApplicationProperty;

static GParamSpec *properties[PROP_LAST] = { NULL };

struct _GaeulRelayApplication
{
  GaeulApplication parent;

  HwangsaeRelay *relay;

  guint sink_port;
  guint source_port;
  gchar *external_ip;

  GSettings *settings;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulRelayApplication, gaeul_relay_application, GAEUL_TYPE_APPLICATION)
/* *INDENT-ON* */

static void
gaeul_relay_application_on_io_error (GaeulRelayApplication * app,
    GInetSocketAddress * address, GError * error)
{
  g_autofree gchar *ip = NULL;

  ip = g_inet_address_to_string (g_inet_socket_address_get_address (address));

  g_debug ("(%s:%d) %s", ip, g_inet_socket_address_get_port (address),
      error->message);
}

static void
gaeul_relay_application_activate (GApplication * app)
{
  GaeulRelayApplication *self = GAEUL_RELAY_APPLICATION (app);

  self->settings = gaeul_gsettings_new (GAEUL_RELAY_APPLICATION_SCHEMA_ID,
      gaeul_application_get_config_path (GAEUL_APPLICATION (self)));

  g_settings_bind (self->settings, "uid", self, "uid", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "sink-port", self, "sink-port",
      G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "source-port", self, "source-port",
      G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "external-ip", self, "external-ip",
      G_SETTINGS_BIND_DEFAULT);

  self->relay =
      hwangsae_relay_new (self->external_ip, self->sink_port,
      self->source_port);

  g_signal_connect_swapped (self->relay, "io-error",
      G_CALLBACK (gaeul_relay_application_on_io_error), self);

  g_debug ("Starting hwangsae (sink-uri: %s, source-uri: %s)",
      hwangsae_relay_get_sink_uri (self->relay),
      hwangsae_relay_get_source_uri (self->relay));

  hwangsae_relay_start (self->relay);

  G_APPLICATION_CLASS (gaeul_relay_application_parent_class)->activate (app);
}

static void
gaeul_relay_application_shutdown (GApplication * app)
{
  GaeulRelayApplication *self = GAEUL_RELAY_APPLICATION (app);

  g_clear_object (&self->relay);
  g_clear_object (&self->settings);

  G_APPLICATION_CLASS (gaeul_relay_application_parent_class)->shutdown (app);
}

static void
gaeul_relay_application_dispose (GObject * object)
{
  GaeulRelayApplication *self = GAEUL_RELAY_APPLICATION (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->relay);

  G_OBJECT_CLASS (gaeul_relay_application_parent_class)->dispose (object);
}

static void
gaeul_relay_application_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GaeulRelayApplication *self = GAEUL_RELAY_APPLICATION (object);

  switch (prop_id) {
    case PROP_SINK_PORT:
      g_value_set_uint (value, self->sink_port);
      break;
    case PROP_SOURCE_PORT:
      g_value_set_uint (value, self->source_port);
      break;
    case PROP_EXTERNAL_IP:
      g_value_set_string (value, self->external_ip);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_relay_application_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GaeulRelayApplication *self = GAEUL_RELAY_APPLICATION (object);

  switch (prop_id) {
    case PROP_SINK_PORT:
      self->sink_port = g_value_get_uint (value);
      break;
    case PROP_SOURCE_PORT:
      self->source_port = g_value_get_uint (value);
      break;
    case PROP_EXTERNAL_IP:
      g_free (self->external_ip);
      self->external_ip = g_value_dup_string (value);
      g_debug ("external-ip: %s", self->external_ip);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_relay_application_class_init (GaeulRelayApplicationClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->get_property = gaeul_relay_application_get_property;
  object_class->set_property = gaeul_relay_application_set_property;
  object_class->dispose = gaeul_relay_application_dispose;

  properties[PROP_SINK_PORT] =
      g_param_spec_uint ("sink-port", "sink-port",
      "sink-port", 1, 65535, 7777, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_SOURCE_PORT] =
      g_param_spec_uint ("source-port", "source-port",
      "source-port", 1, 65535, 8888,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_EXTERNAL_IP] =
      g_param_spec_string ("external-ip", "external-ip", "external-ip",
      NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties),
      properties);

  app_class->activate = gaeul_relay_application_activate;
  app_class->shutdown = gaeul_relay_application_shutdown;
}

static void
gaeul_relay_application_init (GaeulRelayApplication * self)
{
}
