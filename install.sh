#!/bin/bash
# - title        : MP620-630 Installation Script
# - description  : Install the MP620/30 On Debian Linux
# - author       : Kevin Carter
# - License      : GPLv3
# - date         : 2013-07-04
# - version      : 5B4
# =============================================================================
# This script was created by Kevin Carter and BK Integration
# This script is for use under GPLv3 and you should feel free to
# Modify it as you see fit, all I ask is that you give me credit
# for some of it...  I hope these files help, Drop me a line and
# let me know.  http://cloudnull.io
# =============================================================================

trap CONTROL_C SIGINT

CONTROL_C(){
    echo -e "AAHAHAAHH! FIRE! CRASH AND BURN!
You Pressed [ CTRL C ]"
    EXITERROR
}

echo "Version 6.0.1 - Universal"

USER=$(whoami)
ARCH=$(uname -m)
DATE=$(date +-%Y-%m-%d-%H%M)
UBUNTUCODEOS=$(lsb_release -r | awk '{print $2}')
KERNELNUM=$(uname -r | awk -F '-' '{print $1}')

if [ `which lsb_release` ];then
    CODENAME=$(lsb_release -d)
else
    CODENAME=$(head -1 /etc/issue | awk '{print $1,$2,$3}')
fi

echo -e "\nThis Script will install several pieces of software and make a
few adjustments to various pieces of your Operating System to
install the access to the Printer on  your Machine\n"

if [ "$(id -u)" != "0" ]; then
    echo -e "This script must be run as ROOT
You have attempted to run this as $USER
use su -c or sudo or change to root and try again.\n"
    exit 0
fi

echo -e "\nThis Installer has been tested on Ubuntu 10.xx - 12.04 & Debian 5 - 6
Current User\t\t= $USER
Operating System\t= $CODENAME
System Architecture\t= $ARCH\n"
read -p "Press [ Enter ] to Continue or Press [ CTRL-C ] to quit"

LOGFILE=/tmp/canon-printing_Install.log
if [ -f $LOGFILE ];then
    mv $LOGFILE $LOGFILE.$(date +%H%M%S)
fi 

EXITERROR(){
    echo -e '\nPlease contact Kevin at info@cloudnull.com 
to see if there is an update for this Installation.
Log file for the installation of the Drivers is in 
your temp Folder located at $LOGFILE\n'
    exit 1
}

echo -e "\n\033[1;31mPurging the old installation file should there be any\033[0m"
echo ' *** Purging OLD Packages  *** ' >> $LOGFILE 2>&1

CHECK1=$(dpkg -l | grep cnijfilter | cut -d " " -f 3)
if [ "$CHECK1" ]  > /dev/null 2>&1;then
    echo 'Directory Warnings for this package are normal.  If you see one please Ignore'
    su -c 'dpkg -l | grep cnijfilter | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi
CHECK2=$(dpkg -l | grep scangear | cut -d " " -f 3)
if [ "$CHECK2" ] > /dev/null 2>&1;then
    su -c 'dpkg -l | grep scangear | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi 
CHECK3=$(dpkg -l | grep libcupsys2 | cut -d " " -f 3)
if [ "$CHECK3" ] > /dev/null 2>&1;then
    su -c 'dpkg -l | grep libcupsys2 | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi
CHECK4=$(grep 'Printer IP Address' /etc/hosts)
if [ "$CHECK4" ] > /dev/null 2>&1;then
    echo "Printer is being removed from the Hostfile."
    su -c 'sed -i "/Printer IP Address/,+1 d" /etc/hosts' >> $LOGFILE 2>&1
fi
CHECK5=$(dpkg -l | grep -o libtiff4 | cut -d " " -f 3)
if [ "$CHECK5" ] > /dev/null 2>&1;then
    su -c 'dpkg -l | grep libtiff4 | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi
CHECK6=$(dpkg -l | grep -o libpopt0 | cut -d " " -f 3)
if [ "$CHECK6" ] > /dev/null 2>&1;then
    su -c 'dpkg -l | grep -o libpopt0 | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi
