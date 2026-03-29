/*
  settings.c
  Settings functions for iGame

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

/* MUI */
#include <libraries/mui.h>

/* Prototypes */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>

/* ANSI C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define iGame_NUMBERS
#include "iGame_strings.h"

#include "iGameExtern.h"
#include "iGameGUI.h"
#include "fsfuncs.h"
#include "strfuncs.h"
#include "funcs.h"
#include "settings.h"
#include "columns.h"

extern struct ObjApp* app;
extern igame_settings *current_settings;
extern BOOL sidepanelChanged;

/* MUI helper to get an attribute value */
static LONG xget(Object* obj, ULONG attribute)
{
	LONG x;
	get(obj, attribute, &x);
	return x;
}

static int get_radio_index(Object* obj)
{
	int index = 0;
	get(obj, MUIA_Radio_Active, &index);
	return index;
}

static void getPrefsPath(STRPTR prefsPath, STRPTR prefsFile)
{
	strcpy(prefsPath, "ENVARC:");
	strncat(prefsPath, prefsFile, MAX_PATH_SIZE - 5);
	if (!check_path_exists(prefsPath))
	{
		strcpy(prefsPath, "PROGDIR:");
		strncat(prefsPath, prefsFile, MAX_PATH_SIZE - 10);
	}
}

static void set_default_settings(igame_settings *settings)
{
	current_settings->no_guigfx				= TRUE;
	current_settings->filter_use_enter		= TRUE;
	current_settings->hide_side_panel		= FALSE;
	current_settings->hide_filter_bar		= FALSE;
	current_settings->save_stats_on_exit	= TRUE;
	current_settings->no_smart_spaces		= FALSE;
	current_settings->use_igamedata_title		= TRUE;
	current_settings->titles_from_dirs		= TRUE;
	current_settings->hide_screenshots		= FALSE;
	current_settings->start_with_favorites	= FALSE;
	current_settings->show_column_year		= TRUE;
	current_settings->show_column_players	= FALSE;
	current_settings->show_column_genre		= TRUE;
	current_settings->show_column_times_played = FALSE;
	current_settings->show_column_rating	= FALSE;
	current_settings->column_order[0]		= COL_GENRE;
	current_settings->column_order[1]		= COL_YEAR;
	current_settings->column_order[2]		= COL_PLAYERS;
	current_settings->column_order[3]		= COL_TIMES_PLAYED;
	current_settings->column_order[4]		= COL_RATING;
	current_settings->short_year			= FALSE;
	current_settings->last_scan_setup			= 0;
}

void apply_settings()
{
	if (current_settings == NULL)
		return;

	set(app->CH_Screenshots, MUIA_Selected, current_settings->hide_screenshots);
	set(app->CH_NoGuiGfx, MUIA_Selected, current_settings->no_guigfx);

	set(app->RA_TitlesFrom, MUIA_Radio_Active, current_settings->titles_from_dirs);
	if (current_settings->titles_from_dirs)
	{
		set(app->CH_SmartSpaces, MUIA_Disabled, FALSE);
		set(app->CH_SmartSpaces, MUIA_Selected, current_settings->no_smart_spaces);
	}
	else
	{
		set(app->CH_SmartSpaces, MUIA_Disabled, TRUE);
		set(app->CH_SmartSpaces, MUIA_Disabled, TRUE);
	}

	set(app->CH_UseIgameDataTitle, MUIA_Selected, current_settings->use_igamedata_title);
	set(app->CH_SaveStatsOnExit, MUIA_Selected, current_settings->save_stats_on_exit);
	set(app->CH_FilterUseEnter, MUIA_Selected, current_settings->filter_use_enter);
	set(app->CH_HideSidepanel, MUIA_Selected, current_settings->hide_side_panel);
	set(app->CH_StartWithFavorites, MUIA_Selected, current_settings->start_with_favorites);
	populate_column_order_list();
	set(app->CH_ShortYear, MUIA_Selected, current_settings->short_year);

	apply_column_settings();
}

