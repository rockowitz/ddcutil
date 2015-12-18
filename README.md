ddctool
=======

`ddctool` is a program for querying and changing monitor settings, such as brightness and color levels.   

`ddctool` uses DDC/CI to communicate with monitors implementing MCCS (Monitor Control Command Set) over I2C.  Normally, the video driver for the monitor exposes the I2C channel as devices named /dev/i2c-n.  

A particular use case for `ddctool` is as part of profile management.  Monitor calibration is relative to the monitor color settings currently in effect, e.g. red gain.  ddctool allows color related settings to be saved at the time a monitor is calibrated, and then restored when the calibration is applied.

## Feedback Needed
This is a preliminary release, and feedback would be very helpful.   The build environment can vary from system to system.   I2C implementation can vary with card, monitor, and driver.    There is variation in MCCS interpretation.  Furthermore, I2C is an inherently unreliable protocol, requiring retry management.  

In particular: 

- Were you able to build `ddctool`?  What changes were required to the Autoconf files?   
- Does it work with given card, driver, and monitor?  I'm not particularly concerned with older monitors whose MCCS version is unspecified (i.e. is less than 2.0).  On the other hand, the I'm very interested in how `ddctool` handles monitors implementing MCCS V3.0, as the V3.0 specific code has not been tested.  In particular, does `ddctool` property read Table type features. 
- And of course, is the program useful?   Does it merit further development?  What features does it need?

Command `ddctool interrogate` collects maximal information about the installation environment, video card and driver, and monitor capabilities.   I'd appreciate it if you could redirect its output to a file and send the file to me. This will help diagnose problems and identify features that should be implemented.

Finally, I am looking for for additional MCCS documentation.   I have copies of MCCS specification versions 2.0 (10/17/2003) and 3.0 (6/27/2006) .  I would appreciate pointers to other versions of the specification, i.e. versions 1.0 (9/11/1998),  2.1 (5/28/2005), and 2.2 (1/19/2009). 

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

There is an extensive set of options for tailoring *ddctool* operation.   Some are described in this readme.   For a full list, use the --help option or see the man page:
~~~
ddctool --help
man 1 ddctool
~~~

### Monitor Selection

If more than one monitor is attached, the desired monitor can be specified using any of the following options:
~~~
--display <display number>
--bus <i2c bus number>
--adl <iAdapterNumber>.<iDisplayNumber>
--edid <256 character hex string>
--model <model name> and --sn <serial number>
~~~

Notes:
- Monitors under control of AMD's proprietary driver (fglrx) are selected by adapter number and display number.  These numbers are specified on the --adl option separated by a period, e.g. "--adlno 1.0"
- If model and serial number are used to identify the monitor, both options must be specified.

To see a list of all attached monitors and their associated identifiers:
~~~
ddctool detect
~~~

### Instrumentation and Tuning

I2C is an inherently unreliable protocol, requiring retry management.  90% of its elapsed time is spent in timeouts mandated by the DDC specification.    `ddctool` has extensive facilities for reporting protocol errors, retry counts, and peformance statistics, and some ability to tweak execution parameters from the command line.

The relevant options are:

| Option | Function
|----------------------------------| ----------------------------------------------------------------------|
| --stats                               | report execution statistics |
| --ddc                                 | report DDC protocol errors |
|--maxtries()                       | sets maximum tries | 


There are 3 kinds of exchanges in which retry is possible: 

- write-only exchange.  Bytes are written with no subsequent read.  Used only to set a VCP feature value.  
- write-read exchange.  A write to the monitor, followed by a read.  Most DDC protocol exchanges are of this form.
- multi-part exchange.  This is a "meta" exchange, consisting multiple write-read exchanges.  Use to query monitor capabilities, and for querying and setting Table type VCP features. 

By default, the maximum number of tries for each exchange is:

- write-only exchange:    4
- write-read exchange: 10
- multi-part exchange:   8

Option --maxtries allows you to play with the maximum try count.  Its argument consists of 3 comma-separated values.  The following example sets the maximum try counts to 3 for write-only exchanges, 6 for write-read exchanges, and 9 for multi-part exchanges.

--maxtries(3,6,9) 

A blank value leaves the corresponding try count unchanged.   The following example changes only the maximum write-read try count:

--maxtries(,7,) 

The maximum maximum value is 15.

## Building and Running ddctool

Because of the variation among distributions, only general guidelines can be given for some `ddctool` prerequisites.

### Packages

`ddctool` requires the following package for both building and execution:

- i2c-tools

At least on Ubuntu, the i2c.h header file is found in a separate package.   If the following package exists, it
is needed to build `ddctool`

- libi2c-dev 

### /dev/i2c permissions

Except when using AMD's proprietary driver (see below) `ddctool` requires write access to /dev/i2c-*.  

Some versions of i2c-tools create group i2c, and make that the group for /dev/i2c-* devices.  In that case all that is necessary is to add your user name to group i2c: 

    sudo usermod your-user-name -G i2c

For testing, it may be simpler to give everyone permission to write to /dev/i2c-* for the current boot:
    sudo chmod a+rw /dev/i2c-*

See resources/etc/udev/rules.d

The following section from the udev documentation 
(<https://www.kernel.org/pub/linux/utils/kernel/hotplug/udev/udev.html>) may be helpful:

> The udev rules are read from the files located in the system rules directory /usr/lib/udev/rules.d, the volatile runtime directory /run/udev/rules.d and the local administration directory /etc/udev/rules.d. All rules files are collectively sorted and processed in lexical order, regardless of the directories in which they live. However, files with identical file names replace each other. Files in /etc have the highest priority, files in /run take precedence over files with the same name in /lib. This can be used to override a system-supplied rules file with a local file if needed; a symlink in /etc with the same name as a rules file in /lib, pointing to /dev/null, disables the rules file entirely.

### Kernel Modules

If using an open source driver, kernel module i2c-dev must be loaded.

TODO: discuss modprobe

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


### Special Nvidia driver settings

When using Nvidia's proprietary driver, I2C communication fails on some cards.   It worked on several older Nvidia cards I have, but failed with my newer GTX660Ti.   (Specfically, I2C reads of 1 or 2 bytes succeeded, but reads of 3 or more bytes failed.)  Others have reported similar problems.   Adding the following to the "Device" section for the Nvidia driver:
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

### Installation Diagnostics

If `ddctool` is successfully built but execution fails, command `ddctool environment` probes the I2C environment and may provide clues as to the problem.


## Comparison with ddccontrol

The program `ddccontrol` appears to no longer be maintained.    It has a fragility reflecting the environment at the time it was written.   In particular:

- `ddcctontrol`, at least some of the time, uses its own I2C driver code.  `ddctool`, on the other hand, relies exclusively on the the i2c-dev userspace interface to i2c. (And also, ADL for fglrx).   This should make it less fragile.
- `ddccontrol` relies on a monitor attribute database to interpret VCP code.  With MCCS 2.0 and greater, VCP feature code definitions are largely standardized.  `ddctool` uses the MCCS specification to interpret VCP feature values.  It makes no attempt to interpret values for feature codes designated as manufacturer specific (E0..FF). 

## To Do

This is a preliminary release.  There's an extensive TODO list

- installation without local build:
-- downloadable tarball
-- Ubuntu ppa, Fedora Copr, OpenSUSE build service
- cross-compile 32 bit executables on 64 bit systems
- write VCP Table type fields
- document the format of .VCP files (i.e. files read by `ddctool loadvcp`)
- UI
- package as library for use by other C programs, probably with gobject introsepection
-- Python API

## Author

Sanford Rockowitz  <rockowitz@minsoft.com>
