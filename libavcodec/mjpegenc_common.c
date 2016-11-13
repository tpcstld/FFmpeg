/*
 * lossless JPEG shared bits
 * Copyright (c) 2000, 2001 Fabrice Bellard
 * Copyright (c) 2003 Alex Beregszaszi
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

#include <stdint.h>
#include <string.h>

#include "libavutil/common.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixfmt.h"

#include "avcodec.h"
#include "idctdsp.h"
#include "jpegtables.h"
#include "put_bits.h"
#include "mjpegenc.h"
#include "mjpegenc_common.h"
#include "mjpegenc_huffman.h"
#include "mjpeg.h"

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

/* table_class: 0 = DC coef, 1 = AC coefs */
static int put_huffman_table(PutBitContext *p, int table_class, int table_id,
                             const uint8_t *bits_table, const uint8_t *value_table)
{
    int n, i;

    put_bits(p, 4, table_class);
    put_bits(p, 4, table_id);

    n = 0;
    for(i=1;i<=16;i++) {
        n += bits_table[i];
        put_bits(p, 8, bits_table[i]);
    }

    for(i=0;i<n;i++)
        put_bits(p, 8, value_table[i]);

    return n + 17;
}

static void jpeg_table_header(AVCodecContext *avctx, PutBitContext *p,
                              ScanTable *intra_scantable,
                              uint16_t luma_intra_matrix[64],
                              uint16_t chroma_intra_matrix[64],
                              int hsample[3])
{
    int i, j, size;
    uint8_t *ptr;
    MpegEncContext *s = avctx->priv_data;

    if (avctx->codec_id != AV_CODEC_ID_LJPEG) {
        int matrix_count = 1 + !!memcmp(luma_intra_matrix,
                                        chroma_intra_matrix,
                                        sizeof(luma_intra_matrix[0]) * 64);
    if (s->force_duplicated_matrix)
        matrix_count = 2;
    /* quant matrixes */
    put_marker(p, DQT);
    put_bits(p, 16, 2 + matrix_count * (1 + 64));
    put_bits(p, 4, 0); /* 8 bit precision */
    put_bits(p, 4, 0); /* table 0 */
    for(i=0;i<64;i++) {
        j = intra_scantable->permutated[i];
        put_bits(p, 8, luma_intra_matrix[j]);
    }

        if (matrix_count > 1) {
            put_bits(p, 4, 0); /* 8 bit precision */
            put_bits(p, 4, 1); /* table 1 */
            for(i=0;i<64;i++) {
                j = intra_scantable->permutated[i];
                put_bits(p, 8, chroma_intra_matrix[j]);
            }
        }
    }

    if(avctx->active_thread_type & FF_THREAD_SLICE){
        put_marker(p, DRI);
        put_bits(p, 16, 4);
        put_bits(p, 16, (avctx->width-1)/(8*hsample[0]) + 1);
    }

    /* huffman table */
    put_marker(p, DHT);
    flush_put_bits(p);
    ptr = put_bits_ptr(p);
    put_bits(p, 16, 0); /* patched later */
    size = 2;
    size += put_huffman_table(p, 0, 0, s->mjpeg_ctx->bits_dc_luminance,
                              s->mjpeg_ctx->val_dc_luminance);
    size += put_huffman_table(p, 0, 1, s->mjpeg_ctx->bits_dc_chrominance,
                              s->mjpeg_ctx->val_dc_chrominance);

    size += put_huffman_table(p, 1, 0, s->mjpeg_ctx->bits_ac_luminance,
                              s->mjpeg_ctx->val_ac_luminance);
    size += put_huffman_table(p, 1, 1, s->mjpeg_ctx->bits_ac_chrominance,
                              s->mjpeg_ctx->val_ac_chrominance);
    AV_WB16(ptr, size);
}

