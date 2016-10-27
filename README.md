[OpenNOP][]
===========
[OpenNOP][] is an open source Linux based network accelerator. It's designed to optimize network traffic over point-to-point, partially-meshed and full-meshed IP networks.

Installing
==================
You can install [OpenNOP][] from the [OpenNOP Repositories][] or [OpenNOP Development Repositories][].

Building
================
Prerequisites:

* autoconf
* automake
* openssl
* ncurses-devel
* libnfnetlink-devel
* libnetfilter_queue-devel
* libnl-devel
* readline-devel
* openssl-devel
* libuuid-devel
* uuid-devel
* pkg-config

Compiling:

1. Clone the repository.
 1. `git clone https://github.com/OpenNOP/opennop.git`
2. Go to source folder.
 1. `cd opennop`
3. Build binaries.
 1. `./autogen.sh`
 2. `./configure`
 3. `make`
4. Start OpenNOP Daemon.
 1. `./opennopd/opennopd`
5. Enter OpenNOP CLI
 1. `./opennop/opennop`


Contributing
=======================
[Sign the Contributor License Agreement][]

[![CLA assistant](https://cla-assistant.io/readme/badge/OpenNOP/opennop)](https://cla-assistant.io/OpenNOP/opennop)
[![Code Triagers Badge](https://www.codetriage.com/opennop/opennop/badges/users.svg)](https://www.codetriage.com/opennop/opennop)

Build Status
=======================
[![Build Status](https://travis-ci.org/OpenNOP/opennop.svg)](https://travis-ci.org/OpenNOP/opennop)
<a href="https://scan.coverity.com/projects/opennop">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/809/badge.svg"/>
</a>

[OpenNOP]:                                   http://www.opennop.org/
[Sign the Contributor License Agreement]:    https://cla-assistant.io/OpenNOP/opennop
[OpenNOP Repositories]:                      https://build.opensuse.org/project/repositories/network:opennop
[OpenNOP Development Repositories]:          https://build.opensuse.org/project/repositories/network:opennop:devel
