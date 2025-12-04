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

#ifndef FONT8X16_H
#define FONT8X16_H

#include <stdint.h>

#define FONT_WIDTH      8
#define FONT_HEIGHT     16
#define FONT_FIRST_CHAR 32   /* Space */
#define FONT_LAST_CHAR  126  /* Tilde */
#define FONT_NUM_CHARS  (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)

/**
 * 8x16 monospace bitmap font data.
 * Each character is 16 bytes (one byte per row, 8 pixels wide).
 * Bit 7 is leftmost pixel, bit 0 is rightmost.
 */
extern const uint8_t font8x16_data[FONT_NUM_CHARS][FONT_HEIGHT];

#endif /* FONT8X16_H */
