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

#ifndef __GAEUL_TUPLE_H__
#define __GAEUL_TUPLE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GAEUL_TYPE_TUPLE      (gaeul_tuple_get_type ())
G_DECLARE_FINAL_TYPE          (GaeulTuple, gaeul_tuple, GAEUL, TUPLE, GObject)

GaeulTuple     *gaeul_tuple_new                (void);

gboolean        gaeul_tuple_insert             (GaeulTuple         *self, 
                                                const gchar        *first_key,
                                                const gchar        *second_key,
                                                GObject            *object);

gboolean        gaeul_tuple_remove             (GaeulTuple         *self, 
                                                const gchar        *first_key,
                                                const gchar        *second_key);

G_END_DECLS

#endif // __GAEUL_TUPLE_H__
