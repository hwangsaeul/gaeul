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

#include <glib.h>
#include <glib-unix.h>

#include "gaeul/relay/relay-application.h"

static gboolean
intr_handler (gpointer user_data)
{
  GApplication *app = user_data;

  g_debug ("releasing app");
  g_application_release (app);

  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  g_autoptr (GApplication) app = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GOptionContext) context = NULL;
  g_autofree gchar *app_id = NULL;

  const gchar *config = NULL;
  GOptionEntry entries[] = {
    {"config", 'c', 0, G_OPTION_ARG_FILENAME, &config, NULL, NULL},
    {NULL}
  };

  context = g_option_context_new (NULL);

  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("%s\n", error->message);
    return -1;
  }

  app_id = g_strdup_printf (GAEUL_RELAY_APPLICATION_SCHEMA_ID "_%d", getpid ());

  app = G_APPLICATION (g_object_new (GAEUL_TYPE_RELAY_APPLICATION,
          "application-id", app_id, "config-path", config, NULL));

  g_unix_signal_add (SIGINT, (GSourceFunc) intr_handler, app);

  g_application_hold (app);

  return gaeul_application_run (GAEUL_APPLICATION (app), argc, argv);
}
