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

#include "libavutil/pixdesc.h"

#include "avcodec.h"
#include "jpegtables.h"
#include "mjpegenc_common.h"
#include "mpegvideo.h"
#include "mjpeg.h"
#include "mjpegenc.h"

av_cold int ff_mjpeg_encode_init(MpegEncContext *s)
{
    MJpegContext *m;

    if (s->width > 65500 || s->height > 65500) {
        av_log(s, AV_LOG_ERROR, "JPEG does not support resolutions above 65500x65500\n");
        return AVERROR(EINVAL);
    }

    m = av_malloc(sizeof(MJpegContext));
    if (!m)
        return AVERROR(ENOMEM);

    s->min_qcoeff=-1023;
    s->max_qcoeff= 1023;

    // Build default Huffman tables.
    // These may be overwritten later with more optimal Huffman tables, but
    // they are needed at least right now for some processes like trellis.
    ff_mjpeg_build_huffman_codes(m->huff_size_dc_luminance,
                                 m->huff_code_dc_luminance,
                                 avpriv_mjpeg_bits_dc_luminance,
                                 avpriv_mjpeg_val_dc);
    ff_mjpeg_build_huffman_codes(m->huff_size_dc_chrominance,
                                 m->huff_code_dc_chrominance,
                                 avpriv_mjpeg_bits_dc_chrominance,
                                 avpriv_mjpeg_val_dc);
    ff_mjpeg_build_huffman_codes(m->huff_size_ac_luminance,
                                 m->huff_code_ac_luminance,
                                 avpriv_mjpeg_bits_ac_luminance,
                                 avpriv_mjpeg_val_ac_luminance);
    ff_mjpeg_build_huffman_codes(m->huff_size_ac_chrominance,
                                 m->huff_code_ac_chrominance,
                                 avpriv_mjpeg_bits_ac_chrominance,
                                 avpriv_mjpeg_val_ac_chrominance);

    init_uni_ac_vlc(m->huff_size_ac_luminance,   m->uni_ac_vlc_len);
    init_uni_ac_vlc(m->huff_size_ac_chrominance, m->uni_chroma_ac_vlc_len);
    s->intra_ac_vlc_length      =
    s->intra_ac_vlc_last_length = m->uni_ac_vlc_len;
    s->intra_chroma_ac_vlc_length      =
    s->intra_chroma_ac_vlc_last_length = m->uni_chroma_ac_vlc_len;

    // Buffers start out empty.
    m->buffer = NULL;
    m->buffer_last = NULL;
    m->error = 0;

    s->mjpeg_ctx = m;
    return 0;
}

av_cold void ff_mjpeg_encode_close(MpegEncContext *s)
{
    // In the event of an error, not all the buffers may be freed.
    while (s->mjpeg_ctx->buffer) {
        struct MJpegBuffer *buffer = s->mjpeg_ctx->buffer;
        s->mjpeg_ctx->buffer = buffer->next;

        av_freep(&buffer);
    }
    s->mjpeg_ctx->buffer_last = NULL;

    av_freep(&s->mjpeg_ctx);
}

/**
 * Encodes and outputs the entire frame in the JPEG format.
 *
 * @param s The MpegEncContext.
 */
void ff_mjpeg_encode_picture_frame(MpegEncContext *s)
{
    int i, nbits, code, table_id;
    MJpegContext *m = s->mjpeg_ctx;
    uint8_t *huff_size[4] = {m->huff_size_dc_luminance,
                             m->huff_size_dc_chrominance,
                             m->huff_size_ac_luminance,
                             m->huff_size_ac_chrominance};
    uint16_t *huff_code[4] = {m->huff_code_dc_luminance,
                              m->huff_code_dc_chrominance,
                              m->huff_code_ac_luminance,
                              m->huff_code_ac_chrominance};
    MJpegBuffer* current;
    MJpegBuffer* next;

    for (current = m->buffer; current;) {
        int size_increase =  s->avctx->internal->byte_buffer_size/4
                           + s->mb_width*MAX_MB_BYTES;

        ff_mpv_reallocate_putbitbuffer(s, MAX_MB_BYTES, size_increase);

        for (i = 0; i < current->ncode; ++i) {
            code = current->codes[i];
            nbits = code & 0xf;
            table_id = current->table_ids[i];
            put_bits(&s->pb, huff_size[table_id][code], huff_code[table_id][code]);
            if (nbits != 0) {
                put_sbits(&s->pb, nbits, current->mants[i]);
            }
        }

        next = current->next;
        av_freep(&current);
        current = next;
    }

    m->buffer = NULL;
    m->buffer_last = NULL;
}

/**
 * Add code and table_id to the JPEG buffer.
 *
 * @param s The MJpegContext which contains the JPEG buffer.
 * @param table_id Which Huffman table the code belongs to.
 * @param code The encoded exponent of the coefficients and the run-bits.
 */
