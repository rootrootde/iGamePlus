/*
  fsfuncs.h
  Filesystem functions header for iGame

  Copyright (c) 2018, Emmanuel Vasilakis

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

#ifndef _FS_FUNCS_H
#define _FS_FUNCS_H

void get_parent_path(char *, char *, int);
void get_name_from_path(char *, char *, unsigned int);
void get_full_path(const char *, char *);
BOOL check_path_exists(char *);
BOOL get_filename(const char *, const char *, const BOOL);
void slaves_list_load_from_csv(char *);
void slaves_list_save_to_csv(const char *);
int get_title_from_slave(char *, char *);
void get_title_from_path(char *, char *, int);
const char *get_directory_name(const char *);
char *get_executable_name(int, char **);
void open_current_dir(void);
void get_path(char *, char *);
BOOL is_path_folder(char *);
void get_icon_tooltypes(char *, char *);
void set_icon_tooltypes(char *, char *);
BOOL check_slave_in_tooltypes(char *, char *);
void prepare_whd_execution(char *, char *);
void get_igame_data_info(char *, slavesList *);
void load_genres_from_file(void);
BOOL delete_directory(const char *);
BOOL is_path_on_assign(const char *);

#endif
