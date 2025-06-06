/******************************************************************************
** MIT License
**
** Copyright (c) 2025 Andrey Tsigler
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
**
******************************************************************************/
/******************************************************************************
** LATEST REVISION/AUTHOR
** January-09-2025/Andrey Tsigler
******************************************************************************/
/******************************************************************************
** This file contains APIs to manage routing table hash tables.
**
******************************************************************************/
#include <arpa/inet.h>

#include "hash-internal.h"
#include "lpmht-api.h"

union ipv4PrefixMap_u {
  unsigned int prefix32;
  unsigned char prefix[4];
};

const static union ipv4PrefixMap_u ipv4_prefix_map[33] = {
    {.prefix = {0x00, 0x00, 0x00, 0x00}}, {.prefix = {0x80, 0x00, 0x00, 0x00}},
    {.prefix = {0xc0, 0x00, 0x00, 0x00}}, {.prefix = {0xe0, 0x00, 0x00, 0x00}},
    {.prefix = {0xf0, 0x00, 0x00, 0x00}}, {.prefix = {0xf8, 0x00, 0x00, 0x00}},
    {.prefix = {0xfc, 0x00, 0x00, 0x00}}, {.prefix = {0xfe, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0x00, 0x00, 0x00}}, {.prefix = {0xff, 0x80, 0x00, 0x00}},
    {.prefix = {0xff, 0xc0, 0x00, 0x00}}, {.prefix = {0xff, 0xe0, 0x00, 0x00}},
    {.prefix = {0xff, 0xf0, 0x00, 0x00}}, {.prefix = {0xff, 0xf8, 0x00, 0x00}},
    {.prefix = {0xff, 0xfc, 0x00, 0x00}}, {.prefix = {0xff, 0xfe, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0x00, 0x00}}, {.prefix = {0xff, 0xff, 0x80, 0x00}},
    {.prefix = {0xff, 0xff, 0xc0, 0x00}}, {.prefix = {0xff, 0xff, 0xe0, 0x00}},
    {.prefix = {0xff, 0xff, 0xf0, 0x00}}, {.prefix = {0xff, 0xff, 0xf8, 0x00}},
    {.prefix = {0xff, 0xff, 0xfc, 0x00}}, {.prefix = {0xff, 0xff, 0xfe, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0x00}}, {.prefix = {0xff, 0xff, 0xff, 0x80}},
    {.prefix = {0xff, 0xff, 0xff, 0xc0}}, {.prefix = {0xff, 0xff, 0xff, 0xe0}},
    {.prefix = {0xff, 0xff, 0xff, 0xf0}}, {.prefix = {0xff, 0xff, 0xff, 0xf8}},
    {.prefix = {0xff, 0xff, 0xff, 0xfc}}, {.prefix = {0xff, 0xff, 0xff, 0xfe}},
    {.prefix = {0xff, 0xff, 0xff, 0xff}},
};

union ipv6PrefixMap_u {
  unsigned __int128 prefix128;
  unsigned char prefix[16];
};

const static union ipv6PrefixMap_u ipv6_prefix_map[129] = {
    {.prefix = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0x80, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xc0, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xe0, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xf0, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xf8, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xfc, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xfe, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0x80, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xc0, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xe0, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xf0, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xf8, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xfc, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xfe, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0x00, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0x80, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xc0, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xe0, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xf0, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xf8, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xfc, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xfe, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0x00, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0x80, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xc0, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xe0, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xf0, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xf8, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xfc, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xfe, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0x00, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0x80, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xc0, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xe0, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xf0, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xf8, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xfc, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xfe, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0x00}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0x80}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xc0}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xe0}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xf0}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xf8}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xfc}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xfe}},
    {.prefix = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff}}};

/******************************************************************************
** Apply a network mask to the specified IPv4 prefix.
**
**   prefix32 - IPv4 Prefix
**   prefix_size - Number of significant bits in the prefix (0-32).
**
** Return Values:
**  Masked IPv4 Prefix
******************************************************************************/
inline static unsigned int ipv4MaskedPrefixCompute(const unsigned int prefix32,
                                            const unsigned char prefix_size)
{
  return prefix32 & ipv4_prefix_map[prefix_size].prefix32;
}

/******************************************************************************
** Apply a network mask to the specified IPv6 prefix.
**
**   prefix128 - IPv6 Prefix
**   prefix_size - Number of significant bits in the prefix (0-128).
**
** Return Values:
**  Masked IPv6 Prefix
******************************************************************************/
inline static unsigned __int128 ipv6MaskedPrefixCompute(const unsigned __int128 prefix128,
                                                 const unsigned char prefix_size)
{
  return prefix128 & ipv6_prefix_map[prefix_size].prefix128;
}

/******************************************************************************
** Generate a hash index from an IPv4 address.
**
**   masked_prefix - IPv4 Prefix.
**   prefix_size - Number of significant bits in the prefix.
**   table_size - Max prefixes in the hash table.
**
** Return Values:
**  Hash Index.
******************************************************************************/
inline static unsigned int ipv4HashCompute(unsigned int masked_prefix,
                                    const unsigned char prefix_size,
                                    const unsigned int table_size)
{
  unsigned int hval = 0x811c9dc5;
#if defined(__SSE4_1__) && (LPMHT_HASH_HARDWARE == 1)
  /* x86 hardware assisted hash.
  ** The CRC function is not as good as FNV, but the overall performance is
  ** better, even with more collisions.
  */
  hval = __builtin_ia32_crc32si(hval, masked_prefix);
  hval = __builtin_ia32_crc32qi(hval, prefix_size);

#elif defined(__ARM_FEATURE_CRC32) && defined(__linux) &&                      \
    (LPMHT_HASH_HARDWARE == 1)
  /* The ARM64 CRC Instructions compiling on Linux VM on Mac M1.
   */
  hval = __builtin_aarch64_crc32w(hval, masked_prefix);
  hval = __builtin_aarch64_crc32b(hval, prefix_size);
#elif defined(__ARM_FEATURE_CRC32) && defined(__APPLE__) &&                    \
    (LPMHT_HASH_HARDWARE == 1)

  /* The ARM64 CRC Instructions compiling natively on MacOS (M1).
   */
  hval = __builtin_arm_crc32w(hval, masked_prefix);
  hval = __builtin_arm_crc32b(hval, prefix_size);

#else
  int i;

  for (i = 0; i < 4; i++)
  {
    hval ^= ((unsigned char)masked_prefix);
    masked_prefix >>= 8;
    hval *= 0x01000193;
  }
  hval ^= prefix_size;
  hval *= 0x01000193;
#endif

  hval %= table_size;

  return hval;
}

