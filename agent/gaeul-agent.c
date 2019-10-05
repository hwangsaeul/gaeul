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

struct _GaeulAgent
{
  GApplication parent;
  GaeulDBusManager *dbus_manager;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulAgent, gaeul_agent, G_TYPE_APPLICATION)
/* *INDENT-ON* */

static void
gaeul_agent_activate (GApplication * app)
{
  g_debug ("activate");
}

static gboolean
gaeul_agent_dbus_register (GApplication * app,
    GDBusConnection * connection, const gchar * object_path, GError ** error)
{
  gboolean ret = TRUE;
  GaeulAgent *self = GAEUL_AGENT (app);

  g_application_hold (app);

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

  g_application_release (app);

  /* chain up */
  G_APPLICATION_CLASS (gaeul_agent_parent_class)->dbus_unregister (app,
      connection, object_path);
}

static void
gaeul_agent_dispose (GObject * object)
{
  GaeulAgent *self = GAEUL_AGENT (object);

  g_clear_object (&self->dbus_manager);

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
}

static void
gaeul_agent_init (GaeulAgent * self)
{
  self->dbus_manager = gaeul_dbus_manager_skeleton_new ();
}

int
main (int argc, char **argv)
{
  g_autoptr (GApplication) app = NULL;

  app = G_APPLICATION (g_object_new (GAEUL_TYPE_AGENT,
          "application-id", "org.hwangsaeul.Gaeul",
          "flags", G_APPLICATION_IS_SERVICE, NULL));

  g_application_set_inactivity_timeout (app, 10000);

  return g_application_run (app, argc, argv);
}