static inline void ff_mjpeg_encode_code(MJpegContext *s, uint8_t table_id, int code)
{
    MJpegBuffer *m = s->buffer_last;
    m->table_ids[m->ncode] = table_id;
    m->codes[m->ncode++] = code;
}

/**
 * Add the coefficient's data to the JPEG buffer.
 *
 * @param s The MJpegContext which contains the JPEG buffer.
 * @param table_id Which Huffman table the code belongs to.
 * @param val The coefficient.
 * @param run The run-bits.
 */
static void ff_mjpeg_encode_coef(MJpegContext *s, uint8_t table_id, int val, int run)
{
    int mant, code;
    MJpegBuffer *m = s->buffer_last;
    av_assert0(m->ncode >= 0);
    av_assert0(m->ncode < sizeof(m->codes) / sizeof(m->codes[0]));

    if (val == 0) {
        av_assert0(run == 0);
        ff_mjpeg_encode_code(s, table_id, 0);
    } else {
        mant = val;
        if (val < 0) {
            val = -val;
            mant--;
        }

        code = (run << 4) | (av_log2_16bit(val) + 1);

        m->mants[m->ncode] = mant;
        ff_mjpeg_encode_code(s, table_id, code);
    }
}

/**
 * Add the block's data into the JPEG buffer.
 *
 * @param s The MJpegEncContext that contains the JPEG buffer.
 * @param block The block.
 * @param n The block's index or number.
 * @return int Return code, 0 if succeeded.
 */
static int encode_block(MpegEncContext *s, int16_t *block, int n)
{
    int i, j, table_id;
    int component, dc, last_index, val, run;
    MJpegContext *m = s->mjpeg_ctx;
    MJpegBuffer* buffer_block = m->buffer_last;

    // Any block has at most 64 coefficients.
    if (buffer_block == NULL || buffer_block->ncode + 64
            > sizeof(buffer_block->codes) / sizeof(buffer_block->codes[0])) {
        buffer_block = av_malloc(sizeof(MJpegBuffer));
        if (!buffer_block) {
            return AVERROR(ENOMEM);
        }

        buffer_block->next = NULL;
        buffer_block->ncode = 0;

        // Add to end of buffer.
        if (!m->buffer) {
            m->buffer = buffer_block;
            m->buffer_last = buffer_block;
        } else {
            m->buffer_last->next = buffer_block;
            m->buffer_last = buffer_block;
        }
    }

    /* DC coef */
    component = (n <= 3 ? 0 : (n&1) + 1);
    table_id = (n <= 3 ? 0 : 1);
    dc = block[0]; /* overflow is impossible */
    val = dc - s->last_dc[component];

    ff_mjpeg_encode_coef(m, table_id, val, 0);

    s->last_dc[component] = dc;

    /* AC coefs */

    run = 0;
    last_index = s->block_last_index[n];
    table_id |= 2;

    for(i=1;i<=last_index;i++) {
        j = s->intra_scantable.permutated[i];
        val = block[j];

        if (val == 0) {
            run++;
        } else {
            while (run >= 16) {
                ff_mjpeg_encode_code(m, table_id, 0xf0);
                run -= 16;
            }
            ff_mjpeg_encode_coef(m, table_id, val, run);
            run = 0;
        }
    }

    /* output EOB only if not already 64 values */
    if (last_index < 63 || run != 0)
        ff_mjpeg_encode_code(m, table_id, 0);

    return 0;
}

int ff_mjpeg_encode_mb(MpegEncContext *s, int16_t block[12][64])
{
    int i;
    int ret = 0;
    if (s->mjpeg_ctx->error)
        return s->mjpeg_ctx->error;
    if (s->chroma_format == CHROMA_444) {
        if (!ret) ret = encode_block(s, block[0], 0);
        if (!ret) ret = encode_block(s, block[2], 2);
        if (!ret) ret = encode_block(s, block[4], 4);
        if (!ret) ret = encode_block(s, block[8], 8);
        if (!ret) ret = encode_block(s, block[5], 5);
        if (!ret) ret = encode_block(s, block[9], 9);

        if (16*s->mb_x+8 < s->width) {
            if (!ret) ret = encode_block(s, block[1], 1);
            if (!ret) ret = encode_block(s, block[3], 3);
            if (!ret) ret = encode_block(s, block[6], 6);
            if (!ret) ret = encode_block(s, block[10], 10);
            if (!ret) ret = encode_block(s, block[7], 7);
            if (!ret) ret = encode_block(s, block[11], 11);
        }
    } else {
        for(i=0;i<5;i++) {
            if (!ret) ret = encode_block(s, block[i], i);
        }
        if (s->chroma_format == CHROMA_420) {
            if (!ret) ret = encode_block(s, block[5], 5);
        } else {
            if (!ret) ret = encode_block(s, block[6], 6);
            if (!ret) ret = encode_block(s, block[5], 5);
            if (!ret) ret = encode_block(s, block[7], 7);
        }
    }
    if (ret) {
        s->mjpeg_ctx->error = ret;
        return ret;
    }

    s->i_tex_bits += get_bits_diff(s);
    return 0;
}

