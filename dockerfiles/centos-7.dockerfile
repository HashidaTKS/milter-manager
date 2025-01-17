FROM centos:7

ENV SCL=rh-ruby30

RUN yum update -q -y && \
    yum install -q -y \
      centos-release-scl-rh \
      http://sourceforge.net/projects/cutter/files/centos/cutter-release-1.3.0-1.noarch.rpm && \
    yum install -q -y \
      cutter \
      gcc \
      gettext \
      git \
      glib2-devel \
      gobject-introspection-devel \
      gtk-doc \
      intltool \
      libtool \
      make \
      ${SCL}-ruby \
      ${SCL}-ruby-devel \
      ${SCL}-rubygem-rexml \
      rpm-build \
      rrdtool \
      sudo \
      tar \
      tzdata

RUN useradd -m --user-group --shell /bin/bash milter-manager
RUN mkdir /build && \
    chown -R milter-manager: /build

USER milter-manager
