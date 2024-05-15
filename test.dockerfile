# syntax=docker/dockerfile:1
# Basic build:
# $ docker buildx build --tag maud-test --file test.dockerfile .
#
# To override the docker image:
# $ !! --build-context ubuntu=docker-image://amd64/ubuntu:24.04
#
# Basic run:
# $ docker run --rm --interactive --tty --volume .:/maud maud-test bash
FROM ubuntu

ARG CONDA_ENV
ARG CONDA_VERSION

ENV CONDA_ENV=${CONDA_ENV:-conda_env}
ENV CONDA_VERSION=${CONDA_VERSION:-latest}

RUN <<ENDRUN bash -s # Update apt
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -y -q
  apt-get install -y -q curl wget tzdata libc6-dbg gdb
  #apt-get clean
  #rm -rf /var/lib/apt/lists/*
ENDRUN

# Mamba has more up-to-date versions of most packages but only gcc-13
RUN apt-get install -y g++-14

RUN <<ENDRUN bash -s # Install conda/mamba via Mambaforge
  URL="https://github.com/conda-forge/miniforge/releases"
  URL="\$URL/\$CONDA_VERSION"
  URL="\$URL/download/Mambaforge-$(uname)-$(uname -m).sh"
  wget \$URL -nv -O /tmp/installer.sh
  bash /tmp/installer.sh -b -p /opt/conda
  rm /tmp/installer.sh
ENDRUN

RUN /opt/conda/bin/mamba init

ADD $CONDA_ENV.txt /
RUN <<ENDRUN bash -s # Initialize and configure conda/mamba
  . /opt/conda/etc/profile.d/conda.sh
  . /opt/conda/etc/profile.d/mamba.sh

  conda config --set show_channel_urls True
  conda config --set remote_connect_timeout_secs 12
  #mamba clean --all --yes

  mamba create --name maud --file /\$CONDA_ENV.txt --yes

  # Activate the created environment by default
  echo "mamba activate maud" >> ~/.bashrc
ENDRUN

# use login shell when running the container
ENTRYPOINT ["/bin/bash", "-l", "-c"]