static void jpeg_put_comments(AVCodecContext *avctx, PutBitContext *p)
{
    int size;
    uint8_t *ptr;

    if (avctx->sample_aspect_ratio.num > 0 && avctx->sample_aspect_ratio.den > 0) {
        AVRational sar = avctx->sample_aspect_ratio;

        if (sar.num > 65535 || sar.den > 65535) {
            if (!av_reduce(&sar.num, &sar.den, avctx->sample_aspect_ratio.num, avctx->sample_aspect_ratio.den, 65535))
                av_log(avctx, AV_LOG_WARNING,
                    "Cannot store exact aspect ratio %d:%d\n",
                    avctx->sample_aspect_ratio.num,
                    avctx->sample_aspect_ratio.den);
        }

        /* JFIF header */
        put_marker(p, APP0);
        put_bits(p, 16, 16);
        avpriv_put_string(p, "JFIF", 1); /* this puts the trailing zero-byte too */
        /* The most significant byte is used for major revisions, the least
         * significant byte for minor revisions. Version 1.02 is the current
         * released revision. */
        put_bits(p, 16, 0x0102);
        put_bits(p,  8, 0);              /* units type: 0 - aspect ratio */
        put_bits(p, 16, sar.num);
        put_bits(p, 16, sar.den);
        put_bits(p, 8, 0); /* thumbnail width */
        put_bits(p, 8, 0); /* thumbnail height */
    }

    /* comment */
    if (!(avctx->flags & AV_CODEC_FLAG_BITEXACT)) {
        put_marker(p, COM);
        flush_put_bits(p);
        ptr = put_bits_ptr(p);
        put_bits(p, 16, 0); /* patched later */
        avpriv_put_string(p, LIBAVCODEC_IDENT, 1);
        size = strlen(LIBAVCODEC_IDENT)+3;
        AV_WB16(ptr, size);
    }

    if (((avctx->pix_fmt == AV_PIX_FMT_YUV420P ||
          avctx->pix_fmt == AV_PIX_FMT_YUV422P ||
          avctx->pix_fmt == AV_PIX_FMT_YUV444P) && avctx->color_range != AVCOL_RANGE_JPEG)
        || avctx->color_range == AVCOL_RANGE_MPEG) {
        put_marker(p, COM);
        flush_put_bits(p);
        ptr = put_bits_ptr(p);
        put_bits(p, 16, 0); /* patched later */
        avpriv_put_string(p, "CS=ITU601", 1);
        size = strlen("CS=ITU601")+3;
        AV_WB16(ptr, size);
    }
}

void ff_mjpeg_init_hvsample(AVCodecContext *avctx, int hsample[4], int vsample[4])
{
    int chroma_h_shift, chroma_v_shift;

    av_pix_fmt_get_chroma_sub_sample(avctx->pix_fmt, &chroma_h_shift,
                                     &chroma_v_shift);
    if (avctx->codec->id == AV_CODEC_ID_LJPEG &&
        (   avctx->pix_fmt == AV_PIX_FMT_BGR0
         || avctx->pix_fmt == AV_PIX_FMT_BGRA
         || avctx->pix_fmt == AV_PIX_FMT_BGR24)) {
        vsample[0] = hsample[0] =
        vsample[1] = hsample[1] =
        vsample[2] = hsample[2] =
        vsample[3] = hsample[3] = 1;
    } else if (avctx->pix_fmt == AV_PIX_FMT_YUV444P || avctx->pix_fmt == AV_PIX_FMT_YUVJ444P) {
        vsample[0] = vsample[1] = vsample[2] = 2;
        hsample[0] = hsample[1] = hsample[2] = 1;
    } else {
        vsample[0] = 2;
        vsample[1] = 2 >> chroma_v_shift;
        vsample[2] = 2 >> chroma_v_shift;
        hsample[0] = 2;
        hsample[1] = 2 >> chroma_h_shift;
        hsample[2] = 2 >> chroma_h_shift;
    }
}

