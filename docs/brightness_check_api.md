# Brightness Support Test API

## Overview

The `ddca_check_brightness_support()` function provides the most reliable method to determine if a display actually supports brightness control. Unlike other methods that only check capabilities or metadata, this function performs a real set operation to verify that brightness control actually works.

## Function Signature

```c
DDCA_Status
ddca_check_brightness_support(
      DDCA_Display_Handle  ddca_dh,
      bool*               is_supported,
      uint16_t*           current_value,
      uint16_t*           max_value);
```

## Parameters

- `ddca_dh`: Display handle for the display to test
- `is_supported`: Pointer to return whether brightness control is supported
- `current_value`: Pointer to return current brightness value (0-100)
- `max_value`: Pointer to return maximum brightness value (0-100)

## Return Value

- `0`: Test completed successfully
- Non-zero: Test failed (check error details)

## How It Works

The function performs the following steps:

1. **Reads current brightness value** and maximum value from the display
2. **Checks if maximum value is 0** - if so, brightness control is not supported
3. **Calculates a test value** (current value ± 1, depending on available range)
4. **Saves original verification setting** to restore later
5. **Disables verification** to avoid verification failures affecting the test
6. **Attempts to set the test value** using DDC/CI
7. **Verifies the test value was actually set** by reading it back immediately
8. **Restores the original value** to minimize visual impact
9. **Re-enables verification** setting

## Key Verification Logic

The most important improvement in this function is the verification step (step 7). After setting the test value, the function immediately reads back the current brightness value to verify that the set operation actually took effect. This distinguishes between:

- **True support**: Set command succeeds AND the value actually changes
- **False support**: Set command succeeds but the value doesn't change (display ignores the command)

## Important Notes

- **Visual Impact**: This function will briefly change the display brightness during testing
- **Immediate Restoration**: The original brightness value is restored immediately after the test
- **Minimal Change**: The test uses the smallest possible change (±1) to minimize visual impact
- **Most Reliable**: This is the most accurate method because it tests actual functionality
- **Verification Logic**: The function verifies that the test value was actually set before determining support

## Usage Example

```c
#include "ddcutil_c_api.h"

DDCA_Display_Handle dh;
// ... open display ...

bool is_supported;
uint16_t current_value, max_value;

DDCA_Status rc = ddca_check_brightness_support(dh, &is_supported, &current_value, &max_value);
if (rc == 0) {
    if (is_supported) {
        printf("Display supports brightness control\n");
        printf("Current brightness: %d\n", current_value);
        printf("Maximum brightness: %d\n", max_value);
    } else {
        printf("Display does not support brightness control\n");
    }
} else {
    printf("Test failed: %s\n", ddca_rc_name(rc));
}
```

## When to Use

Use this function when you need to:

- **Verify brightness control works** before providing brightness adjustment features
- **Handle displays that report support but don't actually work**
- **Provide accurate feedback to users** about brightness control capability
- **Avoid runtime failures** in brightness control applications

## Alternative Methods

Other methods for checking brightness support are less reliable:

- **Capabilities string**: May report support even when set operations fail
- **Feature metadata**: Only checks if the feature is defined as writable
- **Simple read**: Only verifies the feature can be read, not written

## Demo Program

A demo program is available at `src/sample_clients/demo_check_brightness_support.c` that shows how to use this API to test all connected displays.

## Compilation

To compile the demo program:

```bash
cd src/sample_clients
make demo_test_brightness
```

## Running the Demo

```bash
sudo ./demo_test_brightness
```

The demo will test all connected displays and report whether each one supports brightness control.
