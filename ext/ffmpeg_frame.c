/* Copyright (c)2008 Antonin Amand.
 * Licensed under the Ruby License. See LICENSE for details.
 *
 */
#include "ffmpeg.h"
#include "ffmpeg_utils.h"

VALUE rb_cFFMPEGFrame;

static VALUE
frame_to_rawdata_rgb(VALUE self)
{
  int width = NUM2INT(rb_iv_get(self, "@width"));
  int height = NUM2INT(rb_iv_get(self, "@height"));
  int pixel_format = NUM2INT(rb_iv_get(self, "@pixel_format"));

  switch (pixel_format)
  {
    case AV_PIX_FMT_YUVJ420P : pixel_format = AV_PIX_FMT_YUV420P; break;
    case AV_PIX_FMT_YUVJ422P : pixel_format = AV_PIX_FMT_YUV422P; break;
    case AV_PIX_FMT_YUVJ444P : pixel_format = AV_PIX_FMT_YUV444P; break;
    case AV_PIX_FMT_YUVJ440P : pixel_format = AV_PIX_FMT_YUV440P;
    default: break;
  }

  struct SwsContext *img_convert_ctx = NULL;
  img_convert_ctx = sws_getContext(width, height, pixel_format,
                                  width, height, AV_PIX_FMT_RGB24,
                                  SWS_BICUBIC, NULL, NULL, NULL);

  AVFrame * from = get_frame(self);
  uint8_t *rgb24_buf[4];
  int dst_linesize[4];
  av_image_alloc(rgb24_buf, dst_linesize, width, height, AV_PIX_FMT_RGB24, 1);

  sws_scale(img_convert_ctx, from->data, from->linesize,
      0, height, rgb24_buf, dst_linesize);

  int stride = dst_linesize[0];
  int size = stride*height;
  // fprintf(stderr, "RGB SIZE: %d, HEIGHT: %d, WIDTH: %d\n", size, height, width);
  // fprintf(stderr, "RGB LINESIZE: [%d, %d, %d, %d]\n", dst_linesize[0],
  //   dst_linesize[1], dst_linesize[2], dst_linesize[3]);
  char * data_string = malloc(size);
  memcpy(data_string, rgb24_buf[0], size);

  VALUE ret = rb_str_new(data_string, size);
  rb_iv_set(self, "@bytes", ret);
  rb_iv_set(self, "@stride", INT2NUM(stride));
  if (img_convert_ctx)
    sws_freeContext(img_convert_ctx);
  if(data_string)
    free(data_string);
  if(rgb24_buf)
    av_freep(&rgb24_buf[0]);
  data_string = NULL;
  return self;
}

static VALUE
frame_to_rawdata_bgra(VALUE self)
{
  int width = NUM2INT(rb_iv_get(self, "@width"));
  int height = NUM2INT(rb_iv_get(self, "@height"));
  int pixel_format = NUM2INT(rb_iv_get(self, "@pixel_format"));

  switch (pixel_format)
  {
    case AV_PIX_FMT_YUVJ420P : pixel_format = AV_PIX_FMT_YUV420P; break;
    case AV_PIX_FMT_YUVJ422P : pixel_format = AV_PIX_FMT_YUV422P; break;
    case AV_PIX_FMT_YUVJ444P : pixel_format = AV_PIX_FMT_YUV444P; break;
    case AV_PIX_FMT_YUVJ440P : pixel_format = AV_PIX_FMT_YUV440P;
    default: break;
  }

  struct SwsContext *img_convert_ctx = NULL;
  img_convert_ctx = sws_getContext(width, height, pixel_format,
                                  width, height, AV_PIX_FMT_BGRA,
                                  SWS_BICUBIC, NULL, NULL, NULL);

  AVFrame * from = get_frame(self);
  uint8_t *bgra_buf[4];
  int dst_linesize[4];
  av_image_alloc(bgra_buf, dst_linesize, width, height, AV_PIX_FMT_BGRA, 1);

  sws_scale(img_convert_ctx, from->data, from->linesize,
      0, height, bgra_buf, dst_linesize);

  int stride = dst_linesize[0];
  int size = stride*height;
  // fprintf(stderr, "BGRA SIZE: %d, HEIGHT: %d, WIDTH: %d\n", size, height, width);
  // fprintf(stderr, "BGRA LINESIZE: [%d, %d, %d, %d]\n", dst_linesize[0],
  //   dst_linesize[1], dst_linesize[2], dst_linesize[3]);
  char * data_string = malloc(size);
  memcpy(data_string, bgra_buf[0], size);

  VALUE ret = rb_str_new(data_string, size);
  rb_iv_set(self, "@bytes", ret);
  rb_iv_set(self, "@stride", INT2NUM(stride));
  if (img_convert_ctx)
    sws_freeContext(img_convert_ctx);
  if(data_string)
    free(data_string);
  if(bgra_buf)
    av_freep(&bgra_buf[0]);
  data_string = NULL;
  return self;
}

