MP 620 - 630 | Debian Based Univeral Installer
##############################################

This should Work on ALL Debian Based Systems

This Installer has been built for x86 and x86\_64. The Installer has been tested on :
  * Ubuntu 8.04 - 12.04
  * Debian 5 - 6 
  * Mint 10 - 12.

  The Universal install has been updated to Version 5.0 and now provides a function to clean up old packages, additionally there is no more compiling, the installer uses all precompiled DEBS which will allow for easy upgrading later as well as better capabillity to uninstall if need be.

  As with my previous post, this article will help you install the Canon MP620 and or MP630 Printer on a Debian Based Linux machine. What differentiates this HOW-TO from the others is that this is a place to download an installer script to allow you to install the printer with minimal effort and by minimal I mean that the Printer in normally functional in around 15 minutes. 

This Installer works on BOTH 32bit and 64bit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

I developed this script to help ease the installation and transition from Windows to Linux, and to allow people to get their Linux systems running with the least amount of effort.

! Upgrade Process !
-------------------

If you are upgrading to your installation of the Printer drivers / PPD all you now have to do is install the new version. The script will do all of the package purging and PPD removal for you everything else is automatically taken care of.

--------------

Installing the printer
^^^^^^^^^^^^^^^^^^^^^^

The first thing that you need to do is to open a terminal. Go to from the terminal enter the following commands.

.. code-block:: bash

   sudo su -

You will need to go to where you downloaded the repository, once there tell your system that the file is Executable

.. code-block:: bash

    sudo chmod +x install.sh

Now all you need to do is run the installer

.. code-block:: bash

    ./install.sh

If you know your ROOT users password and do not feel like using SUDO, you can simply switch to root for the install of the script. 

.. code-block:: bash

    su -c 'bash ./install.sh'

The installer will take around 5 - 15 minutes to complete. Once Completed you will have to go to your system printer configuration screen.  You should have the canon BJNP protocol available. Click the network discovered Printer, either Canon MP620 or 630 Allow the configuration window to search the for the Drivers The Installer knows what the printer is, if you choose to change the name you can feel free, but the name is auto prompted. Finish the installer and print the test page and close the Printer Configuration Window Upon Competion you will have the ability to print and Scan via USB and or Networking.

--------------

Scanning from the MP620 - 630
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

After Completing the installation your printer will have been setup
and configured for Printing as Well as Scanning.

To user your scanner you have to activate the scan process from the
Linux Host system.

***Your Printer will not see the Linux System from the On-Printer-Menues***

Here is the Simple Scan Process

Open Simple Scan, this can be located in the Graphics Menu, If you
do not have it already you can find Simple Scan in the software
center.
Once Simple Scan is open click Document >> Preferences
Make Sure you see the Canon MP620 Printer Selected, if it is not
selected Select it.

Choose the settings that you want and then click close
Now go to the printer and load a document on the scanning bed
Go back to the Linux System and click the Scan Button
**That is IT!**

--------------

Supplementary Information
-------------------------

Some users have reported that in the 64Bit, x64, installation they
have had to input the IP address of the printer in the "Canon
Networking field" of the Printer install. If your system is not
detecting the Canon Printer in the detected Printers field you can
input the IP address manually

Select Canon Networking type 
  * A bar will open up and allow you to input the Printer protocol, IP and port
  * The input should look like: ``bjnp://X.X.X.X:8611`` (*Replace "X" with the IP address of the Printer*)
  * Now click forward through the menu and make sure you select the Canon Driver set with the "MP630 ver.3.00" Driver. Complete the installation and print a test page.

--------------

Hope this helps
---------------

If this helps you out please let me know and post a comment. I love hearing from you guys.
