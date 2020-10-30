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

#ifndef __GAEUL_RELAY_AGENT_TEST_H__
#define __GAEUL_RELAY_AGENT_TEST_H__

#include <hwangsae/test/test.h>

#include "gaeul/relay/relay-generated.h"

#define GAEUL_TYPE_RELAY_AGENT_TEST     (gaeul_relay_agent_test_get_type ())
G_DECLARE_DERIVABLE_TYPE (GaeulRelayAgentTest, gaeul_relay_agent_test, GAEUL,
    RELAY_AGENT_TEST, GObject)

struct _GaeulRelayAgentTestClass
{
  GObjectClass parent_class;

  void      (* setup)               (GaeulRelayAgentTest * self);
  void      (* teardown)            (GaeulRelayAgentTest * self);
};

void                   gaeul_relay_agent_test_run                 (GaeulRelayAgentTest *self);

void                   gaeul_relay_agent_test_add_sink_token      (GaeulRelayAgentTest *self,
                                                                   const gchar         *username);

void                   gaeul_relay_agent_test_add_source_token    (GaeulRelayAgentTest *self,
                                                                   const gchar         *username,
                                                                   const gchar         *resource);

void                   gaeul_relay_agent_test_remove_sink_token   (GaeulRelayAgentTest *self,
                                                                   const gchar         *username);

void                   gaeul_relay_agent_test_remove_source_token (GaeulRelayAgentTest *self,
                                                                   const gchar         *username,
                                                                   const gchar         *resource);

Gaeul2DBusRelay      * gaeul_relay_agent_test_get_dbus_proxy      (GaeulRelayAgentTest *self);

HwangsaeTestStreamer * gaeul_relay_agent_test_create_sink         (GaeulRelayAgentTest *self);

GstElement           * gaeul_relay_agent_test_create_source       (GaeulRelayAgentTest *self);

void                   gaeul_relay_agent_test_finish              (GaeulRelayAgentTest *self);

#endif // __GAEUL_RELAY_AGENT_TEST_H__
