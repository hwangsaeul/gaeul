/**
 *  Copyright 2019-2020 SK Telecom Co., Ltd.
 *    Author: Walter Lozano <walter.lozano@collabora.com>
 *            Jeongseok Kim <jeongseok.kim@sk.com>
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

#include "gaeul.h"

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

GSettings *
gaeul_gsettings_new (const gchar * schema_id, const gchar * path)
{
  g_autoptr (GSettingsBackend) backend = NULL;

  g_return_val_if_fail (schema_id != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  backend = g_keyfile_settings_backend_new (path, "/", NULL);

  /* TODO: Do not crash if schema doesn't exist. */
  return g_settings_new_with_backend (schema_id, backend);
}

static char
_search_delimiter (const char *from, guint * position)
{
  *position = 0;

  for (;;) {
    char ch = *from++;
    (*position)++;

    switch (ch) {
      case ':':
      case 0:
      case '/':
      case '?':
      case '#':
      case '=':
        return ch;
      default:
        break;
    }
  }
}

void
gaeul_parse_srt_uri (const gchar * uri, gchar ** host, guint * port,
    gchar ** mode)
{
  gchar delimiter = 0;
  g_autofree gchar *port_str = NULL;
  guint position = 0;

  g_return_if_fail (uri != NULL);
  g_return_if_fail (host != NULL);
  g_return_if_fail (port != NULL);
  g_return_if_fail (mode != NULL);
  g_return_if_fail (strncmp (uri, "srt://", 6) == 0);

  if (!strncmp (uri, "srt://", 6)) {
    uri += 6;
  }

  delimiter = _search_delimiter (uri, &position);
  *host = g_strndup (uri, position - 1);

  if (delimiter == ':') {
    uri += position;
    delimiter = _search_delimiter (uri, &position);
    port_str = g_strndup (uri, position - 1);
  }

  if (port_str) {
    gchar *end = NULL;
    *port = strtol (port_str, &end, 10);

    if (port_str == end || *end != 0 || *port < 0 || *port > 65535) {
      return;
    }
  }

  if (delimiter == '?' && mode != NULL) {
    uri += position;
    delimiter = _search_delimiter (uri, &position);

    if (delimiter != '=') {
      return;
    }

    *mode = g_strndup (uri, position - 1);
    if (!g_strcmp0 (*mode, "mode")) {
      uri += position;
      delimiter = _search_delimiter (uri, &position);
      if (position > 1) {
        g_free (*mode);
        *mode = g_strndup (uri, position - 1);
      }
    } else {
      g_free (*mode);
      *mode = NULL;
    }
  }
}
