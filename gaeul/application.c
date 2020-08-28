/**
 *  Copyright 2019-2020 SK Telecom Co., Ltd.
 *    Author: Jakub Adam <jakub.adam@collabora.com>
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

#include "application.h"
#include "enumtypes.h"

typedef enum
{
  PROP_UID = 1,
  PROP_CONFIG_PATH,
  PROP_DBUS_TYPE,

  /*< private > */

  PROP_LAST
} _GaeulApplicationProperty;

static GParamSpec *properties[PROP_LAST] = { NULL };

typedef struct
{
  gint exit_status;

  gchar *uid;
  gchar *config_path;
  GaeulApplicationDBusType dbus_type;

} GaeulApplicationPrivate;

/* *INDENT-OFF* */
G_DEFINE_TYPE_WITH_PRIVATE (GaeulApplication, gaeul_application, G_TYPE_APPLICATION)
/* *INDENT-ON* */

int
gaeul_application_run (GaeulApplication * self, int argc, char **argv)
{
  GaeulApplicationPrivate *priv = gaeul_application_get_instance_private (self);

  int ret = g_application_run (G_APPLICATION (self), argc, argv);

  return (ret == 0) ? priv->exit_status : ret;
}

#if 0

/** FIXME: When trying to acquire system bus on nvidia,
  * it is always failed. This should be fixed later */

static void
_dbus_name_acquired (GDBusConnection * connection, const gchar * name,
    gpointer user_data)
{
  GaeulApplication *self = user_data;
  g_autoptr (GError) error = NULL;

  G_APPLICATION_GET_CLASS (self)->dbus_register (G_APPLICATION (self),
      connection, name, &error);

  if (error) {
    GaeulApplicationPrivate *priv =
        gaeul_application_get_instance_private (self);

    g_debug ("Failed to register D-Bus interface '%s' (reason: %s)", name,
        error->message);

    priv->exit_status = 1;
    g_application_quit (G_APPLICATION (self));
  }
}

static void
_dbus_name_lost (GDBusConnection * connection, const gchar * name,
    gpointer user_data)
{
  GaeulApplication *self = user_data;
  GaeulApplicationPrivate *priv = gaeul_application_get_instance_private (self);

  if (connection) {
    g_warning ("Couldn't acquire '%s' on D-Bus", name);
  } else {
    g_warning ("Couldn't connect to D-Bus");
  }

  priv->exit_status = 1;
  g_application_quit (G_APPLICATION (self));
}

#endif

static void
gaeul_application_startup (GApplication * app)
{
  G_APPLICATION_CLASS (gaeul_application_parent_class)->startup (app);

#if 0
  if (getppid () == 1) {
    g_bus_own_name (G_BUS_TYPE_SYSTEM, g_application_get_application_id (app),
        0, NULL, _dbus_name_acquired, _dbus_name_lost, app, NULL);
  }
#endif
}

static void
gaeul_application_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GaeulApplication *self = GAEUL_APPLICATION (object);
  GaeulApplicationPrivate *priv = gaeul_application_get_instance_private (self);

  switch (prop_id) {
    case PROP_UID:
      g_value_set_string (value, priv->uid);
      break;
    case PROP_CONFIG_PATH:
      g_value_set_string (value, priv->config_path);
      break;
    case PROP_DBUS_TYPE:
      g_value_set_enum (value, priv->dbus_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_application_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GaeulApplication *self = GAEUL_APPLICATION (object);
  GaeulApplicationPrivate *priv = gaeul_application_get_instance_private (self);

  switch (prop_id) {
    case PROP_UID:
      g_free (priv->uid);
      priv->uid = g_value_dup_string (value);
      break;
    case PROP_CONFIG_PATH:
      g_free (priv->config_path);
      priv->config_path = g_value_dup_string (value);
      break;
    case PROP_DBUS_TYPE:
      priv->dbus_type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gaeul_application_dispose (GObject * object)
{
  GaeulApplication *self = GAEUL_APPLICATION (object);
  GaeulApplicationPrivate *priv = gaeul_application_get_instance_private (self);

  g_clear_pointer (&priv->uid, g_free);
  g_clear_pointer (&priv->config_path, g_free);

  G_OBJECT_CLASS (gaeul_application_parent_class)->dispose (object);
}

static void
gaeul_application_class_init (GaeulApplicationClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);


  object_class->get_property = gaeul_application_get_property;
  object_class->set_property = gaeul_application_set_property;
  object_class->dispose = gaeul_application_dispose;

  properties[PROP_UID] =
      g_param_spec_string ("uid", "uid", "uid", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_CONFIG_PATH] =
      g_param_spec_string ("config-path", "config-path", "config-path", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_DBUS_TYPE] =
      g_param_spec_enum ("dbus-type", "dbus-type", "dbus-type",
      GAEUL_TYPE_APPLICATION_DBUS_TYPE, GAEUL_APPLICATION_DBUS_TYPE_SESSION,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties),
      properties);

  app_class->startup = gaeul_application_startup;
}

static void
gaeul_application_init (GaeulApplication * self)
{
  GaeulApplicationPrivate *priv = gaeul_application_get_instance_private (self);

  priv->dbus_type = GAEUL_APPLICATION_DBUS_TYPE_SESSION;
}

void
gaeul_application_set_id (GaeulApplication * self, const gchar * id)
{
  g_return_if_fail (GAEUL_IS_APPLICATION (self));
  g_return_if_fail (id != NULL);

  g_object_set (self, "id", id, NULL);
}

const gchar *
gaeul_application_get_id (GaeulApplication * self)
{
  GaeulApplicationPrivate *priv = gaeul_application_get_instance_private (self);

  g_return_val_if_fail (GAEUL_IS_APPLICATION (self), NULL);

  return priv->uid;
}

void gaeul_application_set_config_path
    (GaeulApplication * self, GaeulApplication * config_path)
{
  g_return_if_fail (GAEUL_IS_APPLICATION (self));

  g_return_if_fail (config_path != NULL);

  g_object_set (self, "config-path", config_path, NULL);
}

const gchar *
gaeul_application_get_config_path (GaeulApplication * self)
{
  GaeulApplicationPrivate *priv = gaeul_application_get_instance_private (self);

  g_return_val_if_fail (GAEUL_IS_APPLICATION (self), NULL);

  return priv->config_path;
}
