Name:       libcommhistory-qt5
Summary:    Communications event history database API
Version:    1.12.6
Release:    1
License:    LGPLv2
URL:        https://github.com/sailfishos/libcommhistory
Source0:    %{name}-%{version}.tar.bz2
Source1:    %{name}.privileges
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Contacts)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(qtcontacts-sqlite-qt5-extensions) >= 0.3.0
BuildRequires:  pkgconfig(contactcache-qt5) >= 0.3.0
BuildRequires:  libphonenumber-devel

%description
Library for accessing the communications (IM, SMS and call) history database.

%package tests
Summary: Test files for libcommhistory
Requires: blts-tools

%description tests
Test files for libcommhistory

%package tools
Summary: Command line tools for libcommhistory
Requires: %{name} = %{version}-%{release}
Conflicts: libcommhistory-tools

%description tools
Command line tools for the commhistory library.

%package declarative
Summary: QML plugin for libcommhistory
License: BSD and LGPLv2
Requires: %{name} = %{version}-%{release}

%description declarative
QML plugin for libcommhistory

%package devel
Summary: Development files for libcommhistory
Requires: %{name} = %{version}-%{release}

%description devel
Headers and static libraries for the commhistory library.

%package doc
Summary: Documentation for libcommhistory

%description doc
Documentation for libcommhistory

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake_install

mkdir -p %{buildroot}%{_datadir}/mapplauncherd/privileges.d
install -m 644 -p %{SOURCE1} %{buildroot}%{_datadir}/mapplauncherd/privileges.d

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_libdir}/libcommhistory-qt5.so.*
%license COPYING

%files tools
%{_bindir}/commhistory-tool
%{_datadir}/mapplauncherd/privileges.d/*

%files declarative
%{_libdir}/qt5/qml/org/nemomobile/commhistory

%files tests
/opt/tests/libcommhistory-qt5

%files devel
%{_libdir}/libcommhistory-qt5.so
%{_libdir}/pkgconfig/commhistory-qt5.pc
%{_includedir}/commhistory-qt5

%files doc
%{_datadir}/doc/libcommhistory-qt5
