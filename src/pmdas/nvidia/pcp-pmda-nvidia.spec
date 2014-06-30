Name:		pcp-pmda-nvidia
Version:	3.9.4
Release:	1
Summary:	A PMDA for PCP that tracks metrics for NVIDIA graphics cards

Group:		Applications/System
License:	GPLv2+
URL:		http://ccr.buffalo.edu/
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

AutoReqProv:	no
BuildRequires:	pcp-libs-devel = 3.9.4
Requires:	/bin/sh  
Requires:	libc.so.6()(64bit)  
Requires:	libc.so.6(GLIBC_2.2.5)(64bit)  
Requires:	libc.so.6(GLIBC_2.3.4)(64bit)  
Requires:	libc.so.6(GLIBC_2.4)(64bit)  
Requires:	libdl.so.2()(64bit)  
Requires:	libm.so.6()(64bit)  
Requires:	libpcp.so.3()(64bit)  
Requires:	libpcp_pmda.so.3()(64bit)  
Requires:	libpthread.so.0()(64bit)  
Requires:	pcp = 3.9.4
Requires:	rpmlib(CompressedFileNames) <= 3.0.4-1
Requires:	rpmlib(FileDigests) <= 4.6.0-1
Requires:	rpmlib(PayloadFilesHavePrefix) <= 4.0-1
Requires:	rtld(GNU_HASH)  
Requires:	rpmlib(PayloadIsXz) <= 5.2-1

%description
The NVIDIA PMDA is a PCP module for gathering metrics on the performance of 
NVIDIA graphics cards.

By default, this package will install the PMDA as both a collector and monitor,
and it will run in DSO mode. To use the PMDA in a different configuration,
re-run the install script in its folder. It will also first add this PMDA to
the PCP namespace file, if it has not already been included.

This spec file is partially based on the pcp-pmda-infiniband RPM.

%global pmdadir %{_sharedstatedir}/pcp/pmdas/nvidia
%global pmnsdir %{_sharedstatedir}/pcp/pmns

%prep
%setup -q -n nvidia


%build
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}%{pmdadir}


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%doc README
%{pmdadir}/*

%changelog

%post

cd %{pmnsdir}

# The name and default domain number for the PMDA.
pmda_name="NVML"
pmda_number=120

# The maximum number a custom PMDA is allowed to use.
MAX_VALID_NUMBER=510

# Check if the PMDA has already been added to PCP's namespace files.
pmda_found=false
while read macro name number other ; do
	if [ "$name" == "$pmda_name" ] ; then
		pmda_found=true
		break
	fi
done < %{pmnsdir}/stdpmid

if ! $pmda_found ; then
	while read name number other ; do
		if [ "$name" == "$pmda_name" ] ; then
			pmda_found=true
			break
		fi
	done < %{pmnsdir}/stdpmid.local
fi

# If it has not been added...
if ! $pmda_found ; then

	# Find a domain number that has not been used in the config file yet,
	# starting with the default number. If one can't be found in the valid
	# range, stop looking.
	available_number_found=false
	while ! $available_number_found ; do
		available_number_found=true
		while read macro name number other ; do
			if [ "$number" == "$pmda_number" ] ; then
				available_number_found=false
				break
			fi
		done < %{pmnsdir}/stdpmid

		if $available_number_found ; then
			while read name number other ; do
				if [ "$number" == "$pmda_number" ] ; then
					available_number_found=false
					break
				fi
			done < %{pmnsdir}/stdpmid.local
		fi

		if ! $available_number_found ; then
			pmda_number=`expr $pmda_number + 1`
			if [ "$pmda_number" -gt "$MAX_VALID_NUMBER" ] ; then
				break
			fi
		fi
	done

	# If no available number was found, report the error and exit.
	if ! $available_number_found ; then
		echo "No available domain number found for $pmda_name." >&2
		exit 1
	fi

	# Append the PMDA name and number to the local namespace file.
	printf "$pmda_name\t\t$pmda_number\n" >> %{pmnsdir}/stdpmid.local

	# Run the script to generate a new combined namespace file.
	%{pmnsdir}/Make.stdpmid

	# Report the addition.
	echo "$pmda_name added to PCP namespace file with number $pmda_number"
fi

# Run the PMDA install script.
cd %{pmdadir}
printf "b\ndso\n" | %{pmdadir}/Install

%preun

# If uninstalling the module completely,
# run the PMDA remove script before uninstalling files.
if [ "$1" == 0 ] ; then
	cd %{pmdadir}
	%{pmdadir}/Remove
fi
