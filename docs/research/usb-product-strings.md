# USB product and interface strings on Arduino-ESP32 3.1.1

The deck's product string (`iProduct`, string index 2 of the device descriptor) is not
`"LucydDeck"` today; it is the PlatformIO board name `"LucydDeck (Waveshare ESP32S3 Touch LCD
4.3)"`, because `cores/esp32/USB.cpp` defaults `USB_PRODUCT` to `ARDUINO_BOARD` and PlatformIO
defines `ARDUINO_BOARD` from the board JSON `name`. That value is latched inside `USB.begin()`, and
in this build `USB.begin()` is already called from the core's `app_main()` before `setup()`
(`ARDUINO_USB_CDC_ON_BOOT=1`), so `USB.productName("LucydDeck")` in `UsbManager::begin()` would
return `false` and change nothing. The minimal fix is the compile-time default
`-D USB_PRODUCT=\"LucydDeck\"` in `[common] build_flags`. With that, Linux shows `LucydDeck`
(hidapi reads `iProduct` through udev), and Windows still shows `TinyUSB HID`, because on Windows
node-hid's `device.product` comes from `HidD_GetProductString`, which for a composite HID interface
is the **interface** string `iInterface`. Arduino hardcodes that string to `"TinyUSB HID"` in
`USBHID.cpp` with no API to change it, and none exists in 3.2.x or master; the interface string is
therefore not settable on 3.1.1 without patching the core. For Windows, the only shippable fix is an
app-side label keyed on VID/PID.

Unless a row says otherwise, core citations are the pinned tree in
`~/.platformio/packages/framework-arduinoespressif32` (its `package.json` says `3.1.1`), which
matches <https://github.com/espressif/arduino-esp32/tree/3.1.1>.

## Which descriptor field ends up where

For this firmware the composite is: one CDC (two interfaces), one HID interface carrying all four
HID devices as report IDs (keyboard 1, mouse 2, consumer 4, vendor 6, from the enum in
`USBHID.h:30-37`), and no MSC/DFU.

| Descriptor field | Who sets it in 3.1.1 | Linux shows it as | Windows shows it as | App chip shows it |
| --- | --- | --- | --- | --- |
| `iManufacturer` (device, index 1) | `USB_MANUFACTURER`, default `"Espressif Systems"` (`USB.cpp:34-36`), via `USB.manufacturerName()` | udev attribute `manufacturer`; hidapi `manufacturer` (not used by the chip) | manufacturer of the USB device / HID collection | no |
| **`iProduct` (device, index 2)** | `USB_PRODUCT`, default `ARDUINO_BOARD` = board JSON `name` (`USB.cpp:37-39`), via `USB.productName()` | udev attribute `product` = exactly this string | only the composite parent's bus-reported description; **not** the HID collection's product string | **yes on Linux, no on Windows** |
| `iSerialNumber` (device, index 3) | `USB_SERIAL`, `"__MAC__"` expanded to the MAC in `ESPUSB::begin()` (`USB.cpp:157-167`) | udev `serial` | `HidD_GetSerialNumberString` | no |
| **HID interface `iInterface` (index 4 here)** | `tinyusb_add_string_descriptor("TinyUSB HID")` (`USBHID.cpp:200`), used as `TUD_HID_INOUT_DESCRIPTOR(*itf, str_index, ...)` | nothing; only `HID_NAME` in sysfs/dmesg, which hidapi 0.15.0 ignores for USB | `HidD_GetProductString` → `product_string` | **yes on Windows, no on Linux** |
| CDC interface `iInterface` (index 5 here) | `tinyusb_add_string_descriptor("TinyUSB CDC")` (`USBCDC.cpp:32`) | nothing the app reads | the COM port's description | no |
| `iConfiguration` (index 6 here) | `tinyusb_add_string_descriptor("TinyUSB Device")` (`esp32-hal-tinyusb.c:675`) | not used | not used | no |

Indices 4-6 are derived from the core, not documented: the config-descriptor builder walks
`USB_INTERFACE_MSC, DFU, HID, VENDOR, CDC, MIDI, CUSTOM` in order (`esp32-hal-tinyusb.h:85-94`,
`esp32-hal-tinyusb.c:664-675`) and adds the configuration string last, so with only HID and CDC
enabled the order is HID, CDC, configuration.

## 1. Where the product and manufacturer strings are set

`USB_PRODUCT` is a compile-time fallback:

