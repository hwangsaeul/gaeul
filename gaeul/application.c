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

typedef struct
{
  gint exit_status;
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

static void
gaeul_application_startup (GApplication * app)
{
  G_APPLICATION_CLASS (gaeul_application_parent_class)->startup (app);

  if (getppid () == 1) {
    g_bus_own_name (G_BUS_TYPE_SYSTEM, g_application_get_application_id (app),
        0, NULL, _dbus_name_acquired, _dbus_name_lost, app, NULL);
  }
}

static void
gaeul_application_class_init (GaeulApplicationClass * klass)
{
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  app_class->startup = gaeul_application_startup;
}

static void
gaeul_application_init (GaeulApplication * self)
{
}