/******************************************************************************
** Generate a hash index from an IPv6 address.
**
**   masked_prefix - IPv6 Prefix.
**   prefix_size - Number of significant bits in the prefix.
**   table_size - Max prefixes in the hash table.
**
** Return Values:
**  Hash Index.
******************************************************************************/
inline static unsigned int ipv6HashCompute(unsigned __int128 masked_prefix,
                                    const unsigned char prefix_size,
                                    const unsigned int table_size)
{
  unsigned int hval = 0x811c9dc5;
#if defined(__SSE4_1__) && (LPMHT_HASH_HARDWARE == 1)
  {
    unsigned long long *prefix = (unsigned long long *)&masked_prefix;

    /* x86 hardware assisted hash.
    ** The CRC function is not as good as FNV, but the overall performance is
    ** better even with more collisions.
    */
    hval = __builtin_ia32_crc32di(hval, prefix[0]);
    hval = __builtin_ia32_crc32di(hval, prefix[1]);
    hval = __builtin_ia32_crc32qi(hval, prefix_size);
  }
#elif defined(__ARM_FEATURE_CRC32) && defined(__linux) &&                      \
    (LPMHT_HASH_HARDWARE == 1)
  {
    unsigned int *prefix = (unsigned int *)&masked_prefix;

    /* The ARM64 CRC Instructions compiling on Linux VM on Mac M1.
     */
    hval = __builtin_aarch64_crc32w(hval, prefix[0]);
    hval = __builtin_aarch64_crc32w(hval, prefix[1]);
    hval = __builtin_aarch64_crc32w(hval, prefix[2]);
    hval = __builtin_aarch64_crc32w(hval, prefix[3]);
    hval = __builtin_aarch64_crc32b(hval, prefix_size);
  }
#elif defined(__ARM_FEATURE_CRC32) && defined(__APPLE__) &&                    \
    (LPMHT_HASH_HARDWARE == 1)
  {
    unsigned long long *prefix = (unsigned long long *)&masked_prefix;

    /* The ARM64 CRC Instructions compiling natively on MacOS (M1).
     */
    hval = __builtin_arm_crc32d(hval, prefix[0]);
    hval = __builtin_arm_crc32d(hval, prefix[1]);
    hval = __builtin_arm_crc32b(hval, prefix_size);
  }
#else
  int i;

  for (i = 0; i < 16; i++)
  {
    hval ^= ((unsigned char)masked_prefix);
    masked_prefix >>= 8;
    hval *= 0x01000193;
  }

  hval ^= prefix_size;
  hval *= 0x01000193;
#endif

  hval %= table_size;

  return hval;
}

/******************************************************************************
** Find specified route.
**
** Return Values:
** 0 - Route Not Found
** 1 - Route Found.
******************************************************************************/
int sharedHash::routeFind(const unsigned int prefix32, const unsigned __int128 prefix128,
                          const unsigned char prefix_size, unsigned long *user_data,
                          const unsigned int clear_hit_count,
                          unsigned long long *hit_count, unsigned int *hash_key,
                          unsigned int *masked_prefix32,
                          unsigned __int128 *masked_prefix128,
                          unsigned int *route_idx)
{
  unsigned int key;

  /* We need to perform a hash table search.
   */
  if (route_table.ip_mode == LPMHT_IPV4)
  {
    hashIpv4Route_t *ipv4_route;
    unsigned int route_index;
    unsigned int masked_prefix;

    /* Search the IPv4 hash table.
     */
    masked_prefix = ipv4MaskedPrefixCompute(prefix32, prefix_size);
    *masked_prefix32 = masked_prefix;

    if (0 == route_table.hash_table_size)
      return 0;

    /* We need to compute the hash key, even when there are
    ** no entries in the hash table.
    */
    key = ipv4HashCompute(masked_prefix, prefix_size,
                          route_table.hash_table_size);
    *hash_key = key;

    /* If there are no routes in the prefix hash table then skip the hash table
     * search.
     */
    if (0 == route_table.num_routes_in_prefix[prefix_size])
      return 0;

    route_index = route_table.hash_table[key];
    if (!route_index)
      return 0;

    ipv4_route = &route_table.ipv4_route[route_index];
    *route_idx = route_index;
    while ((ipv4_route->prefix_size != prefix_size) ||
           (ipv4_route->ipv4_addr != masked_prefix))
    {
      if (!ipv4_route->next)
        return 0;

      *route_idx = ipv4_route->next;
      ipv4_route = &route_table.ipv4_route[ipv4_route->next];
    }
    *user_data = ipv4_route->user_data;
    if (hit_count)
      *hit_count = 0;
    if ((route_table.enable_hit_count) && (hit_count || clear_hit_count))
    {
      if (clear_hit_count)
      {
        if (hit_count)
          *hit_count =
              std::atomic_exchange_explicit(&ipv4_route->hit_count, 0, 
			      std::memory_order_relaxed);
        else
          (void)std::atomic_exchange_explicit(&ipv4_route->hit_count, 0,
                                    std::memory_order_relaxed);
      }
      else
      {
        if (hit_count)
          *hit_count =
              std::atomic_load_explicit(&ipv4_route->hit_count, 
			      			std::memory_order_relaxed);
      }
    }
  }
  else
  {
    hashIpv6Route_t *ipv6_route;
    unsigned int route_index;
    unsigned __int128 masked_prefix;

    /* Search the IPv6 hash table.
     */
    masked_prefix = ipv6MaskedPrefixCompute(prefix128, prefix_size);
    *masked_prefix128 = masked_prefix;

    if (0 == route_table.hash_table_size)
      return 0;

    /* We need to generate the hash key even when the route is not in the
     * database.
     */
    key = ipv6HashCompute(masked_prefix, prefix_size,
                          route_table.hash_table_size);
    *hash_key = key;

    /* If there are no routes in the prefix hash table then skip the hash table
     * search.
     */
    if (0 == route_table.num_routes_in_prefix[prefix_size])
      return 0;
    route_index = route_table.hash_table[key];
    if (!route_index)
      return 0;

    ipv6_route = &route_table.ipv6_route[route_index];
    *route_idx = route_index;
    while ((ipv6_route->prefix_size != prefix_size) ||
           (ipv6_route->ipv6_addr != masked_prefix))
    {
      if (!ipv6_route->next)
        return 0;

      *route_idx = ipv6_route->next;
      ipv6_route = &route_table.ipv6_route[ipv6_route->next];
    }
    *user_data = ipv6_route->user_data;
    if ((route_table.enable_hit_count) && (hit_count || clear_hit_count))
    {
      if (clear_hit_count)
      {
        if (hit_count)
          *hit_count =
              std::atomic_exchange_explicit(&ipv6_route->hit_count, 0, 
			      std::memory_order_relaxed);
        else
          (void)std::atomic_exchange_explicit(&ipv6_route->hit_count, 0,
                                    std::memory_order_relaxed);
      }
      else
      {
        if (hit_count)
          *hit_count =
              std::atomic_load_explicit(&ipv6_route->hit_count, 
			      std::memory_order_relaxed);
      }
    }
  }

  return 1;
}

