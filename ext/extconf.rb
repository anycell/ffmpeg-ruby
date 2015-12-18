# --with-ffmpeg-dir=/opt/ffmpeg

require 'mkmf'

if find_executable('pkg-config')
  $CFLAGS << ' ' + `pkg-config libavfilter --cflags`.strip
  $CFLAGS << ' ' + `pkg-config libavcodec --cflags`.strip
  $CFLAGS << ' ' + `pkg-config libavutil --cflags`.strip
  $CFLAGS << ' ' + `pkg-config libswscale --cflags`.strip
  $CFLAGS << ' ' + `pkg-config libswresample --cflags`.strip
=begin
  $LDFLAGS << ' ' + `pkg-config libavfilter --libs`.strip
  $LDFLAGS << ' ' + `pkg-config libavcodec --libs`.strip
  $LDFLAGS << ' ' + `pkg-config libavutil --libs`.strip
  $LDFLAGS << ' ' + `pkg-config libswscale --libs`.strip
  $LDFLAGS << ' ' + `pkg-config libswresample --libs`.strip
=end
end

ffmpeg_include, ffmpeg_lib = dir_config("ffmpeg")
dir_config("libswscale")

$CFLAGS << " -W -Wall"
#$LDFLAGS << " -rpath #{ffmpeg_lib}"

if have_library(":libavformat.so.57") and find_header('libavformat/avformat.h') and
   have_library(":libavcodec.so.57")  and find_header('libavutil/avutil.h') and
   have_library(":libavutil.so.55")   and find_header('libavcodec/avcodec.h') and
   have_library(":libswresample.so.2")   and find_header('libswresample/swresample.h') and
   have_library(":libswscale.so.4")  and find_header('libswscale/swscale.h') then
 
$objs = %w(ffmpeg.o ffmpeg_format.o ffmpeg_input_format.o ffmpeg_stream.o ffmpeg_utils.o ffmpeg_frame.o ffmpeg_codec.o)

create_makefile("FFMPEG_core")

else
  STDERR.puts "missing library"
  exit 1
end