igame_settings *load_settings(const char* filename)
{
	const int buffer_size = 512;

	STRPTR file_line = malloc(buffer_size * sizeof(char));
	if (file_line == NULL)
	{
		msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
		return NULL;
	}

	STRPTR prefsPath = AllocVec(sizeof(char) * MAX_PATH_SIZE, MEMF_CLEAR);
	if(prefsPath == NULL)
	{
		msg_box((const char*)GetMBString(MSG_NotEnoughMemory));

		free(file_line);
		return NULL;
	}
	getPrefsPath(prefsPath, (STRPTR)filename);

	if (current_settings != NULL)
	{
		free(current_settings);
		current_settings = NULL;
	}
	current_settings = (igame_settings *)calloc(1, sizeof(igame_settings));
	set_default_settings(current_settings);

	if (check_path_exists(prefsPath))
	{
		const BPTR fpsettings = Open(prefsPath, MODE_OLDFILE);
		if (fpsettings)
		{
			do
			{
				if (FGets(fpsettings, file_line, buffer_size) == NULL)
					break;

				int len = strlen(file_line);
				if (len > 0 && file_line[len - 1] == '\n') file_line[--len] = '\0';
				if (len == 0 || file_line[0] == ';' || file_line[0] == '#')
					continue;

				if (!strncmp(file_line, "no_guigfx=", 10))
					current_settings->no_guigfx = atoi((const char*)file_line + 10);
				if (!strncmp(file_line, "filter_use_enter=", 17))
					current_settings->filter_use_enter = atoi((const char*)file_line + 17);
				if (!strncmp(file_line, "hide_side_panel=", 16))
					current_settings->hide_side_panel = atoi((const char*)file_line + 16);
				if (!strncmp(file_line, "hide_filter_bar=", 16))
					current_settings->hide_filter_bar = atoi((const char*)file_line + 16);
				if (!strncmp(file_line, "save_stats_on_exit=", 19))
					current_settings->save_stats_on_exit = atoi((const char*)file_line + 19);
				if (!strncmp(file_line, "no_smart_spaces=", 16))
					current_settings->no_smart_spaces = atoi((const char*)file_line + 16);
				if (!strncmp(file_line, "titles_from_dirs=", 17))
					current_settings->titles_from_dirs = atoi((const char*)file_line + 17);
				if (!strncmp(file_line, "hide_screenshots=", 17))
					current_settings->hide_screenshots = atoi((const char*)file_line + 17);
				if (!strncmp(file_line, "start_with_favorites=", 21))
					current_settings->start_with_favorites = atoi((const char*)file_line + 21);
				if (!strncmp(file_line, "use_igame.data_title=", 21))
					current_settings->use_igamedata_title = atoi((const char*)file_line + 21);
				if (!strncmp(file_line, "last_scan_setup=", 16))
					current_settings->last_scan_setup = atoi((const char*)file_line + 16);
				if (!strncmp(file_line, "show_column_year=", 17))
					current_settings->show_column_year = atoi((const char*)file_line + 17);
				if (!strncmp(file_line, "show_column_players=", 20))
					current_settings->show_column_players = atoi((const char*)file_line + 20);
				if (!strncmp(file_line, "show_column_genre=", 18))
					current_settings->show_column_genre = atoi((const char*)file_line + 18);
				if (!strncmp(file_line, "show_column_times_played=", 25))
					current_settings->show_column_times_played = atoi((const char*)file_line + 25);
				if (!strncmp(file_line, "show_column_rating=", 19))
					current_settings->show_column_rating = atoi((const char*)file_line + 19);
				if (!strncmp(file_line, "column_order=", 13))
				{
					char *p = (char *)file_line + 13;
					for (int i = 0; i < NUM_OPTIONAL_COLUMNS && *p; i++)
					{
						current_settings->column_order[i] = atoi(p);
						p = strchr(p, ',');
						if (p) p++; else break;
					}
				}
				if (!strncmp(file_line, "short_year=", 11))
					current_settings->short_year = atoi((const char*)file_line + 11);
			}
			while (1);

			Close(fpsettings);
		}
	}

	if (current_settings->last_scan_setup == 0)
		set_last_scan_bitfield();

	rebuild_visible_columns();

	if (prefsPath)
		FreeVec(prefsPath);

	if (file_line)
		free(file_line);

	return current_settings;
}

void setting_filter_use_enter_changed(void)
{
	current_settings->filter_use_enter = (BOOL)xget(app->CH_FilterUseEnter, MUIA_Selected);
}

void setting_save_stats_on_exit_changed(void)
{
	current_settings->save_stats_on_exit = (BOOL)xget(app->CH_SaveStatsOnExit, MUIA_Selected);
}

void setting_smart_spaces_changed(void)
{
	current_settings->no_smart_spaces = (BOOL)xget(app->CH_SmartSpaces, MUIA_Selected);
}

void setting_use_igamedata_title_changed(void)
{
	current_settings->use_igamedata_title = (BOOL)xget(app->CH_UseIgameDataTitle, MUIA_Selected);
}

void setting_titles_from_changed(void)
{
	const int index = get_radio_index(app->RA_TitlesFrom);

	// Index=0 -> Titles from Slaves
	// Index=1 -> Titles from Dirs
	current_settings->titles_from_dirs = index;

	if (index == 1)
		set(app->CH_SmartSpaces, MUIA_Disabled, FALSE);
	else
		set(app->CH_SmartSpaces, MUIA_Disabled, TRUE);
}

