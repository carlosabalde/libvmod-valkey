Summary: Valkey VMOD for Varnish
Name: vmod-valkey
Version: 1.0
Release: 1%{?dist}
License: BSD
URL: https://github.com/carlosabalde/libvmod-valkey
Group: System Environment/Daemons
Source0: libvmod-valkey.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: varnish >= 6.0.0, libvalkey >= 0.1.0, libev >= 4.03
BuildRequires: make, python-docutils, varnish >= 6.0.0, varnish-devel >= 6.0.0, libvalkey-devel >= 0.1.0, libev-devel >= 4.03

%description
Valkey VMOD for Varnish

%prep
%setup -n libvmod-valkey

%build
./autogen.sh
./configure --prefix=/usr/ --docdir='${datarootdir}/doc/%{name}' --libdir='%{_libdir}'
%{__make}
%{__make} check

%install
[ %{buildroot} != "/" ] && %{__rm} -rf %{buildroot}
%{__make} install DESTDIR=%{buildroot}

%clean
[ %{buildroot} != "/" ] && %{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_libdir}/varnish*/vmods/lib*
%doc /usr/share/doc/%{name}/*
%{_mandir}/man?/*

%changelog
* Fri Apr 28 2023 Carlos Abalde <carlos.abalde@gmail.com> - 1.0-1.20230428
- Initial release.
