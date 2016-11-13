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
#include "mjpegenc_huffman.h"

static av_cold void init_uni_ac_vlc(const uint8_t huff_size_ac[256], uint8_t *uni_ac_vlc_len)
{
    int i;

    for (i = 0; i < 128; i++) {
        int level = i - 64;
        int run;
        if (!level)
            continue;
        for (run = 0; run < 64; run++) {
            int len, code, nbits;
            int alevel = FFABS(level);

            len = (run >> 4) * huff_size_ac[0xf0];

            nbits= av_log2_16bit(alevel) + 1;
            code = ((15&run) << 4) | nbits;

            len += huff_size_ac[code] + nbits;

            uni_ac_vlc_len[UNI_AC_ENC_INDEX(run, i)] = len;
            // We ignore EOB as its just a constant which does not change generally
        }
    }
}

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

    /* buffers start out empty */
    m->buffer = NULL;
    m->buffer_last = NULL;
    m->error = 0;

    s->intra_ac_vlc_length = s->intra_ac_vlc_last_length = NULL;
    s->intra_chroma_ac_vlc_length = s->intra_chroma_ac_vlc_last_length = NULL;

    s->mjpeg_ctx = m;
    return 0;
}

av_cold void ff_mjpeg_encode_close(MpegEncContext *s)
{
    while (s->mjpeg_ctx->buffer) {
        struct MJpegValue *buffer = s->mjpeg_ctx->buffer;
        s->mjpeg_ctx->buffer = buffer->next;

        if (buffer->ac_coefficients)
            av_freep(&buffer->ac_coefficients);
        av_freep(&buffer);
    }
    s->mjpeg_ctx->buffer_last = NULL;

    av_freep(&s->mjpeg_ctx);
}

