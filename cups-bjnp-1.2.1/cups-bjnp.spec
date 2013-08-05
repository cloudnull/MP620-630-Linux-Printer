Summary		: CUPS backend for the Canon BJNP network printers 
Name		: cups-bjnp
Version		: 1.0
Release		: 1%{?dist}
License		: GPLv2
Source		: http://downloads.sourceforge.net/cups-bjnp/cups-bjnp-%{version}.tar.gz
Group		: System Environment/Daemons
URL		: https://sourceforge.net/projects/cups-bjnp/
BuildRoot	: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	cups-devel
Requires: cups

%define cups_backend_dir %{_exec_prefix}/lib/cups/backend
%description
This package contains a backend for CUPS for Canon printers using the 
proprietary BJNP network protocol.

%prep
%setup -q

%build
%configure --prefix=%{_exec_prefix} --with-cupsbackenddir=%{cups_backend_dir}
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT INSTALL="install -p" install

%clean
rm -Rf $RPM_BUILD_ROOT
 
%files
%defattr(-,root,root,-)
%{cups_backend_dir}/bjnp
%doc COPYING ChangeLog TODO NEWS README

%changelog
* Tue Jul 05 2011 Louis Lagendijk <llagendijk@users.sourceforge.net> - 1.0-1
- New upstream release
- Fix FTBS on Fedora BZ715649

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.5.5-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Sat Nov 27 2010 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.5-2
  added patch for compilation for cups-1.3

* Fri Nov 26 2010 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.5-1
  Released 0.5.5, no further changes

* Sat Oct 09 2010 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.5b1-7
  Corrected a number of bugs reported by Ron Murray (sourceforge patches tracker
  ID: 3083455). We got confused when the printer had an IPv6 address (MP990)

* Thu May 13 2010 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.4-6
- Rebuilt for EL-6

* Thu May 13 2010 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.4-5
- Rebuilt for EL-5

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.5.4-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Mon Jun 08 2009 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.4-3
- corrected backendir again, forgot the backend suffix

* Sat May 30 2009 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.4-2
- corrected backendir
- Removed -n option from setup

* Fri May 29 2009 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.4-1
- More specfile cleanups suggested by Igor Jurišković <juriskovic.igor@gmail.com>
  and mtasaka@ioa.s.u-tokyo.ac.jp:
- removed INSTALL 
- use macros where appropriate),
- updated files macro so we only own bjnp, not the directory
- Removed generation of spec file from configure as it caused a lot of pain
- bumped version to 0.5.4

* Mon May 18 2009 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.3-2
- Specfile cleanups

* Sat May 16 2009 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.3-1
- Reverted changes to spec file generation
- Fixed some rpmlint comments that show up only on complete package but
  not when rpmlint is run on spec file

* Sat May 16 2009 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5.2-2
- Corrected silly errors in configure script
- Added missing address family setting in bjnp-send-broadcast

* Thu Mar 12 2009 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5-1
- Documentation improvements
- Debugging improvements
- Release 0.5

* Thu Feb 19 2009 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.5beta4-1
- Release 0.5beta4
- added autoconf/automake support
- added support for OS's that do not have getifaddr(). this support is limited as
  global broadcasts may not work when more than one active interface is found
- corrected an error when getifaddr finds a ptp interface

* Mon Nov 17 2008 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.4-1
- Release 0.4
- Added keep-alive to keep TCP connection open (printer will close it when
  connection is idle for ~30 sec). This will occur when filter is slow with
  sending more data for whatever reason.
- Added a delay of 15 seconds at end of printjob as some printers will otherwise not 
  accept new printjobs
- Cleanup of Makefile and specfile so it works for all RedHat style systems

* Tue Oct 7 2008 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.3-1
- added some sanity checks on call to gethostbyaddr so we do not bomb out
  when hostname field is not set (can this occur?) or when a buggy Avm fritz box
  returns noname rather then fail when no name is available on a reverse name lookup
- Added a version string printout when incorrect number of arguments is recieved on command line
- changed printbuffer to 4k. Recommended by Zedo, the Turboprint people
  changed throttle pause from 3 sec to 40 msec. Improves performance. Recommended by Zedo.
- fixed a bug in scanning our interfaces when ifa_addr could be 0
  (reported by Matt McHenry <jerith@speakeasy.org>)
- added new/ improved compiler flags that trap more mistakes
- sone cleanup with feedback from Sane-BJNP (revised protocol codes etc)
- minor bugfixes for off-line behaviour

* Sat Aug 9 2008 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.2-1
- release 0.2

* Sat Aug 2 2008 Louis Lagendijk <llagendijk@users.sourceforge.net> - 0.1-1
- Initial RPM release for release 0.1
