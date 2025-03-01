# Base image
FROM ubuntu:20.04

# Gem5 dependencies
ENV DEBIAN_FRONTEND=noninteractive
RUN apt -y update && apt -y upgrade && \
    apt -y install build-essential git m4 scons zlib1g zlib1g-dev \
    libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
    python3-dev python-is-python3 doxygen libboost-all-dev \
    libhdf5-serial-dev python3-pydot libpng-dev libelf-dev pkg-config pip \
    python3-venv black
RUN pip install mypy pre-commit

# Custom environment
# install tqdm package for progress management
RUN pip install tqdm
# install gdb for debug
RUN apt -y install gdb
# Install mold linker
RUN git clone https://github.com/rui314/mold.git && mkdir mold/build && \
    cd mold/build && git checkout v2.4.1 && ../install-build-deps.sh && \
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-10 .. && \
    cmake --build . -j $(nproc)  && \
    cmake --build . --target install

# Customized command prompt
RUN echo 'LC_ALL="C.UTF-8"' >> /.bashrc
RUN echo 'PS1="\[\e[1;34m\]🐳 \u@\h \W> \[\e[m\]"' >> /.bashrc
