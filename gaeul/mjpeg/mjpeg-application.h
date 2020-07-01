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

#ifndef __GAEUL_MJPEG_APPLICATION_H__
#define __GAEUL_MJPEG_APPLICATION_H__

#include "application.h"

G_BEGIN_DECLS

#define GAEUL_TYPE_MJPEG_APPLICATION    (gaeul_mjpeg_application_get_type())
G_DECLARE_FINAL_TYPE                    (GaeulMjpegApplication, gaeul_mjpeg_application, GAEUL, MJPEG_APPLICATION, GaeulApplication)

#define GAEUL_MJPEG_APPLICATION_SCHEMA_ID      "org.hwangsaeul.Gaeul2.MJPEG"

G_END_DECLS

#endif // __GAEUL_MJPEG_APPLICATION_H__
