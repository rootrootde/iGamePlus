/*
  screenshot.h
  Screenshot functions header for iGame

  Copyright (c) 2019, Emmanuel Vasilakis and contributors
  Copyright (c) 2026, rootrootde

  This file is part of iGame.

  iGame is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  iGame is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with iGame. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SCREENSHOT_H
#define _SCREENSHOT_H

void screenshot_update(void);
void screenshot_tick(void);
BOOL screenshot_is_pending(void);
void screenshot_set_pending(const char *title);
void show_default_screenshot(void);

#endif