void ff_mjpeg_encode_picture_header(AVCodecContext *avctx, PutBitContext *pb,
                                    ScanTable *intra_scantable, int pred,
                                    uint16_t luma_intra_matrix[64],
                                    uint16_t chroma_intra_matrix[64])
{
    const int lossless = avctx->codec_id != AV_CODEC_ID_MJPEG && avctx->codec_id != AV_CODEC_ID_AMV;
    int hsample[4], vsample[4];
    int i;
    int components = 3 + (avctx->pix_fmt == AV_PIX_FMT_BGRA);
    int chroma_matrix = !!memcmp(luma_intra_matrix,
                                 chroma_intra_matrix,
                                 sizeof(luma_intra_matrix[0])*64);

    ff_mjpeg_init_hvsample(avctx, hsample, vsample);

    put_marker(pb, SOI);

    // hack for AMV mjpeg format
    if(avctx->codec_id == AV_CODEC_ID_AMV) goto end;

    jpeg_put_comments(avctx, pb);

    jpeg_table_header(avctx, pb, intra_scantable, luma_intra_matrix, chroma_intra_matrix, hsample);

    switch (avctx->codec_id) {
    case AV_CODEC_ID_MJPEG:  put_marker(pb, SOF0 ); break;
    case AV_CODEC_ID_LJPEG:  put_marker(pb, SOF3 ); break;
    default: av_assert0(0);
    }

    put_bits(pb, 16, 17);
    if (lossless && (  avctx->pix_fmt == AV_PIX_FMT_BGR0
                    || avctx->pix_fmt == AV_PIX_FMT_BGRA
                    || avctx->pix_fmt == AV_PIX_FMT_BGR24))
        put_bits(pb, 8, 9); /* 9 bits/component RCT */
    else
        put_bits(pb, 8, 8); /* 8 bits/component */
    put_bits(pb, 16, avctx->height);
    put_bits(pb, 16, avctx->width);
    put_bits(pb, 8, components); /* 3 or 4 components */

    /* Y component */
    put_bits(pb, 8, 1); /* component number */
    put_bits(pb, 4, hsample[0]); /* H factor */
    put_bits(pb, 4, vsample[0]); /* V factor */
    put_bits(pb, 8, 0); /* select matrix */

    /* Cb component */
    put_bits(pb, 8, 2); /* component number */
    put_bits(pb, 4, hsample[1]); /* H factor */
    put_bits(pb, 4, vsample[1]); /* V factor */
    put_bits(pb, 8, lossless ? 0 : chroma_matrix); /* select matrix */

    /* Cr component */
    put_bits(pb, 8, 3); /* component number */
    put_bits(pb, 4, hsample[2]); /* H factor */
    put_bits(pb, 4, vsample[2]); /* V factor */
    put_bits(pb, 8, lossless ? 0 : chroma_matrix); /* select matrix */

    if (components == 4) {
        put_bits(pb, 8, 4); /* component number */
        put_bits(pb, 4, hsample[3]); /* H factor */
        put_bits(pb, 4, vsample[3]); /* V factor */
        put_bits(pb, 8, 0); /* select matrix */
    }

    /* scan header */
    put_marker(pb, SOS);
    put_bits(pb, 16, 6 + 2*components); /* length */
    put_bits(pb, 8, components); /* 3 components */

    /* Y component */
    put_bits(pb, 8, 1); /* index */
    put_bits(pb, 4, 0); /* DC huffman table index */
    put_bits(pb, 4, 0); /* AC huffman table index */

    /* Cb component */
    put_bits(pb, 8, 2); /* index */
    put_bits(pb, 4, 1); /* DC huffman table index */
    put_bits(pb, 4, lossless ? 0 : 1); /* AC huffman table index */

    /* Cr component */
    put_bits(pb, 8, 3); /* index */
    put_bits(pb, 4, 1); /* DC huffman table index */
    put_bits(pb, 4, lossless ? 0 : 1); /* AC huffman table index */

    if (components == 4) {
        /* Alpha component */
        put_bits(pb, 8, 4); /* index */
        put_bits(pb, 4, 0); /* DC huffman table index */
        put_bits(pb, 4, 0); /* AC huffman table index */
    }

    put_bits(pb, 8, lossless ? pred : 0); /* Ss (not used) */

    switch (avctx->codec_id) {
    case AV_CODEC_ID_MJPEG:  put_bits(pb, 8, 63); break; /* Se (not used) */
    case AV_CODEC_ID_LJPEG:  put_bits(pb, 8,  0); break; /* not used */
    default: av_assert0(0);
    }

    put_bits(pb, 8, 0); /* Ah/Al (not used) */

end:
    if (!lossless) {
        MpegEncContext *s = avctx->priv_data;
        av_assert0(avctx->codec->priv_data_size == sizeof(MpegEncContext));

        s->esc_pos = put_bits_count(pb) >> 3;
        for(i=1; i<s->slice_context_count; i++)
            s->thread_context[i]->esc_pos = 0;
    }
}

