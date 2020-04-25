# Copyright (C) 2019 Ola Benderius
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

FROM alpine:3.11 as builder
RUN echo http://dl-4.alpinelinux.org/alpine/v3.8/main > /etc/apk/repositories && \
    echo http://dl-4.alpinelinux.org/alpine/v3.8/community >> /etc/apk/repositories && \
    apk update && \
    apk --no-cache add \
        glfw-dev \
        glew-dev \
        glm-dev \
        g++ \
        make \
        cmake \
        upx

ADD . /opt/sources
WORKDIR /opt/sources
RUN mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/tmp/dest .. && \
    make && make install #&& upx -9 /tmp/bin/opendlv-sim-camera-rpi-kiwi


FROM alpine:3.11
RUN echo http://dl-4.alpinelinux.org/alpine/v3.8/main > /etc/apk/repositories && \
    echo http://dl-4.alpinelinux.org/alpine/v3.8/community >> /etc/apk/repositories && \
    apk update && \
    apk --no-cache add \
        glfw \
        glew \
        mesa-gl \
        mesa-dri-intel \
       	mesa-dri-swrast

WORKDIR /usr/bin
COPY --from=builder /tmp/dest/ /usr
ENTRYPOINT ["/usr/bin/opendlv-sim-camera-rpi-kiwi"]


