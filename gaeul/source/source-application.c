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
#include "source/source-application.h"
#include "source/source-generated.h"

#include <gaeguli/gaeguli.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* *INDENT-OFF* */
#if !GLIB_CHECK_VERSION(2,57,1)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GEnumClass, g_type_class_unref)
#endif
/* *INDENT-ON* */

typedef struct _GaeguliNest
{
  gint refcount;

  gchar *id;
  gchar *record_location;
  GaeguliPipeline *pipeline;
  GaeguliTarget *target_stream;
  GaeguliTarget *record_stream;
  GaeguliStreamAdaptor *adaptor;
  Gaeul2DBusSourceChannel *dbus;
} GaeguliNest;

static gboolean _channel_handle_get_stats (GaeguliNest * nest,
    GDBusMethodInvocation * invocation);

static GaeguliNest *
gaeguli_nest_new (const gchar * id, GaeguliPipeline * pipeline)
{
  GaeguliNest *nest = g_new0 (GaeguliNest, 1);

  nest->id = g_strdup (id);
  nest->refcount = 1;
  nest->pipeline = g_object_ref (pipeline);

  return nest;
}

static GaeguliNest *
gaeguli_nest_ref (GaeguliNest * nest)
{
  g_return_val_if_fail (nest != NULL, NULL);
  g_return_val_if_fail (nest->id != NULL, NULL);
  g_return_val_if_fail (nest->pipeline != NULL, NULL);

  g_atomic_int_inc (&nest->refcount);

  return nest;
}

static void
_on_target_property_change (GaeguliNest * nest, GParamSpec * pspec)
{
  GValue value = G_VALUE_INIT;

  g_object_get_property (G_OBJECT (nest->target_stream), pspec->name, &value);

  if (G_TYPE_IS_ENUM (pspec->value_type)) {
    g_autoptr (GEnumClass) enum_class;
    GEnumValue *enum_value;

    enum_class = g_type_class_ref (pspec->value_type);
    enum_value = g_enum_get_value (enum_class, g_value_get_enum (&value));

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, enum_value->value_nick);
  }

  g_object_set_property (G_OBJECT (nest->dbus), pspec->name, &value);

  g_value_unset (&value);
}

static void
_on_dbus_property_change (GaeguliNest * nest, GParamSpec * pspec)
{
  GParamSpec *target_pspec;
  GValue value = G_VALUE_INIT;

  target_pspec = g_object_class_find_property
      (G_OBJECT_GET_CLASS (nest->target_stream), pspec->name);

  if (!target_pspec) {
    g_warning ("Unknown target stream property '%s'", pspec->name);
    return;
  }

  g_object_get_property (G_OBJECT (nest->dbus), pspec->name, &value);

  if (G_TYPE_IS_ENUM (target_pspec->value_type)) {
    g_autoptr (GEnumClass) enum_class = NULL;
    GEnumValue *enum_value;
    const gchar *str_val;

    str_val = g_value_get_string (&value);
    if (!str_val) {
      g_warning ("No value for '%s'", g_type_name (target_pspec->value_type));
      goto out;
    }

    enum_class = g_type_class_ref (target_pspec->value_type);
    enum_value = g_enum_get_value_by_nick (enum_class, str_val);
    if (!enum_value) {
      g_warning ("Invalid value '%s' for '%s'", str_val,
          g_type_name (target_pspec->value_type));
      goto out;
    }

    g_value_unset (&value);
    g_value_init (&value, target_pspec->value_type);
    g_value_set_enum (&value, enum_value->value);
  }

  g_object_set_property (G_OBJECT (nest->target_stream), pspec->name, &value);

out:
  g_value_unset (&value);
}

static void
_channel_handle_save_snapshot_finish (GaeguliPipeline * pipeline,
    GAsyncResult * result, GDBusMethodInvocation * invocation)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GVariant) filename_pattern = NULL;
  g_autofree gchar *filename = NULL;
  const gchar *jpg_data;
  gsize jpg_size;

  filename_pattern = g_variant_get_child_value
      (g_dbus_method_invocation_get_parameters (invocation), 0);

  filename = g_strdup_printf (g_variant_get_string (filename_pattern, NULL),
      g_get_real_time ());

  bytes = gaeguli_pipeline_create_snapshot_finish (pipeline, result, &error);
  if (error) {
    goto error;
  }

  jpg_data = g_bytes_get_data (bytes, &jpg_size);

  g_file_set_contents (filename, jpg_data, jpg_size, &error);
  if (error) {
    goto error;
  }

  gaeul2_dbus_source_channel_complete_save_snapshot (NULL, invocation,
      filename);
  return;

