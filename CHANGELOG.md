# Changelog

## [1.2.3] 2022-05-13

### Changed
- Option ***--force-slave-address*** no longer has any effect. The dev-i2c 
  ioctl() interface is now used exclusively instead of write() and read()
  calls for writing to and reading from the I2C bus. As a result, 
  ioctl(SLAVE_ADDRESS), which has been the source of EBUSY errors from driver
  i2-dev, is no longer used. In principle, EBUSY errors are still possible from
  within individual video drivers, but this has never been observed.
- Sleeps immediately after opening a /dev/i2c device and after completion of a
  read operation are completely eliminated. The sleep-suppression related 
  uptions, ***--sleep-less***, ***--less-sleep, ***--enable-sleep-suppression***,
  and ***--disable-sleep-suppression*** no longer have any effect.
- Option ***--dca***: The Dynamic Sleep Adjustment algorithm was reqritten to 
  more sensibly increment sleep times after before each retry. 
- Commands **getvcp** and **vcpinfo**: 
  - Allow specification of multiple feature codes, for example 
    ***ddcutil getvcp 10 12*** , ***ddcutil vcpinfo 14 16 18 1a***
- Command **detect**: 
  - Option ***--verbose*** produces addtional information: 
   - The product code is reported in hex as well as decimal
   - The EDID source field is set to **I2C** in the normal case where the EDID
     is read directly from slave address X50.  Alternative values include 
     **USB**, **X11**, and **SYSFS**.
- Command **environment**:
   - Scanning of /sys by option ***--verbose*** has been improved.
   - Add msg re SYSENV_QUICK_TEST environment variable
- More user friendly messages at startup regarding /dev/i2c buses that cannont
  be opened.  If the problem is inadequate permissions (EACCES), the user is 
  directed to www.ddcutil.com/permissions.
- Better handle malformed EDIDs
  - Trailing blanks on model and serial number a stripped.  This affects 
    commands **detect --terse**, **loadvcp** and **dumpvcp**, and also the 
    file names of user defined features.
- Source code has been extensively cleaned up. In particular, directory **adl**
  containing code for the old proprietary AMD video driver, has been removed.
- Building ddcutil:
  - Library **libi2c.so** is no longer linked.  It was needed only for some
    experimental code.
libddcutil: 
- ddca_get_display_refs(), ddca_get_display_info_list2(): 
  - Open errors can be retrieved using ddca_get_error_info(). 
    Note that the API calls still succeed.

### Fixed
- The sleep multiplier value was not respected for new API threads.
- User Defined Features: Keyword **NC** set the incorrect flag in a feature
  descriptor.
- Do not use glib function g_byte_array_steal(), which requires glib 2.60.
  ddcutil requires only glib 2.40. 
- Miscellaneous memory leaks

## [1.2.2] 2022-01-22

### Added
- API function ddca_enable_force_slave_address()
- API function ddca_is_force_slave_address_enabled()

### Changed
- Improve handling of and messages regarding DDC communication failures with 
  errno EBUSY. In particular, this error occurs when driver ddcci is loaded.
  - Command **detect**: If DDC communication fails with error EBUSY, report the
    display as "Busy" instead of "Invalid" and suggest use of option 
    ***--force-slave-address***.
  - Command **environment**: Suggest use of option ***--force-slave-address*** 
    if driver ddcci is detected.
  - Messages re EBUSY errors are always written to the system log.
- Command **detect**:
  - Do not report the EDID source unless there is a value to show.
    This value is set only for USB connected monitors.
  - Show extended output based on option ***--verbose***, not undocumented
    option ***--very-verbose***.
  - Report color bit depth if EDID version >= 1.4
- Command **environment**: Simplify the exploration of sysfs.
- API changes:
  - Field latest_sl_values in struct DDCA_Feature_Metadata struct is no 
    longer set,
  - API function ddca_report_display_info(): include binary serial number
- Building and porting:
  - When building ddcutil, allow for building a static library if **configure** 
    option ***--enable-static*** is set. Linux distributions frown on packaging 
    static libraries, but if a user wants to build it who am I to judge. 
    By default, static libraries are not built,
  - Replace use of Linux specific function **__assert_fail()** with **exit()**
    in traced assertions.  **__assert_fail** is used in the Linux implementation
    of **assert()**, but is not in the C specification.  This can present a 
    problem in porting ddcutil. 
- Code cleanup:
  - Delete incomplete, experimental code for asynhronous feature access, 
    including files src/ddc/ddc_async.c/h. 
  - Remove unused files src/util/output_sink.c/h.

