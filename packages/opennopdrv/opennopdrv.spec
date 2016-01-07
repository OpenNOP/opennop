%define modname	opennopdrv

Summary:		A program that increases WAN performance
Name:			%{modname}
URL:			http://www.opennop.org
License:		GPL-3.0
Group:			System/Kernel
Version:		0.5.0.0
Release:		0
Epoch:			0
Source:			%{modname}-%{version}.tar.gz
%if 0%{?suse_version}
Source98:       preamble
%endif
BuildRoot:		%{_tmppath}/%{name}-%{version}-build
%define kernel_version %(uname -r)

%if 0%{?suse_version}
ExcludeArch:	s390 s390x
%else
BuildArch:		noarch
%endif

%if 0%{?suse_version}
%suse_kernel_module_package -n %{name}  -p %{SOURCE98} xen um
BuildRequires:	pkgconfig
BuildRequires:	gettext
BuildRequires:	automake
BuildRequires:	autoconf
BuildRequires:	libtool
BuildRequires:	libnetfilter_queue-devel
BuildRequires:	libnfnetlink-devel
BuildRequires:	ncurses-devel
BuildRequires:	kernel-headers
BuildRequires:	kernel-devel
BuildRequires:  binutils
BuildRequires:	git
BuildRequires:	%kernel_module_package_buildreqs
BuildRequires:	libnfnetlink-devel
%endif

%if 0%{?mdkversion}
Requires: dkms-minimal
%endif

%if 0%{?suse_version} == 0 && 0%{?mdkversion} == 0
Requires: dkms
%endif

%if 0%{?suse_version} > 1140
Requires:		libnl-1_1
Requires:		libnfnetlink 
%else
Requires:		libnl
%endif

%if 0%{?suse_version}
Requires:		opennopdrv-kmp
Requires:		kernel-default-devel
Requires:		libnetfilter_queue1
Requires:		libnfnetlink0
%else
Requires:		libnetfilter_queue
Requires:		libnfnetlink
Requires:		libnl
%endif

Requires:		gcc
Requires:		kernel-headers
Requires:		make
Requires:		binutils
Requires:		ncurses

%description
OpenNOP is an open source Linux based network accelerator. It's designed to optimise network traffic over point-to-point, partially-meshed and full-meshed IP networks.

%prep
%setup -q
%if 0%{?suse_version}
set -- *
mkdir source
mv "$@" source/
mkdir obj
%endif

%build
%if 0%{?suse_version}
for flavor in %flavors_to_build; do
	rm -rf obj/$flavor
	cp -r source obj/$flavor
	make -C /usr/src/linux-obj/%_target_cpu/$flavor modules M=$PWD/obj/$flavor opennopdrv.ko
done
%endif

%install
#rm -rf ${RPM_BUILD_ROOT}
%if 0%{?suse_version}
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
for flavor in %flavors_to_build; do
	make -C /usr/src/linux-obj/%_target_cpu/$flavor modules_install M=$PWD/obj/$flavor opennopdrv.ko
done
%endif
install -d ${RPM_BUILD_ROOT}/usr/src/opennop
install -v ${RPM_SOURCE_DIR}/%{modname}-%{version}.tar.gz ${RPM_BUILD_ROOT}/usr/src/opennop/%{modname}-%{version}.tar.gz


%if 0%{?suse_version} == 0
%posttrans
pushd /usr/src >/dev/null 2>&1
#
#	Install dkms sources
#
pushd /usr/src/opennop >/dev/null 2>&1
tar -xf %{modname}-%{version}.tar.gz
popd >/dev/null 2>&1

#
#	Setup dkms.conf
#
cat > /usr/src/opennop/%{modname}-%{version}/dkms.conf << EOF
PACKAGE_NAME=%{modname}
PACKAGE_VERSION=%{version}
MAKE[0]="make -C \${kernel_source_dir} SUBDIRS=\${dkms_tree}/\${PACKAGE_NAME}/\${PACKAGE_VERSION}/build modules"
CLEAN="make -f /usr/src/%{modname}-%{version}/Makefile -C \${kernel_source_dir} SUBDIRS=\${dkms_tree}/\${PACKAGE_NAME}/\${PACKAGE_VERSION}/build clean"
DEST_MODULE_LOCATION[0]="/updates/"
BUILT_MODULE_NAME[0]=%{modname}
EOF
#
set -x
if [ -x /usr/sbin/dkms ] ; then
	/usr/sbin/dkms add -m %{modname} -v %{version} --rpm_safe_upgrade
	/usr/sbin/dkms build -m %{modname} -v %{version}
	/usr/sbin/dkms install -m %{modname} -v %{version}
fi
#
popd >/dev/null 2>&1
%endif

%if 0%{?suse_version} == 0
%preun
#
#	Remove the module, rmmod can fail
#
if [ -x /sbin/rmmod ] ; then
	/sbin/rmmod %{modname} >/dev/null 2>&1
fi
#
#	Remove the module from dkms
#
set -x
if [ -x /usr/sbin/dkms ] ; then
	/usr/sbin/dkms remove -m %{modname} -v %{version} --all --rpm_safe_upgrade || :
fi
%endif

%clean
rm -rf ${RPM_BUILD_ROOT} 

%files
%defattr(-,root,root)
%dir /usr/src/opennop/
/usr/src/opennop/%{modname}-%{version}.tar.gz

%changelog