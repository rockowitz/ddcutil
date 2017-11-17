ddcutil
=======

ddcutil is a program for querying and changing monitor settings, such as 
brightness and color levels.   

ddcutil uses DDC/CI to communicate with monitors implementing MCCS 
(Monitor Control Command Set) over I2C.  Normally, the video driver for the
monitor exposes the I2C channel as devices named /dev/i2c-n.  There is also
psupport for monitors (such as Apple Cinema and Eizo ColorEdge)
that implement MCCS using a USB connection. 

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

## Feedback Needed

Many aspects of ddcutil can vary from system to system.  In particular:

- The build environment can vary. 
- I2C implementation can vary with card, monitor, and driver.  
- There is variation in MCCS interpretation.  
- I2C is an inherently unreliable protocol, requiring retry management.  

Given the wide variability, it is not possible to test all combinations. 
Some questions that arise:

- Were changes required to build ddcutil?
- Does it work with given card, driver, and monitor?  I'm not particularly 
  concerned with older monitors whose MCCS version is unspecified (i.e. is 
  less than 2.0).  On the other hand, I'm very interested in how ddcutil
  handles monitors implementing MCCS V3.0, as the V3.0 specific code has not 
  been tested. In particular, does ddcutil properly read Table type features? 
- What features should be added? 

Command `ddcutil interrogate` collects maximal information about the 
installation environment, video card and driver, and monitor capabilities.   
I'd appreciate it if you could redirect its output to a file and send the file
to me. This will help to document the variability of monitor DDC/CI implementations,
diagnose problems, and identify features that should be implemented.


### Installation Diagnostics

If ddcutil is successfully built but execution fails, command `ddcutil environment` 
probes the I2C environment and may provide clues as to the problem.


## Author

Sanford Rockowitz  <rockowitz@minsoft.com>
