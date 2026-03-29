# iGamePlus

A WHDLoad game launcher for Amiga, forked from [iGame](https://github.com/MrZammler/iGame) by Emmanuel Vasilakis.

![iGamePlus screenshot](/screenshot_github.png?raw=true "iGamePlus screenshot")

## What's new in iGamePlus (compared to iGame)

### Sidebar
- Game info panel with title, genre, developer, year, chipset, players, rating, times played
- Screenshots scale to fill available sidebar space on all screen modes

### Filter bar
- Togglable filter bar (hotkey T) with genre and chipset dropdowns
- Rating filter — type `r>8` in the filter field to show games rated 8.0+, combine with title search like `r>7.5 elite`

### Columns
- Configurable columns — show/hide Year, Players, Genre, Times Played, Rating
- Drag-sortable column order in Settings
- Short Year display option (96 instead of 1996)

### Game management
- Favourite hotkey (F), favourites shown in bold
- Hide game hotkey (H)
- Unhide Selected, Unhide All, Delete All Hidden (with confirmation)
- Import ratings from `PROGDIR:igame.ratings` (Actions menu)
- Stale game detection on rescan

### Fixes
- Fixed memory leaks in CSV loading, screenshot handling, and WHDLoad execution
- Fixed stack buffer overflows in path handling
- Fixed OS4 ExamineDir API usage

## Warning

This software has only been tested on an Amiga 1200 with PiStorm32 (040 build). Other configurations may work but are untested. **Use at your own risk.** In particular, the "Delete All" button in the hidden games window will **permanently delete game files from disk**. Make sure you have backups before using destructive features. The author takes no responsibility for any data loss.

## Installing

Place the iGame folder anywhere on your drive. Requirements:

- Kickstart 2.04+
- Workbench 2.1+
- MUI 3.8+
- icon.library v37+ (v44+ recommended)
- guigfx.library (optional)
- render.library (optional)
- guigfx.mcc (optional)
- Texteditor.mcc
- NListviews.mcc
- Urltext.mcc (optional)

## Builds

Available CPU targets:

| Binary | Target |
|--------|--------|
| iGamePlus | 68020 (A1200, A3000, A4000/030) |
| iGamePlus.040 | 68040 (A4000/040, accelerators) |
| iGamePlus.060 | 68060 (accelerators) |
| iGamePlus.MOS | MorphOS (PPC) |
| iGamePlus.OS4 | AmigaOS 4.1 (PPC) |

Download from the [Releases](https://github.com/rootrootde/iGamePlus/releases) page.

### Compiling with Docker

```bash
# m68k (walkero/docker4amigavbcc:latest-m68k)
make -f Makefile.docker CPU=020    # 68020
make -f Makefile.docker CPU=040    # 68040
make -f Makefile.docker CPU=060    # 68060

# PPC
# walkero/docker4amigavbcc:latest-mos
make -f Makefile.docker CPU=MOS    # MorphOS

# walkero/docker4amigavbcc:latest-ppc
make -f Makefile.docker CPU=OS4    # AmigaOS 4.1
```

## License

GPLv3 — see [COPYING](COPYING)

Based on iGame by Emmanuel Vasilakis — https://github.com/MrZammler/iGame
