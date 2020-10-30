/**
 *  Copyright 2020 SK Telecom Co., Ltd.
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

#include "stream-authenticator.h"

#include <gio/gio.h>

struct _GaeulStreamAuthenticator
{
  GObject parent;

  HwangsaeRelay *relay;
  gulong authenticate_signal_id;
  gulong passphrase_asked_signal_id;
  gulong pbkeylen_asked_signal_id;
  GHashTable *sink_tokens;
  GHashTable *source_tokens;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulStreamAuthenticator, gaeul_stream_authenticator, G_TYPE_OBJECT)
/* *INDENT-ON* */

typedef struct
{
  gchar *username;
  gchar *resource;
  gchar *passphrase;
  GaeguliSRTKeyLength pbkeylen;
} TokenData;

static void
token_data_free (TokenData * data)
{
  g_clear_pointer (&data->username, g_free);
  g_clear_pointer (&data->resource, g_free);
  g_clear_pointer (&data->passphrase, g_free);
  g_clear_pointer (&data, g_free);
}

enum
{
  PROP_RELAY = 1
};

GaeulStreamAuthenticator *
gaeul_stream_authenticator_new (HwangsaeRelay * relay)
{
  g_return_val_if_fail (HWANGSAE_IS_RELAY (relay), NULL);

  return g_object_new (GAEUL_TYPE_STREAM_AUTHENTICATOR, "relay", relay, NULL);
}

void
gaeul_stream_authenticator_add_sink_token (GaeulStreamAuthenticator * self,
    const gchar * username)
{
  g_return_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self));
  g_return_if_fail (username != NULL);

  if (!g_hash_table_contains (self->sink_tokens, username)) {
    TokenData *data = g_new0 (TokenData, 1);
    data->username = g_strdup (username);
    data->pbkeylen = GAEGULI_SRT_KEY_LENGTH_0;

    g_hash_table_insert (self->sink_tokens, data->username, data);
  }
}

void
gaeul_stream_authenticator_add_source_token (GaeulStreamAuthenticator * self,
    const gchar * username, const gchar * resource)
{
  g_autofree gchar *token = NULL;

  g_return_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self));
  g_return_if_fail (username != NULL);
  g_return_if_fail (resource != NULL);

  token = g_strdup_printf ("%s:%s", username, resource);

  if (!g_hash_table_contains (self->source_tokens, token)) {
    TokenData *data = g_new0 (TokenData, 1);
    data->username = g_strdup (username);
    data->resource = g_strdup (resource);
    data->pbkeylen = GAEGULI_SRT_KEY_LENGTH_0;

    g_hash_table_insert (self->source_tokens, g_steal_pointer (&token), data);
  }
}

void
gaeul_stream_authenticator_set_sink_credentials (GaeulStreamAuthenticator *
    self, const gchar * username, const gchar * passphrase,
    GaeguliSRTKeyLength pbkeylen)
{
  TokenData *data = g_hash_table_lookup (self->sink_tokens, username);
  if (data) {
    g_clear_pointer (&data->passphrase, g_free);
    data->passphrase = g_strdup (passphrase);
    data->pbkeylen = pbkeylen;
  } else {
    g_warning ("Unknown sink token %s", username);
  }
}

static TokenData *
gaeul_stream_authenticator_get_source_token_data (GaeulStreamAuthenticator *
    self, const gchar * username, const gchar * resource)
{
  g_autofree gchar *token = g_strdup_printf ("%s:%s", username, resource);

  return g_hash_table_lookup (self->source_tokens, token);
}

void
gaeul_stream_authenticator_set_source_credentials (GaeulStreamAuthenticator *
    self, const gchar * username, const gchar * resource,
    const gchar * passphrase, GaeguliSRTKeyLength pbkeylen)
{
  TokenData *data = gaeul_stream_authenticator_get_source_token_data (self,
      username, resource);
  if (data) {
    g_clear_pointer (&data->passphrase, g_free);
    data->passphrase = g_strdup (passphrase);
    data->pbkeylen = pbkeylen;
  } else {
    g_warning ("Unknown source token %s:%s", username, resource);
  }
}

gboolean
gaeul_stream_authenticator_remove_sink_token (GaeulStreamAuthenticator * self,
    const gchar * username)
{
  g_return_val_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self), FALSE);
  g_return_val_if_fail (username != NULL, FALSE);

  return g_hash_table_remove (self->sink_tokens, username);
}

gboolean
gaeul_stream_authenticator_remove_source_token (GaeulStreamAuthenticator * self,
    const gchar * username, const gchar * resource)
{
  g_autofree gchar *token = NULL;

  g_return_val_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self), FALSE);
  g_return_val_if_fail (username != NULL, FALSE);
  g_return_val_if_fail (resource != NULL, FALSE);

  token = g_strdup_printf ("%s:%s", username, resource);

  return g_hash_table_remove (self->source_tokens, token);
}

GVariant *
gaeul_stream_authenticator_list_sink_tokens (GaeulStreamAuthenticator * self)
{
  GVariantBuilder builder;
  GHashTableIter it;
  const gchar *token;

  g_return_val_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(si)"));

  g_hash_table_iter_init (&it, self->sink_tokens);

  while (g_hash_table_iter_next (&it, (gpointer *) & token, NULL)) {
    g_variant_builder_open (&builder, G_VARIANT_TYPE ("(si)"));
    g_variant_builder_add (&builder, "s", g_strdup (token));
    /* TODO: it should be extracted from actual status. */
    g_variant_builder_add (&builder, "i", 0);

    g_variant_builder_close (&builder);
  }

  return g_variant_builder_end (&builder);
}