```
cores/esp32/USB.cpp:34-39
#ifndef USB_MANUFACTURER
#define USB_MANUFACTURER "Espressif Systems"
#endif
#ifndef USB_PRODUCT
#define USB_PRODUCT ARDUINO_BOARD
#endif
```

The `ESPUSB` singleton captures it in its constructor (`USB.cpp:129-136`,
`product_name(USB_PRODUCT)`); `USB.productName(const char *)` only mutates that `String` while
`_started` is false (`USB.cpp:312-320`):

```
bool ESPUSB::productName(const char *name) {
  if (!_started) {
    product_name = name;
  }
  return !_started;
}
```

`ESPUSB::begin()` then copies `product_name.c_str()` into `tinyusb_device_config_t` and calls
`tinyusb_init()` (`USB.cpp:157-186`). `_started` is set from that call at `USB.cpp:184`, so the
window for a runtime name closes at `USB.begin()`, not at the first peripheral.

Inside the core, `tinyusb_init()` applies the config to file-static buffers before installing the
driver (`esp32-hal-tinyusb.c:811-847`, `tinyusb_apply_device_config()` at `:723-759`):

```
typedef char tusb_str_t[127];                    // esp32-hal-tinyusb.c:192
static tusb_str_t USB_DEVICE_PRODUCT = "";        // :197
static tusb_str_t USB_DEVICE_MANUFACTURER = "";   // :198
...
snprintf(USB_DEVICE_PRODUCT, 126, "%s", config->product_name);       // :725
snprintf(USB_DEVICE_MANUFACTURER, 126, "%s", config->manufacturer_name); // :729
```

The descriptors point at those buffers:

```
.iManufacturer = 0x01, .iProduct = 0x02, .iSerialNumber = 0x03   // :221-223
#define MAX_STRING_DESCRIPTORS 20                                // :231
static uint32_t tinyusb_string_descriptor_len = 4;               // :232
static char *tinyusb_string_descriptor[20] = { language, manufacturer, product, serial }; // :233-239
```

and `tud_descriptor_string_cb()` converts them to UTF-16 at request time, capping at 126
characters (`esp32-hal-tinyusb.c:368-393`). `tinyusb_add_string_descriptor()` appends further
strings and returns their index, failing (returning 0) once 20 are used
(`esp32-hal-tinyusb.c:851-858`).

When the strings are latched: the descriptor only reads the static buffers when the host asks, but
the buffers are filled once, inside `USB.begin()` -> `tinyusb_init()` -> `tinyusb_apply_device_config()`.
`USB.productName()` after that has no effect (it returns `false`).

The peripheral order in this firmware (`src/usb/UsbManager.cpp:65-70`,
`Serial.begin(); keyboard.begin(); consumer.begin(); mouse.begin(); customHIDDevice.begin();
USB.begin();`) would be fine if `USB.begin()` were the first start, because `USBHID::begin()` and
`USBCDC::begin()` only create semaphores and register the object (`libraries/USB/src/USBHID.cpp:289-294`,
`cores/esp32/USBCDC.cpp:144-155`). It is not fine in this build, because the strings are latched
before `setup()` ever runs:

```
cores/esp32/main.cpp:91-102
#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
  Serial.begin();
#endif
...
#if ARDUINO_USB_ON_BOOT && !ARDUINO_USB_MODE
  USB.begin();
#endif
```

