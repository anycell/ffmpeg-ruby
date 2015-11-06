/* Copyright (c)2008 Antonin Amand.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 */
#include "ffmpeg.h"
#include "ffmpeg_utils.h"

VALUE rb_cFFMPEGStream;

static int
next_packet(AVFormatContext * format_context, AVPacket * packet)
{
    if(packet->data != NULL)
        av_free_packet(packet);

    int ret = av_read_frame(format_context, packet);
    if(ret < 0)
        return -1;
    
    return 0;
}

static int 
next_packet_for_stream(AVFormatContext * format_context, int stream_index, AVPacket * packet)
{
    int ret = 0;
    do {
        ret = next_packet(format_context, packet);
    } while(packet->stream_index != stream_index && ret == 0);

    return ret;
}

static VALUE stream_codec(VALUE self)
{
    AVStream * stream = get_stream(self);
    
    VALUE rb_codec = rb_iv_get(self, "@codec");
    
    if (rb_codec == Qnil && NULL != stream->codec)
        rb_codec = rb_iv_set(self, "@codec", build_codec_object(stream->codec));
    
    return rb_codec;
}

static VALUE stream_index(VALUE self)
{
    AVStream * stream = get_stream(self);
    return INT2FIX(stream->index);
}

static VALUE
stream_duration(VALUE self)
{
    AVStream * stream = get_stream(self);
    if (stream->duration == AV_NOPTS_VALUE) {
        return Qnil;
    }
    return(rb_float_new(stream->duration * av_q2d(stream->time_base)));
}

static VALUE
stream_frame_rate(VALUE self)
{
    AVStream * stream = get_stream(self);
    return(rb_float_new(av_q2d(stream->r_frame_rate)));
}

static VALUE
stream_seek(VALUE self, VALUE position)
{
    AVFormatContext * format_context = get_format_context(rb_iv_get(self, "@format"));
    AVStream * stream = get_stream(self);
    
    int64_t timestamp = NUM2LONG(position) / av_q2d(stream->time_base);
    
    int ret;
    if (format_context->start_time != AV_NOPTS_VALUE)
        timestamp += format_context->start_time;
    
    //fprintf(stderr, "seeking to %d\n", NUM2INT(position));
    ret = av_seek_frame(format_context, stream->index, timestamp, 0);
    if (ret < 0) {
        rb_raise(rb_eRangeError, "could not seek %s to pos %f",
            format_context->filename, timestamp * av_q2d(stream->time_base));
    }
    
    //fprintf(stderr, "seeked.\n");
    return self;
}

static VALUE
stream_position(VALUE self)
{
    AVFormatContext * format_context = get_format_context(rb_iv_get(self, "@format"));
    AVStream * stream = get_stream(self);
    AVPacket decoding_packet;
    
    av_init_packet(&decoding_packet);
        
    do {
        if(av_read_frame(format_context, &decoding_packet) < 0) {
            rb_raise(rb_eRuntimeError, "error extracting packet");
        }
    } while(decoding_packet.stream_index != stream->index);

    return rb_float_new(decoding_packet.pts * (double)av_q2d(stream->time_base));
}

static int
extract_next_frame(AVFormatContext * format_context, AVCodecContext * codec_context,
    int stream_index, AVFrame * frame, AVPacket * decoding_packet)
{
    // open codec to decode the video if needed
    if (NULL == codec_context->codec) {
            rb_fatal("codec should have already been opened");
    }
    
    uint8_t * databuffer;
    
    int remaining = 0;
    int decoded;
    int frame_complete = 0;
    int next;
    
    while(!frame_complete &&
            0 == (next = next_packet_for_stream(format_context, stream_index, decoding_packet))) {
        // setting parameters before processing decoding_packet data
        remaining = decoding_packet->size;
        databuffer = decoding_packet->data;
        
        while(remaining > 0) {
            decoded = avcodec_decode_video(codec_context, frame, &frame_complete,
                databuffer, remaining);
            remaining -= decoded;
            // pointer seek forward
            databuffer += decoded;
        }
    }
    
    return next;
}

static VALUE 
extract_next_audio(AVFormatContext * format_context, AVCodecContext * codec_context, 
    int stream_index, AVPacket * decoding_packet)
{
    if (NULL == codec_context->codec) {
            rb_fatal("codec should have already been opened");
    }
    
    uint8_t * databuffer;

    int remaining = 0;
    int decoded;
    int frame_complete = 0;
    int next;

    int buf_cap = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    int buf_size = 0;

    ReSampleContext * re_codec_context = 
        av_audio_resample_init(codec_context->channels, 
            codec_context->channels,
            16000, codec_context->sample_rate,
            SAMPLE_FMT_S16, SAMPLE_FMT_S16, 16, 10, 0, 1.0);

    char *raw_data = malloc(buf_cap);
    while(!frame_complete &&
            0 == (next = next_packet_for_stream(format_context, stream_index, decoding_packet))) {
        remaining = decoding_packet->size;
        databuffer = decoding_packet->data;

        while(remaining > 0) {

            int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
            char *out_buffer = malloc(out_size);
            decoded = avcodec_decode_audio2(codec_context, (int16_t*)out_buffer, 
                &out_size, databuffer, remaining);

            remaining -= decoded;
            databuffer += decoded;
        
            if ((buf_size+out_size)>=buf_cap){
                buf_cap *= 2;
                uint8_t *tmp = malloc(buf_cap);
                memcpy(tmp, raw_data, buf_size);
                free(raw_data);
                raw_data = tmp;
                tmp = NULL;
            }
            memcpy(raw_data+buf_size, out_buffer, out_size);
            buf_size += out_size;

            if (out_buffer) {
                free(out_buffer);
                out_buffer = NULL;
            }
        }
    }

    VALUE ret = Qnil;

    char *resample_buffer = malloc(buf_cap);
    int sample_num = buf_size/(codec_context->channels*2);
    int resample_size = audio_resample(re_codec_context, (int16_t *)resample_buffer, raw_data, sample_num);
    //printf("%d %d---\n", buf_size, resample_size*2);

    if (buf_size != 0)
        ret = rb_str_new(resample_buffer, resample_size*2*codec_context->channels);

    if (raw_data) {
        free(raw_data);
        raw_data = NULL;
    }

    if (resample_buffer) {
        free(resample_buffer);
        resample_buffer = NULL;
    }

    return ret;
}

