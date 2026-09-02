FROM ghcr.io/loveretro/tg5040-toolchain:latest AS taglib

ARG TAGLIB_COMMIT=7d86716194777e0294453bfdc9dd170bd033e1f4
ARG UTF8CPP_COMMIT=df857efc5bbc2aa84012d865f7d7e9cccdc08562

RUN git init /tmp/taglib \
    && git -C /tmp/taglib remote add origin https://github.com/taglib/taglib.git \
    && git -C /tmp/taglib fetch --depth 1 origin ${TAGLIB_COMMIT} \
    && git -C /tmp/taglib checkout --detach FETCH_HEAD \
    && git clone https://github.com/nemtrif/utfcpp.git /tmp/taglib/3rdparty/utfcpp \
    && git -C /tmp/taglib/3rdparty/utfcpp checkout --detach ${UTF8CPP_COMMIT}

RUN cmake -S /tmp/taglib -B /tmp/taglib-build \
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/pocket-music-deps \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_TESTING=OFF \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_BINDINGS=OFF \
      -DWITH_ZLIB=OFF \
    && cmake --build /tmp/taglib-build -j4 \
    && cmake --install /tmp/taglib-build

FROM ghcr.io/loveretro/tg5040-toolchain:latest AS ffmpeg

ARG FFMPEG_COMMIT=894da5ca7d742e4429ffb2af534fcda0103ef593

RUN git init /tmp/ffmpeg \
    && git -C /tmp/ffmpeg remote add origin https://git.ffmpeg.org/ffmpeg.git \
    && git -C /tmp/ffmpeg fetch --depth 1 origin ${FFMPEG_COMMIT} \
    && git -C /tmp/ffmpeg checkout --detach FETCH_HEAD

RUN cd /tmp/ffmpeg \
    && ./configure \
      --prefix=/opt/pocket-music-deps \
      --arch=aarch64 \
      --target-os=linux \
      --cross-prefix=${CROSS_COMPILE} \
      --enable-cross-compile \
      --enable-static \
      --disable-shared \
      --disable-programs \
      --disable-doc \
      --disable-debug \
      --disable-autodetect \
      --disable-network \
      --disable-avdevice \
      --disable-avfilter \
      --disable-everything \
      --enable-avformat \
      --enable-avcodec \
      --enable-avutil \
      --enable-swresample \
      --enable-protocol=file \
      --enable-demuxer=flac,mp3,ogg,wav \
      --enable-decoder=flac,mp3,opus,pcm_s16le,pcm_s24le,pcm_s32le,pcm_u8,vorbis \
      --enable-parser=flac,mpegaudio,opus,vorbis \
    && make -j4 \
    && make install

FROM ghcr.io/loveretro/tg5040-toolchain:latest AS app

COPY --from=taglib /opt/pocket-music-deps /opt/pocket-music-deps
COPY --from=ffmpeg /opt/pocket-music-deps /opt/pocket-music-deps
COPY . /src

RUN cmake -S /src -B /tmp/pocket-music-build \
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} \
      -DCMAKE_PREFIX_PATH=/opt/pocket-music-deps \
      -DCMAKE_BUILD_TYPE=Release \
      -DPOCKET_MUSIC_TRIMUI=ON \
      -DBUILD_TESTING=OFF \
    && cmake --build /tmp/pocket-music-build -j4 \
    && ${CROSS_COMPILE}strip /tmp/pocket-music-build/pocket-music

RUN mkdir -p /package/PocketMusic/assets/fonts /package/PocketMusic/bin /package/PocketMusic/data \
    && cp /tmp/pocket-music-build/pocket-music /package/PocketMusic/bin/ \
    && cp /src/assets/fonts/NotoSans-Regular.ttf /package/PocketMusic/assets/fonts/ \
    && cp /src/assets/background-hello-kitty-v6.png /package/PocketMusic/assets/ \
    && cp /src/assets/fallback-vinyl.png /package/PocketMusic/assets/ \
    && cp /src/assets/icon.png /package/PocketMusic/icon.png \
    && cp /src/platform/trimui/config.json /src/platform/trimui/launch.sh /package/PocketMusic/ \
    && chmod +x /package/PocketMusic/launch.sh /package/PocketMusic/bin/pocket-music

FROM scratch AS export
COPY --from=app /package/ /
