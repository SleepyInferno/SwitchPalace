FROM devkitpro/devkita64:latest
# Pin by digest in production; latest for initial development

# Install Switch development packages
RUN dkp-pacman -Syu --noconfirm && \
    dkp-pacman -S --noconfirm \
        switch-dev \
        switch-libzstd \
        switch-zlib \
        switch-mbedtls \
        switch-cmake

WORKDIR /build
COPY . .

# Initialize submodules and build
RUN git submodule update --init --recursive && \
    mkdir -p build && \
    cd build && \
    cmake .. -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON -DUSE_STD_THREAD=ON -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make SwitchPalace.nro
