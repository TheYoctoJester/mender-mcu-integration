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

#ifndef DISPLAY_SCROLL_H
#define DISPLAY_SCROLL_H

/**
 * Initialize and start the scrolling text animation.
 * Text scrolls continuously below the logo showing version/build info.
 *
 * Requires CONFIG_DISPLAY to be enabled.
 * If no display is configured, this function does nothing.
 */
void display_scroll_start(void);

#endif /* DISPLAY_SCROLL_H */
