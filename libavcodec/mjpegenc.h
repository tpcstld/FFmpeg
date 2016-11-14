/*
 * MJPEG encoder
 * Copyright (c) 2000, 2001 Fabrice Bellard
 * Copyright (c) 2003 Alex Beregszaszi
 * Copyright (c) 2003-2004 Michael Niedermayer
 *
 * Support for external huffman table, various fixes (AVID workaround),
 * aspecting, new decode_frame mechanism and apple mjpeg-b support
 *                                  by Alex Beregszaszi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * MJPEG encoder.
 */

#ifndef AVCODEC_MJPEGENC_H
#define AVCODEC_MJPEGENC_H

#include <stdint.h>

#include "mjpeg.h"
#include "mpegvideo.h"
#include "put_bits.h"

typedef struct MJpegValue {
    // Store one macroblock's worth of storage
    uint16_t mants[64 * 10];
    uint8_t codes[64 * 10];
    uint8_t is_dc_bits[64 * 10 / 8];
    int ncode;
    // TODO(jjiang): Make this into a boolean indicating luminance or chrominance.
    int n;
    struct MJpegValue *next;
} MJpegValue;

typedef struct MJpegContext {
    uint8_t huff_size_dc_luminance[12]; //FIXME use array [3] instead of lumi / chroma, for easier addressing
    uint16_t huff_code_dc_luminance[12];
    uint8_t huff_size_dc_chrominance[12];
    uint16_t huff_code_dc_chrominance[12];

    uint8_t huff_size_ac_luminance[256];
    uint16_t huff_code_ac_luminance[256];
    uint8_t huff_size_ac_chrominance[256];
    uint16_t huff_code_ac_chrominance[256];

    // Storage for VLC (in MpegEncContext)
    uint8_t uni_ac_vlc_len[64 * 64 * 2];
    uint8_t uni_chroma_ac_vlc_len[64 * 64 * 2];

    // All Huffman tables
    // Default DC tables have exactly 12 values
    uint8_t bits_dc_luminance[17];
    uint8_t val_dc_luminance[12];
    uint8_t bits_dc_chrominance[17];
    uint8_t val_dc_chrominance[12];

    // 8-bit JPEG has max 256 values
    uint8_t bits_ac_luminance[17];
    uint8_t val_ac_luminance[256];
    uint8_t bits_ac_chrominance[17];
    uint8_t val_ac_chrominance[256];

    MJpegValue *buffer;
    MJpegValue *buffer_last;
    int error;
} MJpegContext;

static inline void put_marker(PutBitContext *p, enum JpegMarker code)
{
    put_bits(p, 8, 0xff);
    put_bits(p, 8, code);
}

int  ff_mjpeg_encode_init(MpegEncContext *s);
void ff_mjpeg_encode_picture_frame(MpegEncContext *s);
void ff_mjpeg_encode_close(MpegEncContext *s);
int ff_mjpeg_encode_mb(MpegEncContext *s, int16_t block[12][64]);

#endif /* AVCODEC_MJPEGENC_H */
