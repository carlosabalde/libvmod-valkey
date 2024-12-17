FROM ubuntu:noble-20240423

ENV DEBIAN_FRONTEND noninteractive

RUN groupadd -g 5000 dev \
    && useradd -u 5000 -g 5000 -m -s /bin/bash dev

RUN apt update \
    && apt install -y \
        apt-transport-https \
        automake \
        autotools-dev \
        bindfs \
        binutils \
        curl \
        dpkg-dev \
        git \
        gpg \
        graphviz \
        jq \
        less \
        libedit-dev \
        libev-dev \
        libjemalloc-dev \
        libncurses-dev \
        libpcre2-dev \
        libssl-dev \
        libtool \
        make \
        nano \
        netcat-traditional \
        pkg-config \
        python3 \
        python3-docutils \
        python3-sphinx \
        python3-venv \
        tar \
        telnet \
        unzip \
        wget \
    && apt clean \
    && rm -rf /var/lib/apt/lists/*

RUN cd /tmp \
    && wget --no-check-certificate https://varnish-cache.org/_downloads/varnish-7.6.0.tgz \
    && tar zxvf varnish-*.tgz \
    && rm -f varnish-*.tgz \
    && cd varnish-* \
    && ./autogen.sh \
    && ./configure \
    && make \
    && make PREFIX='/usr/local' install \
    && ldconfig

RUN cd /tmp \
    && wget --no-check-certificate https://github.com/valkey-io/libvalkey/archive/c370ca99f22ef7cc6ad83aaaf287020090327a44.zip -O libvalkey-c370ca99f22ef7cc6ad83aaaf287020090327a44.zip \
    && unzip libvalkey-*.zip \
    && rm -f libvalkey-*.zip \
    && cd libvalkey* \
    && make USE_TLS=1 \
    && make USE_TLS=1 PREFIX='/usr/local' install \
    && ldconfig

RUN cd /tmp \
    && wget --no-check-certificate https://github.com/valkey-io/valkey/archive/refs/tags/8.0.1.tar.gz -O valkey-8.0.1.tar.gz \
    && tar zxvf valkey-*.tar.gz \
    && rm -f valkey-*.tar.gz \
    && cd valkey-* \
    && make BUILD_TLS=yes \
    && make BUILD_TLS=yes PREFIX='/usr/local' USE_REDIS_SYMLINKS=no install \
    && ldconfig

COPY ./docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
