# Flasher

<img src='PCB/Flasher/Flasher.png' align=right width=33%>

The Flasher board is designed to allow simple flashing of code on to an ESP device, and run the code, and confirm self test results on LEDs.

The main use case is a factory functional test with feedback of pass/fail via LEDs.

The idea is a factory worker can simply plug the lead in to a target device, see a row of LEDs light one by one, then all go green (or red if failed). This simple operation should be foolproof!

## Connections

There is a USB-C for power of the flasher and connected board, this would typically be connected to a USB charger (e.g. 2A+). This can also be used for serial debug/flashing of the flasher board itself if needed.

There is a USB-A connector which can be used to connect via USB to a target device. This could be via a normal USB lead (e.g. USB-A to USB-C) or via tag-connect lead such as [TC2030-USB-NL](https://www.tag-connect.com/product/tc2030-usb-nl) or a tag-connect USB to serial lead such as TC2030-NL-FTDI lead.

There is a 6 pin RJ12 connector which is primarilly intended to be used with a tag-connect [TC2030-MCP-NL-10](https://www.tag-connect.com/product/tc2030-mcp-nl-10-6-pin-cable-with-rj12-modular-plug-for-microchip-icd-10-version) lead. This can be used for direct USB flashing boards with USB TC2030 pads.

|Pin|USB|Serial|
|---|---|------|
|1|5V|3.3V|
|2|Loop|Boot|
|3|D-|Rx|
|4|D+|Tx|
|5|GND|GND|
|6|Loop|Boot|

Note *loop* is a connection between contacts 2 and 6 to allow a target device to use this for a loop back ATE test. These are not used for USB.

For serial this is slightly non standard as tag-connect has `DTR` on 2 and `RTS` on 6, but these are looped and used for *boot* (i.e. `DTR`). For such usage the *boot* would be connected to GPIO0 allowing power control of the 3.3V to reset the device rather than using a `RTS` connection. If `RTS` is connected on the device it may be necessary to disconnect pin 6 to avoid holding a device in reset after power on with *boot* low for programming.

Note also that the board is desigend to provide 5V (USB incoming pass through) or 3.3V to the power output so it can be used with devices needing either voltage.

**Note that at present direct serial is not supported (due to flasher library)**

## Basic operation

The basic operation is as follows...

1. Connect power to flasher (USB-C)
2. Ensure SD card inserted (LED shows green)
3. Connect to target device
4. Single orange LED for a moment while checking device
5. Row of LEDs progress blue to show flashing progress (unless already flashed)
6. Row of LEDs go all green as ATE test pass
7. Stays green until device removed and ready for next device

Failure modes

1. Nothing happens when connecting to target device (this indicates serious issue with target device) - not that common for *nothing* to happen
2. Single red LED indicating issue connecting to device (or wrong device type if flashing)
3. Either immediately (if already flashed), or after blue LED sequence, all LEDs show red (this indicates device flashed but failed ATE)
4. Stays red until device removed and ready for next device
 
## LEDs

An LED by the SD card shows...

|Colour|Meaning|
|------|-------|
|🔵|Card not mounted, waiting (e.g. during firmware upgrade)|
|🟡|Card not inserted|
|🟢|Card inserted, OK|
|🔴|Card did not mount (flashing if file error)|

A row of 10 LEDs indicates progress, can be changing one LED at a time from 0% to 100% of progress. The first three show one LED which is one of the 10, indicating which manifest `0` to `9` is selected. Later design has a pattern of LEDs to show the number `0` to `9`. those shown with ... are moving from one colour to the other to show progress.

|Colour|Meaning|
|------|-------|
|🟣|Waiting for device|
|🟠|USB connected, checking|
|🔴|Bad USB connection, or flashing, or file issue such as wrong chip|
|🔵🔵🔵🔵🔵...⚫⚫⚫⚫⚫|Flashing device progress|
|🟢🟢🟢🟢🟢🟢🟢🟢🟢🟢|ATE passed|
|🔴🔴🔴🔴🔴🟢🟢🟢🟢🟢|Code seems to be running but no ATE status|
|🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴|ATE failed, may show fewer red LEDs depending on failure|
|⚫⚫⚫⚫⚫...🔵🔵🔵🔵🔵|Erasing device progress|
|🟡🟡🟡🟡🟡...⚫⚫⚫⚫⚫|Flasher s/w upgrade progress|
|🟡🟡🟡🟡🟡🟡🟡🟡🟡🟡|All flashing - this is target firmware upgrade checks in progress, wait a moment|

## Updates

If the flasher is on-line it will check for s/w update, and upgrade, and also check for updates to manifest and files.

## Button

1. When no target device, cycles an LED in the row of 10 LEDs to select s/w to use from SD card for next flash. Note this only cycles for manifests that are present on SD card, so no action if only one manifest.
2. When target device connected (pass or fail or even flashing), starts a full flash erase and program cycle.

## SD card file format

The card contains files to flash, and manifest files. The manifest files are called `manifestN.json` where `N` is the manifest `0` to `9`. This contains details of the flash operation, and is a JSON object with the following fields. You can create any files and directories. Do not create `LOG` or `DOWNLOAD` as used interally.

|Field|Meaning|
|-----|-------|
|`"chip"`|The chip type (see below)|
|`"voltage"`|Either `3.3` or `5` (default `5`)||
|`"erase"`|If `true` then a full erase is always done first regardless of state of target device|
|`"check"`|If `false` then goes direct to flashing without checking first|
|`"button"`|If `false` then button erase function is disabled|
|`"flash"`|An array of files to flash - see below|
|`"url"`|The URL for this manifest file|
|`"id"`|The first part expected for `ID:` sent from target|
|`"version"`|The second part expected for `ID:` sent from target (normally use `"build"` in one file to set this)|
|`"build"`|The third part expected for `ID:` sent from target (normally use `"build"` in one file to set this)|
|`"setting"`|A object to be sent to the target after `ID:` is seen. target should reply `OK:` if accepted, or `ERR:` if not, or reboot (first time) if needed to apply settings.|

The `"flash"` array is objects with the following...

|Field|Meaning|
|-----|-------|
|`"address"`|The address to which it is to be flashed - can be a number, or a string. If a string then assumed to be hex. Default 0|
|`"filename"`|The filename on the SD card|
|`"url"`|The URL for this file|
|`"build"`|This is expected on only one file, and can be `true` for normal build info offset `32`, or can be a number specifying a different offset - if set then do not include `"version"` and `"build"` at top level - you can also omit `"id"` at top level.|

The `"url"` allows a file to be checked for update, using `If-Modified-Since"`, and replaced. This can be `http://` or `https://` (recommended Let's Encrypt cert for https). It is faster if all files are on the same host, especially if using `https://`.

The `"chip"` is based on chip type, e.g. `ESP32S3`, `MC` for multi core, `PICO` if known, flash `Nx`, and PSRAM `Rx`, e.g. `ESP32S3MCN4R2`. This has to match the device, else a file error is shown. See serial log for the identified chip type. This works for chip type and flash size for all, and addition info (like PSRAM) for some chips (currently ESP32S3).

## Target code

The target code is expected to provide simple text line output with information for ATE. This should be done using a simple `printf` and not an `ESP_LOG` as (a) the logs can be disabled, and (b) it has a prefix, and (c) the logs have colour codes.

|Output|Meaning|
|------|-------|
|`ID:`|Initial ID containing *appID*, *space*, *version*, *space*, *ISO build date*|
|`ATE: PASS`|ATE tests passed|
|`ATE: FAIL`|ATE tests failed|
|`ERR:`|Error in settings|
|`OK:`|Settings accepted and stored|

The `ATE:` messages are the only ones required.

If `ID:` is sent then the *appname*+*buildsuffix* is checked against `"id"` field - if no match then this is a file error. If `"id"` is not set but `"build"` is in the file, then the *appname* has to match the start of what is sent in `ID:`.
If `ID:` is sent and *version* and/or *build* are set, these are checked, and if a mismatch then flashing is done regardless of ATE pass/fail.

On receipt of `ID:` any object in `"setting"` is sent to the target so settings can be applied. An `OK:` or `ERR:` response is expected but the target may reboot (first time) if needed to apply the new settings.
