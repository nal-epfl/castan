FROM ubuntu:16.04
MAINTAINER Luis Pedrosa <luis.pedrosa@epfl.ch>

RUN apt-get update && apt-get install -y sudo

RUN useradd -m castan && \
    echo 'castan  ALL=(root) NOPASSWD: ALL' >> /etc/sudoers

COPY . /home/castan/castan

USER castan
WORKDIR /home/castan

RUN /home/castan/castan/install.sh
