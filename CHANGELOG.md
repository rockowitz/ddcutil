## [2.2.2] 2025-09-xx

#### Added

- Option ***--trcback***: report call stack that led to specified function
  (initial implementation)
 - Option ***--trace-to_syslog***

#### Changed

implement --watch-mode udev

better error messages if invalid config file, validate section names/key names

#### Fixed

- Command **environment --verbose**: incorrectly formed path name for examining 
  /sys/class/drm
- Out-of-tree build reference to generated file /src/base/build_details.h. 
  Pull request #544
- Command **getvcp --verbose**: output was partially in format intended for syslog
- Command **getvcp --verify** and API function **ddca_set_non_table_vcp_value()**: 
  were not performing verification
- **ddca_display_ref_from_handle()**: was not converting internal to external display ref

- Relax the check of device class when checking if a device is a video controller
  Look only at the first byte.  An AMD Ryzen AI 9 365 based system was seen to 
  report 0x038000, not 0x030000.  Addresses issue # 539?  #530?

- **ddca_redetect_displays()**: return DDCRC_INVALID_OPERATION if built without display 
fails to build with ***--disable-drm***
  configure option that cuases***--disable-watch-displays*** 
    Addresses issue #506

    Commit 2ed9275 in branch 2.2.2-dev allows building to complete without the undefined reference error when configure option --disable-drm was specified. API function ddca_redetect_displays() returns DDCA_INVALID_OPERATION in this case. (An alternative code path through ddca_redetect_displays() is possible for the --disable-drm case, but is non-trivial and so not implemented for now.)

 man page: correct typo in hyperlink qbarnes patch #535



#### Building

-  out of tree builds pull request #544

## [2.2.1] 2025-07-07

### General

#### Added

- Do not perform permission checks if running with effective user root, i.e. when 
  running with setuid bit set. Partially addresses issue #509

#### Changed

- Command **environment**: report if running as root, with sudo, or setuid bit set
- Command **environment --verbose**: eliminate some unneeded edid read tests
- Command **setvcp**: Emit more more useful messages when the cause of a 
  DDCRC_RETRIES error is itself a DDCRC_RETRIES error.
- Add option ***--disable-traced-function-stack***
  The default traced-function-stack option value (disabled) is set in parms.h. 

#### Fixed

- Avoid segfault if invalid feature code specified
- Maximum wait time on function lock_display() not respected, possible infinte
  loop.  Addresses issue #511
- Fix --verify/--noverify parsing.  Addresses issue #512.
- ddca_close_display(): do not respect quiesce, allowing the display lock
  record to be released, avoiding possible deadlock
- Fix the spelling of company name HON HAI PRECISION" (obtained from UEFI), 
  avoiding a lintian message. Addresses issue #500
- Fix errors reported by clang 19: unused variable, expression which evaluates
  to 0 treated as null pointer. Addresses issue #502.
- Command **getvcp --terse**: Eliminate irrelevant messages regarding VCP version not set.
- Make test for missing DRM card-connector directories more robust. Issue #507.
- DDC communication not detected with old (340 series) Nvidia proprietary driver. Issue #507
- Function end_capture(): call close() in case of fflush() error, ensuring that 
  messages are always sent to the terminal after message capture complete
- Functions **ddca_find_display_ref()** and **ddca_get_display_ref()** were 
  returning a pointer to the internal Display_Ref struct rather than an external 
  integer DDCA_Display_Ref, causing segfault or return code DDCRC_INVALID_DISPLAY
  on a subsequent api call such as **ddca_open_display2()**.  Addresses issue #528.
- Several API functions were not recognized as arguments to ***--trcfunc*** and ***--trcfrom***. 
  These included **ddca_find_display_ref()**, **ddca_get_display_ref()**, **report_parsed_capabilities()** 
- Option ***--stats*** was not reporting data 
- **ddc_open_display()**: return DDCRC_DISCONNECTED instead of an assert failure if 
  bus_info == NULL.  Addresses KDE Powerdevil issue #504861 (powerdevil crash from 
  libddcutil, no restart happens)
- Macro TRACED_ASSERT() calls **__assert_fail()** instead of exit(1). 
  Addresses Powerdevil issue #504861.
- Fix troff formatting in ddcutil man page. Pull request #531.

### Building