error:
  g_dbus_method_invocation_return_gerror (invocation, error);
}

static gboolean
_channel_handle_save_snapshot (GaeguliNest * nest,
    GDBusMethodInvocation * invocation, const gchar * file_path,
    GVariant * tags)
{
  if (!file_path || strlen (file_path) == 0) {
    g_dbus_method_invocation_return_error (invocation, G_IO_ERROR,
        G_IO_ERROR_INVALID_ARGUMENT, "Empty file path is not allowed.");
    return TRUE;
  }

  gaeguli_pipeline_create_snapshot_async (nest->pipeline, g_variant_ref (tags),
      NULL, (GAsyncReadyCallback) _channel_handle_save_snapshot_finish,
      invocation);

  return TRUE;
}

static void
gaeguli_nest_dbus_register (GaeguliNest * nest, GDBusConnection * connection)
{
  g_autofree gchar *dbus_obj_path = NULL;
  g_autoptr (GError) error = NULL;
  GParamSpec **pspecs;
  guint n_prop;

  nest->dbus = gaeul2_dbus_source_channel_skeleton_new ();

  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (nest->dbus),
      &n_prop);

  while (n_prop-- > 0) {
    GParamSpec *pspec = pspecs[n_prop];

    if (g_str_equal (pspec->name, "g-flags")) {
      continue;
    }

    _on_target_property_change (nest, g_object_class_find_property
        (G_OBJECT_GET_CLASS (nest->target_stream), pspec->name));
  }

  g_free (pspecs);

  g_signal_connect_swapped (nest->target_stream, "notify",
      G_CALLBACK (_on_target_property_change), nest);
  g_signal_connect_swapped (nest->dbus, "notify::bitrate-control",
      G_CALLBACK (_on_dbus_property_change), nest);
  g_signal_connect_swapped (nest->dbus, "notify::bitrate",
      G_CALLBACK (_on_dbus_property_change), nest);
  g_signal_connect_swapped (nest->dbus, "notify::quantizer",
      G_CALLBACK (_on_dbus_property_change), nest);
  g_signal_connect_swapped (nest->dbus, "notify::adaptive-streaming",
      G_CALLBACK (_on_dbus_property_change), nest);

  g_signal_connect_swapped (nest->dbus, "handle-get-stats",
      (GCallback) _channel_handle_get_stats, nest);
  g_signal_connect_swapped (nest->dbus, "handle-save-snapshot",
      (GCallback) _channel_handle_save_snapshot, nest);

  dbus_obj_path = g_strdup_printf ("/org/hwangsaeul/Gaeul2/Source/channels/%s",
      nest->id);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (nest->dbus),
          connection, dbus_obj_path, &error)) {
    g_warning ("Failed to export %s on D-Bus (reason: %s)", dbus_obj_path,
        error->message);
  }
}

static void
_stream_quality_dropped_cb (GaeguliNest * nest)
{
  g_autoptr (GError) error = NULL;

  nest->record_stream =
      gaeguli_pipeline_add_recording_target (nest->pipeline,
      nest->record_location, &error);
  if (error) {
    g_debug ("Failed to add recording target (reason: %s)", error->message);
    return;
  }
  gaeguli_target_start (nest->record_stream, &error);
  if (error) {
    g_debug ("Failed to start srt target (reason: %s)", error->message);
  }
}

static void
_stream_quality_regained_cb (GaeguliNest * nest)
{
  g_autoptr (GError) error = NULL;

  gaeguli_pipeline_remove_target (nest->pipeline, nest->record_stream, &error);
  nest->record_stream = NULL;

  if (error) {
    g_debug ("%s", error->message);
  } else {
    g_debug ("record target removed");
  }
}

static void
_stream_started_cb (GaeguliPipeline * pipeline, guint target_id,
    gpointer user_data)
{
  GaeguliNest *nest = user_data;

  if (nest->adaptor) {
    return;
  }

  nest->adaptor = gaeguli_target_get_stream_adaptor (nest->target_stream);

  g_signal_connect_swapped (nest->adaptor, "stream-quality-dropped",
      G_CALLBACK (_stream_quality_dropped_cb), nest);

  g_signal_connect_swapped (nest->adaptor, "stream-quality-regained",
      G_CALLBACK (_stream_quality_regained_cb), nest);
}