`boards/waveshare_esp32s3_touch_lcd_4_3.json` sets `-DARDUINO_USB_MODE=0` and
`-DARDUINO_USB_CDC_ON_BOOT=1`, and `.pio/build/debug/idedata.json` in
`/home/lucyd/Code/LucydDeck-firmware` confirms both are on the command line. ESP-IDF runs C++
static constructors before `app_main()` (`esp_system/startup.c:69` and `:199-204` in
<https://github.com/espressif/esp-idf/blob/v5.3.2/components/esp_system/startup.c>), so
`usb::UsbManager usbManager` (`src/main.cpp:19`) and its four HID objects, plus the global
`USBHIDVendor Vendor` in `src/usb/CustomHIDDevice.cpp:10`, exist before `app_main()` calls
`USB.begin()`; that is why the composite enumerates at all. `setup()` and `UsbManager::begin()`
run afterwards, and their `USB.begin()` is a no-op that returns the already-true `_started`.

Which is more robust, runtime or compile-time: for this firmware, **compile-time**. A call in
`UsbManager::begin()` silently does nothing. A call from a global constructor is unspecified-order
against `ESPUSB USB` (`cores/esp32/USB.cpp:352`) and can be overwritten by it. `USB_PRODUCT` is read
by `USB.cpp` only, so `-D USB_PRODUCT=\"LucydDeck\"` is the one form that cannot lose the race.
There is no other runtime API: `USB.h` exposes `productName()`
(`cores/esp32/USB.h:82-83`) and nothing else.

## 2. Where the HID interface string comes from

`tusb_hid_load_descriptor()` is Arduino's loader for the single `USB_INTERFACE_HID` interface:

```
libraries/USB/src/USBHID.cpp:194-215
extern "C" uint16_t tusb_hid_load_descriptor(uint8_t *dst, uint8_t *itf) {
  ...
  uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB HID");   // :200
  ...
  TUD_HID_INOUT_DESCRIPTOR(*itf, str_index, tinyusb_interface_protocol, ...)  // :209-211
```

`TUD_HID_INOUT_DESCRIPTOR` puts `_stridx` into `bInterfaceString` (`tinyusb/src/device/usbd.h:294-296`
in the bundled TinyUSB 0.18.0, `framework-arduinoespressif32-libs/esp32s3/include/arduino_tinyusb/tinyusb/src/tusb_option.h:33-38`).

(a) One string for all HID interfaces — in 3.1.1 there is only one. `USBHID`'s constructor registers
the interface exactly once (`USBHID.cpp:277-288`); every further `USBHIDDevice` is packed into one
concatenated report descriptor by `tinyusb_on_add_descriptor()` (`USBHID.cpp:107-152`,
`tinyusb_load_enabled_hid_devices()` at `:154-192`). The four devices here therefore share a single
`iInterface` and are distinguished only by report ID (`libraries/USB/src/USBHID.h:30-37`;
`USBHIDVendor::_onGetDescriptor()` emits `HID_REPORT_ID_VENDOR` = 6, `USBHIDVendor.cpp:61-64`).

(b) No public API. `class USBHID` (`libraries/USB/src/USBHID.h:73-88`) and the concrete devices
(`USBHIDKeyboard.h`, `USBHIDConsumerControl.h`, `USBHIDMouse.h`, `USBHIDVendor.h:39-72`) expose
`begin`, `end`, `ready`, `SendReport`, `onEvent`, `addDevice` and Stream methods only — no name or
string-index setter anywhere in `libraries/USB/src`.

(c) Added at config-descriptor build time, inside `USB.begin()`: `tinyusb_init()` calls
`tinyusb_load_enabled_interfaces()` (`esp32-hal-tinyusb.c:818-823`), which walks the enabled
interface mask and invokes each registered loader (`:664-683`), so `"TinyUSB HID"` is added only
then, not when `USBHIDKeyboard` is constructed. The CDC does the same with `"TinyUSB CDC"`
(`USBCDC.cpp:31-35`).

## 3. Which string each OS shows the app

The app reads names through node-hid 3.4.0 (`package.json:28`), which vendors hidapi 0.15.0
(`node_modules/node-hid/hidapi/VERSION`), and builds its label from `device.product`
(`~/Code/LucydDeck/electron/main/services/UsbService.ts:114-116`, used for the connected status
label at `:284-287`). node-hid maps hidapi's `product_string` to `product`
(`node_modules/node-hid/src/devices.cc:50-52`). So the question is what each hidapi backend puts in
`product_string`.

**Linux (hidraw).** hidapi 0.15.0 does **not** use `HIDIOCGRAWNAME`. For USB devices it walks up to
the `usb_device` and copies its udev strings:

```
hidapi/linux/hid.c:736-742
usb_dev = udev_device_get_parent_with_subsystem_devtype(raw_dev, "usb", "usb_device");
...
cur_dev->manufacturer_string = copy_udev_string(usb_dev, "manufacturer");
cur_dev->product_string      = copy_udev_string(usb_dev, "product");
```

`copy_udev_string()` (`linux/hid.c:190`) reads a sysfs/udev attribute; the `product` attribute of a
USB device is `udev->product`, populated in `usb_new_device()` from `descriptor.iProduct`
(`drivers/usb/core/hub.c:2439-2442` in
<https://github.com/torvalds/linux/blob/v6.6/drivers/usb/core/hub.c>) and exposed by
`dev_string_attrs` (`drivers/usb/core/sysfs.c:141-142`, `:819-836`). On Linux the app therefore
shows **`iProduct` and only `iProduct`**.

The kernel also has a second name, which the older hidapi path used and which still shows up in
`dmesg` and sysfs: `hid->name` is `iManufacturer` + `" "` + `iProduct`, built in `usbhid_probe()`
(`drivers/hid/usbhid/hid-core.c:1389-1398`), exported as `HID_NAME` (`drivers/hid/hid-core.c:2729`,
`hid_uevent()`) and returned by the `HIDIOCGRAWNAME` ioctl (`drivers/hid/hidraw.c:468-472`). hidapi
0.15.0 parses that uevent name (`linux/hid.c:572-620`) but only keeps it for uhid, Bluetooth and
I2C devices; the USB branch overwrites it with the udev `product` attribute above. That is why the
app's chip does not read `Espressif Systems <product>`.

**Windows.** hidapi reads the HID collection's product string through
`HidD_GetProductString` (`hidapi/windows/hid.c:790`, with only manufacturer and serial repaired
from devnode properties in `hid_internal_get_usb_info()`, `:463-563`). Microsoft documents what that
string is for a USB device:

> The **iProduct** member of a USB_DEVICE_DESCRIPTOR structure for a particular interface is set by
> the USB common class generic parent driver based on the following rules: if the **iInterface**
> member of the USB_INTERFACE_DESCRIPTOR structure for the interface is nonzero, the **iProduct**
> member ... is set to the **iInterface** member ...
>
> — <https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/hidsdi/nf-hidsdi-hidd_getproductstring>

This device is composite, so `usbccgp` creates a child devnode per interface and applies exactly
that rule. The HID interface's `iInterface` is `"TinyUSB HID"`, so `HidD_GetProductString` returns
`"TinyUSB HID"` and the deck's `iProduct` never reaches the app. On Windows the chip shows the
**HID interface string, not the product string**.

Consequences:

- `USB.productName()` (or `-D USB_PRODUCT`) is enough for Linux, and is what Windows shows as the
  composite parent's bus-reported description, but **not** for the app's chip on Windows.
- Windows only changes the HID name if `iInterface` changes, which needs a core patch.
- The app already falls back to `"LucydDeck"` when `product` is missing (`UsbService.ts:115`); that
  fallback does not fire on Windows because the string is present, just wrong.

**Reconciling the Linux string with the issue text.** `boards/waveshare_esp32s3_touch_lcd_4_3.json`
says `"name": "LucydDeck (Waveshare ESP32S3 Touch LCD 4.3)"` (no space in `ESP32S3`), PlatformIO
turns that into the `ARDUINO_BOARD` define
(`framework-arduinoespressif32-libs/esp32s3/pioarduino-build.py:513`, and the same in
`framework-arduinoespressif32/platform.txt:148`), and `.pio/build/debug/idedata.json` records
`ARDUINO_BOARD="LucydDeck (Waveshare ESP32S3 Touch LCD 4.3)"`. `USB_PRODUCT` is that literal
(`USB.cpp:37-39`), so the only string this firmware can put in `iProduct` is
**`LucydDeck (Waveshare ESP32S3 Touch LCD 4.3)`**. The issue's `ESP32 S3` is a paraphrase; the
space would also be visible in `lsusb -v`, so the check below settles it. Before commit `dfeafe4`
the board JSON name was plain `"LucydDeck"`, so a deck flashed from a pre-0.2.0 build would have
shown just `LucydDeck`.

## 4. Can the HID interface string be set without patching the core?

**Weak `tud_descriptor_string_cb` override — possible in principle, a trap in practice.** The core
definition is `__attribute__((weak))` (`esp32-hal-tinyusb.c:368`), so a sketch can supply a strong
one instead. But it cannot delegate: the string table
(`tinyusb_string_descriptor[20]`, `tinyusb_string_descriptor_len`, the `USB_DEVICE_*` buffers) is
file-static (`:192-239`), there is no exported getter, and the symbol being replaced is the same
symbol, so there is nothing to call. An override would have to reimplement index 0 (the
`"\x09\x04"` language blob), 1 manufacturer, 2 product, 3 serial (already expanded from `__MAC__`
by `USB.begin()` at `USB.cpp:160-166`), and 4-6 (`TinyUSB HID`, `TinyUSB CDC`, `TinyUSB Device`),
plus the UTF-16 conversion and 126-character cap (`:373-391`). The index of the HID interface is a
runtime detail — it depends on which interfaces are enabled and on the core's enumeration order — so
the override would hardcode `4` and silently rename the wrong string the day an interface is added
or the core's order changes. It also takes the CDC string with it, or not, depending on how it is
written, with no compile-time check either way. It compiles, it can work, and it is a copy of core
internals.

**`tusb_hid_load_descriptor` override — no.** It is a normal `extern "C"` definition in
`USBHID.cpp:194`, not weak. The registration that matters happens in `USBHID`'s constructor
(`USBHID.cpp:285`), inside the same translation unit; a second strong definition is a duplicate
symbol, and even a "winning" replacement would not alter the `add_string_descriptor()` call already
inside the loader's own body.

**Registering the vendor HID through the ESP-IDF/Arduino TinyUSB API — possible, disproportionate.**
`tinyusb_enable_interface()` / `tinyusb_enable_interface2()` and `tinyusb_add_string_descriptor()`
are exported (`cores/esp32/esp32-hal-tinyusb.h:98-104`), so a sketch can register
`USB_INTERFACE_HID` with its own loader and its own string index. The costs are the whole HID layer:
report descriptors, report-ID routing, `tud_hid_descriptor_report_cb()` and the `tud_hid_*` data
callbacks (all defined in `USBHID.cpp:220-277`), the `SendReport` mutex, and the `Stream` wrapper
the protocol uses. It also collides with the library: constructing any `USBHID` registers the
interface first, and a second registration is rejected (`esp32-hal-tinyusb.c:782-795`). That is a
rewrite of `libraries/USB/src/USBHID.cpp`, not a configuration change.

**Newer cores and upstream TinyUSB — no help.** `libraries/USB/src/USBHID.cpp:200` still reads
`tinyusb_add_string_descriptor("TinyUSB HID")` on tags `3.2.0` and `3.3.0` and on `master`, and
`USBHID.h` on `master` still has no setter. The feature request for exactly this
(<https://github.com/espressif/arduino-esp32/issues/11394>, still open) documents the only current
workaround as editing `USBCDC.cpp` / the core in place. TinyUSB itself has no interface-name API:
the application owns all strings through `tud_descriptor_string_cb()`
(`tinyusb/src/device/usbd.h:137`), and Arduino is that application.

**Conclusion: not settable on 3.1.1 without patching the core.** Fallbacks, in order of cost:
accept `TinyUSB HID` on Windows; label the device in the app when VID/PID match (already the
existing fallback path, `UsbService.ts:115`); an INF or registry rename is not an option — it needs
a signed driver package, does not ship with an Electron app, and would have to be repeated on every
machine.

## 5. Recommended change for #22

One line, in `[common]` so both environments get it:

```diff
 [common]
 build_flags =
 	-std=gnu++2b
 	-I src
+	-D USB_PRODUCT=\"LucydDeck\"
 	-D PROTOCOL_VERSION=1
```

Do **not** add `USB.productName("LucydDeck")` to `UsbManager::begin()`. With
`ARDUINO_USB_CDC_ON_BOOT=1` the core starts USB in `app_main()` (`cores/esp32/main.cpp:100-102`)
before `setup()`, so `_started` is already true, `productName()` returns `false`
(`USB.cpp:312-320`), and the name stays the board name. If the product name ever has to be a
runtime decision, the only clean routes are a core patch or
`-DARDUINO_USB_CDC_ON_BOOT=0`, and the latter turns `Serial` into `HWCDC` on the S3 and changes the
composite, which this firmware does not want.

Caveats, verified:

- Length. The core buffer is 127 bytes and is filled with `snprintf(..., 126, ...)`
  (`esp32-hal-tinyusb.c:192, 725`), so 125 characters plus NUL; the descriptor callback caps at 126
  UTF-16 characters (`:381-384`). `USB.productName()` itself does no length checking in 3.1.1 —
  the `bool` only reports whether USB had not started yet.
- VID/PID are untouched: `variants/esp32s3/pins_arduino.h:7-8` defines `USB_VID 0x303a` and
  `USB_PID 0x1001`; `USB_PRODUCT` feeds `product_name` only (`USB.cpp:129-136`, `:170`), and the
  endpoint layout is decided by the interface loaders, which this flag does not reach.
- Host matching is name-independent: `matchDevice()` checks VID, PID and usage page/usage
  (`~/Code/LucydDeck/shared/protocol.ts:312-343`).
- `RESP_DEVICE_INFO.board` is `BOARD_NAME` (`platformio.ini [common]`), not `USB_PRODUCT`, so the
  firmware manifest lookup is unaffected.
- The CDC interface keeps its own `"TinyUSB CDC"` string (`USBCDC.cpp:32`); Windows may show that
  for the COM port. It is not what the app opens (the HID vendor interface is), and it does not
  matter to the updater: `usb_persist_restart(RESTART_BOOTLOADER)` re-enumerates into the ROM
  bootloader, whose descriptors come from the bootloader, not from this image
  (`cores/esp32/esp32-hal-tinyusb.c:604-620`, `src/usb/UsbManager.cpp:91-99`).
- Windows: `iProduct` is the deck's `IRP_MN_QUERY_DEVICE_TEXT` string, i.e. what the composite parent
  reports as `DEVPKEY_Device_BusReportedDeviceDesc`
  (<https://learn.microsoft.com/en-us/windows-hardware/drivers/install/devpkey-device-busreporteddevicedesc>),
  so the parent's displayed description changes. Hardware IDs
  (`USB\VID_303A&PID_1001&MI_xx`) and the instance ID do not — the serial number is the MAC and is
  unchanged — so PnP matching, driver binding and the port the app opens stay the same. The new
  description appears after re-enumeration (replug or disable/enable), not on the running session.
- Optional, same mechanism: `-D USB_MANUFACTURER=\"LucydDev\"` if the manufacturer should stop
  saying `Espressif Systems`. It does not affect either OS's HID name.

With this change the deck reports `iProduct = "LucydDeck"`, so the app's chip becomes
`LucydDeck (0x303a:0x1001)` on Linux and still `TinyUSB HID (0x303a:0x1001)` on Windows. If #22's
acceptance criterion is the chip reading `LucydDeck` on both systems, the issue also needs the
host-side label described above; this flag is the whole firmware half.

## What stays open

- **Windows behaviour is documented but not measured here** (no Windows host in this
  environment). The inference is: `HidD_GetProductString` → `iInterface` per the Microsoft remarks,
  and the user's report that Windows already says `TinyUSB HID` while Linux says the board name.
  The check below confirms it on hardware.
- Whether clearing `iInterface` (index 0) would make Windows fall back to the device's `iProduct`.
  The Microsoft rule suggests it would, but that needs a core patch to test and is not actionable
  for 3.1.1.
- macOS: hidapi reads `IOHIDDeviceGetProperty(kIOHIDProductKey)`
  (`hidapi/mac/hid.c:423-425`), but whether IOKit fills that from `iProduct` or `iInterface` is not
  verified here. Settle it with the check below.
- The exact spacing in the observed Linux string (`ESP32 S3` vs `ESP32S3`). The firmware can only
  emit the board JSON literal; `lsusb -v` shows which build is actually flashed.
- The string-index order (HID 4, CDC 5, configuration 6) is derived from the core's code and is not
  a documented contract.
- The app's Windows label. If a single cross-platform name is wanted, the app should prefer a
  VID/PID-keyed name over `device.product`, which is a host change outside this repository.

## Hardware checks

Linux:

```bash
lsusb -v -d 303a:1001 | grep -E "iProduct|iInterface|iManufacturer"
lsusb -d 303a:1001                      # one-line product string
udevadm info -q property -n /dev/hidrawN | grep -E "^(PRODUCT|ID_MODEL|ID_MODEL_FROM_DATABASE)="
cat /sys/class/hidraw/hidrawN/device/uevent    # HID_NAME = iManufacturer + " " + iProduct
node -e 'console.log(require("node-hid").devices().filter(d=>d.vendorId===0x303a))'
```

Expect `iProduct` `LucydDeck` / `iInterface` `TinyUSB HID` after the change, and the app chip
`LucydDeck (0x303a:0x1001)`.

Windows:

```powershell
Get-PnpDevice -PresentOnly | Where-Object InstanceId -like "USB\VID_303A&PID_1001*"
Get-PnpDeviceProperty -InstanceId "USB\VID_303A&PID_1001\<serial>" -KeyName DEVPKEY_Device_BusReportedDeviceDesc
node -e "console.log(require('node-hid').devices().filter(d=>d.vendorId===0x303a))"
```

Device Manager, the HID-compliant device under Human Interface Devices, Details tab: `Bus reported
device description` should read `TinyUSB HID` before and after the change (`Device description` will
stay the class name, `HID-compliant device`), while the parent under Universal Serial Bus devices
reports the new `LucydDeck`. The app chip is expected to stay `TinyUSB HID (0x303a:0x1001)` until
the core string or the app label changes.