/******************************************************************************
** Changes the size of the hash table. The function is used
** to increase and decrease the size of the table.
**
** When the hash table size is changed, all entries must be rehashed.
**
**   desired_blocks_in_hash - How many blocks we need.
**
** Return Values:
**  none
******************************************************************************/
void sharedHash::hashRehash(const unsigned int desired_blocks_in_hash)
{
  unsigned int last_route_idx;
  unsigned int i, idx;
  unsigned int hash_key;
  unsigned int hash_table_size = route_table.hash_table_size;
  unsigned int *hash_table = route_table.hash_table;

  /* Clear out the current hash table.
   */
  memset(hash_table, 0, hash_table_size * sizeof(route_table.hash_table[0]));

  /* Add or remove memory to/from the hash table.
  ** When adding memory, clear the newly added memory to 0.
  */
  if (desired_blocks_in_hash > route_table.num_blocks_in_hash)
  {
    unsigned int bytes_in_block =
        LPMHT_HASH_BLOCK_SIZE * sizeof(route_table.hash_table[0]);

    for (i = 0; i < (desired_blocks_in_hash - route_table.num_blocks_in_hash);
         i++)
    {
      if (hash_mb.elementAlloc(&idx))
	      throw ERR_MSG ("Memory Allocation Error");

      memset(&route_table.hash_table[idx * LPMHT_HASH_BLOCK_SIZE], 0,
             bytes_in_block);
      route_table.memory_size += bytes_in_block;
    }
  }
  else
  {
    for (i = 0; i < (route_table.num_blocks_in_hash - desired_blocks_in_hash);
         i++)
    {
      (void)hash_mb.elementFree();
      route_table.memory_size -=
          LPMHT_HASH_BLOCK_SIZE * sizeof(route_table.hash_table[0]);
    }
  }

  /* Set the new hash table size.
   */
  route_table.num_blocks_in_hash = desired_blocks_in_hash;
  route_table.hash_table_size =
      route_table.num_blocks_in_hash * LPMHT_HASH_BLOCK_SIZE;
  hash_table_size = route_table.hash_table_size;

  if (route_mb.lastElementGet(&last_route_idx))
	      throw ERR_MSG ("Unexpected Error");

  /* Re-insert all routes into the new hash table.
   */
  if (route_table.ip_mode == LPMHT_IPV4)
  {
    hashIpv4Route_t *ipv4_route = route_table.ipv4_route;
    ;

    for (i = 1; i <= last_route_idx; i++)
    {
      hash_key = ipv4HashCompute(ipv4_route[i].ipv4_addr,
                                 ipv4_route[i].prefix_size, hash_table_size);
      ipv4_route[i].next = hash_table[hash_key];
      ipv4_route[i].prev = 0;
      hash_table[hash_key] = i;
      if (ipv4_route[i].next)
        ipv4_route[ipv4_route[i].next].prev = i;
    }
  }
  else
  {
    hashIpv6Route_t *ipv6_route = route_table.ipv6_route;

    for (i = 1; i <= last_route_idx; i++)
    {
      hash_key = ipv6HashCompute(ipv6_route[i].ipv6_addr,
                                 ipv6_route[i].prefix_size, hash_table_size);
      ipv6_route[i].next = hash_table[hash_key];
      ipv6_route[i].prev = 0;
      hash_table[hash_key] = i;
      if (ipv6_route[i].next)
        ipv6_route[ipv6_route[i].next].prev = i;
    }
  }
}

/******************************************************************************
** IPv4 Rule Generator Thread.
******************************************************************************/
void sharedHash::ipv4_rules_thread(void)
{
  int do_work, do_restart;
  unsigned int i;
  auto st = ipv4_rules_generator.get_stop_token();

  do
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (st.stop_requested())
      			return;

    READ_LOCK();

    do_work = route_table.ipv4_new_rules_needed;
    READ_UNLOCK();

    if (0 == do_work)
      continue;

    WRITE_LOCK();
    route_table.ipv4_new_rules_needed = 0;
    WRITE_UNLOCK();

    /* This loop repeats around 16 million times...
    ** We release the semaphore after each iteration to allow
    ** route adds/deletes to proceed if needed.
    ** If route is added/deleted during this loop then
    ** we go back to the sleep at the top, and start
    ** again later.
    */
    for (i = 0; i < (1 << 24); i++)
    {
      READ_LOCK();
      {
        const unsigned int num_active_prefixes =
            route_table.num_active_prefixes;
        const unsigned int hash_table_size = route_table.hash_table_size;
        const unsigned int prefix32 = htonl(i << 8);
        unsigned int idx = 0;

        /* Perform the longest prefix match on the prefix.
        ** The algorithm is almost identical to the standard LPM, except we
        ** only search the prefixes that are 24 bits and shorter.
        */
        for (unsigned int j = 0; j < num_active_prefixes; j++)
        {
          const unsigned char p_size = route_table.active_prefix_list[j];

          if (p_size > 24)
            continue;

          {
            const unsigned int masked_prefix32 =
                ipv4MaskedPrefixCompute(prefix32, p_size);
            const unsigned int hash_key =
                ipv4HashCompute(masked_prefix32, p_size, hash_table_size);

            idx = route_table.hash_table[hash_key];

            if (0 == idx)
              continue;

            while ((route_table.ipv4_route[idx].prefix_size != p_size) ||
                   (route_table.ipv4_route[idx].ipv4_addr != masked_prefix32))
            {
              idx = route_table.ipv4_route[idx].next;
              if (0 == idx)
                break;
            }
            if (idx)
              break;
          }
        }
        /* At this point if we found a matching route then idx is the index of
        *the route.
        ** If we didn't find the route then idx is 0.
        ** Simply add the idx into the rules table.
        */
        route_table.ipv4_rule_table[i] = idx;
      }
      do_restart = route_table.ipv4_new_rules_needed;
      READ_UNLOCK();

      /* Check if the routing table is being deleted.
       */
      if (st.stop_requested())
      			return;

      if (do_restart)
        break;
    }
    if (do_restart)
      continue;

    WRITE_LOCK();
    if (0 == route_table.ipv4_new_rules_needed)
    {
      route_table.ipv4_rules_ready = 1;
    }
    WRITE_UNLOCK();

  } while (1);
}

/******************************************************************************
** Try locking the flow entry.
** If the lock is not available then return an error.
**
** lock - Lock that we try to take.
**
** Return Values:
** true - Lock Not Available.
** false - Lock Obtained.
******************************************************************************/
inline static bool ipv6FlowLock(std::atomic_flag *lock)
{
  return lock->test_and_set( std::memory_order_acquire);
}

