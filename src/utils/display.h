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

#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Font dimensions (5x7 pixel font with 1px spacing) */
#define FONT_WIDTH  5
#define FONT_HEIGHT 7
#define CHAR_WIDTH  6  /* includes 1px spacing */
#define CHAR_HEIGHT 8  /* includes 1px spacing */

/* Footer configuration */
#define FOOTER_HEIGHT    10
#define FOOTER_Y_OFFSET  2  /* padding from bottom */

/* RGB565 colors */
#define COLOR_WHITE  0xFFFF
#define COLOR_BLACK  0x0000
#define COLOR_TEAL   0x0410  /* Mender teal ~#00837f */

/**
 * @brief Initialize display subsystem
 * @return 0 on success, negative errno on error
 */
int display_init(void);

/**
 * @brief Display the Mender logo centered on screen
 */
void display_logo(void);

/**
 * @brief Update the footer overlay with current status
 * @param version Version string to display
 * @param ip_addr IP address string
 * @param state Client state string
 */
void display_update_footer(const char *version, const char *ip_addr, const char *state);

/**
 * @brief Get the current IP address as a string
 * @param buf Buffer to store IP address
 * @param buf_len Length of buffer
 * @return 0 on success, negative errno on error
 */
int display_get_ip_string(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* __DISPLAY_H__ */