void ff_mjpeg_escape_FF(PutBitContext *pb, int start)
{
    int size;
    int i, ff_count;
    uint8_t *buf = pb->buf + start;
    int align= (-(size_t)(buf))&3;
    int pad = (-put_bits_count(pb))&7;

    if (pad)
        put_bits(pb, pad, (1<<pad)-1);

    flush_put_bits(pb);
    size = put_bits_count(pb) - start * 8;

    av_assert1((size&7) == 0);
    size >>= 3;

    ff_count=0;
    for(i=0; i<size && i<align; i++){
        if(buf[i]==0xFF) ff_count++;
    }
    for(; i<size-15; i+=16){
        int acc, v;

        v= *(uint32_t*)(&buf[i]);
        acc= (((v & (v>>4))&0x0F0F0F0F)+0x01010101)&0x10101010;
        v= *(uint32_t*)(&buf[i+4]);
        acc+=(((v & (v>>4))&0x0F0F0F0F)+0x01010101)&0x10101010;
        v= *(uint32_t*)(&buf[i+8]);
        acc+=(((v & (v>>4))&0x0F0F0F0F)+0x01010101)&0x10101010;
        v= *(uint32_t*)(&buf[i+12]);
        acc+=(((v & (v>>4))&0x0F0F0F0F)+0x01010101)&0x10101010;

        acc>>=4;
        acc+= (acc>>16);
        acc+= (acc>>8);
        ff_count+= acc&0xFF;
    }
    for(; i<size; i++){
        if(buf[i]==0xFF) ff_count++;
    }

    if(ff_count==0) return;

    flush_put_bits(pb);
    skip_put_bytes(pb, ff_count);

    for(i=size-1; ff_count; i--){
        int v= buf[i];

        if(v==0xFF){
            buf[i+ff_count]= 0;
            ff_count--;
        }

        buf[i+ff_count]= v;
    }
}

