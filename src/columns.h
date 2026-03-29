/*
  columns.h
  Column management header for iGame

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

#ifndef _COLUMNS_H
#define _COLUMNS_H

extern int visible_columns[];
extern int num_visible_columns;

int is_column_visible(int col);
void rebuild_visible_columns(void);
void apply_column_settings(void);
void sync_column_order_from_list(void);
void populate_column_order_list(void);
void column_toggle_changed(void);
void column_order_changed(void);

#endif