/******************************************************************************
** Unlock the flow entry.
** This function should only be invoked on locked locks.
**
** lock - Lock that we try to take.
**
******************************************************************************/
inline static void ipv6FlowUnlock(std::atomic_flag *lock)
{
  lock->clear(std::memory_order_release);
}

/******************************************************************************
** IPv6 Flow Age Thread.
**
** The thread wakes up periodically, and iterates through all flows.
** If the "flow_detected" flag is set, the the function clears it.
** If the "flow_detected" is not set then the function deletes the flow.
******************************************************************************/
void sharedHash::ipv6_flow_age_thread(void)
{
  unsigned int i;
  auto st = ipv6_flow_ageing_thread.get_stop_token();

  do
  {
    for (unsigned int sleep_time = 0; sleep_time < route_table.flow_age_dispatch_time; sleep_time++)
    {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (st.stop_requested())
        return;
    }

    for (i = 0; i < route_table.ipv6_max_flows; i++)
    {
      READ_LOCK();
      if (false == ipv6FlowLock(&route_table.ipv6_flow_table[i].entry_lock))
      {
        if (route_table.ipv6_flow_table[i].flow_detected)
        {
          route_table.ipv6_flow_table[i].flow_detected = 0;
        }
        else
        {
          if (route_table.ipv6_flow_table[i].route_index)
            route_table.ipv6_flow_table[i].route_index = 0;
        }

        ipv6FlowUnlock(&route_table.ipv6_flow_table[i].entry_lock);
      }
      READ_UNLOCK();

      /* Check if the routing table is being deleted.
       */
      if (st.stop_requested())
      			return;
    }

  } while (1);
}
/******************************************************************************
*******************************************************************************
** API Functions
*******************************************************************************
******************************************************************************/

/******************************************************************************
** Create a new Hash Route Table.
******************************************************************************/
sharedHash::sharedHash(const unsigned int max_routes, const lpmhtIpMode_e ip_mode,
                       const lpmhtTableProp_t& prop)
{
  unsigned int idx;
  unsigned int hash_prealloc = 0;

  memset(&route_table, 0, sizeof(route_table));
  route_table.ipv6_flow_not_found = new std::atomic <unsigned long long>;
  if (!route_table.ipv6_flow_not_found)
	      throw ERR_MSG ("Memory Allocation Error");

  route_table.ipv6_flow_not_found->store(0);

  if (prop.mem_prealloc)
  {
    route_table.mem_prealloc = 1;
    hash_prealloc = 1;
  }

  if (hash_prealloc || prop.hash_prealloc)
    route_table.reserve_hash_memory = 1;

  if ((ip_mode == LPMHT_IPV4) && prop.ipv4_rules)
    route_table.ipv4_rules_enabled = 1;

  if ((ip_mode == LPMHT_IPV6) && prop.ipv6_flow)
  {
    route_table.ipv6_flow_enabled = 1;
    route_table.ipv6_max_flows = (0 == prop.ipv6_max_flows)
                                     ? IPV6_DEFAULT_FLOW_COUNT
                                     : prop.ipv6_max_flows;
    route_table.flow_age_dispatch_time =
        (0 == prop.ipv6_flow_age_time) ? IPV6_DEFAULT_FLOW_AGE_DISPATCH_TIME
                                        : prop.ipv6_flow_age_time;
  }

  route_table.ip_mode = ip_mode;
  route_table.max_routes = max_routes;

  if (prop.hit_count)
    route_table.enable_hit_count = 1;

  /* Create the hash table.
  ** We are using the memory block utility to help us manage
  ** the hash table. Initially, only virtual memory is allocated for the
  ** hash table. Physical memory will be allocated later.
  */
  route_table.max_blocks_in_hash =
      (route_table.max_routes * LPMHT_HASH_FACTOR) / LPMHT_HASH_BLOCK_SIZE;
  if ((route_table.max_routes * LPMHT_HASH_FACTOR) % LPMHT_HASH_BLOCK_SIZE)
    route_table.max_blocks_in_hash++;

  route_table.hash_table = (unsigned int *)hash_mb.init(
      sizeof(unsigned int) * LPMHT_HASH_BLOCK_SIZE,
      route_table.max_blocks_in_hash, route_table.mem_prealloc);
  route_table.virtual_memory_size += hash_mb.blockVirtualSizeGet();

  if (0 != route_table.reserve_hash_memory)
  {
    unsigned int i;
    /* The caller wants to allocate memory for the whole hash table
     */
    for (i = 0; i < route_table.max_blocks_in_hash; i++)
    {
      (void)hash_mb.elementAlloc(&idx);
      route_table.num_blocks_in_hash++;
      route_table.hash_table_size += LPMHT_HASH_BLOCK_SIZE;
    }
    memset(route_table.hash_table, 0,
           sizeof(unsigned int) * LPMHT_HASH_BLOCK_SIZE *
               route_table.max_blocks_in_hash);

    route_table.memory_size += sizeof(unsigned int) * LPMHT_HASH_BLOCK_SIZE *
                               route_table.max_blocks_in_hash;
  }

  /* Create either IPv4 or IPv6 routing table. Initially only virtual memory
  ** is allocated for the routes.
  */
  if (ip_mode == LPMHT_IPV4)
  {
    route_table.ipv4_route = (hashIpv4Route_t *)route_mb.init(
        sizeof(hashIpv4Route_t), route_table.max_routes + 1,
        route_table.mem_prealloc);
  }
  else
  {
    route_table.ipv6_route = (hashIpv6Route_t *)route_mb.init(
        sizeof(hashIpv6Route_t), route_table.max_routes + 1,
        route_table.mem_prealloc);
  }
  route_table.virtual_memory_size += route_mb.blockVirtualSizeGet();
  if (0 != route_table.mem_prealloc)
    route_table.memory_size = route_table.virtual_memory_size;

  /* Discard the first entry from the routing table. We do this because
  ** the first entry has index 0, and 0 means "not in use" in the hash
  ** table management algorithm.
  */
  (void)route_mb.elementAlloc(&idx);

  /* If the IPv4 rule generation feature is enabled then create the rule
  ** generation thread and allocate memory for the rule table.
  */
  if (route_table.ipv4_rules_enabled)
  {
    unsigned int rule_table_size = (0x1UL << 24) * sizeof(unsigned int);
    /* Allocate 64MB memory block for the IPv4 rule table.
     */
    route_table.ipv4_rule_table =
        (unsigned int *)aligned_alloc(sysconf(_SC_PAGESIZE), rule_table_size);
    if (0 == route_table.ipv4_rule_table)
	      throw ERR_MSG ("Memory Allocation Error");

    {
      lpmhtUtil util;
      util.useHugePages(route_table.ipv4_rule_table, rule_table_size);
    }

    memset(route_table.ipv4_rule_table, 0, rule_table_size);
    route_table.memory_size += rule_table_size;
    route_table.virtual_memory_size += rule_table_size;

    ipv4_rules_generator = std::jthread(&sharedHash::ipv4_rules_thread, this);
  }

  if (route_table.ipv6_flow_enabled)
  {
    size_t flow_table_size =
        sizeof(ipv6FlowTable_t) * route_table.ipv6_max_flows;
    unsigned int page_size = sysconf(_SC_PAGESIZE);

    if (flow_table_size % page_size)
      flow_table_size = ((flow_table_size / page_size) + 1) * page_size;

    /* Allocate memory for the IPv6 flow table.
     */
    route_table.ipv6_flow_table =
        (ipv6FlowTable_t *)aligned_alloc(page_size, flow_table_size);
    if (0 == route_table.ipv6_flow_table)
	      throw ERR_MSG ("Memory Allocation Error");

    {
      lpmhtUtil util;
      util.useHugePages(route_table.ipv6_flow_table, flow_table_size);
    }
    for (auto i = 0U; i < route_table.ipv6_max_flows; i++)
    {
      route_table.ipv6_flow_table[i].entry_lock.clear();
      route_table.ipv6_flow_table[i].flow_detected = 0;
      route_table.ipv6_flow_table[i].route_index = 0;
      route_table.ipv6_flow_table[i].route_flow_correlator = 0;
      route_table.ipv6_flow_table[i].ipv6_addr = 0;
    }
    route_table.memory_size += flow_table_size;
    route_table.virtual_memory_size += flow_table_size;

    ipv6_flow_ageing_thread =
        std::jthread(&sharedHash::ipv6_flow_age_thread, this);
  }
}

