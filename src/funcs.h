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
void screenshot_update(void);
void screenshot_tick(void);
BOOL screenshot_is_pending(void);
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
void slaveProperties(void);
void saveItemProperties(void);
void updateToolTypesText(const char *);
void add_non_whdload(void);
void non_whdload_ok(void);
void repo_stop(void);
void repo_add(void);
void repo_remove(void);
void setting_filter_use_enter_changed(void);
void setting_save_stats_on_exit_changed(void);
void setting_smart_spaces_changed(void);
void setting_titles_from_changed(void);
void setting_hide_screenshot_changed(void);
void setting_no_guigfx_changed(void);
void settings_save(void);
void setting_hide_side_panel_changed(void);
void toggle_side_panel(void);
void toggle_filter_bar(void);
void setting_start_with_favorites_changed(void);
void settings_use(void);
void settingUseIgameDataTitleChanged(void);
igame_settings *load_settings(const char *);
void getItemInformation(void);
void saveItemInformation(void);
void column_toggle_changed(void);
void column_order_changed(void);
void populate_column_order_list(void);
void sync_column_order_from_list(void);
void setting_short_year_changed(void);
void apply_column_settings(void);
int is_column_visible(int col);
void import_ratings(void);

#endif
