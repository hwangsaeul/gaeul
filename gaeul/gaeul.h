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

#ifndef __GAEUL_H__
#define __GAEUL_H__

#include <gio/gio.h>
#include <gaeguli/gaeguli.h>

G_BEGIN_DECLS

GSettings      *gaeul_gsettings_new         (const gchar * schema_id,
                                             const gchar * path);

gboolean        gaeul_parse_srt_uri         (const gchar       *url,
                                             gchar            **host,
                                             guint             *port,
                                             GaeguliSRTMode    *mode);
G_END_DECLS

#endif // __GAEUL_H__
