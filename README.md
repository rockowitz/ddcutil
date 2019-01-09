ddcutil
=======

ddcutil is a program for querying and changing monitor settings, such as 
brightness and color levels.   

ddcutil uses DDC/CI to communicate with monitors implementing MCCS 
(Monitor Control Command Set) over I2C.  Normally, the video driver for the
monitor exposes the I2C channel as devices named /dev/i2c-n.  There is also
psupport for monitors (such as Apple Cinema and Eizo ColorEdge) that implement 
MCCS using a USB connection and conform to the VESA USB Monitor Control Class Specification. 

A particular use case for ddcutil is as part of color profile management. 
Monitor calibration is relative to the monitor color settings currently in effect, 
e.g. red gain.  ddcutil allows color related settings to be saved at the time 
a monitor is calibrated, and then restored when the calibration is applied.

For detailed instructions on building and using ddcutil, see the website: 
www.ddcutil.com. 

In particular, for information on building ddcutil, see www.ddcutil.com/building. 

Once ddcutil is installed, online help is also available.  
Use the --help option or see the man page:
~~~:
ddcutil --help
man 1 ddcutil
~~~

### Installation Diagnostics

If ddcutil is successfully built but execution fails, command `ddcutil environment` 
probes the I2C environment and may provide clues as to the problem.

### User Support

Please direct technical support questions, bug reports, and feature requests to the
[Issue Tracker](https://github.com/rockowitz/ddcutil/issues) on the github repository.
Use of this forum allows everyone to benefit from individual questions and ideas.

When posting questions regarding **ddcutil** configuration, please execute the following command,
capture its output in a file, and submit the output as an attachement.

~~~
$ ddcutil interrogate 2>&1
~~~

For further information about technical support, see http://www.ddcutil.com/support.

## Author

Sanford Rockowitz  <rockowitz@minsoft.com>