static gboolean
gaeguli_nest_start (GaeguliNest * nest, const gchar * stream_id,
    const gchar * uri, GaeguliVideoCodec codec, guint bitrate,
    GaeguliVideoBitrateControl bitrate_control,
    const gchar * passphrase, GaeguliSRTKeyLength pbkeylen)
{
  g_autoptr (GError) error = NULL;

  g_return_val_if_fail (nest->target_stream == NULL, FALSE);

  nest->target_stream =
      gaeguli_pipeline_add_srt_target_full (nest->pipeline, codec, bitrate, uri,
      stream_id, &error);

  if (!nest->target_stream) {
    g_debug ("Failed to add srt target to pipeline (reason: %s)",
        error->message);
    return FALSE;
  }

  g_object_set (nest->target_stream, "passphrase", passphrase, "pbkeylen",
      pbkeylen, "bitrate-control", bitrate_control, NULL);

  g_signal_connect (nest->pipeline, "stream-started",
      G_CALLBACK (_stream_started_cb), nest);

  gaeguli_target_start (nest->target_stream, &error);
  if (error) {
    g_debug ("Failed to start srt target (reason: %s)", error->message);
    return FALSE;
  }

  return TRUE;
}

static void
gaeguli_nest_stop (GaeguliNest * nest)
{
  if (nest->target_stream) {
    g_autoptr (GError) error = NULL;
    gaeguli_pipeline_remove_target (nest->pipeline, nest->target_stream,
        &error);
    nest->target_stream = NULL;

    if (error != NULL) {
      g_warning ("%s", error->message);
    }
  }

  if (nest->record_stream) {
    g_autoptr (GError) error = NULL;
    gaeguli_pipeline_remove_target (nest->pipeline, nest->record_stream,
        &error);
    nest->record_stream = NULL;
  }

  if (nest->dbus) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (nest->dbus));
    g_clear_object (&nest->dbus);
  }

  gaeguli_pipeline_stop (nest->pipeline);
}

static void
gaeguli_nest_unref (GaeguliNest * nest)
{
  g_return_if_fail (nest != NULL);
  g_return_if_fail (nest->pipeline != NULL);

  if (g_atomic_int_dec_and_test (&nest->refcount)) {

    gaeguli_nest_stop (nest);

    g_clear_object (&nest->pipeline);
    g_clear_object (&nest->adaptor);
    g_clear_pointer (&nest->id, g_free);
    g_clear_pointer (&nest->record_location, g_free);
    g_free (nest);
  }
}

/* *INDENT-OFF* */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GaeguliNest, gaeguli_nest_unref)
/* *INDENT-ON* */

struct _GaeulSourceApplication
{
  GaeulApplication parent;

  GSettings *settings;
  Gaeul2DBusSource *dbus_service;

  GList *gaegulis;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulSourceApplication, gaeul_source_application, GAEUL_TYPE_APPLICATION)
/* *INDENT-ON* */