CHECK7=$(dpkg -l | grep -o libpopt0:i386 | cut -d " " -f 3)
if [ "$CHECK7" ] > /dev/null 2>&1;then
    su -c 'dpkg -l | grep -o libpopt0:i386 | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi
CHECK8=$(dpkg -l | grep -o libtiff4:i386 | cut -d " " -f 3)
if [ "$CHECK8" ] > /dev/null 2>&1;then
    su -c 'dpkg -l | grep -o libtiff4:i386 | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi
CHECK9=$(dpkg -l | grep -o cups-bjnp | cut -d " " -f 3)
if [ "$CHECK9" ] > /dev/null 2>&1;then
    su -c 'dpkg -l | grep -o cups-bjnp | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi
CHECK10=$(dpkg -l | grep -o cups-bjnp_1.0-1_all | cut -d " " -f 3)
if [ "$CHECK10" ] > /dev/null 2>&1;then
    su -c 'dpkg -l | grep -o cups-bjnp_1.0-1_all | cut -d " " -f 3 | xargs dpkg -P' >> $LOGFILE 2>&1
fi

if [ ! /lib32/libpng.so.3 ]; then 
    su -c 'rm -v /lib32/libpng.so.3' >> $LOGFILE 2>&1
fi
if [ ! /usr/lib32/libpng.so.3 ]; then 
    su -c 'rm -v /lib32/libpng.so.3' >> $LOGFILE 2>&1
fi
if [ ! /usr/lib32/libtiff.so.3 ]; then 
    su -c 'rm -v /lib32/libpng.so.3' >> $LOGFILE 2>&1
fi
if [ ! /lib/libpng.so.3 ]; then 
    su -c 'rm -v /lib32/libpng.so.3' >> $LOGFILE 2>&1
fi
if [ ! /usr/lib/libtiff.so.3 ]; then 
    su -c 'rm -v /lib32/libpng.so.3' >> $LOGFILE 2>&1
fi
if [ ! /usr/lib/libpng.so.3 ]; then 
    su -c 'rm -v /lib32/libpng.so.3' >> $LOGFILE 2>&1
fi
if [ -f /usr/share/ppd/canonmp620-630_Universal.ppd ]; then
    su -c 'rm -v /usr/share/ppd/canonmp*' >> $LOGFILE 2>&1
fi 
if [ -f /usr/share/ppd/canonip1800_Universal.ppd ]; then
    su -c 'rm -v /usr/share/ppd/canonip*' >> $LOGFILE 2>&1
fi
if [ ! /lib/i386-linux-gnu/libpng.so.3 ];then
    su -c 'rm -v /lib/i386-linux-gnu/libpng.so.3' >> $LOGFILE 2>&1
fi
if [ ! /usr/lib/i386-linux-gnu/libtiff.so.3 ];then
    su -c 'rm -v /usr/lib/i386-linux-gnu/libtiff.so.3' >> $LOGFILE 2>&1
fi

echo ' *** Purge Complete *** ' >> $LOGFILE 2>&1

echo -e "\n\033[1;31mUpdating Packages\033[0m"
su -c 'apt-get update' > /dev/null 2>&1

CHECKGDEB=$(dpkg -l | grep -o gdebi-core)
if [ ! "$CHECKGDEB" ];then
    echo -e "\n\033[1;31mInstalling \"gdebi\" tool\033[0m"
    su -c 'apt-get -y install gdebi-core' >> $LOGFILE 2>&1
fi

echo -e "\033[1;31mInstalling any and all dependencies for Printing and Scanning\033[0m"

su -c 'apt-get -y --force-yes install build-essential libcups2 libcups2-dev \
autotools-dev fakeroot dh-make build-essential' >> $LOGFILE 2>&1
    
echo -e "\033[1;31mChanging files to root ownership\033[0m"
su -c 'chown root:root -R ./bjnp' >> $LOGFILE 2>&1

