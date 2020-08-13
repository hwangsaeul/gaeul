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
#include "gaeul/relay/relay-application.h"

struct _GaeulRelayApplication
{
  GaeulApplication parent;
};

/* *INDENT-OFF* */
G_DEFINE_TYPE (GaeulRelayApplication, gaeul_relay_application, GAEUL_TYPE_APPLICATION)
/* *INDENT-ON* */

static void
gaeul_relay_application_dispose (GObject * object)
{
  G_OBJECT_CLASS (gaeul_relay_application_parent_class)->dispose (object);
}

static void
gaeul_relay_application_class_init (GaeulRelayApplicationClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gaeul_relay_application_dispose;
}

static void
gaeul_relay_application_init (GaeulRelayApplication * self)
{

}