- **configure** option ***--enable-watch-displays***/***--disable-watch-displays***
  controls whether display watch functionaliity is built.  The default is 
  ***--enable-watch-displays***.
  - If built with ***--disable-watch-displays***, API functions related 
    to display change detection return DDCRC_UNIMPLEMENTED
- If **configure** option ***--disable-x11*** is specified, the only watch-mode 
  is POLL. watch-mode XEVENT does not exist.

#### Shared Library

### Added

- Struct DDCA_Display_Info2 is an extended version of DDCA_Display_Info that 
  adds fields for the display's sysfs card-connector directory, how that 
  directory was determined (precisely using I2C bus number or heristically
  using EDID), and the DRM connector id. API functions have been added to 
  use this struct: **ddca_get_display_info2()**, **ddca_free_display_info2()**, 
  and **ddca_report_display_info2()**.  Addresses issue #518.

### Changed

- Callback function handling made more bulletproof
  - Close any unlock displays left open by user callback function
  - Write syslog messages (with log level NOTICE) before and after executing
    the callback function

### Fixed

- **ddca_redetect_displays()**: block if called with callback threads active or 
  if already executing
- **ddca_close_display()**: allow execution even if libddcutil is quiesced.
  Avoids a possible deadlock due to an internal display lock not being released.
- **ddca_stop_watch_displays()** did not release the X11/Wayland connection 
  obtained by **ddca_start_watch_displays()**.  This also occured with each 
  **ddca_redetect_displays()** call. As a result KDE PowerDevil eventually 
  exhausted X11 connections of a very long running system with repeated
  display connections and disconnections.  Pull request #519.
- KDE powerdevil was terminating with status success instead of failure if
  libddcutil terminated with a traced assertion, causing the powerdevil 
  service to not restart automatically. Macro TRACE_ASSERT() is modified to
  use **__assert_fail()** instead of **exit()** for termination. 

## [2.2.0] 2025-02-10

### General

#### Added

- Support DisplayLink devices
- Add command **noop**, which allows for executing options such as 
  ***--settings*** without having to execute a real command.
- As an aid to development, the build date and time are normally embedded in
  the ddcutil and libddcutil executables.  This is reported using command
  **ddcutil --version --verbose".  **libddcutil** reports this to the system
  log. If reproducible builds are required, use **configure** option 
  ***--disable-build-timestamp***. (For reproducible builds, building typically
  is performed using a script or build system, so it's not inconvenient to 
  specify ***--disable-build-timestamp*** in this situation.)
- Add ***--enable-flock*** and ***--disable-flock*** as aliases for 
  ***--enable-cross-instance-locks*** and ***--disable-cross-instance-locks***
- Add option ***--ignore-mmid*** to ignore problematic monitor models.
  Takes a Monitor Model Id, e.g. SAM-U32H75x-3587 as an argument. 
  Indicates that DDC/CI communication is disabled for monitors with this id.  
  Typically, this will be added to the [libddcutil] section of configuration 
  file ddcutilrc. It can also be included in the options string passed in the
  opts argument to ddca_init2(). Addresses issue #446.
- Maintain a stack of traced functions for debugging. Turned on by option 
  ***--enable-traced-function-stack***

#### Changed

- User Defined Features: 
  - Add XNC (Extended Non-Continous) like simple NC, but the SH byte is
    also reported.
  - Allow SNC (Simple Non-Continuous) as alternative name for NC.
  - Report user defined features as part of parsed capabilities.
  - Commands recognizing user defined features now fail if there's an error
    loading a user defined feature file.  These are **capabilities**, 
    **setvcp**, **dumpvcp**, and **probe**.
- /usr/lib/udev/rules.d/60-ddcutil-i2c-rules:
  - Give logged on user r/w access to /dev/dri/cardN, needed
    to allow logged on user to probe connectors using DRM.
- Do not install /usr/lib/udev/rules.d/60-ddcutil-usb.rules, delete it
  if previously installed. Addresses issues #405, #428, #437
- Command **ddcutil chkusbmon**:  
  - Skip processing and always return 1 (failure) if option ***--disable-usb***
    is in effect.
- Command **detect**: 
  - Only show communication error detail if --verbose
  - Provide a clearer message if slave address x37 is inactive: 
     - "Monitor does not support DDC" instead of generic "DDC_commnication failed"
     - If option ***--verbose*** is in effect, emit an additional message to
       check the monitor's OSD.
- Parser changes:
  - Add alternative option names for symmetry with other options: 
    - ***--discard-capabilities-cache*** is an alias for ***--discard-cache capabilities***
    - ***--discard-sleep-cache*** is an alias for ***--discard cache dsa***
    - ***--discard-dsa-cache*** is an alias for ***--discard cache dsa***
  - Eliminate --enable-dsa-cache as alias for --enable-dynamic-sleep-cache
  - Improve handling of --verify/--noverify, error if both specified
- File .gitignore:
  - Add: *.tar.gz, docs/ddcutil-c-api
- Commands **interrogate** and **environment --verbose**:
  - Force settings --disable-cross-instance-locking, --disable-dynamic-sleep (VERIFY)
  - Forced settings apply to **environment --verbose** as well as **interrogate**
  - When probing DRM, recognize bus types DRM_BUS_PLATFORM, DRM_BUS_HOST1X, 
    report as "platform", "host1x"
- Change the system log message level when sleep time is adjusted from WARNING
  to VERBOSE. Addresses issue #427: Adjusting multiplier message fills system
  log when libddcutil used by clightd
- Configuration file ddcutilrc: If there is a pound sign "#" on a line, 
  the remainder of the line is treated as a comment.
- Add -Wformat-security to compiler options.  Addresses issue #458.
- If option --bus specified, only check accessability for that bus, avoiding
  irrelevant warning messages regarding other buses.  Addresses issue #461.
- Return DDCRC_CONFIG_ERROR instead of DDCRC_BAD_DATA for User Defined Feature File
  errors.
- Report the Monitor Model Id in the ***--verbose*** output to **ddcutil detect**.
- Command **setvcp**: Do not report "Interpretation may not be accurate.", which
  is irrelevant for this command. Partially addresses issue #454.
- rpt...() functions can redirect output to syslog, making lines coming from 
  multiple threads more coherent
- Enable additional compiler warnings to tighten the code.
- Additional trace groups SYSFS, CONN

#### Fixed

- Rework laptop detection. A non-laptop display can have an eDP connector.
  This is an i915 video driver bug that will not be fixed. See freedesktop.org 
  issue "DRM connector for external monitor has name card1-eDP-1" 
  https://gitlab.freedesktop.org/drm/i915/kernel/-/issues/10389
- When processing environment variable user $XDG_DATA_DIRS, or $XDG_CONFIG_DIRS,  
  the final directory in the list was ignored.  Issue #438
- When processing a user defined feature file, recognize any whitespace character
  (e.g. tab), not just space.
- Fix core dump on ddcutil getvcp. Issue #407 
- Commands **interrogate** and **environment --verbose**
   - Simple getvcp test was not reporting the bytes of the response packet.
   - If no device with class x03 was found, the user's home directory
     was dumped. Issue #413.
   - Remove "-i" option on get-edid command. Does not exist on some versions.
- Fix display not found on Raspberry Pi. Do not rely on /sys/class/drm to read 
  EDID, which is not valid for some drivers.  Addresses issue #403
- Fix DDC communication failed on Raspberry Pi. Do not rely on sysfs attributes
  that do not exist for ARM devices. Resolves issue #413.
- User Define Features file: fix error msg when nothing follow VALUE
- Convert CRLF line endings to LF
- Use printf() formats %jd and %zd to portably print variables of type ssize_t, 
  time_t, so as to build  unchanged on architectures such as armel, armhf.
- Avoid compiler warning possible depending on compiler configuration when
  a switch() construct is used. Replaced with if/else if/else. Resolves issue #458.
- Do not use function strerrrorname_np(). Requires glibc >= 2.32.
- Miscellaneous changes to allow for building on raspbian (debian bullseye).
- Replace function sysfs_find_adapter().  Fixes display detection problem aspect
  of issue #465.
- Dump information to syslog instead of asserting failure if unable to get flock
  on /dev/i2c device.
- Option ***--skip-ddc-checks*** set vcp version in Display_Ref to DDCA_VSPEC_UNKNOWN
  to avoid possible assert failure.
- Prepend thread id to most syslog messages.
- Make syslog messages more consistent in form. 
- Memory leaks.

### Building 

- Re-enable autoconf/configure option --enable-x11/--disable-x11.
  X11 specific code is used in display change and sleep state detection.
  The default is --enable-x11.   
- Add autoconf/configure option ***--enable-static-functions-visible***. 
  If set, storage class specifier "static" is removed from many functions so
  that their names appear in backtrace reports from valgrind, asan, and glibc 
  function backtrace().


### Shared Library

The shared library **libddcutil** is backwardly compatible with the one in 
ddcutil 2.1.x. The SONAME is unchanged as libddcutil.so.5. The released library
file is libddcutil.so.5.2.0. 

#### Added

- Option ***--disable-api*** completely disables the API. Most API calls, including
  those performing DDC communications, will fail. This can be useful for testing 
  whether **libddcutil** is the source of a system error in the case of client
  applications, e.g. KDE PowerDevil, that will not build without the shared library.
- Add libddcutil only option ***--disable-watch-displays***, which unconditionally
  blocks **ddca_start_watch_displays()** from starting the thread that watches
  for display changes. Workaround for issue #470.
- **ddca_get_display_watch_settings()**, **ddca_set_display_watch_settings()**

#### Changed

- **ddca_start_watch_displays()**: 
  - The only event class that can currently be enabled is DDCA_EVENT_CLASS_DISPLAY_CONNECTION. 
    Watching for sleep state changes is not currently supported.  
  - Regards DDCA_EVENT_CLASS_ALL as same as DDCA_EVENT_CLASS_DISPLAY_CONNECTION
  - Error if either DDCA_EVENT_CLASS_DPMS or DDCA_EVENT_CLASS_NONE are specified.
- Status code DDCRC_INVALID_CONFIG_FILE renamed to more general DDCRC_CONFIG_ERROR. 
  DDCRC_INVALID_CONFIG_FILE is a valid alias.
- Write build date and time to system log when starting libddcutil.
- Rework libdccutil output to avoid duplicate msgs in system log when all terminal 
  output is directed to the log, as with KDE Plasma
- Most API functions that specify a display reference now return status code 
  DDCRC_DISCONNECTED if the display reference is no longer valid.
- Quiesce the API during **ddca_redetect_displays()**.  Operations that access
  display state are not permitted, and return DDCRC_QUIESCED.
- Add DDCA_STATS_API to enum DDCA_Stats_Type, for reporting API specific stats.
- Compile using option -Wformat-security. Issue #458.
- Opaque pointer DDCA_Display_Ref now contains a display reference id instead
  of an actual pointer. It's type continues to be void* so client program use
  of this type is unchanged.
- **libddcutil** maintains a table of DDCA_Display_Refs that have been
  "published" by the API, for validating DDCA_Display_Ref args on API 
  function calls.
- The opaque value in DDCA_Display_Ref is now an integer id number instead of 
  pointer into the libddcutil data structures, making it slightly more opaque.
  The type of DDCA_Display_Ref remains "void*", so no client changes are needed
- syslog output is generally prefixed with date and thread id

#### Fixed

- Whan a display is connected, the display number assigned to its display 
  reference is one greater than the highest already assigned, instead of 99.  
- **ddca_start_watch_displays()**: 
  Fixed segfault that occured with driver nvidia when checking if all video
  adapters implement drm. Issue #390. 
- Ignore phantom displays when searching for a display reference. Issue #412. 
- **ddca_get_display_refs()**, **ddca_get_display_info_list2()** always
  return 0, even if an error occured when examining a particular monitor. 
  Addresses issue #417. 
  - Errors that occur opening individual displays or reading their EDIDs are
    are still reported using **ddca_get_error_detail()**. In addition, error
    messages are written to the terminal and, depending on the current
    syslog level, to the system log.
- **ddca_get_display_refs()** and **ddac_get_display_info_list2()** do not 
  include display references for displays that are no longer connected.
- **ddca_get_display_info()** succeeds even if DDC communication is not working.  
    Addresses issue #???.
- Display reference validation: Do not use dref->drm_connector, which may be 
  invalid after hotplug. Addresses issue #418.
- **ddca_dref_repr()**: Do not check that the display reference is still valid.
  It is meaningful to create a string representation of a display reference even
  if it is no longer usable. Addresses ddcui issue #55.
- Protect hash table of open monitors to avoid a possible race condition.
- Recover instead of abort when more than one non-removed display refs exist 
  for the same display.
- Do not call ddca_stop_watch_displays() at library termination if client has
  already called it. 
- Use mutexes to control access to corruptable data structures.
- Memory leaks.

#### Display Change Detection

- Alternative algorithms for detecting display changes, specified by option 
  ***--watch-mode***
  - watch mode XEVENT
    - Scans for changes only when a X11 change notification occurs. 
      (Uses X11 API extension RANDR, which is also implemented on Wayland.)
  - watch mode POLL
    - doesn't use X11 
    - doesn't rely on /sys 
    - reads EDIDs in polling loop
    - can consume a significant amount of CPU time on older machines
  - watch mode DYNAMIC (the default)
    - resolves to XEVENT on X11 or Wayland, otherwise to POLL
- Extensively reworked display change detection
  - use /sys to get EDID if possible
  - handle MST hub devices if driver/device allow
    - not all drivers work
  - only perform stabilization for removed display
  - not checking for asleep
- Named options affecting display change detection:
  - --watch-mode POLL, XEVENT, DYNAMIC
  - --enable/disable-try-get-edid-from-sysfs (default is --enable-try-get-edid-from-sysfs)
- options ***--xevent-watch-loop-millisec*** ***--poll-watch-loop-millisec***
- Added **ddca_get_display_watch_settings()**, **ddca_set_display_watch_settings()**
- Use constants in parms.h to specify retry intevals and counts
- Handle possible delay between time that EDID can be read and DDC becomes functional
- Added **flags** field in unused secton of DDCA_Display_Status Event, with bit DDCA_DISPLAY_EVENT_DDC_WORKING.
  Normally, this bit is set on display connection events.  In case DDC is not immediately available after 
  EDID detection, this bit is not set.  If DDC subsequently becomes enabled, and event of type DDCA_EVENT_DDC_ENABLED occurs. 

- It's possible that there's a delay between the time a monitor is turned on 
  (and X11/Wayland generate a display change event) and the time that DDC 
  becomes enabled. There's a newly added flags field in DDCA_Display_Status_Event,
  with one bit defined, DDCA_DISPLAY_EVENT_DDC_WORKING.  Normally, this bit is 
  set in the emitted DDCA_Display_Status_Event. However, if DDC is not immediately
  enabled the bit is not set, and the display reference goes onto a recheck queue 
  to be processed by a separate thread. An event of type DDCA_EVENT_DDC_ENABLED
  will be emitted if and when the recheck thread determines that DDC is working.
- There's a tension in display change detection between minimizing the time between
  when X11/Wayland detects a monitor having been turned on and libddcutil issuing
  an event of type DDCA_DISPLAY_EVENT_CONNECTED versus checking and rechecking 
  failed states (e.g. DDC not working).  In many caes, the frequency and wait 
  intervals are controlled by settings in file src/base/parms.h.



## [2.1.4] 2024-02-17

### Shared Library

- Reinstall previously deprecated and removed **ddca_create_display_ref()**, 
  allowing existing clients to build unchanged.


## [2.1.3] 2024-02-07

### Changed

- Option ***--settings*** reports build options.

### Fixed

- Memory leaks

### Shared Library

The shared library **libddcutil** is backwardly compatible with the one in 
ddcutil 2.1.0. The SONAME is unchanged as libddcutil.so.5. The released library
file is libddcutil.so.5.2.0.

### Fixed

- Due to overly aggressive DDCA_Display_Ref validation, ddca_get_display_info()
  returned status DDCRC_ARG when obtaining information about an invalid display 
  (i.e. one for which the EDID is detected but DDC communication fails). This
  fixes ddcui issue #55, which reported an abort if a display was invalid.
- Checking for whether the video adapter supports DRM was consolidated.
  This fixes a case where all displays were reported to support DRM when that
  was not the case, causing incorrect display status change monitoring.
  
## [2.1.2] 2024-01-27

### General

### Added

- Option ***--vstats*** reports minimum, maximum, and average successful
  sleep multiplier

### Changed

- Make hidden option ***--min-dynamic-multiplier*** non-hidden
- Revert **ddca_get/set_sleep_multiplier()** to 2.0.0 semantics
- Allow for the fact that the proprietary Nvidia driver always reports
  the value of /sys/class/drm/<connector name>/enabled as "disabled"

### Fixed

- Invalid assert when checking how monitor reports unsupported features. 
  (Issue #371)
- Segfault in environment command when examining procfs if compiled using clang
  option -flto (Issue #354)
- Bring man page up to date (Issue #364)

### Shared Library

The shared library **libddcutil** is backwardly compatible with the one in 
ddcutil 2.1.0. The SONAME is unchanged as libddcutil.so.5. The released library
file is libddcutil.so.5.1.1.

### Changed

- **ddca_start_watch_displays()** requires that all video drivers support DRM. 
  If the API call fails, detailed error information is available using 
  **ddca_get_error_detail()**.
- Additional syslog messages

### Fixed

- SIGABRT in API calls that have a display reference or handle argument if 
  **ddca_init2()** (or **ddca_init()**) had not been called.  This caused
  KDE PowerDevil to repeatedly crash and restart. 
- Fix the check of whether all video drivers implement DRM, required for
  display hotplug detection.


## [2.1.0] 2024-01-16

### General

#### Added
- Option ***--skip-ddc-checks***
  - Skips initialization checks to confirm that DDC communication is working
    and the monitor properly uses the unsupported feature bit in Get Feature 
    Reply packets, thereby improving initialization time.
- Cross-instance locking (experimental). Uses flock() to coordinate I2C bus
  access to /dev/i2c devices when multiple instances of ddcutil or libddcutil
  are executing. Enabled by option ***--enable-cross-instance-locks***.
- Option ***--min-dynamic-multiplier*** Specifies a floor to how low dynamic
  sleep adjustment sets the sleep multiplier. (experimental)

#### Changed
- Multiple "Options:" lines in an ini file segment are combined
- Option ***--help***: 
  - Only show extended information when ***--verbose*** specified
- Option ***--version***: 
  - Show only the version, without prefix if ***--brief*** specified
  - Include detailed build information if ***--verbose** specified 
- I2C bus examination during initialization can be parallelized, improving performance
  (This is distinct from the ddc protocol checking.) This is an experimental
  feature.  It can be enabled by using a low value as an argument to option 
  ***--i2c-init-async-min***, e.g. ***--i2c-init-async-min 4***.
  THIS OPTION IS DISABLED BY DEFAULT AS IT OCCASIONALLY TRIGGERS A BUG IN
  DRIVER amdgpu THAT CAN CAUSE THE MOUSE AND KEYBOARD TO BECOME UNRESPONSIVE.
  See freedesktop.org bug report "lockup in dce_i2c_submit_command_hw" at
  https://gitlab.freedesktop.org/drm/amd/-/issues.
- Command detect: better messages when laptop display detected
  - do not report "DDC communication failed"
  - report "Is laptop display" instead of "Is eDP device" or "Is LVDS device"
- Better accomodate the variation in use of sysfs by different drivers
- Turned off unconditional message that reported an elusive Nvidia/i2c-dev
  driver compatibility error.  The incompatibility has been full diagnosed 
  as being caused by use of a legacy proprietary Nvidia driver. A message
  is still written to the system log.
- Make more extensive use of the system log.
- Options ***--enable-displays-cache***, ***--disable-displays-cache*** are marked
  hidden since displays caching is not a released feature.
- Deprecate vaguely named option ***--force***.  Replace its single use with  
  option ***--permit-unknown-feature***.
- Deprecate vaguely named option ***--async***. Use ***--ddc-checks-async-min***
- Deprecate vaguely named option ***--ddc***. Use option ***--ddcdata***, which more
  clearly indicates that it causes DDC data errors to be reported.
- **configure** option ***--enable-asan*** causes libasan to be linked into binaries 
- **configure** option ***--enable-x11*** is deprecated. The X11 API is no longer used.

#### Fixed
- Better handling of DDC Null Message recognition and adjustments
- Dynamic sleep: make it easier to reduce time - once high didn't come down
- Command detect: ensure output of model name, serial number, and extra display descriptor 
  use only ASCII character in the range 0..127.
- Some USB-only code was not iftested out when **configure** option ***--disable-usb*** was set. (Issue 355)
- Always set sleep multiplier to at least 1.0 for commands **setvcp** and **scs**. Addresses reports
  that aggressive optimization caused setvcp to fail.
- Cross-thread locking handles situtations where a display ref does not yet exist, e.g. reading EDID.
- Memory leaks.

### Shared library

The shared library **libddcutil** is backwardly compatible with the one in 
ddcutil 2.0.0. The SONAME is unchanged as libddcutil.so.5. The released library
file is libddcutil.so.5.1.0.

#### Added
- Implemented display hotplug event detection
  - Requires DRM video drivers (e.g. amdgpu, i915)
  - Can detect physical connection/disconnection and DPMS sleep status chanes,
    but the effect of turning a monitor on or off is monitor dependant and 
    cannot reliably be detected.
  - Enabled by libddcutil option ***--enable-watch-displays***, or
    API call **ddca_enable_start_watch_displays()**
  - API uses callbacks to report report status changes to client
  - new status codes possible for many current API functions: 
    DDCRC_DISCONNECTED, DDCRC_DPMS_ASLEEP
  - When a display connection event is reported, the client should call 
    **ddca_redetect_displays()**
- **ddca_validate_display_ref()**: Exposes the validation that occurs on any
    API call that has a DDCA_Display_Ref argument.  Can be used to check 
    whether a monitor asleep, disconnected, etc.
- New functions for sleep multiplier control:
  - **ddca_enable_dynamic_sleep() 
  - **ddca_disable_dynamic_sleep() 
  - **ddca_get_current_sleep_multiplier() 
  - **ddca_set_display_sleep_multiplier() 
- **ddca_init2()** replaces **ddca_init()**, which is deprecated: 
  Has an additional argument for collecting informational msgs. Allows for not
  issuing information messages regarding the assembly of options and parsing directly
  from libddcutil to the terminal (now enabled by setting flag DDCA_INIT_OPTIONS_ENABLE_INIT_MSGS),
  but instead gives client complete control as to what to do with the messages.

#### Changed
- Functions that depend on initialization and that return a status code now 
  return DDCRC_UNINITIALIZED if ddca_init() failed.
- Revert **ddca_get_sleep_multiplier()**, **ddca_set_sleep_multiplier()** to 
  their pre 2.0 semantics changing the multiplier on the current thread.
  However, these functions are marked as deprecated.

#### Fixed
- Argument passing on **ddca_get_any_vcp_value_using_implicit_type()**
- Fixed cause of assert() failure in **ddca_init()** when the libopts string
  argument has a value and the configuration file is enabled but no options 
  are obtained from the configuation file.
- Contents of the libopts arg were added twice to the string passed to the
  libddcutil parser.

## [2.0.0] 2023-09-25

Release 2.0.0 contains extensive changes.  Shared library libddcutil is not 
backwards compatible.  

#### Added
- Install /usr/lib/modules-load.d/ddcutil.conf. Ensures that driver i2c-dev
  is loaded.
- Install file /usr/share/udev/rules.d/60-ddcutil-i2c.rules, autmatically granting
  the logged on user read/write access to /dev/i2c devices for video displays.
  For most configurations, use of group i2c is no longer necessary.
- Command options not of interest to general users are now hidden when help is 
  requested.  Option ***--hh*** exposes them, and implies option ***--help***.
- Option ***--noconfig***. Do not process the configuration file.
- Option ***--verbose***.  If specified on the command line, the options obtained
  from the configuration file are reported.
- Added utility options --f7, --f8, --i2, --s1, --s2, --s3, --s4, --fl1, --fl2
  These options are for temporary use during development. The current use of 
  the utility options is reported by option ***--settings***. 
- Added utility command C1 for temporary use during development.
- Added option ***--enable-mock-data*** for testing
- Option ***--trcfrom***. Traces the call stack starting with the specified 
  function.  This option applies only to functions for which tracing has been enabled.
- API performance profiling
- Added options ***--ignore-hiddev*** and ***--ignore-usb-vid-pid***
- Added: Sample file nvidia-i2c.conf that can be installed in directory 
  /etc/modprobe.d to set options sometimes needed by the proprieatry NVidia 
  video driver.
- Added option ***--discard-cache*** to discard cached data at the start of 
  program execution.  Valid arguments are ***capabilities***, ***dsa***, and ***all***.
- Command **discard [capabilites|dsa|all] cache(s)** discards cached data.
- Option ***--pid*** (alt ***--process-id***) prepends the process id to trace messages.
- Command **traceable-functions*** lists functions that can be traced by name, 
  i.e. ones that can be specified as a ***--trcfunc*** argument.
- If using X11, terminate immediately if a DPMS sleep mode is active.

#### Changed
- The dynamic sleep algorithm has been completely rewritten to both dynamically
  increase the sleep-multiplier factor (as needed) and decrease the sleep 
  multiplier factor (insofar as possible).  Data is maintained across program 
  executions in file $HOME/.cache/ddcutil/stats. Option ***-dsa***, or one of 
  its variants such as ***--enable-dsa*** turn it on.
- Option ***--sleep-multiplier***:  0 is now allowed as an argument. Some 
  DisplayPort monitors have been observed to work with this value. 
- Writing to the system log has been generalized. 
  Option ***--syslog <level>*** controls what is written to the system log.
  Recognized levels are NEVER, ERROR, WARN, INFO, and DEBUG.  This option 
  replaces ***--enable-syslog***, ***--disable-syslog***, and ***--trace-to-syslog***. 
- **environment --verbose**: Option ***--quickenv*** skips some slow tests such as 
  use of program i2cdetect.
- **environment --verbose**: extended sysfs scan for ARM SOC devices to explore how 
  those devices use /sys
- Detailed statistics are now maintained on a per-display instead of per-thread basis.
- Option ***--vstats*** includes per-display statistics in its reports.  It takes
  the same arguments as ***--stats***. 
- Cached capabilities are not erased by ddcutil calls that are not executed with 
  ***--enable-capabilities-cache***.  This makes the behavior the same as cached 
  displays and cached performance statistics.
- **environment --verbose** disables caching, reports contents of cached files.
- loosen criteria for when to try fallback methods to read EDID when using USB 
  to communicate with Eizo monitors
- udev rule changes: 
  - install /usr/lib/udev/rules.d/60-ddcutil-usb.rules
  - rename /usr/lib/udev/rules.d/60-ddcutil.rules to 60-ddcutil-i2c.rules
  - update and rename sample rules files installed in /usr/share/data/ddcutil as 
    60-ddcutil-i2c.rules and  60-ddcutil-usb.rules.  The user can modify these 
    files and install them in /etc/udev/rules.d to override the files installed 
    in /usr/lib/udev/rules.d. 
- ***--enable-dsa*** is a valid synonym for ***--enable-dynamic-sleep***
- Display detection improved
  - Rework the algorithm for detecting display communication and testing how 
    invalid features are reported.
  - Handle the phantom monitor case where a MST capabile monitor is detected as
    a separeate i2c bus/drm connector along with that on the video card
- Command **detect** improved:
  - verbose output: 
    - Reports the sysfs DRM values for dpms, enabled, and status 
     - Reports if I2C slave address x37 is responsive
  - reports an error summary if DDC communication fails
  - Issue warnings that output may be inaccurate if the monitor is sleeping 
    or if it cannot be determined how unsupported features are indicated.
- Messages regarding DDC data errors (controlled by option ***--ddc***) are 
  written to the system log with level LOG_WARNING instead of LOG_NOTICE.

#### Fixed
- More robust checks during display detection to test for misuse of the DDC Null Message
  and all zero getvcp response to indicate unsupported features.
- Option ***--help***: Document **ELAPSED** as a recognized statistics class
- ddca_dfr_check_by_dref(): do not return an error if no user defined feature file 
  exists for the monitor
- Recognize (but always report failure for) CHKUSBMON command even when ddcutil not 
  built with USB support.
- Improve reporting of /dev/hiddev* open failures to reduce confusion caused by this
  typically benign error.
- Check that no display identifier is included on commands that don't use one
- Increase buffer size to allow for a Get Feature Reply packet that contains an 
  incorrectly large length field.
- Change the USB infomation shown for option ***--version*** to emphasize that
  ddcutil uses USB for communicating with the monitor's Virtual Control Pandl, 
  not for video transmission.
- Memory leaks

### Shared library changes

The shared library **libddcutil** in is not backwardly compatible.  
The SONAME is now libddcutil.so.5. The released library file is libddcutil.so.5.0.0.

Library initialization has been reworked extensively to move operations 
that can fail and other aspects that should be under control of the library user
into a new function **ddca_init()**.

This function: 
- controls the level of messages written to the system log
- optionally processes options obtained from the **ddcutil** configuration file
- processes additional options passed as a string
- sets error information for ddca_get_error_detail()

If this function is not called by the user program, any API function that relies on its 
processing invokes **ddca_init()** using arguments such that it never fails, e.g. 
the configuration file is not processed.

Added typedef 
- struct DDCA_Display_Detection_Report

Added functions: 
- ddca_init() See above.
- ddca_register_display_detection_callback(): Registers a function 
  of type DDCA_Display_Status_Func which will be called to inform the client
  of display hotplug events.
- ddca_library_filename():  Returns the fully qualified name of the 
  shared library.

Changed functions:

- ddca_report_display_info() returns DDCA_Status instead of void
- ddca_get_feature_name() implementation restored

Changed semantics:

The semantics of some functions have changed, reflecting
the fact that some statistics are now maintained on a per-display
rather than per-thread basis.

- ddca_set_sleep_multiplier(), ddca_get_sleep_multiplier(). 
  Instead of operating on the current thread, these functions operate on the
  display, if any, open in the current thread.

- ddca_set_default_sleep_multiplier(), ddca_get_default_sleep_multiplier()
  Operate on newly detected displays, not new threads.

Removed functions: 

With the ability to configure libddcutil operation both by the ddcutil 
configuration file and by passing an option string in the ddca_init() arguments, 
several API functions are no longer needed and have been removed: 

Max-tries options:
  - ddca_max_max_tries()
  - ddca_get_max_tries()
  - ddca_set_max_tries()
  - ddca_set_default_sleep_multiplier(), ddca_set_global_sleep_multiplier()
  - ddca_get_default_sleep_multiplier(), ddca_set_global_sleep_multiplier()

Trace options: 
  - ddca_add_traced_function()
  - ddca_add_traced_file()
  - ddca_set_trace_groups()
  - ddca_add_trace_groups()
  - ddca_trace_group_name_to_value()
  - ddca_set_trace_options()

USB enablement: 
  - ddca_enable_usb_display_detection
  - ddca_disable_usb_display_detection 

Miscellaneous: 
  - ddca_enable_force_slave_adress() 
  - ddca_is_force_slave_address_enabled()
  - ddca_enable_error_info()

Most per-thread statistics are now instead maintained on a per-display basis.
The following functions are no longer useful and have been removed
- ddca_set_thread_description()
- ddca_append_thread_description()
- ddca_get_thread_desription() 

Remove previously deprecated functions: 
- ddca_open_display(). Use ddca_open_display2().
- ddca_create_display_ref(). Use ddca_get_display_ref()
- ddca_free_display_ref(). Had become a NO-OP. All display references are persistent

Symbols for functions and enums that had previously been removed from ddcutil_c_api.h are no longer exported.

Options that apply only to libddcutil (Specified in the ddcutil configuration file or passed to ddca_init())

- Option ***--profile-api***. Applies only to **libddcutil**.  Statistics
  for API functions that perform display IO are collected and subsequently
  reported when the library is terminated.  

- Option ***--trcapi***. Trace the call stack for a specified API function.

#### Building ddcutil

- configure options --enable-syslog/--disable-syslog have been eliminated.   
  Use runtime option ***--syslog NEVER*** to disable all writes to the system log.
- Use of shared library **libkmod** eliminated.
- Shared library **libjansson** is now required

## [1.4.5] 2023-09-18

#### Building ddcutil

- The autotools **configure** command now recognizes ***--enable-install-lib-only***.  If specified, 
  command **make install** only installs the shared library. This is intended to facilitate installation 
  of **libddcutil.so.4** along with the upcoming **libddcutil.so.5**.


## [1.4.2] 2023-02-17

### Added 

- **ddcutil** installation installs files /usr/lib/modules-load.d/ddcutil.conf
  and /usr/lib/modules-load.d#libddcutil.conf to ensure that kernel module 
  i2c-dev is loaded at boot time if it is not built into the kernel. There are
  two files so that when split up into distribution packages, each of the 
  command line **ddcutil** package and the shared library **libddcutil** 
  package installs a file.  

## [1.4.1] 2023-01-16

### Fixed
- The default sleep-multipler value was 0, instead of 1. This resulted in failure of
  most DDC/CI operations, including display detection.
  
## [1.4.0] 2023-01-04

### Added
- **ddcutil** installation installs file /usr/lib/udev/rules.c/60-ddcutil.rules.
  This udev rule uses tag uaccess to  give the logged on user read/write access
  to /dev/i2c devices associated with video adapters. Configuring use of group 
  i2c is no longer necessary.
- configure options ***--enable-syslog*** and ***--disable-syslog*** control 
  whether messages are written to the system log.  The default is enabled.

### Changed
- The ability to use the write()/read() interface of i2c-dev has been restored.
  It is needed to work around a bug in the proprietary Nvidia driver.  By
  default,  ioctl() interface is used for all drivers.  If the Nvidia bug is 
  detected, the write()/read() interface is used instead. Command line options 
  ***--use-file-io*** and ***--use-ioctl-io*** affect this default behavior. 
  When i2c-dev's file io interface is used, option ***--force-slave-address***
  is again meaningful.
- Option ***--sleep-multiplier*** and API functions **ddca_set_sleep_multiplier_value()**, 
  **ddca_set_default_sleep_multiplier_value()** now accept 0 as a valid 
  argument.
- The ddcutil command parser reports an error if a display selection option 
  (e.g. ***--bus***) is given on a command to which it does not apply.
- Write additional error and information messages to the system log.
- Eliminate message "Is DDC/CI enabled in the monitor's on-screen display?"
  It's rarely the cause of communication failures.

### Fixed
- Warn of a possibly invalid DRM connector name in **detect** output if 
  monitors with identical EDIDs are used with the proprietary nvidia driver.
- Handle /dev/i2c device names with a double hyphen, e.g. /dev/i2c--3. 
- Better libddcutil handling of configuration file errors.  Do not abort
  initialization in case of errors.
- Fix interpretation of digital display type bits for EDID version 1.4
- Miscellaneous segfaults.

## [1.3.2] 2022-09-04

### Changed
- Modify tarball creation to eliminate garbage and otherwise unneeded files.

## [1.3.0] 2022-07-19

### Added
- Command **detect**: 
  - Issue warning for monitors for which **ddcutil** should not be used to
    change settings.
    - Currently only entry is Xaomi model "Mi Monitor"
- Debug messages.  Environment variables DDCUTIL_DEBUG_PARSE, 
  DDCUTIL_DEBUG_MAIN, DDCUTIL_DEBUG_LIBINIT can be set to enable trace messages
  in command line **ddcutil** or shared library **libddcutil.so** during 
  initialization and before command options.

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
- Option ***--dca***: The Dynamic Sleep Adjustment algorithm was rewritten to 
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
- Command **interrogate**: 
   - Set --disable-capabilities-cache
- More user friendly messages at startup regarding /dev/i2c buses that cannont
  be opened.  If the problem is inadequate permissions (EACCES), the user is 
  directed to www.ddcutil.com/permissions.
- Better handle malformed EDIDs
  - Trailing blanks on model and serial number are stripped.  This affects 
    commands **detect --terse**, **loadvcp** and **dumpvcp**, and also the 
    file names of user defined features.
- Option ***--stats***: 
  - I2C ioctl() calls for reading and writing are now reported as type IE_IOCTL_WRITE
    and IE_IOCTL_READ rather than IE_OTHER
  - IE_WRITE_READ stats are no longer reported, as they are redundant
- Source code has been extensively cleaned up. In particular, directory **adl**
  containing code for the old proprietary AMD video driver, has been removed.
- Building ddcutil:
  - Library **libi2c.so** is no longer linked.  It was needed only for some
    experimental code.
libddcutil: 
- ddca_get_display_refs(), ddca_get_display_info_list2(): 
  - Open errors can be retrieved using ddca_get_error_info(). 
    Note that the API calls still succeed.
  - Deprecated API functions have no effect:
    - ddca_enable_force_slave_address(), ddca_is_force_slave_address_enabled()

### Fixed
- The sleep multiplier value was not respected for new API threads.
- User Defined Features: Keyword **NC** set the incorrect flag in a feature
  descriptor.
- Option **--dsa**: Fix adjustment factor calculation due to incorrect variable
  type.
- Fixed a segfault that occurred at **ddcui** startup.  The fault was in a 
  trace message for function ddc_start_watch_displays() which watches for
  displays that are added or removed.
- Fixed a segfault in **ddcutil** initialization because of unexpected
  contents in sysfs.
- Do not use glib function g_byte_array_steal(), which requires glib 2.60.
  ddcutil requires only glib 2.40. 
- Miscellaneous memory leaks
- Double count I2C writes in stats. 

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

