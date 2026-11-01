# kbd2trigger

A PS3 SPRX plugin for **webMAN-MOD** that maps USB keyboard keys to DualShock 3 controller triggers.

## Key Mappings

| Keyboard Key | PS3 Input |
|---|---|
| `A` | L2 (analog trigger) |
| `S` | L1 |
| `L` | R1 |
| `;` | R2 (analog trigger) |

## Requirements

- PS3 with CFW or HEN
- [webMAN-MOD](https://github.com/aldostools/webMAN-MOD) installed
- USB keyboard connected to PS3

## Build from Source

### Prerequisites
- [PSL1GHT SDK](https://github.com/ps3dev/PSL1GHT) installed and `PSL1GHT` env var set

### Build
```bash
make
```
Output: `kbd2trigger.sprx`

### GitHub Actions
Every push to `main` triggers an automatic build. Download the compiled `.sprx` from the **Actions** tab → latest run → **kbd2trigger-sprx** artifact.

## Install

1. Copy `kbd2trigger.sprx` to `/dev_hdd0/plugins/` on your PS3
2. Add it to webMAN-MOD boot plugins list via the webMAN setup page
3. Reboot your PS3

## How It Works

The plugin polls the USB keyboard via `sys_io_kbd_read` in a dedicated PPU thread running at ~120 Hz.
It hooks the `cellPadRead` OPD to inject trigger/button inputs into the pad data whenever the mapped keys are held.

## License

MIT
