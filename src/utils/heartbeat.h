// Copyright 2024 Northern.tech AS
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#ifndef HEARTBEAT_H
#define HEARTBEAT_H

/**
 * Initialize and start the heartbeat LED thread.
 * The LED will pulse to indicate the system is running.
 *
 * On ESP32-S3-DevKitC, uses the on-board WS2812 RGB LED.
 * If no LED strip is configured, this function does nothing.
 */
void heartbeat_start(void);

#endif /* HEARTBEAT_H */