// TODO(jjang): Test
void ff_mjpeg_encode_picture_frame(MpegEncContext *s) {
    int i, val, run, mant, nbits, code;
    MJpegContext *m;
    uint8_t *huff_size_ac;
    uint16_t *huff_code_ac;
    MJpegValue* current;
    MJpegValue* next;

    m = s->mjpeg_ctx;

    if (m->error)
        return;

    av_assert0(!s->intra_ac_vlc_length);

    /* build all the huffman tables */
    // TODO(yingted): Refactor
    MJpegEncHuffmanContext dc_luminance_ctx;
    MJpegEncHuffmanContext dc_chrominance_ctx;
    MJpegEncHuffmanContext ac_luminance_ctx;
    MJpegEncHuffmanContext ac_chrominance_ctx;
    ff_mjpeg_encode_huffman_init(&dc_luminance_ctx);
    ff_mjpeg_encode_huffman_init(&dc_chrominance_ctx);
    ff_mjpeg_encode_huffman_init(&ac_luminance_ctx);
    ff_mjpeg_encode_huffman_init(&ac_chrominance_ctx);
    for (current = m->buffer; current; current = current->next) {
        MJpegEncHuffmanContext *dc_ctx = current->n < 4 ? &dc_luminance_ctx : &dc_chrominance_ctx;
        MJpegEncHuffmanContext *ac_ctx = current->n < 4 ? &ac_luminance_ctx : &ac_chrominance_ctx;

        ff_mjpeg_encode_huffman_increment(dc_ctx, current->dc_coefficient);

        for(i = 0; i < current->ac_coefficients_size; i++) {
            ff_mjpeg_encode_huffman_increment(ac_ctx, current->ac_coefficients[i]);
        }
    }
    uint8_t huff_bits[17], huff_val[256];
    ff_mjpeg_encode_huffman_close(&dc_luminance_ctx, huff_bits, huff_val, 12);
    ff_mjpeg_build_huffman_codes(m->huff_size_dc_luminance,
                                 m->huff_code_dc_luminance,
                                 huff_bits,
                                 huff_val);
    ff_mjpeg_encode_huffman_close(&dc_chrominance_ctx, huff_bits, huff_val, 12);
    ff_mjpeg_build_huffman_codes(m->huff_size_dc_chrominance,
                                 m->huff_code_dc_chrominance,
                                 huff_bits,
                                 huff_val);
    ff_mjpeg_encode_huffman_close(&ac_luminance_ctx, huff_bits, huff_val, 256);
    ff_mjpeg_build_huffman_codes(m->huff_size_ac_luminance,
                                 m->huff_code_ac_luminance,
                                 huff_bits,
                                 huff_val);
    ff_mjpeg_encode_huffman_close(&ac_chrominance_ctx, huff_bits, huff_val, 256);
    ff_mjpeg_build_huffman_codes(m->huff_size_ac_chrominance,
                                 m->huff_code_ac_chrominance,
                                 huff_bits,
                                 huff_val);

    // TODO(yingted): use this
    init_uni_ac_vlc(m->huff_size_ac_luminance,   m->uni_ac_vlc_len);
    init_uni_ac_vlc(m->huff_size_ac_chrominance, m->uni_chroma_ac_vlc_len);
    s->intra_ac_vlc_length      =
    s->intra_ac_vlc_last_length = m->uni_ac_vlc_len;
    s->intra_chroma_ac_vlc_length      =
    s->intra_chroma_ac_vlc_last_length = m->uni_chroma_ac_vlc_len;

    for (current = m->buffer; current;) {
        int size_increase =  s->avctx->internal->byte_buffer_size/4
                           + s->mb_width*MAX_MB_BYTES;

        ff_mpv_reallocate_putbitbuffer(s, MAX_MB_BYTES, size_increase);

        if (current->n < 4) {
            ff_mjpeg_encode_dc(&s->pb, current->dc_coefficient,
                m->huff_size_dc_luminance, m->huff_code_dc_luminance);
            huff_size_ac = m->huff_size_ac_luminance;
            huff_code_ac = m->huff_code_ac_luminance;
        } else {
            ff_mjpeg_encode_dc(&s->pb, current->dc_coefficient,
                m->huff_size_dc_chrominance, m->huff_code_dc_chrominance);
            huff_size_ac = m->huff_size_ac_chrominance;
            huff_code_ac = m->huff_code_ac_chrominance;
        }

        run = 0;
        for(i = 0; i < current->ac_coefficients_size; i++) {
            val = current->ac_coefficients[i];

            if (val == 0) {
                run++;
            } else {
                while (run >= 16) {
                    put_bits(&s->pb, huff_size_ac[0xf0], huff_code_ac[0xf0]);
                    run -= 16;
                }
                mant = val;
                if (val < 0) {
                    val = -val;
                    mant--;
                }

                nbits= av_log2_16bit(val) + 1;
                code = (run << 4) | nbits;

                put_bits(&s->pb, huff_size_ac[code], huff_code_ac[code]);

                put_sbits(&s->pb, nbits, mant);
                run = 0;
            }
        }

        /* output EOB only if not already 64 values */
        if (current->ac_coefficients_size < 63 || run != 0)
            put_bits(&s->pb, huff_size_ac[0], huff_code_ac[0]);

        next = current->next;
        av_freep(&current->ac_coefficients);
        av_freep(&current);
        current = next;
    }

    s->intra_ac_vlc_length = s->intra_ac_vlc_last_length = NULL;
    s->intra_chroma_ac_vlc_length = s->intra_chroma_ac_vlc_last_length = NULL;

    m->buffer = NULL;
    m->buffer_last = NULL;
}

static int encode_block(MpegEncContext *s, int16_t *block, int n)
{
    int i, j;
    int component, dc, last_index, val;
    MJpegContext *m = s->mjpeg_ctx;
    MJpegValue* buffer_block;

    // TODO(jjiang): Out of memory error return?
    buffer_block = av_malloc(sizeof(MJpegValue));
    if (!buffer_block) {
        return AVERROR(ENOMEM);
    }

    buffer_block->next = NULL;

    /* Add to end of buffer */
    if (!m->buffer) {
        m->buffer = buffer_block;
        m->buffer_last = buffer_block;
    } else {
        m->buffer_last->next = buffer_block;
        m->buffer_last = buffer_block;
    }

    /* DC coef */
    component = (n <= 3 ? 0 : (n&1) + 1);
    dc = block[0]; /* overflow is impossible */
    val = dc - s->last_dc[component];

    buffer_block->dc_coefficient = val;
    buffer_block->n = n;

    s->last_dc[component] = dc;

    /* AC coefs */

    last_index = s->block_last_index[n];

    buffer_block->ac_coefficients = av_malloc(sizeof(int) * last_index);
    if (!buffer_block->ac_coefficients) {
        return AVERROR(ENOMEM);
    }
    buffer_block->ac_coefficients_size = last_index;

    for(i=1;i<=last_index;i++) {
        j = s->intra_scantable.permutated[i];
        val = block[j];
        buffer_block->ac_coefficients[i - 1] = val;
    }
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
