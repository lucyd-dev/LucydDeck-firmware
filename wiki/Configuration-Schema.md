# Configuration Schema — JSON & Action Specifications

Page configuration files are JSON documents that describe the 15-button grid for a single page. Parsing lives in [`ConfigManager.hpp`](../src/config/ConfigManager.hpp) (`parsePageJson`, `parseActionSequence`), and the data structures and the canonical action-string contract live in [`Actions.hpp`](../src/config/Actions.hpp).

## 1. Button Grid & Index Mapping

The display is a **3×5 grid** (3 rows × 5 columns) of 140 px buttons on an 800×480 panel (`UIConfig`, `DisplayManager`).

Button index is computed row-major: **`index = row * COLS + col`**, giving **indices `0..14`**. These indices are the **JSON object keys** of the `buttons` map.

```
Col:      0     1     2     3     4
       ┌─────┬─────┬─────┬─────┬─────┐
 Row 0 │  0  │  1  │  2  │  3  │  4  │
       ├─────┼─────┼─────┼─────┼─────┤
 Row 1 │  5  │  6  │  7  │  8  │  9  │
       ├─────┼─────┼─────┼─────┼─────┤
 Row 2 │ 10  │ 11  │ 12  │ 13  │ 14  │
       └─────┴─────┴─────┴─────┴─────┘
```

## 2. JSON Schema (Page Config)

Only keys actually read by `parsePageJson` are part of the contract. All are optional at the object level; an absent button index simply renders as an empty slot.

| Key | Type | Meaning |
|---|---|---|
| `buttons` | object (map) | maps button index `"0".."14"` → button object |
| `buttons.<id>.imageName` | string | icon **base name, no extension**; firmware appends `.png` |
| `buttons.<id>.label` | string | optional text label shown on the button |
| `buttons.<id>.click` | array | sequence of plain action strings for a short click |
| `buttons.<id>.longPress` | array | sequence of plain action strings for a long press |

### Example page config

```json
{
  "buttons": {
    "0": {
      "imageName": "github",
      "label": "GitHub",
      "click": ["HID_KEY:CTRL+T", "TEXT:https://github.com", "DELAY:500"],
      "longPress": ["MOUSE_CLICK:MIDDLE,2"]
    },
    "1": {
      "imageName": "discord",
      "click": ["discord:mute"]
    },
    "3": {
      "click": ["PAGE:2"]
    }
  }
}
```

Action strings that fail to parse are **dropped** with a warning (they do not abort the page).

## 3. Action-String Grammar

The canonical form is a single string **`namespace:action:args`**. Parsing splits on the **first** `:` only, so `args` may itself contain `:` (important for `TEXT` and plugin commands). Full rules are documented  in [`Actions.hpp`](../src/config/Actions.hpp) and implemented in `parseActionString`.

### Internal actions — consumed by the firmware

| Namespace | Format | Example | Result |
|---|---|---|---|
| `PAGE` | `PAGE:<id>` | `PAGE:2` | `PageAction` — switch page |
| `PROFILE` | `PROFILE:<name>` | `PROFILE:gaming` | `ProfileAction` — switch profile |

Only `PAGE` and `PROFILE` are executed internally (`isInternalAction`). There is **no ID clamp**: `PAGE:<id>` target existence is checked at load time, so an id that has no matching `<id>.json` in the profile simply fails to load.

### Local hardware actions — executed on the device's USB HID interfaces

| Namespace | Format | Example | Firmware effect |
|---|---|---|---|
| `HID_KEY` | `HID_KEY:<KEY>[+<KEY>...]` | `HID_KEY:CTRL+SHIFT+F2` | press combo ~10 ms, release all (`UsbManager::executeHidKey`) |
| `CONTROL_KEY` | `CONTROL_KEY:<NAME>` | `CONTROL_KEY:VOLUME_UP` | consumer-control press/release (`UsbManager::executeControlKey`) |
| `MOUSE_MOVE` | `MOUSE_MOVE:<x>,<y>[,<wheel>]` | `MOUSE_MOVE:-10,0` | relative mouse move (`UsbManager::executeMouseMove`) |
| `MOUSE_CLICK` | `MOUSE_CLICK:<LEFT\|RIGHT\|MIDDLE>[,<count>]` | `MOUSE_CLICK:LEFT,2` | click; count clamped `1..10` (`UsbManager::executeMouseClick`) |
| `DELAY` | `DELAY:<ms>` | `DELAY:300` | blocking delay (`UsbManager::executeDelay`) |
| `TEXT` | `TEXT:<text>` | `TEXT:Hello:World` | types text verbatim, `:`/`,` preserved (`UsbManager::executeText`) |

`HID_KEY` tokens resolve through [`keyMappings.hpp`](../src/config/keyMappings.hpp) (`getKeyValue`): single characters (typed as their ASCII value), special keys (`CTRL`, `SHIFT`, `ALT`, `GUI`, arrows, `ENTER`, `ESC`, `F1`–`F24`, keypad `KP_*`, etc.), falling back to consumer-control names.

### Host / Plugin actions — transparent forwarding

| Namespace | Format | Example | Result |
|---|---|---|---|
| `CMD` | `CMD:<command>` | `CMD:obs:scene:Scene 1` | `CmdAction{args}` — forwarded to host as `obs:scene:Scene 1` (the `CMD:` prefix is stripped) |
| *(any unknown)* | `name:...` | `discord:mute` | `CmdAction{whole string}` — forwarded verbatim |

When a button press produces such an action, the device sends an **`EVT_ACTION_TRIGGERED`** (`0xA0`) frame over the vendor pipe with the command string verbatim (`UsbManager::executeCmd`). The host app listens for `0xA0` device→host frames and dispatches them to its plugin layer; the event is unsolicited and never ACKed. The host-direction `CMD_NAVIGATE` opcode is the host's only way to trigger actions, and it accepts only `PAGE:`/`PROFILE:` payloads; plugin payloads are rejected with `ERR_UNKNOWN_ACTION`.

### Summary table

| Namespace | Consumer | 
|---|---|
| `PAGE`, `PROFILE` | firmware (internal) |
| `HID_KEY`, `CONTROL_KEY`, `MOUSE_MOVE`, `MOUSE_CLICK`, `DELAY`, `TEXT` | firmware USB HID (local) |
| `CMD`, unknown (`discord:*`, `obs:*`, …) | forwarded to host via `EVT_ACTION_TRIGGERED` |

## 4. Malformed-String Behavior

`parseActionString` returns `std::nullopt` for malformed internal actions, which are silently dropped when encountered inside a page JSON `click`/`longPress`:

- empty string
- `PAGE:` with a negative id
- `PROFILE:` with an empty name
- `HID_KEY:` with no resolvable keys
- `CONTROL_KEY:` with an unknown name
- `MOUSE_CLICK:` with a button other than `LEFT`/`RIGHT`/`MIDDLE`

Plugin strings (unknown namespaces) always parse successfully. (Over the wire, a malformed `CMD_NAVIGATE` payload is rejected with `ERR_UNKNOWN_ACTION` — see [USB-Protocol §6](USB-Protocol#6-error-codes).)

## 5. Validation (`parsePageJson`)

- `deserializeJson` must succeed.
- `root["buttons"]` must be a JSON object, else the page fails to load (`ConfigManager`).
- Button indices are parsed from keys via `atoi`, so non-numeric keys become `0` (be careful: use canonical `"0".."14"` keys).
- Unparseable action entries are skipped; the rest of the page still loads.