// maximum over s->mjpeg_vsample[i]
#define V_MAX 2
static int amv_encode_picture(AVCodecContext *avctx, AVPacket *pkt,
                              const AVFrame *pic_arg, int *got_packet)

{
    MpegEncContext *s = avctx->priv_data;
    AVFrame *pic;
    int i, ret;
    int chroma_h_shift, chroma_v_shift;

    av_pix_fmt_get_chroma_sub_sample(avctx->pix_fmt, &chroma_h_shift, &chroma_v_shift);

#if FF_API_EMU_EDGE
    //CODEC_FLAG_EMU_EDGE have to be cleared
    if(s->avctx->flags & CODEC_FLAG_EMU_EDGE)
        return AVERROR(EINVAL);
#endif

    if ((avctx->height & 15) && avctx->strict_std_compliance > FF_COMPLIANCE_UNOFFICIAL) {
        av_log(avctx, AV_LOG_ERROR,
               "Heights which are not a multiple of 16 might fail with some decoders, "
               "use vstrict=-1 / -strict -1 to use %d anyway.\n", avctx->height);
        av_log(avctx, AV_LOG_WARNING, "If you have a device that plays AMV videos, please test if videos "
               "with such heights work with it and report your findings to ffmpeg-devel@ffmpeg.org\n");
        return AVERROR_EXPERIMENTAL;
    }

    pic = av_frame_clone(pic_arg);
    if (!pic)
        return AVERROR(ENOMEM);
    //picture should be flipped upside-down
    for(i=0; i < 3; i++) {
        int vsample = i ? 2 >> chroma_v_shift : 2;
        pic->data[i] += pic->linesize[i] * (vsample * s->height / V_MAX - 1);
        pic->linesize[i] *= -1;
    }
    ret = ff_mpv_encode_picture(avctx, pkt, pic, got_packet);
    av_frame_free(&pic);
    return ret;
}

#define OFFSET(x) offsetof(MpegEncContext, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
FF_MPV_COMMON_OPTS
{ "pred", "Prediction method", OFFSET(pred), AV_OPT_TYPE_INT, { .i64 = 1 }, 1, 3, VE, "pred" },
    { "left",   NULL, 0, AV_OPT_TYPE_CONST, { .i64 = 1 }, INT_MIN, INT_MAX, VE, "pred" },
    { "plane",  NULL, 0, AV_OPT_TYPE_CONST, { .i64 = 2 }, INT_MIN, INT_MAX, VE, "pred" },
    { "median", NULL, 0, AV_OPT_TYPE_CONST, { .i64 = 3 }, INT_MIN, INT_MAX, VE, "pred" },
{ "huffman", "Huffman table strategy", OFFSET(huffman), AV_OPT_TYPE_INT, { .i64 = HUFFMAN_TABLE_DEFAULT }, 0, 1, VE, "huffman" },
    { "default", NULL, 0, AV_OPT_TYPE_CONST, { .i64 = HUFFMAN_TABLE_DEFAULT }, INT_MIN, INT_MAX, VE, "huffman" },
    { "optimal", NULL, 0, AV_OPT_TYPE_CONST, { .i64 = HUFFMAN_TABLE_OPTIMAL }, INT_MIN, INT_MAX, VE, "huffman" },
{ NULL},
};

#if CONFIG_MJPEG_ENCODER

static const AVClass mjpeg_class = {
    .class_name = "mjpeg encoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_mjpeg_encoder = {
    .name           = "mjpeg",
    .long_name      = NULL_IF_CONFIG_SMALL("MJPEG (Motion JPEG)"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_MJPEG,
    .priv_data_size = sizeof(MpegEncContext),
    .init           = ff_mpv_encode_init,
    .encode2        = ff_mpv_encode_picture,
    .close          = ff_mpv_encode_end,
    .capabilities   = AV_CODEC_CAP_SLICE_THREADS | AV_CODEC_CAP_FRAME_THREADS | AV_CODEC_CAP_INTRA_ONLY,
    .pix_fmts       = (const enum AVPixelFormat[]){
        AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_YUVJ422P, AV_PIX_FMT_YUVJ444P, AV_PIX_FMT_NONE
    },
    .priv_class     = &mjpeg_class,
};
#endif
#if CONFIG_AMV_ENCODER
static const AVClass amv_class = {
    .class_name = "amv encoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_amv_encoder = {
    .name           = "amv",
    .long_name      = NULL_IF_CONFIG_SMALL("AMV Video"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_AMV,
    .priv_data_size = sizeof(MpegEncContext),
    .init           = ff_mpv_encode_init,
    .encode2        = amv_encode_picture,
    .close          = ff_mpv_encode_end,
    .pix_fmts       = (const enum AVPixelFormat[]){
        AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_NONE
    },
    .priv_class     = &amv_class,
};
#endif
