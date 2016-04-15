FROM ubuntu:15.10

RUN apt-get update
RUN apt-get -y install \
	    iproute2 \
	    net-tools \
	    vim \
	    git \
	    tmux \
	    autoconf \
	    automake \
	    libtool \
	    g++ \
	    wget \
	    uuid-dev \
	    dbus \
	    diffstat \
	    texinfo \
	    gawk \
	    chrpath \
	    fakeroot \
	    u-boot-tools \
	    valgrind \
	    intltool \
	    libc6-i386 \
	    psmisc \
	    software-properties-common \
	    cmake \
	    pkg-config

RUN apt-get -y install \
            build-essential \
            libass-dev \
            libfreetype6-dev \
            libsdl1.2-dev \
            libtheora-dev \
            libtool \
            libva-dev \
            libvdpau-dev \
            libvorbis-dev \
            libxcb1-dev \
            libxcb-shm0-dev \
            libxcb-xfixes0-dev \
            libx264-dev \
            libgstreamer-plugins-bad1.0-dev \
            texinfo \
            zlib1g-dev \
            yasm \
            tar

RUN cd /tmp; \
    wget https://github.com/FFmpeg/FFmpeg/releases/download/n3.0/ffmpeg-3.0.tar.bz2; \
    tar xjvf ffmpeg-3.0.tar.bz2; \
    cd ffmpeg-3.0; \
    ./configure --prefix="/usr/local" --extra-cflags="-I/usr/local/include" --extra-ldflags="-L/usr/local/lib" --enable-gpl --enable-libx264; \
    make -j4; \
    make install

RUN cd /tmp; \
    git clone https://github.com/david7482/mp4v2.git; \
    cd mp4v2; \
    git checkout cc17ffe84c08f26011e4a2867f6196e62c4258ee -b build; \
    autoreconf --install; \
    ./configure --prefix=/usr/local --disable-static --disable-debug; \
    make -j4; \
    make install

RUN apt-get -y install gtk-doc-tools libsqlite3-dev libglibmm-2.4-dev glib-networking libsigc++-2.0-dev
RUN cd /tmp; \
    git clone https://github.com/david7482/libsoup.git; \
    cd libsoup; \
    git checkout 6fc5bd261457fd58faa4ca16fbe964566ef152e5 -b build; \
    ./autogen.sh --without-gnome --disable-static --disable-debug --disable-introspection --disable-vala; \
    make -j4; \
    make install
RUN cd /tmp; \
    wget -nd -nH 'https://storage.googleapis.com/golang/go1.6.1.linux-amd64.tar.gz'; \
    tar zxf go1.6.1.linux-amd64.tar.gz -C /usr/local; \
    echo 'export PATH=$PATH:/usr/local/go/bin' >> /root/.bashrc

RUN rm -rf /tmp/*