/******************************************************************************
** Delete an existing hash routing table.
** The function is NOT thread-safe.
******************************************************************************/
sharedHash::~sharedHash(void)
{
  if (route_table.ipv6_flow_enabled)
  {
    ipv6_flow_ageing_thread.request_stop();
    ipv6_flow_ageing_thread.join();
  }
  if (route_table.ipv4_rules_enabled)
  {
    /* Free the rule table memory.
    ** The rule generation thread is stopped automatically.
    */
    free(route_table.ipv4_rule_table);
  }
  if (route_table.ipv6_flow_enabled)
  {
    /* Free the flow table memory.
    ** The flow ageing thread is stopped automatically.
    */
    free(route_table.ipv6_flow_table);
  }

  hash_mb.destroy();
  route_mb.destroy();

  delete route_table.ipv6_flow_not_found;
}

/******************************************************************************
** Insert the specified Route into the hash table..
**  prefix - The pointer to the route. The route must be in
**          network byte order.
**  prefix_size - Number of bits in the network mask for
**               this route.
**  user_data - The data to be associated with the route.
**
** Return Values:
**  0 - Route is inserted successfully.
** -2 - Route already exists.
** -3 - Exceeded the maximum number of route entries.
******************************************************************************/
int sharedHash::routeInsert(const unsigned char* const prefix, 
			    const unsigned char prefix_size,
                            const unsigned long user_data)
{
  unsigned int prefix32 = 0;
  unsigned int masked_prefix32 = 0;
  unsigned __int128 prefix128 = 0;
  unsigned __int128 masked_prefix128 = 0;
  int entry_found;
  unsigned long old_user_data;
  unsigned int hash_key = 0;
  unsigned int num_blocks_needed;
  unsigned int rehash_needed = 0;
  unsigned int idx;

  if (route_table.num_routes >= route_table.max_routes)
    return -3;

  /* Copy network prefix into an integer.
  ** Note that we are not converting from network to host format
  ** because this is not needed for hash key computation and
  ** prefix comparison.
  */
  if (route_table.ip_mode == LPMHT_IPV4)
    memcpy(&prefix32, prefix, sizeof(prefix32));
  else
    memcpy(&prefix128, prefix, sizeof(prefix128));

  entry_found =
      routeFind(prefix32, prefix128, prefix_size, &old_user_data, 0, 0,
                &hash_key, &masked_prefix32, &masked_prefix128, &idx);

  if (entry_found)
    return -2;

  /* Add a new route to the route table.
   */
  route_table.num_routes++;
  if ((route_table.ip_mode == LPMHT_IPV4) && (prefix_size <= 24))
  {
    route_table.ipv4_new_rules_needed = 1;
    route_table.ipv4_rules_ready = 0;
  }
  if (route_table.ip_mode == LPMHT_IPV6)
    route_table.route_flow_correlator++;

  /* Number of routes with this prefix length
   */
  route_table.num_routes_in_prefix[prefix_size]++;
  if (route_table.num_routes_in_prefix[prefix_size] == 1)
  {
    unsigned int i, j;
    /* We got the first route for this prefix length.
    ** Insert the prefix into the active_prefix_list.
    */
    for (i = 0; i < route_table.num_active_prefixes; i++)
      if (prefix_size > route_table.active_prefix_list[i])
        break;

    for (j = route_table.num_active_prefixes; j > i; j--)
      route_table.active_prefix_list[j] = route_table.active_prefix_list[j - 1];

    route_table.active_prefix_list[i] = prefix_size;
    route_table.num_active_prefixes++;
  }

  /* Determine whether the hash table needs to be expanded. If so, then
  ** we will need to re-hash all routes that are currently in the table.
  */
  if (0 == route_table.reserve_hash_memory)
  {
    num_blocks_needed =
        (route_table.num_routes * LPMHT_HASH_FACTOR) / LPMHT_HASH_BLOCK_SIZE;
    if ((route_table.num_routes * LPMHT_HASH_FACTOR) % LPMHT_HASH_BLOCK_SIZE)
      num_blocks_needed++;

    if (num_blocks_needed > route_table.num_blocks_in_hash)
    {
      /* Need to expand the hash table and rehash all routes for
      ** this prefix.
      */
      hashRehash(num_blocks_needed);

      rehash_needed = 1;
    }
  }

  /* Get the index of the new route entry.
   */
  if (route_mb.elementAlloc(&idx))
	      throw ERR_MSG ("Memory Allocation Error");

  if (route_table.ip_mode == LPMHT_IPV4)
  {
    hashIpv4Route_t *route;

    if (rehash_needed)
      hash_key = ipv4HashCompute(masked_prefix32, prefix_size,
                                 route_table.hash_table_size);

    route = &route_table.ipv4_route[idx];
    route->user_data = user_data;
    route->hit_count = 0;
    route->prefix_size = prefix_size;
    route->ipv4_addr = masked_prefix32;
    route->prev = 0;
    route->next = route_table.hash_table[hash_key];
    route_table.hash_table[hash_key] = idx;
    if (route->next)
      route_table.ipv4_route[route->next].prev = idx;

    if (0 == route_table.mem_prealloc)
      route_table.memory_size += sizeof(hashIpv4Route_t);
  }
  else
  {
    hashIpv6Route_t *route;

    if (rehash_needed)
      hash_key = ipv6HashCompute(masked_prefix128, prefix_size,
                                 route_table.hash_table_size);

    route = &route_table.ipv6_route[idx];
    route->user_data = user_data;
    route->hit_count = 0;
    route->prefix_size = prefix_size;
    route->ipv6_addr = masked_prefix128;
    route->prev = 0;
    route->next = route_table.hash_table[hash_key];
    route_table.hash_table[hash_key] = idx;
    if (route->next)
      route_table.ipv6_route[route->next].prev = idx;

    if (0 == route_table.mem_prealloc)
      route_table.memory_size += sizeof(hashIpv6Route_t);
  }
  return 0;
}

