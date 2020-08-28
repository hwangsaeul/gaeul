/**
 *  Copyright 2019-2020 SK Telecom Co., Ltd.
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

#ifndef __GAEUL_APPLICATION_H__
#define __GAEUL_APPLICATION_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define GAEUL_TYPE_APPLICATION      (gaeul_application_get_type ())
G_DECLARE_DERIVABLE_TYPE            (GaeulApplication, gaeul_application, GAEUL, APPLICATION, GApplication)

struct _GaeulApplicationClass
{
  GApplicationClass parent_class;
};

typedef enum {
  GAEUL_APPLICATION_DBUS_TYPE_NONE = G_BUS_TYPE_NONE,
  GAEUL_APPLICATION_DBUS_TYPE_SYSTEM = G_BUS_TYPE_SYSTEM,
  GAEUL_APPLICATION_DBUS_TYPE_SESSION = G_BUS_TYPE_SESSION,
} GaeulApplicationDBusType; 

int     gaeul_application_run       (GaeulApplication      *self,
                                     int                    argc,
                                     char                 **argv);

void            gaeul_application_set_uid   (GaeulApplication      *self,
                                             const gchar           *uid);

const gchar    *gaeul_application_get_uid   (GaeulApplication      *self);

void            gaeul_application_set_config_path
                                            (GaeulApplication      *self,
                                             GaeulApplication      *config_path);

const gchar    *gaeul_application_get_config_path
                                            (GaeulApplication      *self);

G_END_DECLS

#endif // __GAEUL_APPLICATION_H__
