# Changelog

## [1.2.2] 2022-01-16

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
- Command **environment**: Simplify the exploration of sysfs.
- When building ddcutil, allow for building a static library if **configure** 
  option ***--enable-static*** is set. Linux distributions frown on packaging 
  static libraries, but if a user wants to build it who am I to judge. 
  By default, static libraries are not built,
- Delete incomplete, experimental code for asynhronous feature access, 
  including files src/ddc/ddc_async.c/h. 
- Remove unused files src/util/output_sink.c/h.
- Replace use of Linux specific function **__assert_fail()** with **exit()** in
  traced assertions.  **__assert_fail** is not in the C specification but is 
  part of the Linux implementation of assert(), and can present a problem in 
  porting ddcutil.
- API function ddca_report_display_info(): include binary serial number

### Fixed
- Only write Starting/Terminating messages to the system log if option 
  --syslog is specified.
- Avoid compilation warnings when assert() statments are disabled (NDEBUG is
  defined).
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