/******************************************************************************
** Delete the specified node from the route table.
******************************************************************************/
int sharedHash::routeDelete(const unsigned char* const prefix, 
				const unsigned char prefix_size)
{
  unsigned int prefix32 = 0;
  unsigned int masked_prefix32 = 0;
  unsigned __int128 prefix128 = 0;
  unsigned __int128 masked_prefix128 = 0;
  int entry_found;
  unsigned long old_user_data;
  unsigned int hash_key = 0;
  unsigned int num_blocks_needed;
  unsigned int idx, last_idx;

  if (0 == route_table.num_routes)
    return -2;

  /* Copy network prefix into an integer.
  ** Note that we are not converting from network to host format
  ** because this is not needed for hash key computation and
  ** prefix comparison.
  */
  if (route_table.ip_mode == LPMHT_IPV4)
    memcpy(&prefix32, prefix, sizeof(prefix32));
  else
    memcpy(&prefix128, prefix, sizeof(prefix128));

  entry_found =
      routeFind(prefix32, prefix128, prefix_size, &old_user_data, 0, 0,
                &hash_key, &masked_prefix32, &masked_prefix128, &idx);

  if (0 == entry_found)
    return -2;

  /* Delete a route from the route table.
   */
  route_table.num_routes--;
  if ((route_table.ip_mode == LPMHT_IPV4) && (prefix_size <= 24))
  {
    route_table.ipv4_new_rules_needed = 1;
    route_table.ipv4_rules_ready = 0;
  }
  if (route_table.ip_mode == LPMHT_IPV6)
    route_table.route_flow_correlator++;

  /* Get the index of the most recently allocated route entry. This is
  ** the entry that will actually be freed.
  */
  if (route_mb.lastElementGet(&last_idx))
	      throw ERR_MSG ("Memory Allocation Error");

  if (route_table.ip_mode == LPMHT_IPV4)
  {
    hashIpv4Route_t *route;

    route = &route_table.ipv4_route[idx];

    /* Unlink the deleted route from the hash bucket route list.
     */
    if (route->next)
      route_table.ipv4_route[route->next].prev = route->prev;

    if (route->prev)
      route_table.ipv4_route[route->prev].next = route->next;

    if (idx == route_table.hash_table[hash_key])
      route_table.hash_table[hash_key] = route->next;

    /* Since we can only free the last_idx entry, we need to copy the content
    ** of last_idx into the idx, and update the table structure to reflect
    ** this change.
    */
    if (last_idx != idx)
    {
      route->hit_count.store(route_table.ipv4_route[last_idx].hit_count.load());
      route->prefix_size = route_table.ipv4_route[last_idx].prefix_size;
      route->ipv4_addr = route_table.ipv4_route[last_idx].ipv4_addr;
      route->prev = route_table.ipv4_route[last_idx].prev;
      route->next = route_table.ipv4_route[last_idx].next;
      route->user_data = route_table.ipv4_route[last_idx].user_data;

      /* If the last_idx route is the first route in the hash bucket then we
      ** need to update the hash bucket.
      */
      if (!route->prev)
      {
        hash_key = ipv4HashCompute(route->ipv4_addr, route->prefix_size,
                                   route_table.hash_table_size);
        route_table.hash_table[hash_key] = idx;
      }
      else
      {
        route_table.ipv4_route[route->prev].next = idx;
      }

      if (route->next)
        route_table.ipv4_route[route->next].prev = idx;
    }

    if (0 == route_table.mem_prealloc)
      route_table.memory_size -= sizeof(hashIpv4Route_t);
  }
  else
  {
    hashIpv6Route_t *route;

    route = &route_table.ipv6_route[idx];

    /* Unlink the deleted route from the hash bucket route list.
     */
    if (route->next)
      route_table.ipv6_route[route->next].prev = route->prev;

    if (route->prev)
      route_table.ipv6_route[route->prev].next = route->next;

    if (idx == route_table.hash_table[hash_key])
      route_table.hash_table[hash_key] = route->next;

    /* Since we can only free the last_idx entry, we need to copy the content
    ** of last_idx into the idx, and update the table structure to reflect
    ** this change.
    */
    if (last_idx != idx)
    {
      route->hit_count.store(route_table.ipv6_route[last_idx].hit_count.load());
      route->prefix_size = route_table.ipv6_route[last_idx].prefix_size;
      route->ipv6_addr = route_table.ipv6_route[last_idx].ipv6_addr;
      route->prev = route_table.ipv6_route[last_idx].prev;
      route->next = route_table.ipv6_route[last_idx].next;
      route->user_data = route_table.ipv6_route[last_idx].user_data;

      /* If the last_idx route is the first route in the hash bucket then we
      ** need to update the hash bucket.
      */
      if (!route->prev)
      {
        hash_key = ipv6HashCompute(route->ipv6_addr, route->prefix_size,
                                   route_table.hash_table_size);
        route_table.hash_table[hash_key] = idx;
      }
      else
      {
        route_table.ipv6_route[route->prev].next = idx;
      }

      if (route->next)
        route_table.ipv6_route[route->next].prev = idx;
    }

    if (0 == route_table.mem_prealloc)
      route_table.memory_size -= sizeof(hashIpv6Route_t);
  }

  (void)route_mb.elementFree();

  /* Determine whether the hash table needs to be shrunk. If so, then
  ** we will need to re-hash all routes that are currently in the table.
  */
  route_table.num_routes_in_prefix[prefix_size]--; /* Number of routes with this
                                                      prefix length */
  if (route_table.num_routes_in_prefix[prefix_size] == 0)
  {
    unsigned int i, j;
    /* We deleted the last route for this prefix length.
    ** delete the prefix from the active_prefix_list.
    */
    for (i = 0; i < route_table.num_active_prefixes - 1; i++)
      if (prefix_size == route_table.active_prefix_list[i])
        break;

    for (j = i; j < route_table.num_active_prefixes - 1; j++)
      route_table.active_prefix_list[j] = route_table.active_prefix_list[j + 1];

    route_table.num_active_prefixes--;
  }

  if (0 == route_table.reserve_hash_memory)
  {
    num_blocks_needed =
        (route_table.num_routes * LPMHT_HASH_FACTOR) / LPMHT_HASH_BLOCK_SIZE;
    if ((route_table.num_routes * LPMHT_HASH_FACTOR) % LPMHT_HASH_BLOCK_SIZE)
      num_blocks_needed++;

    /* Note that we re-hash only when the number of routes for the prefix
    ** is reduced to 0 or when the number of required blocks is two less than
    ** the current number of blocks. Waiting until two blocks can be freed
    ** reduces re-hashing activity.
    */
    if ((0 == num_blocks_needed) ||
        ((route_table.num_blocks_in_hash >= 2) &&
         (num_blocks_needed < (route_table.num_blocks_in_hash - 2))))
    {
      /* Need to shrink the hash table and rehash all routes for
      ** this prefix.
      */
      hashRehash(num_blocks_needed);
    }
  }

  return 0;
}

