# BLE Advertisement Test for OFW 1.4.3

This branch builds a minimal, non-connectable BLE advertisement test app using
Official Firmware 1.4.3's public extra beacon API. It replaces the built app with
**BLE Adv Test**; it does not restore the original vendor spam features.
The original sources remain available for reference and are excluded from the build.

## Usage

Copy `dist/ble_spam.fap` to `/apps/Bluetooth/` on the Flipper Zero SD card.
Open **Apps → Bluetooth → BLE Adv Test**. Press OK to start/stop, or Back to stop and exit.
The app starts in the stopped state.

It sends a fixed manufacturer-specific advertisement every 200 ms at approximately
−20.85 dBm, using a static random address. The Company ID is `0xFFFF` and the
manufacturer data is `54 45 53 54 01` (`TEST` followed by version `01`).
Use a BLE scanner to verify reception and that its last-seen timestamp stops updating
after Stop. The app logs startup, configuration, start/stop and cleanup via `log info`.

## Implementation

`test_app/` contains the app's runtime sources, not automated tests.
`application.fam` includes only these sources. The backend validates the 31-byte
legacy advertising limit and AD structure lengths before calling
`furi_hal_bt_extra_beacon_set_config`, `set_data`, `start` and `stop`.
HAL operations run on the app thread. On exit, the app stops its beacon,
restores any previously configured stopped beacon data/configuration and frees its UI.

The original code located an internal HCI function by scanning flash, with a fixed
address fallback. A changed function address/ABI is a likely cause of the reported
crash at first transmission, but the original crash was not reproduced or confirmed.
The new backend uses the [official OFW 1.4.3 API](https://github.com/flipperdevices/flipperzero-firmware/blob/1.4.3/targets/furi_hal_include/furi_hal_bt.h).

## Build and validation

With uFBT installed:

```bash
ufbt update --url=https://update.flipperzero.one/builds/firmware/1.4.3/flipper-z-f7-sdk-1.4.3.zip
ufbt -c
ufbt
```

Output: `dist/ble_spam.fap` (f7, API 87.1). Build and API checks passed with uFBT 0.2.6.
An OFW 1.4.3 device passed 100 Start/Stop cycles and 10 exits/reopens during advertising
without a crash. Windows received the fixed payload; no matching packets were
observed during a four-second scan after Stop and a one-second grace period.
Exact over-air interval, long-term operation and normal Bluetooth connection coexistence remain unverified.

## Limitations

- Extra beacon is a firmware-owned singleton. Start is refused if it is already active;
  concurrent changes by another service cannot be excluded atomically.
- OFW 1.4.3's [extra beacon implementation](https://github.com/flipperdevices/flipperzero-firmware/blob/1.4.3/targets/f7/ble_glue/extra_beacon.c)
  can leave its mutex locked on HCI errors. After a HAL failure, this app disables
  further HAL calls and displays a reboot message. A failed Stop cannot guarantee
  transmission has ended; reboot the device in that case.
- There is no API to restore a never-configured beacon state; the stopped test
  configuration remains if no previous configuration existed.

## Credits

[Original app by WillyJL](https://github.com/Flipper-XFW/Xtreme-Firmware/tree/dev/applications/external/ble_spam).
Research and testing: WillyJL, ECTO-1A and Spooks4576. OFW port/backport: noproto and JulanDeAlb.
The existing license is retained.