### Fixed
- Only write Starting/Terminating messages to the system log if option 
  ***--syslog*** is specified.
- Avoid compilation warnings when assert() statments are disabled (NDEBUG is
  defined).
- Fixed a segfault in the debug/trace code of ddca_get_display_refs()
- Memory leaks.

## [1.2.1] 2021-11-15

### Added
- Option ***--syslog***: Send trace and debug messages to the system log in
  addition to the trace location.
- Option ***--wall-timestamp***, ***--wts***: Prefix trace and debug messages
  with the current wall time.
- Option ***--settings***: Report option settings in effect.

### Changed
- Details of current settings are no longer reported by every command invocation
  when option ***--verbose*** is specified.  Use option ***--settings*** to 
  control option reporting.
- Removed sample program demo_watch_displays.

### Fixed
- Numerous memory leaks, in particular ones triggered by ddca_redetect_displays(). 
- Build failure if configure option ***--enable-x11=no*** was specified.
- API functions ddc_open_display(),ddc_open_display2() now always return 
  DDCRC_ALREADY_OPEN if the the display is already open in the current thread.
  Previously an assert() failure would occur under certain circumstances.
- Options ***--disable-capabilities-cache***, ***--disable-udf*** not respected
- Proof of concept code that watches for display hotplug events 

## [1.2.0] 2021-09-28

### Added
- libddcutil log file
- libddcuti and ddcutil write critical events to syslog
- API function ddca_add_trace_group()
- API function ddca_extended_version_string()
- API function ddca_redetect_displays()
- API function ddca_get_display_refs()
- API function ddca_get_display_info()
- API function ddca_free_display_info()
- Macro DDCUTIL_VSUFFIX

### Changed
- If possible, command **ddcutil environment --verbose** calls **get-edid|parse-edid** 
  as an additional EDID check.
- Additional validation of DDCA_Display_Ref and DDCA_Display_Handle arguments to API functions
- Improved tracing of assert() failures
- --enable-capabilities-cache is now the default
- libddcutil name is now libddcutil.so.4.1.0
- Command **detect**: improved analysis of /sys
- Command **detect**: ***--verbose*** option reports raw EDID
- Option ***--help*** does not report undocumented option ***--very-verbose***.

### Fixed
- Incorrect assembly of sysfs path definitions in **ddcutil environment --verbose** 
- ddcutil diagnostics were not finding module i2c-dev if the system (e.g. NixOS) 
  used a non-standard location for the modules directory (Issue #178). The checks 
  have been rewritten to use libkmod.
- Eliminate repeated messages from the experimental display hotplug detection code
  if no /sys/class/drm/cardN devices exist. (libddcutil)

## [1.1.0] 2021-04-05

For details, see [ddcutil Release Notes](https://www.ddcutil.com/release_notes).

### Added
- Configuration file **ddcutilrc**, located on the XDG config path.   
- Cache monitor capabilities strings to improve performance of the **capabilities** command.  
  Controlled by options ***--enable-capabilities-cache***, ***--disable-capabilities-cache***.
- Workarounds for problems in DRM video drivers (e.g. i915, AMDGPU) when displays are connected to 
  a docking station. The same monitor can appear as two different /dev/i2c devices,
  but only one supports DDC/CI.  If possible these are reported as a "Phantom Display" instead 
  of as "Invalid Display". Also, try to work around problems reading the EDID on these 
  monitors, which  can cause the monitor to not be detected.
- Option ***--edid-read-size 128*** or ***--edid-read-size 256*** forces **ddcutil** to request
  that number of bytes when reading the EDID, which can occasionally allow the EDID to
  be read successfully. 
- Issue warning at startup if driver i2c-dev is neither loaded nor built into the kernel.

### Changed
- By default, files generated by **dumpvcp** are saved in the XDG_DATA_HOME directory.
- **environment --verbose** has more detailed reporting of relevant sections of /sys.
- Additional information on **detect --verbose**.
- Additional functions are traceable using option ***--trcfunc***
- User defined features are enabled by default.

### Fixed
- Regard IO operations setting errno EBUSY as recoverable, suggest use of option 
  ***--force-slave-address***.  (EBUSY can occur when ddcontrol's ddcci driver 
  is loaded.)
- Fix build failure when configure option ***--disable-usb*** is combined with 
  ***--enable-envcmds***.
- On AMD Navi2 variants, e.g. RX 6000 series, **ddcutil** display detection put
 the GPU into an inconsistent state when probing a SMU I2C bus exposed by the GPU. 
 This change ensures that **ddcutil** does not attempt to probe such buses. 

