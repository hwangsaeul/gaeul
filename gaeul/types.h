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

#ifndef __GAEUL_TYPES_H__
#define __GAEUL_TYPES_H__

#include <glib.h>

#define GAEUL_AUTHENTICATOR_ERROR       (gaeul_authenticator_error_quark ())
GQuark gaeul_authenticator_error_quark  (void);

typedef enum {
  GAEUL_AUTHENTICATOR_ERROR_NO_SUCH_TOKEN,
} GaeulAuthenticatorError;

#endif // __GAEUL_TYPES_H__