/******************************************************************************
** Modify the content of the specified route.
******************************************************************************/
int sharedHash::set(const unsigned char* const prefix, const unsigned char prefix_size,
                    const unsigned long user_data)
{
  unsigned int hash_key;
  unsigned int masked_prefix32 = 0, prefix32 = 0;
  unsigned __int128 masked_prefix128 = 0, prefix128 = 0;
  int entry_found;
  unsigned long old_user_data;
  unsigned int idx;

  if (route_table.num_routes == 0)
    return -2;

  /* Copy network prefix into an integer.
  ** Note that we are not converting from network to host format
  ** because this is not needed for hash key computation and
  ** prefix comparison.
  */
  if (route_table.ip_mode == LPMHT_IPV4)
    memcpy(&prefix32, prefix, sizeof(prefix32));
  else
    memcpy(&prefix128, prefix, sizeof(prefix128));

  entry_found =
      routeFind(prefix32, prefix128, prefix_size, &old_user_data, 0, 0,
                &hash_key, &masked_prefix32, &masked_prefix128, &idx);

  if (!entry_found)
    return -2;

  if (route_table.ip_mode == LPMHT_IPV4)
  {
    hashIpv4Route_t *route;

    route = &route_table.ipv4_route[idx];
    route->user_data = user_data;
  }
  else
  {
    hashIpv6Route_t *route;

    route = &route_table.ipv6_route[idx];
    route->user_data = user_data;
  }

  return 0;
}

/******************************************************************************
** Get the content of the specified route.
******************************************************************************/
int sharedHash::get(const unsigned char* const prefix, const unsigned char prefix_size,
                    unsigned long *user_data, const unsigned int clear_hit_count,
                    unsigned long long *hit_count)
{
  unsigned int hash_key;
  unsigned int masked_prefix32 = 0, prefix32 = 0;
  unsigned __int128 masked_prefix128 = 0, prefix128 = 0;
  int entry_found;
  unsigned int idx;

  if (route_table.num_routes == 0)
    return -2;
  /* Copy network prefix into an integer.
  ** Note that we are not converting from network to host format
  ** because this is not needed for hash key computation and
  ** prefix comparison.
  */
  if (route_table.ip_mode == LPMHT_IPV4)
    memcpy(&prefix32, prefix, sizeof(prefix32));
  else
    memcpy(&prefix128, prefix, sizeof(prefix128));

  entry_found = routeFind(prefix32, prefix128, prefix_size, user_data,
                          clear_hit_count, hit_count, &hash_key,
                          &masked_prefix32, &masked_prefix128, &idx);

  if (!entry_found)
    return -2;

  return 0;
}

/******************************************************************************
** Perform a longest IPv4 prefix match on the specified prefix.
******************************************************************************/
int sharedHash::hashIpv4LPMatch(const unsigned char* const prefix,
                                unsigned char *prefix_size,
                                unsigned long *user_data)
{
  unsigned int prefix32;
  const unsigned int num_active_prefixes = route_table.num_active_prefixes;
  const unsigned int hash_table_size = route_table.hash_table_size;
  const unsigned int ipv4_rules_ready = route_table.ipv4_rules_ready;

  memcpy(&prefix32, prefix, sizeof(prefix32));

  for (unsigned int i = 0; i < num_active_prefixes; i++)
  {
    const unsigned char p_size = route_table.active_prefix_list[i];
    unsigned int idx;

    if (ipv4_rules_ready && (p_size <= 24))
    {
      idx = route_table.ipv4_rule_table[htonl(prefix32) >> 8];
      if (0 == idx)
        break;
    }
    else
    {
      const unsigned int masked_prefix32 =
          ipv4MaskedPrefixCompute(prefix32, p_size);
      const unsigned int hash_key =
          ipv4HashCompute(masked_prefix32, p_size, hash_table_size);

      idx = route_table.hash_table[hash_key];
      if (0 == idx)
        continue;

      while ((route_table.ipv4_route[idx].prefix_size != p_size) ||
             (route_table.ipv4_route[idx].ipv4_addr != masked_prefix32))
      {
        idx = route_table.ipv4_route[idx].next;
        if (0 == idx)
          break;
      }

      if (0 == idx)
        continue;
    }

    if (route_table.enable_hit_count)
    {
      hashIpv4Route_t *r = &route_table.ipv4_route[idx];
      (void)std::atomic_fetch_add_explicit(&r->hit_count, 1, 
		      std::memory_order_relaxed);
    }

    *user_data = route_table.ipv4_route[idx].user_data;
    *prefix_size = route_table.ipv4_route[idx].prefix_size;

    return 0; /* IPv4 Route Found! */
  }

  return -2;
}

