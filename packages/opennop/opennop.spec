Summary:		A program that increases WAN performance
Name:			opennop
URL:			http://www.opennop.org
License:		GPL-3.0
Group:			Productivity/Networking/Routing
Version:		0.5.0.0
Release:		0
Epoch:			0
Source:			opennop-%{version}.tar.gz
BuildRoot:		%{_tmppath}/%{name}-%{version}-build
ExcludeArch:	s390 s390x
BuildRequires:	pkgconfig
BuildRequires:	gettext
BuildRequires:	automake
BuildRequires:	autoconf
BuildRequires:	libtool
BuildRequires:	libuuid-devel
BuildRequires:	uuid
BuildRequires:	libnetfilter_queue-devel
BuildRequires:	libnfnetlink-devel
BuildRequires:	ncurses-devel
BuildRequires:	git
%if 0%{?suse_version}
BuildRequires:	libnfnetlink-devel
BuildRequires:	libopenssl-devel
%else
BuildRequires:	openssl-devel
%endif


%if 0%{?suse_version}
Requires:		kernel-default-devel
Requires:		libnetfilter_queue1
Requires:		libnfnetlink0
Requires:		libuuid1
%else
Requires:		libnetfilter_queue
Requires:		libnfnetlink
Requires:		libnl
Requires:		uuid
%endif
Requires:		gcc
Requires:		kernel-headers
Requires:		make
Requires:		binutils
Requires:		ncurses
Requires:		openssl

%description
OpenNOP is an open source Linux based network accelerator. It's designed to optimize network traffic over point-to-point, partially-meshed and full-meshed IP networks.

%prep
%setup -q

%build
autoreconf -fi
./configure --sysconfdir=/var/opennop --bindir=/usr/bin --sbindir=/usr/sbin
make

%install
rm -rf ${RPM_BUILD_ROOT}
make install DESTDIR=${RPM_BUILD_ROOT}
install -d ${RPM_BUILD_ROOT}/etc/init.d
install -v ${RPM_SOURCE_DIR}/opennop ${RPM_BUILD_ROOT}/etc/init.d/opennop
install -d ${RPM_BUILD_ROOT}/usr/src/opennop
install -d ${RPM_BUILD_ROOT}/var/opennop

%clean
rm -rf ${RPM_BUILD_ROOT} 
  
%files
%defattr(-,root,root)
/usr/bin/opennop
/usr/sbin/opennopd
/etc/init.d/opennop
/usr/src/opennop
/var/opennop


%changelog