# WARNING
Tested on Dahua cams only, doesn't work on HiWatch DS-N241W

## Frame Timestamp
Extracting absolute timestamp of each frame for RTSP stream(H.264)

## Requirements
Compiled FFMpeg and sources, since timestamp extraction requires internal `<libavformat/rtsp.h>` header file.

## Notes
```
export ff_src=$HOME/ffmpeg_sources
export ff_bld=$HOME/ffmpeg_build

PKG_CONFIG_PATH="$ff_bld" ./configure --prefix="$ff_bld" --pkg-config-flags="--static" --extra-cflags="-I$ff_bld/include" --extra-ldflags="-L$ff_bld/lib" --bindir="$ff_bin" --enable-gpl --enable-nonfree
PKG_CONFIG_PATH="ff_bld/lib/pkgconfig/" gcc -I$ff_src/ main.c -o main.out `pkg-config --cflags --libs libavformat libswscale` -lvdpau -lX11 -lva-x11 -lva-drm
```

## Used resources
1. http://www.cs.columbia.edu/~hgs/rtp/faq.html
2. http://stackoverflow.com/questions/2439096/h264-rtp-timestamp
3. http://stackoverflow.com/questions/20265546/reading-rtcp-packets-from-an-ip-camera-using-ffmpeg
