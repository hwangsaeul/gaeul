/**
 *  Copyright 2019 SK Telecom Co., Ltd.
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

#include "gaeul-agent.h"
#include "dbus/dbus-manager-generated.h"

#include <glib-unix.h>

#include <chamge/chamge.h>
#include <gaeguli/gaeguli.h>

#define GAEUL_SCHEMA_ID "org.hwangsaeul.Gaeul"

struct _GaeulAgent
{
  GApplication parent;
  GaeulDBusManager *dbus_manager;

  GSettings *settings;
  ChamgeEdge *edge;
  GaeguliPipeline *pipeline;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulAgent, gaeul_agent, G_TYPE_APPLICATION)
/* *INDENT-ON* */

static void
gaeul_agent_activate (GApplication * app)
{
  GaeulAgent *self = GAEUL_AGENT (app);

  g_debug ("activate");
}

static gboolean
gaeul_agent_dbus_register (GApplication * app,
    GDBusConnection * connection, const gchar * object_path, GError ** error)
{
  gboolean ret = TRUE;
  GaeulAgent *self = GAEUL_AGENT (app);

  /* chain up */
  ret = G_APPLICATION_CLASS (gaeul_agent_parent_class)->dbus_register (app,
      connection, object_path, error);
  if (ret &&
      !g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON
          (self->dbus_manager), connection, "/org/hwangsaeul/Gaeul/Manager",
          error)) {
    g_warning ("Failed to export Gaeul D-Bus interface (reason: %s)",
        (*error)->message);
  }

  return ret;
}

static void
gaeul_agent_dbus_unregister (GApplication * app,
    GDBusConnection * connection, const gchar * object_path)
{
  GaeulAgent *self = GAEUL_AGENT (app);

  if (self->dbus_manager)
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON
        (self->dbus_manager));

  /* chain up */
  G_APPLICATION_CLASS (gaeul_agent_parent_class)->dbus_unregister (app,
      connection, object_path);
}

static void
gaeul_shutdown (GApplication * app)
{
  GaeulAgent *self = GAEUL_AGENT (app);

  g_debug ("shutdown");

  gaeguli_pipeline_stop (self->pipeline);

  G_APPLICATION_CLASS (gaeul_agent_parent_class)->shutdown (app);
}

static void
gaeul_agent_dispose (GObject * object)
{
  GaeulAgent *self = GAEUL_AGENT (object);

  g_clear_object (&self->edge);
  g_clear_object (&self->pipeline);

  g_clear_object (&self->dbus_manager);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gaeul_agent_parent_class)->dispose (object);
}

static void
gaeul_agent_class_init (GaeulAgentClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = gaeul_agent_dispose;

  app_class->activate = gaeul_agent_activate;
  app_class->dbus_register = gaeul_agent_dbus_register;
  app_class->dbus_unregister = gaeul_agent_dbus_unregister;
  app_class->shutdown = gaeul_shutdown;
}

static void
gaeul_agent_init (GaeulAgent * self)
{
  self->dbus_manager = gaeul_dbus_manager_skeleton_new ();
  self->settings = g_settings_new (GAEUL_SCHEMA_ID);
  g_autofree gchar *uid = NULL;

  uid = g_settings_get_string (self->settings, "edge-id");

  g_debug ("activate edge id:[%s]", uid);

  self->edge = chamge_edge_new (uid);
  self->pipeline = gaeguli_pipeline_new ();
}

static guint signal_watch_intr_id;

static gboolean
intr_handler (gpointer user_data)
{
  GApplication *app = user_data;

  g_debug ("releasing app");

  g_application_release (app);

  signal_watch_intr_id = 0;

  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  g_autoptr (GApplication) app = NULL;
  g_autoptr (GError) error = NULL;
  int ret;

  app = G_APPLICATION (g_object_new (GAEUL_TYPE_AGENT,
          "application-id", "org.hwangsaeul.Gaeul",
          "flags", G_APPLICATION_IS_SERVICE, NULL));

  signal_watch_intr_id =
      g_unix_signal_add (SIGINT, (GSourceFunc) intr_handler, app);
  g_application_set_inactivity_timeout (app, 10000);

  if (!g_application_register (app, NULL, &error)) {
    g_debug ("failed to register app (reason: %s)", error->message);
    return -1;
  }

  g_application_hold (app);

  g_application_activate (app);

  ret = g_application_run (app, argc, argv);

  if (signal_watch_intr_id > 0) {
    g_source_remove (signal_watch_intr_id);
  }

  return ret;
}