static int
gaeul_source_application_command_line (GApplication * app,
    GApplicationCommandLine * command_line)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);
  g_autoptr (GDBusConnection) dbus_connection = NULL;
  g_autofree gchar *uid = NULL;
  GaeulApplicationDBusType bus_type;

  g_auto (GStrv) channel_configs = NULL;
  guint i;

  g_debug ("command_line");

  g_clear_object (&self->settings);
  self->settings = gaeul_gsettings_new (GAEUL_SOURCE_APPLICATION_SCHEMA_ID,
      gaeul_application_get_config_path (GAEUL_APPLICATION (self))
      );

  g_settings_bind (self->settings, "uid", self, "uid", G_SETTINGS_BIND_DEFAULT);

  g_object_get (GAEUL_APPLICATION (self), "uid", &uid, NULL);

  g_object_get (self, "dbus-type", &bus_type, NULL);
  if (bus_type != GAEUL_APPLICATION_DBUS_TYPE_NONE) {
    dbus_connection = g_bus_get_sync (bus_type, NULL, NULL);
  }

  channel_configs = g_settings_get_strv (self->settings, "channel-configs");

  for (i = 0; i < g_strv_length (channel_configs); i++) {
    const gchar *conf_file = channel_configs[i];

    g_autoptr (GaeguliNest) nest = NULL;
    g_autoptr (GaeguliPipeline) pipeline = NULL;

    g_autofree gchar *name = NULL;
    g_autofree gchar *device = NULL;

    g_autofree gchar *target_uri = NULL;
    g_autofree gchar *target_host = NULL;
    GaeguliSRTMode target_mode = GAEGULI_SRT_MODE_UNKNOWN;
    guint target_port = 0;
    g_autofree gchar *stream_id = NULL;
    g_autofree gchar *passphrase = NULL;
    g_autofree gchar *record_location = NULL;
    GaeguliSRTKeyLength pbkeylen = GAEGULI_SRT_KEY_LENGTH_0;

    g_autoptr (GSettings) ssettings =
        gaeul_gsettings_new (GAEUL_SOURCE_APPLICATION_SCHEMA_ID ".Channel",
        conf_file);

    GaeguliVideoSource video_source = g_settings_get_enum (ssettings, "source");
    GaeguliVideoCodec video_codec = g_settings_get_enum (ssettings, "codec");
    GaeguliVideoResolution video_resolution =
        g_settings_get_enum (ssettings, "resolution");
    guint bitrate = g_settings_get_uint (ssettings, "bitrate");
    GaeguliVideoBitrateControl bitrate_control =
        g_settings_get_enum (ssettings, "bitrate-control");
    guint fps = g_settings_get_uint (ssettings, "fps");

    name = g_settings_get_string (ssettings, "name");
    device = g_settings_get_string (ssettings, "device");
    target_uri = g_settings_get_string (ssettings, "target-uri");
    record_location = g_settings_get_string (ssettings, "record-location");

    if (!name) {
      g_info ("config file[%s] doesn't have valid name property", conf_file);
      continue;
    }

    if (!gaeul_parse_srt_uri (target_uri, &target_host, &target_port,
            &target_mode)) {
      g_info ("can't parse srt target uri from config file[%s]", conf_file);
      continue;
    }

    stream_id = g_strconcat (uid, "_", name, NULL);

    pipeline = gaeguli_pipeline_new_full (video_source, device,
        video_resolution, fps);

    nest = gaeguli_nest_new (stream_id, pipeline);

    nest->record_location = g_strdup (record_location);
    passphrase = g_settings_get_string (ssettings, "passphrase");
    pbkeylen = g_settings_get_enum (ssettings, "pbkeylen");

    g_object_set (pipeline, "prefer-hw-decoding",
        g_settings_get_boolean (ssettings, "prefer-hw-decoding"), NULL);

    if (!gaeguli_nest_start (nest, stream_id, target_uri, video_codec, bitrate,
            bitrate_control, passphrase, pbkeylen)) {
      goto error;
    }
    if (dbus_connection) {
      gaeguli_nest_dbus_register (nest, dbus_connection);
    }
    self->gaegulis = g_list_prepend (self->gaegulis, gaeguli_nest_ref (nest));
  }

  return EXIT_SUCCESS;

error:
  g_application_quit (app);
  return EXIT_FAILURE;
}

static void
gaeul_source_application_activate (GApplication * app)
{
  g_debug ("activate");

  // TBD

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->activate (app);
}

static void
gaeul_source_application_shutdown (GApplication * app)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);

  g_debug ("shutdown");

  g_list_free_full (self->gaegulis, (GDestroyNotify) gaeguli_nest_unref);

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->shutdown (app);
}

static void
gaeul_source_application_dispose (GObject * object)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (object);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gaeul_source_application_parent_class)->dispose (object);
}

static gboolean
gaeul_source_application_handle_list_channels (GaeulSourceApplication * self,
    GDBusMethodInvocation * invocation)
{
  g_autofree const gchar **paths = NULL;
  const gchar **path;
  GList *l = NULL;

  paths = g_new0 (const gchar *, g_list_length (self->gaegulis) + 1);
  path = paths;

  for (l = self->gaegulis; l != NULL; l = l->next) {
    GaeguliNest *nest = l->data;

    if (nest->dbus) {
      *path++ = g_dbus_interface_skeleton_get_object_path
          (G_DBUS_INTERFACE_SKELETON (nest->dbus));
    }
  }

  gaeul2_dbus_source_complete_list_channels (self->dbus_service, invocation,
      paths);

  return TRUE;
}