/******************************************************************************
** Check if the incoming packet matches an existing IPv6 flow.
**
** prefix128 - Destination IP address.
** flow_idx - (output) Index where the route was found in the flow table.
** idx - (output) Index in the route table. Only valid on match.
**
** Return Values:
** -1 - No Match. Nothing else to do.
** 0 - No Match. Learn Needed.
** 1 - Match
******************************************************************************/
int sharedHash::hashIpv6FlowMatch(const unsigned __int128 prefix128,
                                  unsigned int *flow_idx, unsigned int *idx)
{
  unsigned int key;
  int rc = -1;

  /* We are using the same function for computing the flow index as for
  ** route table index. The prefix length is irrelevant, so we use a
  ** 0x55, which has a good mix of ones and zeros.
  */
  key = ipv6HashCompute(prefix128, 0x55, route_table.ipv6_max_flows);

  if (true == ipv6FlowLock(&route_table.ipv6_flow_table[key].entry_lock))
    return rc;

  *flow_idx = key;
  do
  {
    if (0 == route_table.ipv6_flow_table[key].route_index)
    {
      /* When route_index is set to 0, the entry is unused.
      ** Therefore we can trigger learning.
      */
      rc = 0;
      break;
    }

    /* If the IP addresses don't match then we have a hash collision.
    ** In this case the flow entry cannot be used and there is no learning.
    */
    if (prefix128 != route_table.ipv6_flow_table[key].ipv6_addr)
      break;

    /* If the IP address matches, but the correlator is different then we
    ** trigger learning.
    */
    if (route_table.route_flow_correlator !=
        route_table.ipv6_flow_table[key].route_flow_correlator)
    {
      rc = 0;
      break;
    }

    /* Found a matching flow! Retrieve the route table index.
     */
    *idx = route_table.ipv6_flow_table[key].route_index;

    /* Load the address of the route entry into the data cache.
     */
    __builtin_prefetch(&route_table.ipv6_route[*idx]);

    route_table.ipv6_flow_table[key].flow_detected = 1;
    rc = 1;
  } while (0);

  ipv6FlowUnlock(&route_table.ipv6_flow_table[key].entry_lock);

  return rc;
}

/******************************************************************************
** Add/Modify flow table.
**
** prefix128 - Destination IP address.
** flow_idx -  Index in the flow table.
** idx - Index in the route table for this flow.
**
******************************************************************************/
void sharedHash::hashIpv6FlowLearn(const unsigned __int128 prefix128,
                                   const unsigned int flow_idx, const unsigned int idx)
{
  if (true == ipv6FlowLock(&route_table.ipv6_flow_table[flow_idx].entry_lock))
    return;

  route_table.ipv6_flow_table[flow_idx].route_index = idx;
  route_table.ipv6_flow_table[flow_idx].route_flow_correlator =
      route_table.route_flow_correlator;
  route_table.ipv6_flow_table[flow_idx].ipv6_addr = prefix128;
  route_table.ipv6_flow_table[flow_idx].flow_detected = 1;

  ipv6FlowUnlock(&route_table.ipv6_flow_table[flow_idx].entry_lock);
}

/******************************************************************************
** Perform a longest IPv6 prefix match on the specified prefix.
** This function works with IPv6 flows.
******************************************************************************/
int sharedHash::hashIpv6FlowLPMatch(const unsigned char* const prefix,
                                    unsigned char *prefix_size,
                                    unsigned long *user_data)
{
  unsigned __int128 prefix128;
  const unsigned int num_active_prefixes = route_table.num_active_prefixes;
  const unsigned int hash_table_size = route_table.hash_table_size;
  int flow_status = -1;
  unsigned int idx = 0;
  unsigned int flow_idx;

  memcpy(&prefix128, prefix, sizeof(prefix128));

  flow_status = hashIpv6FlowMatch(prefix128, &flow_idx, &idx);

  if (flow_status < 1)
  {
    (void)std::atomic_fetch_add_explicit(route_table.ipv6_flow_not_found, 1,
                             std::memory_order_relaxed);
    for (unsigned int i = 0; i < num_active_prefixes; i++)
    {
      const unsigned char p_size = route_table.active_prefix_list[i];
      const unsigned __int128 masked_prefix128 =
          ipv6MaskedPrefixCompute(prefix128, p_size);
      const unsigned int hash_key =
          ipv6HashCompute(masked_prefix128, p_size, hash_table_size);

      idx = route_table.hash_table[hash_key];

      if (0 == idx)
        continue;

      while ((route_table.ipv6_route[idx].prefix_size != p_size) ||
             (route_table.ipv6_route[idx].ipv6_addr != masked_prefix128))
      {
        idx = route_table.ipv6_route[idx].next;
        if (0 == idx)
          break;
      }
      if (idx)
        break;
    }
  }

  if (idx)
  {
    /* If learning is needed then do it now.
     */
    if (0 == flow_status)
      hashIpv6FlowLearn(prefix128, flow_idx, idx);

    if (route_table.enable_hit_count)
    {
      hashIpv6Route_t *r = &route_table.ipv6_route[idx];
      (void)std::atomic_fetch_add_explicit(&r->hit_count, 1, 
		      std::memory_order_relaxed);
    }

    *user_data = route_table.ipv6_route[idx].user_data;
    *prefix_size = route_table.ipv6_route[idx].prefix_size;

    return 0; /* IPv6 Route Found! */
  }

  return -2;
}

/******************************************************************************
** Perform a longest IPv6 prefix match on the specified prefix.
******************************************************************************/
int sharedHash::hashIpv6LPMatch(const unsigned char* const prefix,
                                unsigned char *prefix_size,
                                unsigned long *user_data)
{
  unsigned __int128 prefix128;
  const unsigned int num_active_prefixes = route_table.num_active_prefixes;
  const unsigned int hash_table_size = route_table.hash_table_size;

  memcpy(&prefix128, prefix, sizeof(prefix128));
  for (unsigned int i = 0; i < num_active_prefixes; i++)
  {
    const unsigned char p_size = route_table.active_prefix_list[i];
    const unsigned __int128 masked_prefix128 =
        ipv6MaskedPrefixCompute(prefix128, p_size);
    const unsigned int hash_key =
        ipv6HashCompute(masked_prefix128, p_size, hash_table_size);
    unsigned int idx = route_table.hash_table[hash_key];

    if (0 == idx)
      continue;

    while ((route_table.ipv6_route[idx].prefix_size != p_size) ||
           (route_table.ipv6_route[idx].ipv6_addr != masked_prefix128))
    {
      idx = route_table.ipv6_route[idx].next;
      if (0 == idx)
        break;
    }
    if (0 == idx)
      continue;

    if (route_table.enable_hit_count)
    {
      hashIpv6Route_t *r = &route_table.ipv6_route[idx];
      (void)std::atomic_fetch_add_explicit(&r->hit_count, 1, 
		      std::memory_order_relaxed);
    }

    *user_data = route_table.ipv6_route[idx].user_data;
    *prefix_size = p_size;

    return 0; /* IPv6 Route Found! */
  }

  return -2;
}
/******************************************************************************
** Perform a longest prefix match on the specified prefix.
******************************************************************************/
int sharedHash::LPMatch(const unsigned char* const prefix, unsigned char *prefix_size,
                        unsigned long *user_data)
{
  if (route_table.ip_mode == LPMHT_IPV4)
  {
    return hashIpv4LPMatch(prefix, prefix_size, user_data);
  }
  else
  {
    if (route_table.ipv6_flow_enabled)
      return hashIpv6FlowLPMatch(prefix, prefix_size, user_data);
    else
      return hashIpv6LPMatch(prefix, prefix_size, user_data);
  }
}
