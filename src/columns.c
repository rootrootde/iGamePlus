/*
  columns.c
  Column management functions for iGame

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
#include <mui/NListview_mcc.h>

/* Prototypes */
#include <proto/muimaster.h>

/* ANSI C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "iGameExtern.h"
#include "iGameGUI.h"
#include "columns.h"

extern struct ObjApp *app;
extern igame_settings *current_settings;

/* global variables */
int visible_columns[COL_MAX];
int num_visible_columns = 0;

int is_column_visible(int col)
{
	switch (col)
	{
		case COL_YEAR: return current_settings->show_column_year;
		case COL_PLAYERS: return current_settings->show_column_players;
		case COL_GENRE: return current_settings->show_column_genre;
		case COL_TIMES_PLAYED: return current_settings->show_column_times_played;
		case COL_RATING: return current_settings->show_column_rating;
	}
	return 0;
}

static void set_column_visible(int col, int visible)
{
	switch (col)
	{
		case COL_YEAR: current_settings->show_column_year = visible; break;
		case COL_PLAYERS: current_settings->show_column_players = visible; break;
		case COL_GENRE: current_settings->show_column_genre = visible; break;
		case COL_TIMES_PLAYED: current_settings->show_column_times_played = visible; break;
		case COL_RATING: current_settings->show_column_rating = visible; break;
	}
}

void rebuild_visible_columns(void)
{
	num_visible_columns = 0;
	visible_columns[num_visible_columns++] = COL_TITLE;

	for (int i = 0; i < NUM_OPTIONAL_COLUMNS; i++)
	{
		int col = current_settings->column_order[i];
		if (is_column_visible(col))
			visible_columns[num_visible_columns++] = col;
	}
}

static void rebuild_nlist_format(void)
{
	static char format_buf[128];

	if (num_visible_columns <= 1)
	{
		/* Title only, no BAR separators */
		strcpy(format_buf, "");
	}
	else
	{
		/* Title gets the most space, extra columns get fixed widths */
		int pos = 0;
		for (int i = 0; i < num_visible_columns; i++)
		{
			if (i > 0)
				pos += snprintf(format_buf + pos, sizeof(format_buf) - pos, ",");

			if (i == 0)
				pos += snprintf(format_buf + pos, sizeof(format_buf) - pos, "BAR W=70");
			else if (visible_columns[i] == COL_YEAR && current_settings->short_year)
				pos += snprintf(format_buf + pos, sizeof(format_buf) - pos, "BAR W=10");
			else
				pos += snprintf(format_buf + pos, sizeof(format_buf) - pos, "BAR W=15");
		}
	}

	set(app->LV_GamesList, MUIA_NList_Format, (ULONG)format_buf);
	DoMethod(app->LV_GamesList, MUIM_NList_Redraw, MUIV_NList_Redraw_All);
}

void apply_column_settings(void)
{
	rebuild_visible_columns();
	rebuild_nlist_format();
}

void sync_column_order_from_list(void)
{
	LONG count = 0;
	get(app->LV_ColumnOrder, MUIA_NList_Entries, &count);
	for (int i = 0; i < count && i < NUM_OPTIONAL_COLUMNS; i++)
	{
		char *entry = NULL;
		DoMethod(app->LV_ColumnOrder, MUIM_NList_GetEntry, i, &entry);
		if (entry)
			current_settings->column_order[i] = atoi(entry);
	}
}

void populate_column_order_list(void)
{
	DoMethod(app->LV_ColumnOrder, MUIM_NList_Clear);
	for (int i = 0; i < NUM_OPTIONAL_COLUMNS; i++)
	{
		char col_str[4];
		snprintf(col_str, sizeof(col_str), "%d", current_settings->column_order[i]);
		DoMethod(app->LV_ColumnOrder, MUIM_NList_InsertSingle, col_str,
			MUIV_NList_Insert_Bottom);
	}
}

void column_toggle_changed(void)
{
	char *entry = NULL;
	DoMethod(app->LV_ColumnOrder, MUIM_NList_GetEntry,
		MUIV_NList_GetEntry_Active, &entry);
	if (!entry) return;

	int col = atoi(entry);
	set_column_visible(col, !is_column_visible(col));

	DoMethod(app->LV_ColumnOrder, MUIM_NList_Redraw, MUIV_NList_Redraw_All);

	sync_column_order_from_list();
	apply_column_settings();
}

void column_order_changed(void)
{
	sync_column_order_from_list();
	apply_column_settings();
}
