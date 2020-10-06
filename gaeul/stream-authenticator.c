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
  GSequence *sink_tokens;
  GSequence *source_tokens;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulStreamAuthenticator, gaeul_stream_authenticator, G_TYPE_OBJECT)
/* *INDENT-ON* */

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

  if (!g_sequence_lookup (self->sink_tokens, (gpointer) username,
          (GCompareDataFunc) strcmp, NULL)) {
    g_sequence_insert_sorted (self->sink_tokens, g_strdup (username),
        (GCompareDataFunc) strcmp, NULL);
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

  if (!g_sequence_lookup (self->source_tokens, token, (GCompareDataFunc) strcmp,
          NULL)) {
    g_sequence_insert_sorted (self->source_tokens, g_steal_pointer (&token),
        (GCompareDataFunc) strcmp, NULL);
  }
}

void
gaeul_stream_authenticator_remove_sink_token (GaeulStreamAuthenticator * self,
    const gchar * username)
{
  GSequenceIter *it;

  g_return_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self));
  g_return_if_fail (username != NULL);

  it = g_sequence_lookup (self->sink_tokens, (gpointer) username,
      (GCompareDataFunc) strcmp, NULL);
  if (it) {
    g_sequence_remove (it);
  }
}

void
gaeul_stream_authenticator_remove_source_token (GaeulStreamAuthenticator * self,
    const gchar * username, const gchar * resource)
{
  g_autofree gchar *token = NULL;
  GSequenceIter *it;

  g_return_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self));
  g_return_if_fail (username != NULL);
  g_return_if_fail (resource != NULL);

  token = g_strdup_printf ("%s:%s", username, resource);

  it = g_sequence_lookup (self->source_tokens, token, (GCompareDataFunc) strcmp,
      NULL);
  if (it) {
    g_sequence_remove (it);
  }
}

GVariant *
gaeul_stream_authenticator_list_sink_tokens (GaeulStreamAuthenticator * self)
{
  GVariantBuilder builder;
  GSequenceIter *seq_itr;

  g_return_val_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(si)"));

  seq_itr = g_sequence_get_begin_iter (self->sink_tokens);

  while (seq_itr != g_sequence_get_end_iter (self->sink_tokens)) {
    const gchar *token = g_sequence_get (seq_itr);

    g_variant_builder_open (&builder, G_VARIANT_TYPE ("(si)"));
    g_variant_builder_add (&builder, "s", g_strdup (token));
    /* TODO: it should be extracted from actual status. */
    g_variant_builder_add (&builder, "i", 0);

    g_variant_builder_close (&builder);

    seq_itr = g_sequence_iter_next (seq_itr);
  }

  return g_variant_builder_end (&builder);
}

GVariant *
gaeul_stream_authenticator_list_source_tokens (GaeulStreamAuthenticator * self)
{
  GVariantBuilder builder;
  GSequenceIter *seq_itr;

  g_return_val_if_fail (GAEUL_IS_STREAM_AUTHENTICATOR (self), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssi)"));

  seq_itr = g_sequence_get_begin_iter (self->source_tokens);

  while (seq_itr != g_sequence_get_end_iter (self->source_tokens)) {
    const gchar *token_str = g_sequence_get (seq_itr);
    g_auto (GStrv) tokens = g_strsplit (token_str, ":", -1);

    g_variant_builder_open (&builder, G_VARIANT_TYPE ("(ssi)"));
    g_variant_builder_add (&builder, "s", g_strdup (tokens[0]));
    g_variant_builder_add (&builder, "s", g_strdup (tokens[1]));
    /* TODO: it should be extracted from actual status. */
    g_variant_builder_add (&builder, "i", 0);

    g_variant_builder_close (&builder);

    seq_itr = g_sequence_iter_next (seq_itr);
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
      return g_sequence_lookup (self->sink_tokens, (gpointer) username,
          (GCompareDataFunc) strcmp, NULL) != NULL;
      break;
    case HWANGSAE_CALLER_DIRECTION_SRC:{
      g_autofree gchar *token = NULL;

      if (!resource) {
        return FALSE;
      }

      token = g_strdup_printf ("%s:%s", username, resource);

      return g_sequence_lookup (self->source_tokens, token,
          (GCompareDataFunc) strcmp, NULL) != NULL;
      break;
    }
    default:
      return FALSE;
  }
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
  g_clear_object (&self->relay);
  g_clear_pointer (&self->sink_tokens, g_sequence_free);
  g_clear_pointer (&self->sink_tokens, g_sequence_free);
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
  self->sink_tokens = g_sequence_new (g_free);
  self->source_tokens = g_sequence_new (g_free);
}
