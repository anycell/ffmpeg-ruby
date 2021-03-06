# Copyright (c)2008 Antonin Amand.
# Licensed under the Ruby License. See LICENSE for details.
#
begin
  require 'HFL_core'
rescue LoadError
  require File.dirname(__FILE__) + '/FFMPEG_core'
end

module FFMPEG
  class InputFormat
    def first_video_stream
      streams.find { |st| st.type == :video }
    end

    def first_audio_stream
      streams.find { |st| st.type == :audio }
    end

    def destroy!
      super
    end
  end

  class Stream
    def type
      codec.type
    end
  end

  class Frame
    alias_method :columns, :stride
    alias_method :cols, :width
    alias_method :rows, :height
  end
end
