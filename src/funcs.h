/*
  iGameMain.c
  Misc functions header for iGame

  Copyright (c) 2018, Emmanuel Vasilakis
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

#ifndef _FUNCS_H
#define _FUNCS_H

void msg_box(const char *);
void game_click(void);
void joystick_input(ULONG);
void joystick_direction_repeat(ULONG);
void app_stop(void);
void save_list(void);
ULONG get_wb_version(void);
void scan_repositories(void);
void open_list(void);
int hex2dec(char *);
void save_list_as(void);
void game_toggle_favourite(void);
void game_hide(void);
void game_delete(void);
void filter_change(void);
void launch_game(void);
void app_start(void);
void slave_properties(void);
void save_item_properties(void);
void update_tooltypes_text(const char *);
void add_non_whdload(void);
void non_whdload_ok(void);
void repo_stop(void);
void repo_add(void);
void repo_remove(void);
void toggle_side_panel(void);
void toggle_filter_bar(void);
void get_item_information(void);
void save_item_information(void);
void import_ratings(void);
void set_status_text(char *);
void apply_side_panel_change(void);
void set_last_scan_bitfield(void);

#endif
