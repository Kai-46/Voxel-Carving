FROM ubuntu:16.04

LABEL maintainer="Kai Zhang"


### build opencv 2.4.13.6

WORKDIR /tmp

RUN apt-get update && apt install -y build-essential cmake git pkg-config python-dev python-numpy \
    libavcodec-dev libavformat-dev libswscale-dev \
    libjpeg-dev libpng12-dev libtiff5-dev libjasper-dev \
    libgtk2.0-dev

RUN git clone https://github.com/opencv/opencv.git
WORKDIR /tmp/opencv
RUN git checkout 2.4.13.6
RUN mkdir /tmp/opencv/build
WORKDIR /tmp/opencv/build
RUN cmake -D CMAKE_BUILD_TYPE=RELEASE \
    -D CMAKE_INSTALL_PREFIX=/usr/local \
    -D INSTALL_C_EXAMPLES=ON \
    -D INSTALL_PYTHON_EXAMPLES=ON \
    -D OPENCV_GENERATE_PKGCONFIG=ON \
    -D BUILD_EXAMPLES=ON ..
RUN make -j$(($(nproc) - 1))
RUN make install

### remove temporary files
RUN rm -rf /tmp/*


### install boost
RUN apt-get update && apt install -y libboost-all-dev libproj-dev

### install vtk 5
RUN apt-get update && apt install -y python-vtk libvtk5-dev


### build voxel-carving
RUN mkdir /tools
WORKDIR /tools
# RUN git clone https://github.com/Kai-46/Voxel-Carving.git
ADD . /tools/Voxel-Carving

RUN mkdir /tools/Voxel-Carving/build
WORKDIR /tools/Voxel-Carving/build
RUN cmake ..
RUN make -j$(($(nproc) - 1))

### add to /usr/local/bin
RUN cp /tools/Voxel-Carving/build/main /usr/local/bin/voxel-carving
RUN chmod a+x /usr/local/bin/voxel-carving
