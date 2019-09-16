/**
 *  Copyright 2019 SK Telecom, Co., Ltd.
 *    Author: Jeongseok Kim <jeongseok.kim@sk.com>
 *
 * This library is a proprietary software; you can *NOT* redistribute
 * and/or modify it without the express permission of SK Telecom Co., Ltd.
 *
 */

#ifndef __GAEUL_AGENT_H__
#define __GAEUL_AGENT_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GAEUL_TYPE_AGENT                (gaeul_agent_get_type ())
G_DECLARE_FINAL_TYPE                    (GaeulAgent, gaeul_agent, GAEUL, AGENT, GApplication)

G_END_DECLS

#endif // __GAEUL_AGENT_H__
