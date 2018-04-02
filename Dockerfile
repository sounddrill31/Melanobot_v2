FROM debian:buster as builder

RUN apt-get update && apt-get install -y build-essential g++ make cmake \
    cmake-curses-gui libgeoip-dev libboost-dev libasio-dev \
    libboost-python-dev libboost-thread-dev libboost-system-dev \
    libboost-filesystem-dev libboost-program-options-dev && \
    mkdir build

COPY . /src
RUN cd /build && cmake \
    -DBUILD_TESTING=OFF \
    -DMODULE_FUN=OFF \
    -DMODULE_GITHUB=OFF \
    -DMODULE_TELEGRAM=OFF \
    -DCMAKE_CXX_FLAGS="-fstack-protector -fPIC -pie" \
    -DCMAKE_INSTALL_RPATH='$ORIGIN/:$ORIGIN/../lib:$ORIGIN/lib:$ORIGIN/../../lib' \
    -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=OFF \
    -DCMAKE_SKIP_BUILD_RPATH=OFF \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
    -DCMAKE_INSTALL_PREFIX="/opt/melanobot/" \
    -DCMAKE_BUILD_TYPE=Release /src/ && \
    make -j$(nproc) && \
    make install && \
    mkdir /opt/melanobot/lib && \
    cp /build/lib/*.so /opt/melanobot/lib/ && \
    cp -rf /build/lib/melanobot /opt/melanobot/lib/

FROM debian:buster-slim
RUN apt-get update && apt-get install -y libboost-atomic1.62.0 \
    libboost-chrono1.62.0 libboost-date-time1.62.0 libboost-filesystem1.62.0 \
    libboost-program-options1.62.0 libboost-python1.62.0 libboost-regex1.62.0 \
    libboost-serialization1.62.0 libboost-system1.62.0 libboost-thread1.62.0 \
    libssl1.1 libpython2.7 && \
    useradd -d /opt/melanobot -s /bin/bash melanobot && \
    rm -rf /var/lib/apt/lists/*

USER melanobot:melanobot
COPY --from=builder --chown=melanobot:melanobot /opt/melanobot /opt/melanobot
VOLUME ["/opt/melanobot/config"]
WORKDIR /opt/melanobot/config
