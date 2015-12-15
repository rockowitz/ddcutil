ddctool
=======

ddctool is a program for querying and changing monitor settings, such as brightness and color levels.   

ddctool uses DDC/CI to communicate with monitors implementing MCCS (Monitor Control Command Set) over I2C.  Normally, the video driver for the monitor exposes the I2C channel as devices named /dev/i2c-n.  

##ddctool Commands
| Command | Function
|-----------------------------------------| ----------------------------------------------------------------------|
| detect                                        | report monitors detected |
| capabilities                                 | report a monitor's capabilities string |
| listvcp                                        | list VCP features codes that ddctool knows how to interpret
| getvcp feature-code-or-group   | report a single VCP feature value, or a group of values  
| setvcp feature-code new-value | set a single VCP feature value
| dumpvcp filename                     | save color related VCP feature values in a file
| loadvcp filename                       | set color related VCP feature values from a file 
|environment                               | explore the `ddctool` installation environment
|interrogate                                 | collect maximal information for problem diagnosis

There is an extensive set of options for tailoring *ddctool* operation.   Some are described in this readme.   For a full list, issue the command: 

~~~
ddctool --help
man 1 ddctool
~~~

## Feedback Needed
This is a preliminary release.

Does it work with given device, monitor
- not very concerned with MCCS < 2
- experience with MCCS 3
- does it properly read tables

I2C is an inherently unreliable protocol, requiring retry management.    There is variation in MCCS interpretation.   I2C implementation can vary with card, monitor, and driver.   



Documentation:  I have copies of MCCS specification versions 2.0 (10/17/2003) and 3.0 (6/27/2006) .  I would appreciate pointers to other versions of the specification, i.e. versions 1.0 (9/11/1998) and 2.1 (5/28/2005).

### ddctool as part of profile management
A particular use case for ddctool is as part of profile management.  Monitor calibration is relative to the monitor color settings currently in effect, e.g. red gain.  ddctool allows color related settings to be saved at the time a monitor is calibrated, and then restored when the calibration is applied.

~~~
ddctool dumpvcp
ddctool loadvcp
~~~
### Instrumentation and Tuning

   --ddc

Report I2C communication and DDC protocol errors.

    --stats
ddctool maintains extensive statistics on protocol errors, retry counts, and performance.   This option causes those statistics to be reported.

### Monitor specification

If more than one monitor is attached, the desired monitor can be specified using any of the following options:
~~~
--display <display number>
--busno <i2c bus number>
--adlno <iAdapterNumber>.<iDisplayNumber>
--edid <256 character hex string>
--model <model name> and --sn <serial number>
~~~

Notes:
- Monitors under control of AMD's proprietary driver (fglrx) by adapter number and display number.  These numbers are specified on the --adlno option separated by a period, e.g. "--adlno 1.0"
- If model and serial number are used to identify the monitor, both options must be specified.

To see a list of all attached monitors and their associated identifiers:
~~~
ddctool detect
~~~


## Building ddctool

ddctools requires the i2c-tools package.  i2c-tools appears to be packaged in different ways in different distributions, so only general guidelines can be given.  If any of the following packages exist for your distribution, they should be installed: 

- i2c-tools
- libi2c-dev 

If using an open source driver, ensure that kernel module i2c-dev is loaded.

### /dev/i2c permissions

In order to use ddctool, you must be able to write to /dev/i2c-*.  Again, because of the variation among distributions, only general guidelines can be given.

Some versions of i2c-tools create group i2c, and make that the group for /dev/i2c-* devices.  In that case all that is necessary is to add your user name to group i2c: 

    sudo adduser your-user-name i2c

To give everyone permission to write to /dev/i2c-* for the current boot:
    sudo chmod a+rw /dev/i2c-*

See resources/etc/udev/rules.d






### Comparison with ddccontrol

The program ddccontrol  

- Uses i2c-dev userspace interface to i2c.  Should be less fragile
- Probably reflecting the time it was written, ddccontrol relies on a monitor attribute database to interpret VCP code.


### Building with ADL support

Special consideration is required if using AMD's proprietary driver (fglrx).  This driver does not expose /dev/i2c-* devices.  Instead, the driver provides I2C communication with the montior through its API.

The license for the AMD Device Library (ADL) SDK library allows for incorporating its components in distributed executables.   The downloadable executable is built with ADL support.   

However, the ADL license does not allow for redistribution of its files.  Consequently, if you build ddctool from source, by default it will not support monitors connected to a card using AMD's proprietary driver. 

To build a version of ddctool that supports fglrx connected monitors, do the following:

- Download the ADL sdk (currently at http://developer.amd.com/tools-and-sdks/graphics-development/display-library-adl-sdk/)
- Extract the following files (they will be found in various subdirectories).  Copy them to some directory on your system: 
~~~
 adl_defines.h
 adl_sdk.h
 adl_structures.h
 mccs.h
~~~
 
- When building ddctool, execute configure as follows
~~~
configure --with-adl-headlers=DIR
~~~
where DIR is the name of the directory when you saved the ADL header files.


### Installing ddctool

ddctool requires:

- 
installing,
making /dev/i2c-n accessible

To run ddctool you need write access for /dev/i2c-tools:

For testing, a simple way to do this is:
```
sudo chmod a+rw /dev/i2c-*




### Special Nvidia driver settings

When using Nvidia's proprietary driver, I2C communication fails on some cards.   It worked on several older Nvidia cards I have, but failed with my newer GTX660Ti.  Others have reported similar problems.   (Specfically, I2C reads/writes? of 1 or 2 bytes succeeded, but reads/writes of 3 or my bytes failed.)   Adding the following to the "Device" section for the Nvidia driver:
~~~
 Option     "RegistryDwords"  "RMUseSwI2c=0x01; RMI2cSpeed=100"
~~~
 A file for making this change is 50-nvidia_i2c.conf found in the /resources directory.  
~~~
Section "Device"
   Driver "nvidia"
   Identifier "Dev0"
   Option     "RegistryDwords"  "RMUseSwI2c=0x01; RMI2cSpeed=100"
   # solves problem of i2c errors with nvidia driver
   # per https://devtalk.nvidia.com/default/topic/572292/-solved-does-gddccontrol-work-for-anyone-here-nvidia-i2c-monitor-display-ddc/#4309293
EndSection
~~~
Copy this file to /etc/X11/xorg.conf.d  

Note: This file works if there is no xorg.conf file.  If you do have an xorg.conf file and Identifier field will likely require modification.



Requirements:
package i2c-tools is required

## To Do

The following tasks are planned before initial release
- downloadable tarball
- test 32 bit version
- provide for cross-compiling 32 bit executables on 64 bit systems
- Ubuntu ppa 
- Fedora Copr?
- OpenSUSE build service?



## Future Development
Possible further extensions:

- UI
- package as library for use by other C programs
- Python API


