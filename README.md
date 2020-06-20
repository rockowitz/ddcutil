ddcutil
=======

**ddcutil** is a Linux program for querying and changing monitor settings, such as 
brightness and color levels.

Most mointors, other than lapdop displays, have a Virtual Control Panel (VCP), 
which implements features defined in the Montor Control Command Set (MCCS).
Typically, **ddcutil** communicates with the monitor's VCP over an I2C bus, as per 
the Display Data Channel/Command Interface Standard (DDC/CI).

Alternatively, some monitors (e.g. Eizo ColorEdge, Apple Cinema) provide a USB
interface to the VCP, as described in the USB Monitor Control Class Specification.
**ddcutil** can communicate with these monitors over USB instead of I2C. 

A particular use case for **ddcutil** is as part of color profile management. 
Monitor calibration is relative to the monitor color settings currently in effect, 
e.g. red gain.  **ddcutil** allows color related settings to be saved at the time 
a monitor is calibrated, and then restored when the calibration is applied.

The tarball or github project builds both command line (**ddcutil**) and shared
library (**libddcutil**) executables. The command line executable does not depend on
the shared library.

For detailed information about **ddcutil**, see the project website: www.ddcutil.com. 

In particular, for instructions on building **ddcutil**, see www.ddcutil.com/building. 

Once **ddcutil** is installed, online help is also available.
Use the --help option or see the man page:
~~~:
$ ddcutil --help
$ man 1 ddcutil
~~~

References to the relevant specifictions can be found at www.ddcutil.com/bibliography. 

### Installation Diagnostics

If **ddcutil** is successfully built but execution fails, command `ddcutil environment` 
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