echo -e "\033[1;31mSwitching to bjnp Directory\033[0m"
pushd ./cups-bjnp-1.2.1
su -c 'chmod +x configure' >> $LOGFILE 2>&1

echo -e "\033[1;31mBuilding BJNP Networking\033[0m"
su -c './configure && dh_make -y -s --createorig' >> $LOGFILE 2>&1
popd

if [ `uname -m` = "x86_64" ]; then
    echo -e '\nInstalling the 32Bit Libraries for the system'
    su -c 'apt-get -y install ia32-libs' >> $LOGFILE 2>&1
    if [ $? -ne 0 ] ; then
        echo -e '\nI am sorry though you are using a x64 system and the apt-get 
package management system failed to install ia32-libs. The package ia32-libs is 
needed to continue...\n'
        EXITERROR
    fi
    if [ -e /lib32/libpng12.so.[0-9].* ]; then
        echo 'Creating a Symbolic Link to libpng12 /lib32/libpng.so.3'
        ln -sf /lib32/libpng12.so.[0-9].* /lib32/libpng.so.3 >> $LOGFILE 2>&1
    fi
    if [ -e /usr/lib32/libpng12.so.[0-9].* ]; then
        echo 'Creating a Symbolic Link to libpng12 /usr/lib32/libpng.so.3'
        ln -sf /usr/lib32/libpng12.so.[0-9].* /usr/lib32/libpng.so.3 >> $LOGFILE 2>&1
    fi
    if [ -e /usr/lib32/libtiff.so.[0-9].* ]; then
        echo 'Creating a Symbolic Link to libtiff /usr/lib32/libtiff.so.3'
        ln -sf /usr/lib32/libtiff.so.[0-9].* /usr/lib32/libtiff.so.3 >> $LOGFILE 2>&1
    fi

    if [ "`echo yes|awk \"{if ($UBUNTUCODEOS > 11.10) print $1}\"`" == "yes" ];then
        echo -e "\n\033[1;35mInstalling Ubuntu 12.04+ specific Packages\033[0m"
        apt-get -y install libpopt0:i386 libtiff4:i386
    fi 
elif [ `uname -m` = i686 ] ; then
    if [ -e /lib/libpng12.so.[0-9].* ] ; then
        echo 'Creating a Symbolic Link to libpng12 /lib/libpng.so.3'
        ln -sf /lib/libpng12.so.[0-9].* /lib/libpng.so.3 >> $LOGFILE 2>&1
    fi
    if [ -e /usr/lib/libpng12.so.[0-9].* ] ; then
        echo 'Creating a Symbolic Link to libpng12 /usr/lib/libpng.so.3'
        ln -sf /usr/lib/libpng12.so.[0-9].* /usr/lib/libpng.so.3 >> $LOGFILE 2>&1
    fi
    if [ -e /usr/lib/libtiff.so.[0-9].* ] ; then
        echo 'Creating a Symbolic Link to libtiff /usr/lib/libtiff.so.3'
        ln -sf /usr/lib/libtiff.so.[0-9].* /usr/lib/libtiff.so.3 >> $LOGFILE 2>&1
    fi
else
    ARCHITE=$(uname -m)
    echo -e "\nI apologize though at this time I have not devised a way to support your architecture
Your system is running \"$ARCHITE\". However you can continue this installation
\033[1;34mBut I Can't Guarantee that its going to work.\033[0m\n\nIf you want to continue the installation 
type \"yes\" and press enter. To quit type anything else and press enter." 
    read -p "Do you want to continue? : " ARCHITEFAIL
    case "$ARCHITEFAIL" in
        yes | YES | Yes )
        echo -e "We are going into undeveloped territory\ngood luck and let me know what happens.\nPlease send results to info@bkintegration.com"
        ;;
        *)
        echo -e "Better safe than sorry right..."
        EXITERROR
        ;;
    esac
fi

echo -e "\033[1;31mInstalling Built package\033[0m"
su -c 'dpkg -i cups-bjnp_*.deb'

