# zmk-input-processor-profile-scaler

A [ZMK](https://zmk.dev) input processor that scales pointing device XY output by a different ratio per Bluetooth profile. Use it to tune cursor speed independently for each connected device — a high-resolution monitor needs a different ratio than a phone screen.

When connected via USB, the fallback ratio is used.

## Getting Started

### `config/west.yml`

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    # --- copy from here ---
    - name: bunnyspa
      url-base: https://github.com/bunnyspa
    # --- to here ---
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    # --- copy from here ---
    - name: zmk-input-processor-profile-scaler
      remote: bunnyspa
      revision: main
    # --- to here ---
  self:
    path: config
```

### `<keyboard>.conf`

```ini
CONFIG_ZMK_POINTING=y
CONFIG_ZMK_INPUT_PROCESSOR_PROFILE_SCALER=y
```

### `<keyboard>.overlay` or `<keyboard>.dtsi`

> [!NOTE]
> If you were using `&zip_xy_scaler`, remove it — using both in the same chain will apply scaling twice.

Include the template and override with your ratios:

```c
#include <input/processors/profile_scaler.dtsi>

&profile_scaler {
    profiles = <
        // profile  num  den
           0        1    2
           1        1    1
           2        2    3
    >;
    // USB and unlisted profiles
    fallback-numerator = <1>;
    fallback-denominator = <2>;
};
```

Then add `&profile_scaler` to your input listener. `pointing_device_listener` is a placeholder — use your actual listener name (e.g. `trackball_listener` for PMW3610):

```c
&pointing_device_listener {
    input-processors = <&profile_scaler>;
};
```

## Parameters

All configuration lives in the node — no per-use arguments.

**`profiles`** *(optional)* — flat array of `(profile numerator denominator)` triplets. Profile indices can be listed in any order and gaps are allowed — unlisted profiles use the fallback ratio. If omitted entirely, all connections use the fallback ratio.

**`fallback-numerator` / `fallback-denominator`** *(required)* — ratio used for USB connections and any BT profile not listed in `profiles`.

## Remainder accumulation

Sub-count fractional values are accumulated across events and applied on subsequent frames. This means a ratio like `1/3` at 600 CPI still produces smooth output — no counts are silently discarded.