static VALUE
stream_decode_audio(VALUE self)
{

    AVFormatContext * format_context = get_format_context(rb_iv_get(self, "@format"));
    AVStream * stream = get_stream(self);

    AVCodecContext * codec_context = stream->codec;

    if (!codec_context->codec) {
        AVCodec * codec = avcodec_find_decoder(codec_context->codec_id);
        if (!codec)
            rb_raise(rb_eRuntimeError, "error codec not found");
        if (avcodec_open(codec_context, codec) < 0)
            rb_raise(rb_eRuntimeError, "error while opening codec : %s", codec->name);
    }

    AVPacket decoding_packet;
    av_init_packet(&decoding_packet);
    return extract_next_audio(format_context, codec_context, stream->index, &decoding_packet);
}

static VALUE
stream_decode_frame(VALUE self)
{
    AVFormatContext * format_context = get_format_context(rb_iv_get(self, "@format"));
    AVStream * stream = get_stream(self);
    
    AVCodecContext * codec_context = stream->codec;
    
    // open codec to decode the video if needed
    if (!codec_context->codec) {
        AVCodec * codec = avcodec_find_decoder(codec_context->codec_id);
        if (!codec)
            rb_raise(rb_eRuntimeError, "error codec not found");
        if (avcodec_open(codec_context, codec) < 0)
            rb_raise(rb_eRuntimeError, "error while opening codec : %s", codec->name);
    }
    
   // AVFrame * tmp_frame;
  //  get_buffer(codec_context, tmp_frame);

    VALUE rb_frame = rb_funcall(rb_const_get(rb_mFFMPEG, rb_intern("Frame")),
        rb_intern("new"), 4,
        INT2NUM(codec_context->width),
        INT2NUM(codec_context->height),
        INT2NUM(0),
        INT2NUM(codec_context->pix_fmt));
    
    AVFrame * frame = get_frame(rb_frame);
    avcodec_get_frame_defaults(frame);
    
    AVPacket decoding_packet;
    av_init_packet(&decoding_packet);
    
    if (rb_block_given_p()) {
        int ret;
        do {
            ret = extract_next_frame(format_context, stream->codec,
                stream->index, frame, &decoding_packet);
            rb_yield(
                rb_ary_new3(
                    3,
                    rb_frame,
                    rb_float_new(decoding_packet.pts * (double)av_q2d(stream->time_base)),
                    rb_float_new(decoding_packet.dts * (double)av_q2d(stream->time_base))
                )
            );
        } while (ret == 0);
    } else {
        int ret = extract_next_frame(format_context, stream->codec,
            stream->index, frame, &decoding_packet);
        if (ret != 0)
            return Qnil;
        return rb_frame;
    }
    
    return self;
}


// ######################  CONSTRUCT / DESTROY #############################

void
mark_stream(AVStream * stream)
{}

void
free_stream(AVStream * stream)
{}

static VALUE
alloc_stream(VALUE klass)
{
    AVStream * stream = av_new_stream(NULL, 0);
    return Data_Wrap_Struct(rb_cFFMPEGStream, 0, 0, stream);
}

static VALUE
stream_initialize(VALUE self, VALUE format)
{
    rb_iv_set(self, "@format", format);
    return self;
}

VALUE build_stream_object(AVStream * stream, VALUE rb_format)
{
    VALUE rb_stream = Data_Wrap_Struct(rb_cFFMPEGStream, 0, 0, stream);
    return stream_initialize(rb_stream, rb_format);
}

void
Init_FFMPEGStream()
{
    rb_cFFMPEGStream = rb_define_class_under(rb_mFFMPEG, "Stream", rb_cObject);
    rb_define_alloc_func(rb_cFFMPEGStream, alloc_stream);
    rb_define_method(rb_cFFMPEGStream, "initialize", stream_initialize, 0);
    
    rb_define_method(rb_cFFMPEGStream, "index", stream_index, 0);
    rb_define_method(rb_cFFMPEGStream, "codec", stream_codec, 0);
    rb_define_method(rb_cFFMPEGStream, "duration", stream_duration, 0);
    rb_define_method(rb_cFFMPEGStream, "frame_rate", stream_frame_rate, 0);
    rb_define_method(rb_cFFMPEGStream, "position", stream_position, 0);
    rb_define_method(rb_cFFMPEGStream, "decode_frame", stream_decode_frame, 0);
    rb_define_method(rb_cFFMPEGStream, "decode_audio", stream_decode_audio, 0);
    rb_define_method(rb_cFFMPEGStream, "seek", stream_seek, 1);
}
