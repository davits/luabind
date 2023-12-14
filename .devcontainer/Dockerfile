FROM ubuntu:22.04

SHELL ["/bin/bash", "-c"]

RUN apt-get -q update -y && \
    apt-get -q install -y --no-install-recommends \
    sudo \
    git \
    cmake \
    ninja-build \
    lcov \
    libstdc++6 \
    # the following packages are required by llvm.sh
    lsb-release wget software-properties-common gnupg

RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 17 all && \
    rm -f llvm.sh; \
    ln -s /usr/bin/clang-17 /usr/bin/clang; \
    ln -s /usr/bin/clang++-17 /usr/bin/clang++; \
    ln -s /usr/bin/lldb-17 /usr/bin/lldb; \
    ln -sf /usr/bin/llvm-cov-17 /usr/bin/gcov; \
    ln -sf /usr/bin/clangd-17 /usr/bin/clangd;

# Change these values by your uid/gid during build
ARG USERID=1000
ARG GROUPID=1000

# Create development user (me)
ENV USER=me
ENV LANG=C.UTF-8
RUN groupadd --gid ${GROUPID} --force me && \
    useradd --shell /bin/bash --home-dir /home/me --uid ${USERID} --gid ${GROUPID} --create-home me && \
    echo "me ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers
USER me
WORKDIR /home/me