int ff_mjpeg_encode_stuffing(MpegEncContext *s)
{
    int i;
    PutBitContext *pbc = &s->pb;
    int mb_y = s->mb_y - !s->mb_x;
    int val_ac_luminance_size, val_ac_chrominance_size;
    int ret;
    MJpegValue* current;
    MJpegContext *m;

    m = s->mjpeg_ctx;

    // TODO(jjiang): Ensure that this check is all you need.
    if (s->avctx->codec_id == AV_CODEC_ID_MJPEG) {
        // Build all the Huffman tables.
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

        // TODO(jjiang): Right now we are hardcoding the default huffman tables.
        // We need to get and assign the right huffman table!
        // i.e. call ff_mjpeg_encode_huffman_close

        /* ff_mjpeg_encode_huffman_close( */
        /*     &dc_luminance_ctx, */
        /*     m->bits_dc_luminance, */
        /*     m->val_dc_luminance, */
        /*     12); */
        /* ff_mjpeg_encode_huffman_close( */
        /*     &dc_chrominance_ctx, */
        /*     m->bits_dc_chrominance, */
        /*     m->val_dc_chrominance, */
        /*     12); */
        /* ff_mjpeg_encode_huffman_close( */
        /*     &ac_luminance_ctx, */
        /*     m->bits_ac_luminance, */
        /*     m->val_ac_luminance, */
        /*     256); */
        /* ff_mjpeg_encode_huffman_close( */
        /*     &ac_chrominance_ctx, */
        /*     m->bits_ac_chrominance, */
        /*     m->val_ac_chrominance, */
        /*     256); */

        val_ac_luminance_size = 0;
        val_ac_chrominance_size = 0;

        for(i=0;i<17;i++) {
            m->bits_dc_luminance[i] = avpriv_mjpeg_bits_dc_luminance[i];
            m->bits_dc_chrominance[i] = avpriv_mjpeg_bits_dc_chrominance[i];

            m->bits_ac_luminance[i] = avpriv_mjpeg_bits_ac_luminance[i];
            m->bits_ac_chrominance[i] = avpriv_mjpeg_bits_ac_chrominance[i];

            val_ac_luminance_size += s->mjpeg_ctx->bits_ac_luminance[i];
            val_ac_chrominance_size += s->mjpeg_ctx->bits_ac_chrominance[i];
        }
        av_assert0(val_ac_luminance_size <= 256);
        av_assert0(val_ac_chrominance_size <= 256);

        // TODO(jjiang): Need to make sure that there are only 12 dc values.
        for(i=0;i<12;i++) {
            m->val_dc_luminance[i] = avpriv_mjpeg_val_dc[i];
            m->val_dc_chrominance[i] = avpriv_mjpeg_val_dc[i];
        }

        for (i=0;i<val_ac_luminance_size;i++) {
            m->val_ac_luminance[i] = avpriv_mjpeg_val_ac_luminance[i];
        }
        for (i=0;i<val_ac_chrominance_size;i++) {
            m->val_ac_chrominance[i] = avpriv_mjpeg_val_ac_chrominance[i];
        }
        // TODO(jjiang): All the code above this comment should be replaced.

        // TODO(jjiang): Find the proper error code.
        if (m->error)
            return 1;

        av_assert0(!s->intra_ac_vlc_length);

        ff_mjpeg_build_huffman_codes(m->huff_size_dc_luminance,
                                     m->huff_code_dc_luminance,
                                     m->bits_dc_luminance,
                                     m->val_dc_luminance);
        ff_mjpeg_build_huffman_codes(m->huff_size_dc_chrominance,
                                     m->huff_code_dc_chrominance,
                                     m->bits_dc_chrominance,
                                     m->val_dc_chrominance);
        ff_mjpeg_build_huffman_codes(m->huff_size_ac_luminance,
                                     m->huff_code_ac_luminance,
                                     m->bits_ac_luminance,
                                     m->val_ac_luminance);
        ff_mjpeg_build_huffman_codes(m->huff_size_ac_chrominance,
                                     m->huff_code_ac_chrominance,
                                     m->bits_ac_chrominance,
                                     m->val_ac_chrominance);

        init_uni_ac_vlc(m->huff_size_ac_luminance,   m->uni_ac_vlc_len);
        init_uni_ac_vlc(m->huff_size_ac_chrominance, m->uni_chroma_ac_vlc_len);
        s->intra_ac_vlc_length      =
        s->intra_ac_vlc_last_length = m->uni_ac_vlc_len;
        s->intra_chroma_ac_vlc_length      =
        s->intra_chroma_ac_vlc_last_length = m->uni_chroma_ac_vlc_len;

        ff_mjpeg_encode_picture_header(s->avctx, &s->pb, &s->intra_scantable,
                                       s->pred, s->intra_matrix, s->chroma_intra_matrix);
        ff_mjpeg_encode_picture_frame(s);
        if (m->error < 0) {
            ret = m->error;
            return ret;
        }

        s->intra_ac_vlc_length = s->intra_ac_vlc_last_length = NULL;
        s->intra_chroma_ac_vlc_length = s->intra_chroma_ac_vlc_last_length = NULL;
    }

    ret = ff_mpv_reallocate_putbitbuffer(s, put_bits_count(&s->pb) / 8 + 100,
                                            put_bits_count(&s->pb) / 4 + 1000);

    if (ret < 0) {
        av_log(s->avctx, AV_LOG_ERROR, "Buffer reallocation failed\n");
        goto fail;
    }

    ff_mjpeg_escape_FF(pbc, s->esc_pos);

    if((s->avctx->active_thread_type & FF_THREAD_SLICE) && mb_y < s->mb_height)
        put_marker(pbc, RST0 + (mb_y&7));
    s->esc_pos = put_bits_count(pbc) >> 3;
fail:

    for(i=0; i<3; i++)
        s->last_dc[i] = 128 << s->intra_dc_precision;

    return ret;
}

void ff_mjpeg_encode_picture_trailer(PutBitContext *pb, int header_bits)
{
    av_assert1((header_bits & 7) == 0);

    put_marker(pb, EOI);
}

void ff_mjpeg_encode_dc(PutBitContext *pb, int val,
                        uint8_t *huff_size, uint16_t *huff_code)
{
    int mant, nbits;

    if (val == 0) {
        put_bits(pb, huff_size[0], huff_code[0]);
    } else {
        mant = val;
        if (val < 0) {
            val = -val;
            mant--;
        }

        nbits= av_log2_16bit(val) + 1;

        put_bits(pb, huff_size[nbits], huff_code[nbits]);

        put_sbits(pb, nbits, mant);
    }
}
