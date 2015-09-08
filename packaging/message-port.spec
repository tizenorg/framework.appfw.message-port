Name:       message-port
Summary:    Message Port internal library
Version: 	1.2.2.1
Release:    7
Group:		Application Framework/Libraries
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source101:	messageportd.service
Source1001:	%{name}.manifest
Source1002:	messageportd.manifest
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(bundle)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(chromium)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(libsmack)
BuildRequires:  pkgconfig(pkgmgr)
BuildRequires:  pkgconfig(pkgmgr-info)

# runtime requires
Requires: chromium

Requires(post): /sbin/ldconfig
Requires(post): coreutils
Requires(postun): /sbin/ldconfig

Provides:   lib%{name}.so.1

%description
Message Port internal library

%package devel
Summary:  Message Port internal library (Development)
Group:    Application Framework/Development
Requires: %{name} = %{version}-%{release}

%description devel
Message Port internal library (Development)

%package -n messageportd
Summary:  Message Port Daemon
Group:    Application Framework/Development
Requires: %{name} = %{version}-%{release}

%description -n messageportd
Message Port Daemon

%prep
%setup -q

%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%if 0%{?sec_build_binary_debug_enable}
	CXXFLAGS="$CXXFLAGS -D_SECURE_LOG -DTIZEN_DEBUG_ENABLE"
%endif
%cmake . -DFULLVER=%{version} -DMAJORVER=${MAJORVER}

# Call make instruction with smp support
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}

%make_install

mkdir -p %{buildroot}/usr/share/license
install LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}

mkdir -p %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants
install -m0644 %{SOURCE101} %{buildroot}%{_libdir}/systemd/system/messageportd.service
ln -s ../messageportd.service %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/messageportd.service

mkdir -p %{buildroot}%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/
ln -s %{_libdir}/systemd/system/messageportd.service %{buildroot}%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/

cp %{SOURCE1001} .
cp %{SOURCE1002} .

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%attr(0644,root,root) %{_libdir}/lib%{name}.so.*
/usr/share/license/%{name}
/usr/lib/tmpfiles.d/tmpdir.conf

%files devel
%{_includedir}/appfw/*.h
%{_libdir}/pkgconfig/*.pc
%{_libdir}/lib%{name}.so

%files -n messageportd
%manifest messageportd.manifest
%attr(0755,root,root) %{_bindir}/messageportd
%{_libdir}/systemd/system/messageportd.service
%{_libdir}/systemd/system/multi-user.target.wants/messageportd.service
%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/messageportd.service