static VALUE
frame_to_rawdata_gray(VALUE self)
{
    AVFrame *frame = get_frame(self);

    int width = NUM2INT(rb_iv_get(self, "@width"));
    int height = NUM2INT(rb_iv_get(self, "@height"));

    int stride = frame->linesize[0];
    int size = stride * height;
    // fprintf(stderr, "GRAY SIZE: %d, HEIGHT: %d, WIDTH: %d\n", size, height, width);
    // fprintf(stderr, "GRAY LINESIZE: [%d, %d, %d, %d]\n", frame->linesize[0],
    //   frame->linesize[1], frame->linesize[2], frame->linesize[3]);
    char *data_string = malloc(size);
    memcpy(data_string, frame->data[0], size);

    VALUE ret = rb_str_new(data_string, size);
    rb_iv_set(self, "@bytes", ret);
    rb_iv_set(self, "@stride", INT2NUM(stride));
    if(data_string)
      free(data_string);
    data_string = NULL;
    return self;
}

static VALUE
frame_to_rawdata_yuv(VALUE self)
{
    AVFrame *frame = get_frame(self);

    int width = NUM2INT(rb_iv_get(self, "@width"));
    int height = NUM2INT(rb_iv_get(self, "@height"));

    int stride = frame->linesize[0] +
                frame->linesize[1] +
                frame->linesize[2];
    int size = stride * height;
    char *data_string = malloc(size);
    memcpy(data_string, frame->data[0], frame->linesize[0] * height);
    memcpy(data_string, frame->data[1], frame->linesize[1] * height);
    memcpy(data_string, frame->data[2], frame->linesize[2] * height);

    VALUE ret = rb_str_new(data_string, size);
    rb_iv_set(self, "@bytes", ret);
    rb_iv_set(self, "@stride", INT2NUM(stride));
    if(data_string)
      free(data_string);
    data_string = NULL;
    return self;
}

static void
free_frame(AVFrame * frame)
{
    if (frame)
      av_frame_free(&frame);
}

static VALUE
alloc_frame(VALUE klass)
{
    AVFrame * frame = av_frame_alloc();
    VALUE obj;
    obj = Data_Wrap_Struct(klass, 0, free_frame, frame);
    return obj;
}

static VALUE
frame_initialize(VALUE self, VALUE width, VALUE height, VALUE stride, VALUE pixel_format)
{
    rb_iv_set(self, "@width", width);
    rb_iv_set(self, "@height", height);
    rb_iv_set(self, "@pixel_format", pixel_format);
    rb_iv_set(self, "@stride", stride);
    rb_iv_set(self, "@btyes", Qnil);
    return self;
}

static void
frame_destroy(VALUE self)
{
  AVFrame * frame = NULL;
  Data_Get_Struct(self, AVFrame, frame);
  if (frame)
    free_frame(frame);
}

VALUE
build_frame_object(AVFrame * frame, int width, int height, int stride, int pixel_format)
{
    VALUE obj = Data_Wrap_Struct(rb_cFFMPEGFrame, 0, free_frame, frame);

    return frame_initialize(obj,
        INT2FIX(width),
        INT2FIX(height),
        INT2FIX(stride),
        INT2FIX(pixel_format));
}

void
Init_FFMPEGFrame() {
    rb_cFFMPEGFrame = rb_define_class_under(rb_mFFMPEG, "Frame", rb_cObject);

    rb_define_alloc_func(rb_cFFMPEGFrame, alloc_frame);
    rb_define_method(rb_cFFMPEGFrame, "initialize", frame_initialize, 4);

    rb_funcall(rb_cFFMPEGFrame, rb_intern("attr_reader"), 1, rb_sym("width"));
    rb_funcall(rb_cFFMPEGFrame, rb_intern("attr_reader"), 1, rb_sym("height"));
    rb_funcall(rb_cFFMPEGFrame, rb_intern("attr_reader"), 1, rb_sym("stride"));
    rb_funcall(rb_cFFMPEGFrame, rb_intern("attr_reader"), 1, rb_sym("pixel_format"));
    rb_funcall(rb_cFFMPEGFrame, rb_intern("attr_reader"), 1, rb_sym("bytes"));
    
    rb_define_method(rb_cFFMPEGFrame, "to_rgb", frame_to_rawdata_rgb, 0);
    rb_define_method(rb_cFFMPEGFrame, "to_gray", frame_to_rawdata_gray, 0);
    rb_define_method(rb_cFFMPEGFrame, "to_bgra", frame_to_rawdata_bgra, 0);
    rb_define_method(rb_cFFMPEGFrame, "to_yuv", frame_to_rawdata_yuv, 0);
    rb_define_method(rb_cFFMPEGFrame, "destroy!", frame_destroy, 0);
}
