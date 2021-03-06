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

#ifndef __GAEUL_STREAM_AUTHENTICATOR_H__
#define __GAEUL_STREAM_AUTHENTICATOR_H__

#include <glib-object.h>
#include <gaeguli/gaeguli.h>
#include <hwangsae/hwangsae.h>

G_BEGIN_DECLS

#define GAEUL_TYPE_STREAM_AUTHENTICATOR (gaeul_stream_authenticator_get_type())
G_DECLARE_FINAL_TYPE                    (GaeulStreamAuthenticator, gaeul_stream_authenticator, GAEUL, STREAM_AUTHENTICATOR, GObject)

GaeulStreamAuthenticator *gaeul_stream_authenticator_new (HwangsaeRelay           *relay);

void                      gaeul_stream_authenticator_add_sink_token
                                                        (GaeulStreamAuthenticator *authenticator,
                                                         const gchar              *username);

void                      gaeul_stream_authenticator_add_source_token
                                                        (GaeulStreamAuthenticator *authenticator,
                                                         const gchar              *username,
                                                         const gchar              *resource);

void                      gaeul_stream_authenticator_set_sink_credentials
                                                        (GaeulStreamAuthenticator *authenticator,
                                                         const gchar              *username,
                                                         const gchar              *passphrase,
                                                         GaeguliSRTKeyLength       pbkeylen);

void                      gaeul_stream_authenticator_set_source_credentials
                                                        (GaeulStreamAuthenticator *authenticator,
                                                         const gchar              *username,
                                                         const gchar              *resource,
                                                         const gchar              *passphrase,
                                                         GaeguliSRTKeyLength       pbkeylen);

gboolean                  gaeul_stream_authenticator_remove_sink_token
                                                        (GaeulStreamAuthenticator *authenticator,
                                                         const gchar              *username);

gboolean                  gaeul_stream_authenticator_remove_source_token
                                                        (GaeulStreamAuthenticator *authenticator,
                                                         const gchar              *username,
                                                         const gchar              *resource);

GVariant                 *gaeul_stream_authenticator_list_sink_tokens
                                                        (GaeulStreamAuthenticator *authenticator);

GVariant                 *gaeul_stream_authenticator_list_source_tokens
                                                        (GaeulStreamAuthenticator *authenticator);

G_END_DECLS

#endif // __GAEUL_STREAM_AUTHENTICATOR_H__
