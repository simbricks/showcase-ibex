# Copyright 2024 Max Planck Institute for Software Systems, and
# National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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
mkdir -p /tmp/lowrisc-toolchain-gcc
wget -O /tmp/lowrisc-toolchain-gcc.tar.xz https://github.com/lowRISC/lowrisc-toolchains/releases/download/20250303-1/lowrisc-toolchain-gcc-rv32imcb-x86_64-20250303-1.tar.xz
tar -xf  /tmp/lowrisc-toolchain-gcc.tar.xz --strip-components=1 -C /tmp/lowrisc-toolchain-gcc
cd /tmp/lowrisc-toolchain-gcc
cp -r bin include lib lib64 libexec riscv32-unknown-elf share /usr/local
EOF

COPY --chown=simbricks . /lowrisc-ibex
WORKDIR /lowrisc-ibex

# python
RUN <<EOF
pip3 install -U -r ibex/python-requirements.txt
EOF

# install srecord
RUN apt-get install -y srecord

# Build simulation
RUN <<EOF
make -j `nproc` all
EOF

ENV PYTHONPATH="/lowrisc-ibex"


USER simbricks

WORKDIR /simbricks
