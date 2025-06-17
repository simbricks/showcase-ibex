
ARG REGISTRY=
ARG TAG=:latest

FROM ${REGISTRY}simbricks/simbricks-executor${TAG}

USER root

# install newer version of verilator
RUN <<EOF
cd /tmp
apt-get update
apt-get install help2man
git clone -b v5.032 https://github.com/verilator/verilator
cd verilator
autoupdate
autoconf
./configure
make -j`nproc`
make install
rm -rf /tmp/verilator
EOF

# risc-v compiler toolchain
RUN <<EOF
mkdir -p /lowrisc-toolchain-gcc
wget -O /tmp/lowrisc-toolchain-gcc.tar.xz https://github.com/lowRISC/lowrisc-toolchains/releases/download/20250611-1/lowrisc-toolchain-gcc-rv64imac-x86_64-20250611-1.tar.xz
tar -xf  /tmp/lowrisc-toolchain-gcc.tar.xz --strip-components=1 -C /lowrisc-toolchain-gcc
EOF

COPY --chown=simbricks . /lowrisc-ibex
WORKDIR /lowrisc-ibex

# python
RUN <<EOF
pip3 install -U -r ibex/python-requirements.txt
EOF

# libelf and it's dev libs + srecord
# RUN <<EOF
# apt-get install libelf-dev
# apt-get install srecord
# EOF

# Build simulation
RUN <<EOF
# make -j `nproc` all
EOF

# ENV PYTHONPATH="/corundum_src"


USER simbricks

WORKDIR /simbricks
