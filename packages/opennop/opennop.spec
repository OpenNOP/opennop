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
%define kernel_version %(uname -r)


%if 0%{?suse_version}
ExcludeArch:	s390 s390x
%else
BuildArch:		noarch
%endif

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
BuildRequires:  binutils
BuildRequires:	readline-devel
BuildRequires:	kernel-headers
BuildRequires:	kernel-devel
BuildRequires:	%kernel_module_package_buildreqs

%if 0%{?mdkversion}
Requires: dkms-minimal
%endif

%if 0%{?suse_version} == 0 && 0%{?mdkversion} == 0
Requires: dkms
%endif

%if 0%{?suse_version}
BuildRequires:	libnfnetlink-devel
BuildRequires:	libopenssl-devel
BuildRequires:	kernel-default-devel
%else
BuildRequires:	openssl-devel
%endif

%if 0%{?suse_version} > 1140
Requires:		libnl-1_1
Requires:		libnfnetlink 
%else
Requires:		libnl
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
Requires:		readline

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
make install DESTDIR=${RPM_BUILD_ROOT} INSTALL_MOD_PATH=${RPM_BUILD_ROOT}
rm -rf ${RPM_BUILD_ROOT}/lib
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