void setting_hide_screenshot_changed(void)
{
	current_settings->hide_screenshots = (BOOL)xget(app->CH_Screenshots, MUIA_Selected);
	sidepanelChanged = TRUE;
}

void setting_no_guigfx_changed(void)
{
	current_settings->no_guigfx = (BOOL)xget(app->CH_NoGuiGfx, MUIA_Selected);
}

void settings_use(void)
{
	if (current_settings == NULL)
		return;

	sync_column_order_from_list();

	if (sidepanelChanged)
		apply_side_panel_change();

	set(app->WI_Settings, MUIA_Window_Open, FALSE);
}

void settings_save(void)
{
	settings_use();

	const int buffer_size = 1024;
	char file_line[1024];
	int pos = 0;

	STRPTR prefsPath = AllocVec(sizeof(char) * MAX_PATH_SIZE, MEMF_CLEAR);
	if(prefsPath == NULL)
	{
		msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
		return;
	}
	getPrefsPath(prefsPath, (STRPTR)DEFAULT_SETTINGS_FILE);

	const BPTR fpsettings = Open(prefsPath, MODE_NEWFILE);
	if (!fpsettings)
	{
		msg_box((const char*)"Could not save Settings file!");
		FreeVec(prefsPath);
		return;
	}

	pos += snprintf(file_line + pos, buffer_size - pos, "no_guigfx=%d\n", current_settings->no_guigfx);
	pos += snprintf(file_line + pos, buffer_size - pos, "filter_use_enter=%d\n", current_settings->filter_use_enter);
	pos += snprintf(file_line + pos, buffer_size - pos, "hide_side_panel=%d\n", current_settings->hide_side_panel);
	pos += snprintf(file_line + pos, buffer_size - pos, "hide_filter_bar=%d\n", current_settings->hide_filter_bar);
	pos += snprintf(file_line + pos, buffer_size - pos, "start_with_favorites=%d\n", current_settings->start_with_favorites);
	pos += snprintf(file_line + pos, buffer_size - pos, "save_stats_on_exit=%d\n", current_settings->save_stats_on_exit);
	pos += snprintf(file_line + pos, buffer_size - pos, "no_smart_spaces=%d\n", current_settings->no_smart_spaces);
	pos += snprintf(file_line + pos, buffer_size - pos, "titles_from_dirs=%d\n", current_settings->titles_from_dirs);
	pos += snprintf(file_line + pos, buffer_size - pos, "hide_screenshots=%d\n", current_settings->hide_screenshots);
	pos += snprintf(file_line + pos, buffer_size - pos, "use_igame.data_title=%d\n", current_settings->use_igamedata_title);
	pos += snprintf(file_line + pos, buffer_size - pos, "last_scan_setup=%d\n", current_settings->last_scan_setup);
	pos += snprintf(file_line + pos, buffer_size - pos, "show_column_year=%d\n", current_settings->show_column_year);
	pos += snprintf(file_line + pos, buffer_size - pos, "show_column_players=%d\n", current_settings->show_column_players);
	pos += snprintf(file_line + pos, buffer_size - pos, "show_column_genre=%d\n", current_settings->show_column_genre);
	pos += snprintf(file_line + pos, buffer_size - pos, "show_column_times_played=%d\n", current_settings->show_column_times_played);
	pos += snprintf(file_line + pos, buffer_size - pos, "show_column_rating=%d\n", current_settings->show_column_rating);
	pos += snprintf(file_line + pos, buffer_size - pos, "column_order=");
	for (int i = 0; i < NUM_OPTIONAL_COLUMNS; i++)
	{
		if (i > 0) pos += snprintf(file_line + pos, buffer_size - pos, ",");
		pos += snprintf(file_line + pos, buffer_size - pos, "%d", current_settings->column_order[i]);
	}
	pos += snprintf(file_line + pos, buffer_size - pos, "\n");
	pos += snprintf(file_line + pos, buffer_size - pos, "short_year=%d\n", current_settings->short_year);

	FPuts(fpsettings, (CONST_STRPTR)file_line);
	Close(fpsettings);

	FreeVec(prefsPath);
}

void setting_hide_side_panel_changed(void)
{
	current_settings->hide_side_panel = (BOOL)xget(app->CH_HideSidepanel, MUIA_Selected);
}

void setting_start_with_favorites_changed(void)
{
	current_settings->start_with_favorites = (BOOL)xget(app->CH_StartWithFavorites, MUIA_Selected);
}

void setting_short_year_changed(void)
{
	current_settings->short_year = (BOOL)xget(app->CH_ShortYear, MUIA_Selected);
	apply_column_settings();
}
