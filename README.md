# iGame+

A WHDLoad game launcher for Amiga, forked from [iGame](https://github.com/MrZammler/iGame) by Emmanuel Vasilakis.

![Alt text](/igame_screen.png?raw=true "iGame screenshot")

## What's new in iGame+

### New features
- **Configurable columns** — show/hide Year, Players, Genre, Times Played, Rating columns
- **Drag-sortable column order** — reorder columns via drag and drop in Settings, with live preview
- **Short Year display** — option to show 2-digit years (96 instead of 1996) to save screen space
- **Rating column** — displays game ratings
- **Rating filter** — type `r>8` in the filter field to show games rated 8.0+, combine with title search like `r>7.5 elite`
- **Side panel toggle** — hotkey B to show/hide the genre and chipset filter panel
- **Favourite hotkey** — press F to toggle favourite, favourites shown in bold
- **Hide game hotkey** — press H to hide a game from the list
- **Hidden games management** — Unhide Selected, Unhide All, and Delete All (removes files from disk)
- **Start with favourites** — option to show only favourites on launch
- **Stale game detection** — on rescan, detects games no longer on disk and asks before removing

### Fixes
- Fixed memory leaks in CSV loading, screenshot handling, and WHDLoad execution
- Fixed stack buffer overflows in path handling
- Optimized linked list operations with O(1) tail insertion

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
| iGame.000 | 68000 (any Amiga) |
| iGame.030 | 68030 (A1200, A3000, A4000) |
| iGame.040 | 68040 (A4000/040, accelerators) |
| iGame.060 | 68060 (accelerators) |

Download from the [Releases](https://github.com/rootrootde/iGamePlus/releases) page.

### Compiling with Docker

```bash
make -f Makefile.docker CPU=000    # 68000
make -f Makefile.docker CPU=030    # 68030
make -f Makefile.docker CPU=040    # 68040
make -f Makefile.docker CPU=060    # 68060
```

## License

GPLv3 — see [COPYING](COPYING)

Based on iGame by Emmanuel Vasilakis — https://github.com/MrZammler/iGame