static gboolean
_channel_handle_get_stats (GaeguliNest * nest,
    GDBusMethodInvocation * invocation)
{
  gint64 packets_sent = 0;
  gint packets_sent_lost = 0;
  gint packets_retransmitted = 0;
  gint packet_ack_received = 0;
  gint packet_nack_received = 0;
  gint64 send_duration_us = 0;
  guint64 bytes_sent = 0;
  guint64 bytes_retransmitted = 0;
  guint64 bytes_sent_dropped = 0;
  guint64 packets_sent_dropped = 0;
  gdouble send_rate_mbps = 0;
  gint negotiated_latency_ms = 0;
  gdouble bandwidth_mbps = 0;
  gdouble rtt_ms = 0;

  GVariantDict dict;
  g_autoptr (GVariant) variant = NULL;

  variant = gaeguli_target_get_stats (nest->target_stream);
  g_variant_dict_init (&dict, variant);

  g_variant_dict_lookup (&dict, "packets-sent", "x", &packets_sent);
  g_variant_dict_lookup (&dict, "packets-sent-lost", "i", &packets_sent_lost);
  g_variant_dict_lookup (&dict, "packets-retransmitted", "i",
      &packets_retransmitted);
  g_variant_dict_lookup (&dict, "packet-ack-received", "i",
      &packet_ack_received);
  g_variant_dict_lookup (&dict, "packet-nack-received", "i",
      &packet_nack_received);
  g_variant_dict_lookup (&dict, "send-duration-us", "x", &send_duration_us);
  g_variant_dict_lookup (&dict, "bytes-sent", "t", &bytes_sent);
  g_variant_dict_lookup (&dict, "bytes-retransmitted", "t",
      &bytes_retransmitted);
  g_variant_dict_lookup (&dict, "bytes-sent-dropped", "t", &bytes_sent_dropped);
  g_variant_dict_lookup (&dict, "packets-sent-dropped", "t",
      &packets_sent_dropped);
  g_variant_dict_lookup (&dict, "send-rate-mbps", "d", &send_rate_mbps);
  g_variant_dict_lookup (&dict, "negotiated-latency-ms", "i",
      &negotiated_latency_ms);
  g_variant_dict_lookup (&dict, "bandwidth-mbps", "d", &bandwidth_mbps);
  g_variant_dict_lookup (&dict, "rtt-ms", "d", &rtt_ms);

  g_variant_dict_clear (&dict);

  gaeul2_dbus_source_channel_complete_get_stats (nest->dbus,
      invocation,
      packets_sent,
      packets_sent_lost,
      packets_retransmitted,
      packet_ack_received,
      packet_nack_received,
      send_duration_us,
      bytes_sent,
      bytes_retransmitted,
      bytes_sent_dropped,
      packets_sent_dropped,
      send_rate_mbps, negotiated_latency_ms, bandwidth_mbps, rtt_ms);

  return TRUE;
}

static gboolean
gaeul_source_application_dbus_register (GApplication * app,
    GDBusConnection * connection, const gchar * object_path, GError ** error)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);

  g_debug ("register dbus (%s)", object_path);
  if (!self->dbus_service) {
    self->dbus_service = gaeul2_dbus_source_skeleton_new ();

    g_signal_connect_swapped (self->dbus_service, "handle-list-channels",
        (GCallback) gaeul_source_application_handle_list_channels, self);
  }

  if (!G_APPLICATION_CLASS
      (gaeul_source_application_parent_class)->dbus_register (app, connection,
          object_path, error)) {
    return FALSE;
  }

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON
          (self->dbus_service), connection, "/org/hwangsaeul/Gaeul2/Source",
          error)) {
    g_warning ("Failed to export Gaeul2Source D-Bus interface (reason: %s)",
        (*error)->message);
  }

  return TRUE;
}

static void
gaeul_source_application_dbus_unregister (GApplication * app,
    GDBusConnection * connection, const gchar * object_path)
{
  GaeulSourceApplication *self = GAEUL_SOURCE_APPLICATION (app);

  if (self->dbus_service) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON
        (self->dbus_service));
    g_clear_object (&self->dbus_service);
  }

  G_APPLICATION_CLASS (gaeul_source_application_parent_class)->dbus_unregister
      (app, connection, object_path);
}

static void
gaeul_source_application_class_init (GaeulSourceApplicationClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->dispose = gaeul_source_application_dispose;

  app_class->command_line = gaeul_source_application_command_line;
  app_class->activate = gaeul_source_application_activate;
  app_class->shutdown = gaeul_source_application_shutdown;
  app_class->dbus_register = gaeul_source_application_dbus_register;
  app_class->dbus_unregister = gaeul_source_application_dbus_unregister;
}

static void
gaeul_source_application_init (GaeulSourceApplication * self)
{
  g_application_set_flags (G_APPLICATION (self),
      G_APPLICATION_HANDLES_COMMAND_LINE);
}
