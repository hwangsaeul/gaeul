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

#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

/* *INDENT-OFF* */
#if !GLIB_CHECK_VERSION(2,57,1)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GEnumClass, g_type_class_unref)
#endif
/* *INDENT-ON* */

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

gboolean
gaeul_parse_srt_uri (const gchar * uri, gchar ** host, guint * port,
    GaeguliSRTMode * mode)
{
  gboolean ret = FALSE;
  gchar delimiter = 0;
  g_autofree gchar *port_str = NULL;
  g_autofree gchar *mode_str = NULL;
  guint position = 0;

  g_return_val_if_fail (uri != NULL, FALSE);
  g_return_val_if_fail (host != NULL, FALSE);
  g_return_val_if_fail (port != NULL, FALSE);
  g_return_val_if_fail (strncmp (uri, "srt://", 6) == 0, FALSE);

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
      goto out;
    }
  }

  if (delimiter == '?') {
    uri += position;
    delimiter = _search_delimiter (uri, &position);

    if (delimiter != '=') {
      goto out;
    }

    mode_str = g_strndup (uri, position - 1);
    if (!g_strcmp0 (mode_str, "mode")) {
      uri += position;
      delimiter = _search_delimiter (uri, &position);
      if (position > 1) {

        g_autoptr (GEnumClass) c = g_type_class_ref (GAEGULI_TYPE_SRT_MODE);
        GEnumValue *v = NULL;

        mode_str = g_strndup (uri, position - 1);

        v = g_enum_get_value_by_nick (c, mode_str);
        *mode = v->value;
      }
    }
    ret = TRUE;
  } else if (delimiter == 0) {
    ret = TRUE;
  }

out:
  return ret;
}

gchar *
gaeul_get_local_ip (void)
{
  struct ifaddrs *addrs;
  struct ifaddrs *it;
  gchar *result = NULL;

  if (getifaddrs (&addrs) < 0) {
    return NULL;
  }

  for (it = addrs; !result && it; it = it->ifa_next) {
    socklen_t addr_len;
    char buf[INET6_ADDRSTRLEN + 1];

    /* Ignore interfaces that are down, not running or don't have an IP. We also
     * want to skip loopbacks. */
    if ((it->ifa_flags & (IFF_UP | IFF_RUNNING | IFF_LOOPBACK)) !=
        (IFF_UP | IFF_RUNNING) || it->ifa_addr == NULL) {
      continue;
    }

    switch (it->ifa_addr->sa_family) {
      case AF_INET:
        addr_len = sizeof (struct sockaddr_in);
        break;
      case AF_INET6:
        addr_len = sizeof (struct sockaddr_in6);
        break;
      default:
        continue;
    }

    if (getnameinfo (it->ifa_addr, addr_len, buf, sizeof (buf), NULL, 0,
            NI_NUMERICHOST) != 0) {
      continue;
    }

    result = g_strdup (buf);
  }

  freeifaddrs (addrs);

  return result;
}