GVariant *
gaeul_stream_authenticator_list_source_tokens (GaeulStreamAuthenticator * self)
{
  GVariantBuilder builder;
  GHashTableIter it;
  const TokenData *data;


  g_return_val_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssi)"));

  g_hash_table_iter_init (&it, self->source_tokens);

  while (g_hash_table_iter_next (&it, NULL, (gpointer *) & data)) {
    g_variant_builder_open (&builder, G_VARIANT_TYPE ("(ssi)"));
    g_variant_builder_add (&builder, "s", g_strdup (data->username));
    g_variant_builder_add (&builder, "s", g_strdup (data->resource));
    /* TODO: it should be extracted from actual status. */
    g_variant_builder_add (&builder, "i", 0);

    g_variant_builder_close (&builder);
  }

  return g_variant_builder_end (&builder);
}


static gboolean
gaeul_stream_authenticator_on_authenticate (GaeulStreamAuthenticator * self,
    HwangsaeCallerDirection direction, GSocketAddress * addr,
    const gchar * username, const gchar * resource)
{
  if (!username) {
    return FALSE;
  }

  switch (direction) {
    case HWANGSAE_CALLER_DIRECTION_SINK:
      return g_hash_table_contains (self->sink_tokens, username);
    case HWANGSAE_CALLER_DIRECTION_SRC:{
      g_autofree gchar *token = NULL;

      if (!resource) {
        return FALSE;
      }

      token = g_strdup_printf ("%s:%s", username, resource);

      return g_hash_table_contains (self->source_tokens, token);
      break;
    }
    default:
      return FALSE;
  }
}

static const gchar *
gaeul_stream_authenticator_on_passphrase_asked (GaeulStreamAuthenticator * self,
    HwangsaeCallerDirection direction, GSocketAddress * addr,
    const gchar * username, const gchar * resource)
{
  TokenData *data = NULL;

  if (direction == HWANGSAE_CALLER_DIRECTION_SINK) {
    data = g_hash_table_lookup (self->sink_tokens, username);
  } else {
    data = gaeul_stream_authenticator_get_source_token_data (self, username,
        resource);
  }

  if (!data) {
    g_warning ("Passphrase asked for unknown token %s%s%s", username,
        resource ? ":" : "", resource ? resource : "");
    return NULL;
  }

  return g_strdup (data->passphrase);
}

static GaeguliSRTKeyLength
gaeul_stream_authenticator_on_pbkeylen_asked (GaeulStreamAuthenticator * self,
    HwangsaeCallerDirection direction, GSocketAddress * addr,
    const gchar * username, const gchar * resource)
{
  TokenData *data = NULL;

  if (direction == HWANGSAE_CALLER_DIRECTION_SINK) {
    data = g_hash_table_lookup (self->sink_tokens, username);
  } else {
    data = gaeul_stream_authenticator_get_source_token_data (self, username,
        resource);
  }

  if (!data) {
    g_warning ("SRT key length asked for unknown token %s%s%s", username,
        resource ? ":" : "", resource ? resource : "");
    return GAEGULI_SRT_KEY_LENGTH_0;
  }

  return data->pbkeylen;
}

static void
gaeul_stream_authenticator_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GaeulStreamAuthenticator *self = GAEUL_STREAM_AUTHENTICATOR (object);

  switch (prop_id) {
    case PROP_RELAY:
      self->relay = g_value_dup_object (value);
      self->authenticate_signal_id = g_signal_connect_swapped (self->relay,
          "authenticate",
          (GCallback) gaeul_stream_authenticator_on_authenticate, self);
      self->passphrase_asked_signal_id = g_signal_connect_swapped (self->relay,
          "on-passphrase-asked",
          (GCallback) gaeul_stream_authenticator_on_passphrase_asked, self);
      self->pbkeylen_asked_signal_id = g_signal_connect_swapped (self->relay,
          "on-pbkeylen-asked",
          (GCallback) gaeul_stream_authenticator_on_pbkeylen_asked, self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gaeul_stream_authenticator_dispose (GObject * object)
{
  GaeulStreamAuthenticator *self = GAEUL_STREAM_AUTHENTICATOR (object);

  if (self->authenticate_signal_id) {
    g_signal_handler_disconnect (self->relay, self->authenticate_signal_id);
    self->authenticate_signal_id = 0;
  }
  if (self->passphrase_asked_signal_id) {
    g_signal_handler_disconnect (self->relay, self->passphrase_asked_signal_id);
    self->passphrase_asked_signal_id = 0;
  }
  if (self->pbkeylen_asked_signal_id) {
    g_signal_handler_disconnect (self->relay, self->pbkeylen_asked_signal_id);
    self->pbkeylen_asked_signal_id = 0;
  }
  g_clear_object (&self->relay);
  g_clear_pointer (&self->sink_tokens, g_hash_table_unref);
  g_clear_pointer (&self->sink_tokens, g_hash_table_unref);
}

static void
gaeul_stream_authenticator_class_init (GaeulStreamAuthenticatorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gaeul_stream_authenticator_set_property;
  gobject_class->dispose = gaeul_stream_authenticator_dispose;

  g_object_class_install_property (gobject_class, PROP_RELAY,
      g_param_spec_object ("relay", "HwangsaeRelay instance",
          "HwangsaeRelay instance", HWANGSAE_TYPE_RELAY,
          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
gaeul_stream_authenticator_init (GaeulStreamAuthenticator * self)
{
  self->sink_tokens = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) token_data_free);
  self->source_tokens = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) token_data_free);
}
