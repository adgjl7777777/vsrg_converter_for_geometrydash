# VSRG Converter

This mod brings a customizable VSRG (Vertical Scrolling Rhythm Game) experience into Geometry Dash as a native overlay. It parses your `.gdr.json` macros and maps them dynamically across multiple keys based on density.

## Features
- Import `.gdr.json` macros directly from the map info screen.
- Auto-syncs to the map's start time and song offset.
- Dynamic lane splitting based on CPS.
- "Comfort Mode" and "Fully Random" drop modes.
- Supports custom note skins and per-map saved configurations.

## Setup
- Import your `.gdr.json` using the button in the level menu.
- Use keybinds (configurable via Custom Keybinds mod) to adjust offset and scroll speed in-game.

### Skin Setup
To use custom note skins, place your image files (.png, .wav) in the following directory:
`C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\config\whitechocolate.vsrg_converter\skins\default`

Required files:
- `holdstart.png`, `holdend.png`, `backnote.png`, `lnbody.png`
- `hit.wav` (Optional hit sound)

## Dependencies
- Geode `v3.7.4`
- `geode.custom-keybinds` `v1.10.0`