if [ -d ./debs ];then 
    echo -e "\n\033[1;31mInstalling Printer Packages\033[0m"
    su -c 'for PACKAGES in $(ls ./debs/*.deb); do gdebi -n $PACKAGES; done' >> $LOGFILE 2>&1
else
    echo -e "You are running this script from a directory that does not contain the required dependencies. You are here \"$(pwd)\""
    EXITERROR
fi

if [ "`echo yes|awk \"{if ($KERNELNUM >= 3.0) print $1}\"`" == "yes" ];then
    if [ -f /etc/udev/rules.d/80-canon_mfp.rules ];then
        echo '' 
        echo -e "\033[1;35mModifying the UDEV Rules for the printer Driver to accomodate the UDEV Rule Changes.\033[0m"
        echo "Thank you 'Nero Ubuntu' for finding this."
        sed -i 's/SYSFS/ATTR/g' /etc/udev/rules.d/80-canon_mfp.rules
    fi
fi

echo -e "\n\033[1;31mRestarting CUPS\033[0m"

if [ -f /etc/init.d/cups ];then 
    su -c '/etc/init.d/cups restart' >> $LOGFILE 2>&1
fi

if [ -d ./ppd ];then 
    echo -e "\n\033[1;31mMoving Universal PPD file into place for Use\033[0m"
    su -c 'cp ./ppd/canonmp620-630_Universal.ppd /usr/share/ppd/canonmp620-630_Universal.ppd'
    su -c 'cp ./ppd/canonip1800_Universal.ppd /usr/share/ppd/canonip1800_Universal.ppd'
else
    echo "You are running this script from a directory that does not contain the required dependencies."
    EXITERROR
fi

su -c 'service cups restart' >> $LOGFILE 2>&1

echo -e "\n\nEnter the IP address for the printer. If you do not know it or if 
you are using a dynamic IP address leave it Blank. This entry has been shown 
to improve the speed of the scanner. ( Thank you Pierre Chauveau http://free.fr )"

echo -e "\nAfter your entry press [ Enter ] to continue."
read -p "IP Address : " PRINTERIP

echo "# *** # Printer IP Address # *** #
${PRINTERIP}" >> /etc/hosts

echo -e "\n\033[1;35m********* READ THIS AND PAY ATTENTION *********\033[0m"
sleep 1

if [[ $(which system-config-printer) ]]; then
    system-config-printer > /dev/null 2>&1
    echo -e 'The Printer configuration and Setup window should now Open
if it does not open, open it. Once opened add your printer.
The Canon Printers that you have installed should show up.
Select the printer that has been found and click Next
The Printer configuration should recognize the Printer
Drivers should be automatically found and guild you through the install.
Once you have finished the setup from the Printer window close the window.
This will conclude the installer.\n'
sleep 1
else
    echo -e "Please go and open your Printer Configuration window. 
When you are in Printer Config, you will need to select BJNP printer protocol.
Some users have reported that in the 64Bit, x86_64, installation they have 
had to input the IP address of the printer in the “Canon Networking field”
of the Printer install. If your system is not detecting the Canon Printer 
in the detected Printers field you can input the IP address manually\n"
sleep 1
fi

echo -e "* Select Canon Networking type
* Window Opens Allowing you to input the Printer protocol, IP and port
* Sample Input: bjnp://X.X.X.X:8611 ( Replace \"X\" printers IP )
* Click forward through the menu and make sure you select the Canon Driver
* Name is \"MP620-630 Bkintegration ver.4.0 Universal\" Driver.
* Complete the installation and print a test page.\n"

echo -e "\033[1;35m********* READ THIS AND PAY ATTENTION *********\033[0m"
read -p "Press [ Enter ] to Continue"

echo -e "A log file has been placed built at $LOGFILE
Please refer to this LOG file for any Installation Errors
This script was created by Kevin Carter For More information
Or if you have questions or comments on this installation
Please let me know, I want to here from you...
Goto http://rackerua.com\n"

exit 0
