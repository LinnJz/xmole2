// Generated from Unicode 17.0.0 UnicodeData.txt; do not edit manually.
// Source: https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt
// SHA-256: 2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C
//
// UNICODE LICENSE V3
//
// COPYRIGHT AND PERMISSION NOTICE
//
// Copyright © 1991-2026 Unicode, Inc.
//
// NOTICE TO USER: Carefully read the following legal agreement. BY
// DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING DATA FILES, AND/OR
// SOFTWARE, YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
// TERMS AND CONDITIONS OF THIS AGREEMENT. IF YOU DO NOT AGREE, DO NOT
// DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE THE DATA FILES OR SOFTWARE.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of data files and any associated documentation (the "Data Files") or
// software and any associated documentation (the "Software") to deal in the
// Data Files or Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, and/or sell
// copies of the Data Files or Software, and to permit persons to whom the Data
// Files or Software are furnished to do so, provided that either (a) this
// copyright and permission notice appear with all copies of the Data Files or
// Software, or (b) this copyright and permission notice appear in associated
// Documentation.
//
// THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
// KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
// THIRD PARTY RIGHTS.
//
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
// BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
// OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
// ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE DATA
// FILES OR SOFTWARE.
//
// Except as contained in this notice, the name of a copyright holder shall
// not be used in advertising or otherwise to promote the sale, use or other
// dealings in these Data Files or Software without prior written
// authorization of the copyright holder.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xmole2::cfb::internal
{

struct SimpleUppercaseSegment
{
  std::uint16_t first;
  std::uint16_t last;
  std::uint8_t step;
  std::int32_t delta;
};

// clang-format off
inline constexpr auto kSimpleUppercaseMappingCount = std::size_t { 1198 };
inline constexpr auto kSimpleUppercaseSegments =
    std::array<SimpleUppercaseSegment, 192> {
  SimpleUppercaseSegment { 97, 122, 1, -32 },
  SimpleUppercaseSegment { 181, 181, 1, 743 },
  SimpleUppercaseSegment { 224, 246, 1, -32 },
  SimpleUppercaseSegment { 248, 254, 1, -32 },
  SimpleUppercaseSegment { 255, 255, 1, 121 },
  SimpleUppercaseSegment { 257, 303, 2, -1 },
  SimpleUppercaseSegment { 305, 305, 1, -232 },
  SimpleUppercaseSegment { 307, 311, 2, -1 },
  SimpleUppercaseSegment { 314, 328, 2, -1 },
  SimpleUppercaseSegment { 331, 375, 2, -1 },
  SimpleUppercaseSegment { 378, 382, 2, -1 },
  SimpleUppercaseSegment { 383, 383, 1, -300 },
  SimpleUppercaseSegment { 384, 384, 1, 195 },
  SimpleUppercaseSegment { 387, 389, 2, -1 },
  SimpleUppercaseSegment { 392, 392, 1, -1 },
  SimpleUppercaseSegment { 396, 396, 1, -1 },
  SimpleUppercaseSegment { 402, 402, 1, -1 },
  SimpleUppercaseSegment { 405, 405, 1, 97 },
  SimpleUppercaseSegment { 409, 409, 1, -1 },
  SimpleUppercaseSegment { 410, 410, 1, 163 },
  SimpleUppercaseSegment { 411, 411, 1, 42561 },
  SimpleUppercaseSegment { 414, 414, 1, 130 },
  SimpleUppercaseSegment { 417, 421, 2, -1 },
  SimpleUppercaseSegment { 424, 424, 1, -1 },
  SimpleUppercaseSegment { 429, 429, 1, -1 },
  SimpleUppercaseSegment { 432, 432, 1, -1 },
  SimpleUppercaseSegment { 436, 438, 2, -1 },
  SimpleUppercaseSegment { 441, 441, 1, -1 },
  SimpleUppercaseSegment { 445, 445, 1, -1 },
  SimpleUppercaseSegment { 447, 447, 1, 56 },
  SimpleUppercaseSegment { 453, 453, 1, -1 },
  SimpleUppercaseSegment { 454, 454, 1, -2 },
  SimpleUppercaseSegment { 456, 456, 1, -1 },
  SimpleUppercaseSegment { 457, 457, 1, -2 },
  SimpleUppercaseSegment { 459, 459, 1, -1 },
  SimpleUppercaseSegment { 460, 460, 1, -2 },
  SimpleUppercaseSegment { 462, 476, 2, -1 },
  SimpleUppercaseSegment { 477, 477, 1, -79 },
  SimpleUppercaseSegment { 479, 495, 2, -1 },
  SimpleUppercaseSegment { 498, 498, 1, -1 },
  SimpleUppercaseSegment { 499, 499, 1, -2 },
  SimpleUppercaseSegment { 501, 501, 1, -1 },
  SimpleUppercaseSegment { 505, 543, 2, -1 },
  SimpleUppercaseSegment { 547, 563, 2, -1 },
  SimpleUppercaseSegment { 572, 572, 1, -1 },
  SimpleUppercaseSegment { 575, 576, 1, 10815 },
  SimpleUppercaseSegment { 578, 578, 1, -1 },
  SimpleUppercaseSegment { 583, 591, 2, -1 },
  SimpleUppercaseSegment { 592, 592, 1, 10783 },
  SimpleUppercaseSegment { 593, 593, 1, 10780 },
  SimpleUppercaseSegment { 594, 594, 1, 10782 },
  SimpleUppercaseSegment { 595, 595, 1, -210 },
  SimpleUppercaseSegment { 596, 596, 1, -206 },
  SimpleUppercaseSegment { 598, 599, 1, -205 },
  SimpleUppercaseSegment { 601, 601, 1, -202 },
  SimpleUppercaseSegment { 603, 603, 1, -203 },
  SimpleUppercaseSegment { 604, 604, 1, 42319 },
  SimpleUppercaseSegment { 608, 608, 1, -205 },
  SimpleUppercaseSegment { 609, 609, 1, 42315 },
  SimpleUppercaseSegment { 611, 611, 1, -207 },
  SimpleUppercaseSegment { 612, 612, 1, 42343 },
  SimpleUppercaseSegment { 613, 613, 1, 42280 },
  SimpleUppercaseSegment { 614, 614, 1, 42308 },
  SimpleUppercaseSegment { 616, 616, 1, -209 },
  SimpleUppercaseSegment { 617, 617, 1, -211 },
  SimpleUppercaseSegment { 618, 618, 1, 42308 },
  SimpleUppercaseSegment { 619, 619, 1, 10743 },
  SimpleUppercaseSegment { 620, 620, 1, 42305 },
  SimpleUppercaseSegment { 623, 623, 1, -211 },
  SimpleUppercaseSegment { 625, 625, 1, 10749 },
  SimpleUppercaseSegment { 626, 626, 1, -213 },
  SimpleUppercaseSegment { 629, 629, 1, -214 },
  SimpleUppercaseSegment { 637, 637, 1, 10727 },
  SimpleUppercaseSegment { 640, 640, 1, -218 },
  SimpleUppercaseSegment { 642, 642, 1, 42307 },
  SimpleUppercaseSegment { 643, 643, 1, -218 },
  SimpleUppercaseSegment { 647, 647, 1, 42282 },
  SimpleUppercaseSegment { 648, 648, 1, -218 },
  SimpleUppercaseSegment { 649, 649, 1, -69 },
  SimpleUppercaseSegment { 650, 651, 1, -217 },
  SimpleUppercaseSegment { 652, 652, 1, -71 },
  SimpleUppercaseSegment { 658, 658, 1, -219 },
  SimpleUppercaseSegment { 669, 669, 1, 42261 },
  SimpleUppercaseSegment { 670, 670, 1, 42258 },
  SimpleUppercaseSegment { 837, 837, 1, 84 },
  SimpleUppercaseSegment { 881, 883, 2, -1 },
  SimpleUppercaseSegment { 887, 887, 1, -1 },
  SimpleUppercaseSegment { 891, 893, 1, 130 },
  SimpleUppercaseSegment { 940, 940, 1, -38 },
  SimpleUppercaseSegment { 941, 943, 1, -37 },
  SimpleUppercaseSegment { 945, 961, 1, -32 },
  SimpleUppercaseSegment { 962, 962, 1, -31 },
  SimpleUppercaseSegment { 963, 971, 1, -32 },
  SimpleUppercaseSegment { 972, 972, 1, -64 },
  SimpleUppercaseSegment { 973, 974, 1, -63 },
  SimpleUppercaseSegment { 976, 976, 1, -62 },
  SimpleUppercaseSegment { 977, 977, 1, -57 },
  SimpleUppercaseSegment { 981, 981, 1, -47 },
  SimpleUppercaseSegment { 982, 982, 1, -54 },
  SimpleUppercaseSegment { 983, 983, 1, -8 },
  SimpleUppercaseSegment { 985, 1007, 2, -1 },
  SimpleUppercaseSegment { 1008, 1008, 1, -86 },
  SimpleUppercaseSegment { 1009, 1009, 1, -80 },
  SimpleUppercaseSegment { 1010, 1010, 1, 7 },
  SimpleUppercaseSegment { 1011, 1011, 1, -116 },
  SimpleUppercaseSegment { 1013, 1013, 1, -96 },
  SimpleUppercaseSegment { 1016, 1016, 1, -1 },
  SimpleUppercaseSegment { 1019, 1019, 1, -1 },
  SimpleUppercaseSegment { 1072, 1103, 1, -32 },
  SimpleUppercaseSegment { 1104, 1119, 1, -80 },
  SimpleUppercaseSegment { 1121, 1153, 2, -1 },
  SimpleUppercaseSegment { 1163, 1215, 2, -1 },
  SimpleUppercaseSegment { 1218, 1230, 2, -1 },
  SimpleUppercaseSegment { 1231, 1231, 1, -15 },
  SimpleUppercaseSegment { 1233, 1327, 2, -1 },
  SimpleUppercaseSegment { 1377, 1414, 1, -48 },
  SimpleUppercaseSegment { 4304, 4346, 1, 3008 },
  SimpleUppercaseSegment { 4349, 4351, 1, 3008 },
  SimpleUppercaseSegment { 5112, 5117, 1, -8 },
  SimpleUppercaseSegment { 7296, 7296, 1, -6254 },
  SimpleUppercaseSegment { 7297, 7297, 1, -6253 },
  SimpleUppercaseSegment { 7298, 7298, 1, -6244 },
  SimpleUppercaseSegment { 7299, 7300, 1, -6242 },
  SimpleUppercaseSegment { 7301, 7301, 1, -6243 },
  SimpleUppercaseSegment { 7302, 7302, 1, -6236 },
  SimpleUppercaseSegment { 7303, 7303, 1, -6181 },
  SimpleUppercaseSegment { 7304, 7304, 1, 35266 },
  SimpleUppercaseSegment { 7306, 7306, 1, -1 },
  SimpleUppercaseSegment { 7545, 7545, 1, 35332 },
  SimpleUppercaseSegment { 7549, 7549, 1, 3814 },
  SimpleUppercaseSegment { 7566, 7566, 1, 35384 },
  SimpleUppercaseSegment { 7681, 7829, 2, -1 },
  SimpleUppercaseSegment { 7835, 7835, 1, -59 },
  SimpleUppercaseSegment { 7841, 7935, 2, -1 },
  SimpleUppercaseSegment { 7936, 7943, 1, 8 },
  SimpleUppercaseSegment { 7952, 7957, 1, 8 },
  SimpleUppercaseSegment { 7968, 7975, 1, 8 },
  SimpleUppercaseSegment { 7984, 7991, 1, 8 },
  SimpleUppercaseSegment { 8000, 8005, 1, 8 },
  SimpleUppercaseSegment { 8017, 8023, 2, 8 },
  SimpleUppercaseSegment { 8032, 8039, 1, 8 },
  SimpleUppercaseSegment { 8048, 8049, 1, 74 },
  SimpleUppercaseSegment { 8050, 8053, 1, 86 },
  SimpleUppercaseSegment { 8054, 8055, 1, 100 },
  SimpleUppercaseSegment { 8056, 8057, 1, 128 },
  SimpleUppercaseSegment { 8058, 8059, 1, 112 },
  SimpleUppercaseSegment { 8060, 8061, 1, 126 },
  SimpleUppercaseSegment { 8064, 8071, 1, 8 },
  SimpleUppercaseSegment { 8080, 8087, 1, 8 },
  SimpleUppercaseSegment { 8096, 8103, 1, 8 },
  SimpleUppercaseSegment { 8112, 8113, 1, 8 },
  SimpleUppercaseSegment { 8115, 8115, 1, 9 },
  SimpleUppercaseSegment { 8126, 8126, 1, -7205 },
  SimpleUppercaseSegment { 8131, 8131, 1, 9 },
  SimpleUppercaseSegment { 8144, 8145, 1, 8 },
  SimpleUppercaseSegment { 8160, 8161, 1, 8 },
  SimpleUppercaseSegment { 8165, 8165, 1, 7 },
  SimpleUppercaseSegment { 8179, 8179, 1, 9 },
  SimpleUppercaseSegment { 8526, 8526, 1, -28 },
  SimpleUppercaseSegment { 8560, 8575, 1, -16 },
  SimpleUppercaseSegment { 8580, 8580, 1, -1 },
  SimpleUppercaseSegment { 9424, 9449, 1, -26 },
  SimpleUppercaseSegment { 11312, 11359, 1, -48 },
  SimpleUppercaseSegment { 11361, 11361, 1, -1 },
  SimpleUppercaseSegment { 11365, 11365, 1, -10795 },
  SimpleUppercaseSegment { 11366, 11366, 1, -10792 },
  SimpleUppercaseSegment { 11368, 11372, 2, -1 },
  SimpleUppercaseSegment { 11379, 11379, 1, -1 },
  SimpleUppercaseSegment { 11382, 11382, 1, -1 },
  SimpleUppercaseSegment { 11393, 11491, 2, -1 },
  SimpleUppercaseSegment { 11500, 11502, 2, -1 },
  SimpleUppercaseSegment { 11507, 11507, 1, -1 },
  SimpleUppercaseSegment { 11520, 11557, 1, -7264 },
  SimpleUppercaseSegment { 11559, 11559, 1, -7264 },
  SimpleUppercaseSegment { 11565, 11565, 1, -7264 },
  SimpleUppercaseSegment { 42561, 42605, 2, -1 },
  SimpleUppercaseSegment { 42625, 42651, 2, -1 },
  SimpleUppercaseSegment { 42787, 42799, 2, -1 },
  SimpleUppercaseSegment { 42803, 42863, 2, -1 },
  SimpleUppercaseSegment { 42874, 42876, 2, -1 },
  SimpleUppercaseSegment { 42879, 42887, 2, -1 },
  SimpleUppercaseSegment { 42892, 42892, 1, -1 },
  SimpleUppercaseSegment { 42897, 42899, 2, -1 },
  SimpleUppercaseSegment { 42900, 42900, 1, 48 },
  SimpleUppercaseSegment { 42903, 42921, 2, -1 },
  SimpleUppercaseSegment { 42933, 42947, 2, -1 },
  SimpleUppercaseSegment { 42952, 42954, 2, -1 },
  SimpleUppercaseSegment { 42957, 42971, 2, -1 },
  SimpleUppercaseSegment { 42998, 42998, 1, -1 },
  SimpleUppercaseSegment { 43859, 43859, 1, -928 },
  SimpleUppercaseSegment { 43888, 43967, 1, -38864 },
  SimpleUppercaseSegment { 65345, 65370, 1, -32 },
    };
// clang-format on

} // namespace xmole2::cfb::internal
