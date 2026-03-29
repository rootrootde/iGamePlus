/*
  funcs.c
  Misc functions for iGame

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
#include <mui/Guigfx_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/Urltext_mcc.h>

/* Prototypes */
#include <clib/alib_protos.h>
#include <proto/lowlevel.h>
#include <proto/wb.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

/* System */
#if defined(__amigaos4__)
#include <dos/obsolete.h>
#define ASL_PRE_V38_NAMES
#endif

/* ANSI C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define iGame_NUMBERS
#include "iGame_strings.h"

#include "iGameGUI.h"
#include "iGameExtern.h"
#include "strfuncs.h"
#include "fsfuncs.h"
#include "slavesList.h"
#include "genresList.h"
#include "chipsetList.h"
#include "blacklist.h"
#include "funcs.h"

extern struct ObjApp* app;
extern struct Library *IconBase;
extern char* executable_name;

/* global variables */
char fname[255];
BOOL sidepanelChanged = FALSE; // This is temporary until settings are revamped

/* screenshot debounce state */
static BOOL screenshot_pending = FALSE;
static char pending_screenshot_title[MAX_SLAVE_TITLE_SIZE];
static int screenshot_cooldown = 0;
#define SCREENSHOT_COOLDOWN_TICKS 5 /* ~100ms at 50Hz (Delay(1) per tick) */

/* function definitions */
static int get_cycle_index(Object *);
static void showSlavesList(void);
static LONG xget(Object *, ULONG);
static void setLastScanBitfield(void);
static int msg_box_confirm(const char *);

// Parse a rating string like "8.79" into an integer 879 (value * 100)
static int parse_rating_x100(const char *str)
{
	int result = 0;
	if (!str) return 0;
	while (*str >= '0' && *str <= '9')
	{
		result = result * 10 + (*str - '0');
		str++;
	}
	result *= 100;
	if (*str == '.')
	{
		str++;
		if (*str >= '0' && *str <= '9')
		{
			result += (*str - '0') * 10;
			str++;
			if (*str >= '0' && *str <= '9')
				result += (*str - '0');
		}
	}
	return result;
}

repos_list *item_repos = NULL, *repos = NULL;
igame_settings *current_settings = NULL;
listFilters filters = {0};

int visible_columns[COL_MAX];
int num_visible_columns = 0;

void setStatusText(char *text)
{
	set(app->TX_Status, MUIA_Text_Contents, text);
}

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

static void rebuild_visible_columns(void)
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
		format_buf[0] = '\0';
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

static void apply_settings()
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

	set(app->CH_UseIgameDataTitle, MUIA_Selected, current_settings->useIgameDataTitle);
	set(app->CH_SaveStatsOnExit, MUIA_Selected, current_settings->save_stats_on_exit);
	set(app->CH_FilterUseEnter, MUIA_Selected, current_settings->filter_use_enter);
	set(app->CH_HideSidepanel, MUIA_Selected, current_settings->hide_side_panel);
	set(app->CH_StartWithFavorites, MUIA_Selected, current_settings->start_with_favorites);
	populate_column_order_list();
	set(app->CH_ShortYear, MUIA_Selected, current_settings->short_year);

	apply_column_settings();
}

static void setDefaultSettings(igame_settings *settings)
{
	current_settings->no_guigfx				= TRUE;
	current_settings->filter_use_enter		= TRUE;
	current_settings->hide_side_panel		= FALSE;
	current_settings->hide_filter_bar		= FALSE;
	current_settings->save_stats_on_exit	= TRUE;
	current_settings->no_smart_spaces		= FALSE;
	current_settings->useIgameDataTitle		= TRUE;
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
	current_settings->lastScanSetup			= 0;
}

static void getPrefsPath(STRPTR prefsPath, STRPTR prefsFile)
{
	snprintf(prefsPath, MAX_PATH_SIZE, "ENVARC:%s", prefsFile);
	if (!check_path_exists(prefsPath))
	{
		snprintf(prefsPath, MAX_PATH_SIZE, "PROGDIR:%s", prefsFile);
	}
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
	setDefaultSettings(current_settings);

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
					current_settings->useIgameDataTitle = atoi((const char*)file_line + 21);
				if (!strncmp(file_line, "last_scan_setup=", 16))
					current_settings->lastScanSetup = atoi((const char*)file_line + 16);
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

	if (current_settings->lastScanSetup == 0)
		setLastScanBitfield();

	rebuild_visible_columns();

	if (prefsPath)
		FreeVec(prefsPath);

	if (file_line)
		free(file_line);

	return current_settings;
}

static void load_repos(const char* filename)
{
	const int buffer_size = 512;
	STRPTR file_line = malloc(buffer_size * sizeof(char));

	if (file_line == NULL)
	{
		msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
		return;
	}

	const BPTR fprepos = Open((CONST_STRPTR)filename, MODE_OLDFILE);
	if (fprepos)
	{
		char *repo_path = malloc(sizeof(char) * MAX_PATH_SIZE);

		while (FGets(fprepos, file_line, buffer_size))
		{
			int len = strlen(file_line);
			if (len > 0 && file_line[len - 1] == '\n') file_line[--len] = '\0';
			if (len == 0)
				break;

			item_repos = (repos_list *)calloc(1, sizeof(repos_list));
			item_repos->next = NULL;

			getFullPath(file_line, repo_path);
			strncpy(item_repos->repo, repo_path, sizeof(item_repos->repo) - 1);
			item_repos->repo[sizeof(item_repos->repo) - 1] = '\0';

			if (repos == NULL)
			{
				repos = item_repos;
			}
			else
			{
				item_repos->next = repos;
				repos = item_repos;
			}

			DoMethod(app->LV_GameRepositories, MUIM_List_InsertSingle, item_repos->repo, 1, MUIV_List_Insert_Bottom);
		}

		free(repo_path);
		Close(fprepos);
	}

	if (file_line)
		free(file_line);
}

static void populateGenresLists(void)
{
	if (!current_settings->hide_filter_bar)
	{
		nnset(app->LV_GenresList, MUIA_List_Quiet, TRUE);

		// Add Show All in genres list
		DoMethod(app->LV_GenresList, MUIM_List_InsertSingle, GetMBString(MSG_FilterShowAll), MUIV_List_Insert_Bottom);
	}

	loadGenresFromFile();
	addGenreInList(GetMBString(MSG_UnknownGenre));
	sortGenresList();

	genresList *currPtr = getGenresListHead();

	int cnt = 0;
	while (currPtr != NULL)
	{
		if (!current_settings->hide_filter_bar)
		{
			DoMethod(app->LV_GenresList, MUIM_List_InsertSingle, currPtr->title, MUIV_List_Insert_Bottom);
		}
		app->GenresContent[cnt] = currPtr->title;

		DoMethod(app->LV_WI_Information_Genre, MUIM_List_InsertSingle, currPtr->title, MUIV_List_Insert_Bottom);

		cnt++;
		currPtr = currPtr->next;
	}

	genresListNodeCount(cnt);
	app->GenresContent[cnt] = (unsigned char*)GetMBString(MSG_UnknownGenre);
	app->GenresContent[cnt++] = NULL;

	nnset(app->CY_AddGameGenre, MUIA_Cycle_Entries, app->GenresContent);
	if (!current_settings->hide_filter_bar)
	{
		nnset(app->LV_GenresList, MUIA_List_Active, MUIV_List_Active_Top);
		nnset(app->LV_GenresList, MUIA_List_Quiet, FALSE);
	}
}

static void populateChipsetList(void)
{
	chipsetList *currPtr = getChipsetListHead();

	int cnt = 1;
	app->CY_ChipsetListContent[cnt] = (unsigned char*)GetMBString(MSG_FilterShowAll);
	while (currPtr != NULL)
	{
		app->CY_ChipsetListContent[cnt] = currPtr->title;
		cnt++;
		currPtr = currPtr->next;
	}

	app->CY_ChipsetListContent[cnt++] = NULL;
	set(app->CY_ChipsetList, MUIA_Cycle_Entries, app->CY_ChipsetListContent);
}

// Clears the list of items
static void clear_gameslist(void)
{
	DoMethod(app->LV_GamesList, MUIM_NList_Clear);
	set(app->LV_GamesList, MUIA_NList_Quiet, TRUE);
}

void app_start(void)
{
	// check if the gamelist csv file exists. If not, try to load the old one
	char csvFilename[MAX_PATH_SIZE];
	snprintf(csvFilename, sizeof(csvFilename), "%s.csv", DEFAULT_GAMESLIST_FILE);

	load_repos(DEFAULT_REPOS_FILE);
	apply_settings();

	if (current_settings->start_with_favorites)
	{
		filters.showGroup = GROUP_FAVOURITES;
		set(app->CY_FilterList, MUIA_Cycle_Active, GROUP_FAVOURITES);
	}

	DoMethod(app->App,
		MUIM_Application_Load,
		MUIV_Application_Load_ENVARC
	);

	set(app->WI_MainWindow, MUIA_Window_Open, TRUE);

	// Load the slaves list from the gameslist file
	setStatusText(GetMBString(MSG_LoadingSavedList));
	set(app->WI_MainWindow, MUIA_Window_Sleep, TRUE);
	slavesListLoadFromCSV(csvFilename);
	blacklistLoad(DEFAULT_BLACKLIST_FILE);

	populateGenresLists(); // This calls the filter_change()
	if (!current_settings->hide_filter_bar)
	{
		populateChipsetList();
	}
	filter_change();
	set(app->WI_MainWindow, MUIA_Window_Sleep, FALSE);
	set(app->WI_MainWindow, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);

	// Select first game to populate sidebar and enable window resizing
	LONG entries = xget(app->LV_GamesList, MUIA_NList_Entries);
	if (entries > 0)
	{
		set(app->LV_GamesList, MUIA_NList_Active, 0);
		game_click();
	}
}

void filter_change(void)
{
	char *title = NULL;
	char *genreSelection = NULL;
	filters.showGenre[0] = '\0';
	filters.showGroup = get_cycle_index(app->CY_FilterList);
	filters.showChipset[0] = '\0';
	filters.minRatingX100 = 0;

	get(app->STR_Filter, MUIA_String_Contents, &title);

	// Parse r>N rating prefix (e.g. "r>8" or "r>7.5 elite")
	if (title && title[0] == 'r' && title[1] == '>')
	{
		const char *p = title + 2;
		filters.minRatingX100 = parse_rating_x100(p);
		// Skip past the number (digits and optional decimal)
		while ((*p >= '0' && *p <= '9') || *p == '.')
			p++;
		// Skip whitespace to get remaining title filter
		while (*p == ' ')
			p++;
		title = (char *)p;
	}

	if (!current_settings->filter_use_enter && filters.minRatingX100 == 0 &&
		strlen(title) > 0 && strlen(title) < MIN_TITLE_FILTER_CHARS) {
		return;
	}

	if (title && strlen(title) > 0)
	{
		string_to_lower(title);
		strncpy(filters.title, title, sizeof(filters.title));
	}
	else filters.title[0] = '\0';

	if (!current_settings->hide_filter_bar)
	{
		get(app->STR_GenreFilter, MUIA_String_Contents, &genreSelection);

		if (genreSelection && strlen(genreSelection) > 0)
			strncpy(filters.showGenre, genreSelection, sizeof(filters.showGenre));

		if (!genreSelection || strlen(genreSelection) == 0 || !strcmp(genreSelection, GetMBString(MSG_FilterShowAll)))
			filters.showGenre[0] = '\0';
	}

	if (!current_settings->hide_filter_bar)
	{
		// Get selected chipset from the cycle box
		int chipsetIndex = get_cycle_index(app->CY_ChipsetList);
		strncpy(filters.showChipset, app->CY_ChipsetListContent[chipsetIndex], sizeof(filters.showChipset));
		if (chipsetIndex == 0)
			filters.showChipset[0] = '\0';
	}

	showSlavesList();
}

static BOOL createRunGameScript(slavesList *node)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);

	getParentPath(node->path, buf, bufSize);
	FILE* runGameScript = fopen("T:rungame", "we");
	if (!runGameScript)
	{
		printf("Could not open rungame file!");
		FreeVec(buf);
		return FALSE;
	}
	getParentPath(node->path, buf, bufSize);
	fprintf(runGameScript, "Cd \"%s\"\n", buf);
	getNameFromPath(node->path, buf, bufSize);
	fprintf(runGameScript, "Run >NIL: \"%s\"\n", buf);
	fclose(runGameScript);

	FreeVec(buf);
	return TRUE;
}

static void launchSlave(slavesList *node)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);

	getParentPath(node->path, buf, bufSize);
	const BPTR pathLock = Lock(buf, SHARED_LOCK);

	if (pathLock && IconBase)
	{
		struct FileInfoBlock *FIblock = (struct FileInfoBlock *)AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);

		const BPTR oldLock = CurrentDir(pathLock);

		if (Examine(pathLock, FIblock))
		{
			char *infoPath = AllocVec(bufSize, MEMF_CLEAR);
			while (ExNext(pathLock, FIblock))
			{
				if (
					(FIblock->fib_DirEntryType < 0) &&
					(strcasestr(FIblock->fib_FileName, ".info"))
				) {
					getFullPath(FIblock->fib_FileName, infoPath);
					{
						STRPTR tmp = substring(infoPath, 0, -5);
						if (tmp) { snprintf(infoPath, bufSize, "%s", tmp); free(tmp); }
					}

					// Get the slave filename
					getNameFromPath(node->path, buf, bufSize);
#if !defined(__morphos__)
					if (checkSlaveInTooltypes(infoPath, buf))
					{
#endif
						int execSize = sizeof(char) * MAX_EXEC_SIZE;
						char *exec = AllocVec(execSize, MEMF_CLEAR);

#if defined(__morphos__)
						snprintf(exec, execSize, "open %s", buf);
#else
						prepareWHDExecution(infoPath, exec);
#endif
						// Update statistics info
						node->last_played = 1;
						node->times_played++;

						// Save stats
						if (!current_settings->save_stats_on_exit)
							save_list();

						int success = Execute(exec, 0, 0);
						if (success == 0)
							msg_box((const char*)GetMBString(MSG_ErrorExecutingWhdload));

						FreeVec(exec);
						break;
#if !defined(__morphos__)
					}
#endif
				}
			}

			FreeVec(infoPath);
			FreeVec(FIblock);
		}

		UnLock(pathLock);
		CurrentDir(oldLock);
	}

	FreeVec(buf);
}

static void launchFromWB(slavesList *node)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);
	char *exec = AllocVec(bufSize, MEMF_CLEAR);

	strncpy(buf, "C:WBRun", bufSize);
	if (check_path_exists(buf))
	{
		snprintf(exec, bufSize, "C:WBRun \"%s\"", node->path);
	}

	strncpy(buf, "C:WBLoad", bufSize);
	if (check_path_exists(buf))
	{
		snprintf(exec, bufSize, "C:WBLoad \"%s\"", node->path);
	}

	if (isStringEmpty(exec))
	{
		if (createRunGameScript(node))
		{
			snprintf(exec, bufSize, "Execute T:rungame");
		}
	}


	if (!isStringEmpty(exec))
	{
		// Update statistics info
		node->last_played = 1;
		node->times_played++;

		// Save stats
		if (!current_settings->save_stats_on_exit)
			save_list();

		int success = Execute(exec, 0, 0);
		if (success == 0)
			msg_box((const char*)GetMBString(MSG_ErrorExecutingWhdload));
	}

	FreeVec(exec);
	FreeVec(buf);
}

void launch_game(void)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);
	char *game_title = NULL;

	// Get the selected item from the list
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &game_title);
	if (game_title == NULL || game_title[0] == '\0')
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *existingNode = NULL;
	if ((existingNode = slavesListSearchByTitle(game_title, sizeof(char) * MAX_SLAVE_TITLE_SIZE)) == NULL)
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	if(!check_path_exists(existingNode->path)) {
		msg_box((const char*)GetMBString(MSG_slavePathDoesntExist));
		return;
	}

	getNameFromPath(existingNode->path, buf, bufSize);

	// Slave file found
	if (strcasestr(buf, ".slave"))
	{
		launchSlave(existingNode);
	}
	else
	{
		launchFromWB(existingNode);
	}

	snprintf(buf, bufSize, (const char *)GetMBString(MSG_TotalNumberOfGames), slavesListNodeCount(-1));
	setStatusText(buf);

	FreeVec(buf);
}

static void showSlavesList(void)
{
	int cnt = 0;
	int bufSize = sizeof(char) * MAX_SLAVE_TITLE_SIZE;
	char *buf = malloc(bufSize);
	slavesList *currPtr = getSlavesListHead();
	int mostPlayedTimes = 0;

	clear_gameslist();

	while (currPtr != NULL)
	{
		if (!currPtr->hidden && !currPtr->deleted)
		{
			// Filter the list by the selected genre
			if (
				!isStringEmpty(filters.showGenre) &&
				strncmp(filters.showGenre, currPtr->genre, sizeof(currPtr->genre))
			)
			{
				goto nextItem;
			}

			// Filter the list by the selected chipset
			if (
				!isStringEmpty(filters.showChipset) &&
				strncmp(filters.showChipset, currPtr->chipset, sizeof(currPtr->chipset))
			)
			{
				goto nextItem;
			}

			// Filter by minimum rating
			if (filters.minRatingX100 > 0)
			{
				int gameRating = 0;
				if (!isStringEmpty(currPtr->rating))
					gameRating = parse_rating_x100(currPtr->rating);
				if (gameRating < filters.minRatingX100)
					goto nextItem;
			}

			// Decide where the item name will be taken from
			if(!isStringEmpty(currPtr->user_title))
			{
				snprintf(buf, bufSize, "%s", currPtr->user_title);
			}
			else if(isStringEmpty(currPtr->user_title) && (currPtr->instance > 0))
			{
				snprintf(currPtr->user_title, sizeof(currPtr->user_title),
					"%s [%d]", currPtr->title, currPtr->instance);
				snprintf(buf, bufSize, "%s", currPtr->user_title);
			}
			else
			{
				snprintf(buf, bufSize, "%s", currPtr->title);
			}

			// Filter list based on entered string
			if (!isStringEmpty(filters.title) && !strcasestr(buf, filters.title))
			{
				goto nextItem;
			}

			// Filter results based on selected group
			if (filters.showGroup > 0) {
				switch (filters.showGroup)
				{
					case GROUP_FAVOURITES:
						if (!currPtr->favourite) goto nextItem;
						break;
					case GROUP_LAST_PLAYED:
						if (!currPtr->last_played) goto nextItem;
						break;
					case GROUP_MOST_PLAYED:
						if (currPtr->times_played > 0)
						{
							if (currPtr->times_played < mostPlayedTimes)
							{
								DoMethod(app->LV_GamesList,
									MUIM_NList_InsertSingle, currPtr,
									MUIV_NList_Insert_Bottom);
							}
							else
							{
								mostPlayedTimes = currPtr->times_played;

								DoMethod(app->LV_GamesList,
									MUIM_NList_InsertSingle, currPtr,
									MUIV_NList_Insert_Top);
							}
						}
						goto nextItem;
						break;
					case GROUP_NEVER_PLAYED:
						if (currPtr->times_played > 0)
						{
							goto nextItem;
						}
						break;
				}
			}

			DoMethod(app->LV_GamesList,
				MUIM_NList_InsertSingle, currPtr,
				MUIV_NList_Insert_Bottom);

			cnt++;
nextItem:
		}

		currPtr = currPtr->next;
	}
	DoMethod(app->LV_GamesList, MUIM_NList_Sort);
	set(app->LV_GamesList, MUIA_NList_Quiet, FALSE);

	snprintf(buf, bufSize, (const char *)GetMBString(MSG_TotalNumberOfGames), slavesListNodeCount(cnt));
	setStatusText(buf);
	free(buf);
}

/*
 * Generate a title name based on user settings and the path
 */
static void generateItemName(char *path, char *result, int resultSize)
{
	if (current_settings->titles_from_dirs)
	{
		getTitleFromPath(path, result, resultSize);
	}
	else
	{
		get_title_from_slave(path, result);
		if (!strncmp(result, "", sizeof(char) * MAX_SLAVE_TITLE_SIZE))
		{
			getTitleFromPath(path, result, resultSize);
		}
	}
}

static int calcLastScanBitfield(void)
{
	int scanBitfield = 0;
	scanBitfield = 1 * current_settings->useIgameDataTitle;
	scanBitfield += 2 * current_settings->titles_from_dirs;
	if(current_settings->titles_from_dirs)
	{
		scanBitfield += 4 * current_settings->no_smart_spaces;
	}
	return scanBitfield;
}

static void setLastScanBitfield(void)
{
	current_settings->lastScanSetup = calcLastScanBitfield();
}

static BOOL examineFolder(char *path)
{
	BOOL success = TRUE;
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = malloc(bufSize);
	if(buf == NULL)
	{
		msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
		return FALSE;
	}

	const BPTR lock = Lock(path, SHARED_LOCK);
	if (lock) {
		struct FileInfoBlock *FIblock = (struct FileInfoBlock *)AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);

		if (Examine(lock, FIblock))
		{
			while (ExNext(lock, FIblock))
			{
				if(FIblock->fib_DirEntryType > 0)
				{
					if (
						strncmp((unsigned char *)FIblock->fib_FileName, "data\0", 5) &&
						strncmp((unsigned char *)FIblock->fib_FileName, "Data\0", 5)
					) {
						strncpy(buf, FIblock->fib_FileName, bufSize - 1);
						buf[bufSize - 1] = '\0';

						if(!isPathFolder(path))
						{
							snprintf(buf, bufSize, "%s%s", path, FIblock->fib_FileName);
						}
						else
						{
							snprintf(buf, bufSize, "%s/%s", path, FIblock->fib_FileName);
						}

						getFullPath(buf, buf);
						setStatusText(buf);
						if (!(success = examineFolder(buf))) break;
					}
				}

				if(FIblock->fib_DirEntryType < 0)
				{

					// igame.data found
					if (current_settings->useIgameDataTitle && !strcmp(FIblock->fib_FileName, DEFAULT_IGAMEDATA_FILE))
					{
						slavesList *node = malloc(sizeof(slavesList));
						if(node == NULL)
						{
							msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
							FreeVec(FIblock);
							UnLock(lock);
							free(buf);
							return FALSE;
						}

						char *igameDataPath = malloc(sizeof(char) * MAX_PATH_SIZE);
						if(!isPathFolder(path))
						{
							snprintf(igameDataPath, MAX_PATH_SIZE, "%s%s", path, FIblock->fib_FileName);
						}
						else
						{
							snprintf(igameDataPath, MAX_PATH_SIZE, "%s/%s", path, FIblock->fib_FileName);
						}

						node->instance = 0;
						node->title[0] = '\0';
						node->genre[0] = '\0';
						node->user_title[0] = '\0';
						node->arguments[0] = '\0';
						node->chipset[0] = '\0';
						node->rating[0] = '\0';
						node->times_played = 0;
						node->favourite = 0;
						node->last_played = 0;
						node->exists = 1;
						node->hidden = 0;
						node->deleted = 0;
						node->year = 0;
						node->players = 0;
						node->path[0] = '\0';

						getIGameDataInfo(igameDataPath, node);
						free(igameDataPath);

						strncpy(buf, node->path, bufSize);
						if(!isPathFolder(path))
						{
							snprintf(node->path, sizeof(char) * MAX_PATH_SIZE, "%s%s", path, buf);
						}
						else
						{
							snprintf(node->path, sizeof(char) * MAX_PATH_SIZE, "%s/%s", path, buf);
						}

						// Check the node->path and skip the save if the buf is empty, it is a slave that aleady exists in the slaves list
						// or there is no executable file defined. Free the node before move on.
						if (
							isStringEmpty(buf) || !check_path_exists(node->path) ||
							blacklistContains(node->path) ||
							(slavesListSearchByPath(node->path, sizeof(char) * MAX_PATH_SIZE) != NULL)
						) {
							free(node);
						}
						else
						{
							slavesListAddTail(node);
							addGenreInList(node->genre);
							addChipsetInList(node->chipset);
						}
					}

					// Slave file found
					if (strcasestr(FIblock->fib_FileName, ".slave"))
					{
						slavesList *existingNode = NULL;

						if(!isPathFolder(path))
						{
							snprintf(buf, bufSize, "%s%s", path, FIblock->fib_FileName);
						}
						else
						{
							snprintf(buf, bufSize, "%s/%s", path, FIblock->fib_FileName);
						}

						// Find if already exists in the list or is blacklisted, and ignore it
						if (!(existingNode = slavesListSearchByPath(buf, bufSize)) && !blacklistContains(buf))
						{
							slavesList *node = malloc(sizeof(slavesList));
							if(node == NULL)
							{
								msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
								FreeVec(FIblock);
								UnLock(lock);
								free(buf);
								return FALSE;
							}

							node->instance = 0;
							node->title[0] = '\0';
							node->genre[0] = '\0';
							snprintf(node->genre, sizeof(node->genre), "Unknown");
							node->user_title[0] = '\0';
							node->chipset[0] = '\0';
							node->rating[0] = '\0';
							node->times_played = 0;
							node->favourite = 0;
							node->last_played = 0;
							node->exists = 1;
							node->hidden = 0;
							node->deleted = 0;
							node->year = 0;
							node->players = 0;
							node->path[0] = '\0';

							getFullPath(buf, buf);
							strncpy(node->path, buf, sizeof(node->path));

							// if igame.data is enabled in settings use it
							if (current_settings->useIgameDataTitle)
							{
								char *igameDataPath = malloc(sizeof(char) * MAX_PATH_SIZE);
								snprintf(igameDataPath, sizeof(char) * MAX_PATH_SIZE, "%s/%s", path, DEFAULT_IGAMEDATA_FILE);
								if (check_path_exists(igameDataPath))
								{
									getIGameDataInfo(igameDataPath, node);
								}
								free(igameDataPath);
							}

							// Generate title and add in the list
							// Run the following if the node->title is empty
							if (isStringEmpty(node->title))
							{
								generateItemName(node->path, node->title, sizeof(node->title));
							}

							// Scan how many others with same title exist and increase a number at the end of the list (alt)
							slavesListCountInstancesByTitle(node);

							// TODO: IDEA - Instead of adding at the tail insert it in sorted position
							slavesListAddTail(node);
							addGenreInList(node->genre);
							addChipsetInList(node->chipset);
						}
						else
						{
							if (current_settings->useIgameDataTitle)
							{
								char *igameDataPath = malloc(sizeof(char) * MAX_PATH_SIZE);
								if(igameDataPath == NULL)
								{
									msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
									return FALSE;
								}

								getParentPath(existingNode->path, igameDataPath, sizeof(char) * MAX_PATH_SIZE);
								snprintf(igameDataPath, sizeof(char) * MAX_PATH_SIZE, "%s/%s", igameDataPath, DEFAULT_IGAMEDATA_FILE); // cppcheck-suppress sprintfOverlappingData
								if (check_path_exists(igameDataPath))
								{
									slavesList *node = malloc(sizeof(slavesList));
									if(node == NULL)
									{
										msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
										FreeVec(FIblock);
										UnLock(lock);
										free(buf);
										return FALSE;
									}

									node->instance = 0;
									node->title[0] = '\0';
									node->genre[0] = '\0';
									snprintf(node->genre, sizeof(node->genre), "Unknown");
									node->user_title[0] = '\0';
									node->chipset[0] = '\0';
									node->rating[0] = '\0';
									node->times_played = 0;
									node->favourite = 0;
									node->last_played = 0;
									node->exists = 1;
									node->hidden = 0;
									node->deleted = 0;
									node->year = 0;
									node->players = 0;
									node->path[0] = '\0';

									getIGameDataInfo(igameDataPath, node);

									strncpy(existingNode->title, node->title, MAX_SLAVE_TITLE_SIZE);
									if (isStringEmpty(existingNode->title))
									{
										// Fallback generation of the title and addition in the list
										generateItemName(existingNode->path, existingNode->title, sizeof(existingNode->title));
									}

									if (!isStringEmpty(node->genre))
									{
										strncpy(existingNode->genre, node->genre, MAX_GENRE_NAME_SIZE);
										addGenreInList(node->genre);
									}
									if (!isStringEmpty(node->chipset))
									{
										strncpy(existingNode->chipset, node->chipset, MAX_CHIPSET_SIZE);
										addChipsetInList(node->chipset);
									}
									free(node);
								}

								free(igameDataPath);
							}

							// If the current scan settings are different from last scan settings and
							// the igame.data file is not used, get the title from the file
							if (!current_settings->useIgameDataTitle && current_settings->lastScanSetup != calcLastScanBitfield())
							{
								generateItemName(existingNode->path, existingNode->title, sizeof(existingNode->title));
							}
						}
					}
				}
			}
			FreeVec(FIblock);
		}

		UnLock(lock);
	}

	free(buf);
	return success;
}

void scan_repositories(void)
{
	if (repos)
	{
		set(app->WI_MainWindow, MUIA_Window_Sleep, TRUE);

		for (item_repos = repos; item_repos != NULL; item_repos = item_repos->next)
		{
			if(check_path_exists(item_repos->repo))
			{
				if (!(examineFolder(item_repos->repo))) break;
			}
		}

		// Collect entries whose paths no longer exist on disk
		{
			#define MAX_STALE_DISPLAY 15
			#define MSG_BUF_SIZE 4096
			slavesList *staleEntries[MAX_STALE_DISPLAY];
			int staleCount = 0;
			int staleTotal = 0;

			slavesList *currPtr = getSlavesListHead();
			while (currPtr != NULL)
			{
				if (!currPtr->hidden && !check_path_exists(currPtr->path))
				{
					if (staleCount < MAX_STALE_DISPLAY)
						staleEntries[staleCount++] = currPtr;
					staleTotal++;
				}
				currPtr = currPtr->next;
			}

			if (staleTotal > 0)
			{
				char *titleList = malloc(MSG_BUF_SIZE);
				char *msgBuf = malloc(MSG_BUF_SIZE);
				if (titleList && msgBuf)
				{
					titleList[0] = '\0';
					for (int i = 0; i < staleCount; i++)
					{
						const char *name = !isStringEmpty(staleEntries[i]->user_title)
							? staleEntries[i]->user_title
							: staleEntries[i]->title;
						strncat(titleList, name, MSG_BUF_SIZE - strlen(titleList) - 2);
						strncat(titleList, "\n", MSG_BUF_SIZE - strlen(titleList) - 1);
					}
					if (staleTotal > MAX_STALE_DISPLAY)
					{
						char moreLine[64];
						snprintf(moreLine, sizeof(moreLine),
							(const char *)GetMBString(MSG_AndNMore),
							(long)(staleTotal - MAX_STALE_DISPLAY));
						strncat(titleList, moreLine, MSG_BUF_SIZE - strlen(titleList) - 1);
					}

					snprintf(msgBuf, MSG_BUF_SIZE,
						(const char *)GetMBString(MSG_ConfirmRemoveStale), titleList);

					if (msg_box_confirm(msgBuf))
					{
						currPtr = getSlavesListHead();
						while (currPtr != NULL)
						{
							slavesList *nextPtr = currPtr->next;
							if (!currPtr->hidden && !check_path_exists(currPtr->path))
							{
								slavesListRemoveByPath(currPtr->path, sizeof(char) * MAX_PATH_SIZE);
							}
							currPtr = nextPtr;
						}
					}
				}
				free(titleList);
				free(msgBuf);
			}
			#undef MAX_STALE_DISPLAY
			#undef MSG_BUF_SIZE
		}

		setStatusText(GetMBString(MSG_ScanCompletedUpdatingList));
		save_list();
		showSlavesList();

		// Clear the genres lists before populating them again
		if (!current_settings->hide_filter_bar)
		{
			DoMethod(app->LV_GenresList, MUIM_List_Clear);
		}
		DoMethod(app->LV_WI_Information_Genre, MUIM_List_Clear);
		populateGenresLists();

		populateChipsetList();
		setLastScanBitfield();
		set(app->WI_MainWindow, MUIA_Window_Sleep, FALSE);
	}
}

static void applySidePanelChange(void)
{
	if (!current_settings->hide_side_panel)
	{
		DoMethod(app->GR_sidepanel, MUIM_Group_InitChange);

		DoMethod(app->GR_sidepanel, OM_REMMEMBER, app->GR_GameInfo);
		if (app->GR_spacedScreenshot)
			DoMethod(app->GR_sidepanel, OM_REMMEMBER, app->GR_spacedScreenshot);

		DoMethod(app->GR_sidepanel, OM_ADDMEMBER, app->GR_GameInfo);

		if (!current_settings->hide_screenshots) {
			app->IM_GameImage_0 = NULL;
			app->GR_spacedScreenshot = HGroup, MUIA_Group_Spacing, 0,
				Child, HSpace(0),
				End;
			DoMethod(app->GR_sidepanel, OM_ADDMEMBER, app->GR_spacedScreenshot);
		}

		DoMethod(app->GR_sidepanel, MUIM_Group_ExitChange);
	}
	sidepanelChanged = FALSE;
}

static void replaceScreenshot(void)
{
	struct List *childsList = (struct List *)xget(app->GR_spacedScreenshot, MUIA_Group_ChildList);
	Object *cstate = (Object *)childsList->lh_Head;
	Object *child;

	DoMethod(app->GR_spacedScreenshot, MUIM_Group_InitChange);
	while ((child = (Object *)NextObject(&cstate)))
	{
		DoMethod(app->GR_spacedScreenshot, OM_REMMEMBER, child);
		MUI_DisposeObject(child);
	}
	DoMethod(app->GR_spacedScreenshot, OM_ADDMEMBER, app->IM_GameImage_0 = app->IM_GameImage_1);
	DoMethod(app->GR_spacedScreenshot, MUIM_Group_ExitChange);
}

static void show_screenshot(STRPTR screenshot_path)
{
	static char prvScreenshot[MAX_PATH_SIZE];

	if (strcmp(screenshot_path, prvScreenshot))
	{
		if (current_settings->no_guigfx)
		{
			app->IM_GameImage_1 = MUI_NewObject(Dtpic_Classname,
						MUIA_Dtpic_Name,				screenshot_path,
						MUIA_Frame, 					MUIV_Frame_ImageButton,
			TAG_DONE);
		}
		else
		{
			app->IM_GameImage_1 = GuigfxObject,
						MUIA_Guigfx_FileName,			screenshot_path,
						MUIA_Guigfx_Quality,			MUIV_Guigfx_Quality_Best,
						MUIA_Guigfx_ScaleMode,			NISMF_SCALEFREE | NISMF_KEEPASPECT_PICTURE,
						MUIA_Guigfx_Transparency,		0,
						MUIA_Frame, 					MUIV_Frame_ImageButton,
			End;
		}

		if (app->IM_GameImage_1)
		{
			replaceScreenshot();
		}

		strncpy(prvScreenshot, screenshot_path, MAX_PATH_SIZE - 1);
		prvScreenshot[MAX_PATH_SIZE - 1] = '\0';
	}
}

static void get_screenshot_path(char *game_title, char *result)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char buf[MAX_PATH_SIZE];

	slavesList *existingNode = NULL;
	if (existingNode = slavesListSearchByTitle(game_title, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		getParentPath(existingNode->path, buf, bufSize);

		// Return the igame.iff from the game folder, if exists
		snprintf(result, sizeof(char) * MAX_PATH_SIZE, "%s/igame.iff", buf);
		if(checkImageDatatype(result))
		{
			return;
		}

		// Return the slave icon from the game folder, if exists
		// TODO: Check if this item is a slave. If not don't use the substring
#if !defined(__morphos__)
		{
			STRPTR tmp = substring(existingNode->path, 0, -6);
			if (tmp) { snprintf(result, sizeof(char) * MAX_PATH_SIZE, "%s.info", tmp); free(tmp); }
		}
		if(checkImageDatatype(result))
		{
			return;
		}
#endif

		// Return the default image from iGame folder, if exists
		if(check_path_exists(DEFAULT_SCREENSHOT_FILE))
		{
			snprintf(result, sizeof(char) * MAX_PATH_SIZE, "%s", DEFAULT_SCREENSHOT_FILE);
		}
	}
}

static void showDefaultScreenshot(void)
{
	if (current_settings->hide_side_panel || current_settings->hide_screenshots)
		return;

	show_screenshot(DEFAULT_SCREENSHOT_FILE);
}

static void populate_sidebar_igamedata(slavesList *node)
{
	char developer[64] = "";
	char devYear[80];

	char parentPath[MAX_PATH_SIZE];
	char igameDataPath[MAX_PATH_SIZE];
	getParentPath(node->path, parentPath, sizeof(parentPath));
	snprintf(igameDataPath, sizeof(igameDataPath), "%s/%s", parentPath, DEFAULT_IGAMEDATA_FILE);

	if (check_path_exists(igameDataPath))
	{
		const BPTR fp = Open(igameDataPath, MODE_OLDFILE);
		if (fp)
		{
			char line[64];
			while (FGets(fp, line, sizeof(line)) != NULL)
			{
				char **tokens = str_split(line, '=');
				if (tokens && tokens[1] != NULL)
				{
					int len = strlen(tokens[1]);
					if (len > 0 && tokens[1][len - 1] == '\n')
						tokens[1][len - 1] = '\0';

					if (!strcmp(tokens[0], "by") && !isStringEmpty(tokens[1]))
						strncpy(developer, tokens[1], sizeof(developer) - 1);

					for (int i = 0; *(tokens + i); i++)
						free(*(tokens + i));
					free(tokens);
				}
			}
			Close(fp);
		}
	}

	if (developer[0] && node->year > 0)
		snprintf(devYear, sizeof(devYear), "%s, %d", developer, node->year);
	else if (developer[0])
		snprintf(devYear, sizeof(devYear), "%s", developer);
	else if (node->year > 0)
		snprintf(devYear, sizeof(devYear), "%d", node->year);
	else
		devYear[0] = '\0';

	set(app->TX_SB_ReleasedBy, MUIA_Text_Contents, devYear);
}

void game_click(void)
{
	char *game_title = NULL;
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &game_title);

	if (!game_title)
		return;

	// Screenshot loading
	if (!current_settings->hide_side_panel && !current_settings->hide_screenshots)
	{
		strncpy(pending_screenshot_title, game_title,
				MAX_SLAVE_TITLE_SIZE - 1);
		pending_screenshot_title[MAX_SLAVE_TITLE_SIZE - 1] = '\0';
		screenshot_pending = TRUE;
		screenshot_cooldown = SCREENSHOT_COOLDOWN_TICKS;
	}

	// Sidebar game info
	if (current_settings->hide_side_panel)
		return;

	// Show sidebar game info (hidden on startup)
	set(app->GR_GameInfo, MUIA_ShowMe, TRUE);

	slavesList *node = slavesListSearchByTitle(game_title, sizeof(char) * MAX_SLAVE_TITLE_SIZE);
	if (!node)
		return;

	setSlavesListBuffer(node);

	// Display-only fields
	char tmpStr[16];

	if (!isStringEmpty(node->user_title))
		set(app->TX_SB_Title, MUIA_Text_Contents, node->user_title);
	else
		set(app->TX_SB_Title, MUIA_Text_Contents, node->title);

	snprintf(tmpStr, sizeof(tmpStr), "%dx", node->times_played);
	set(app->TX_SB_TimesPlayed, MUIA_Text_Contents, tmpStr);

	set(app->TX_SB_Genre, MUIA_Text_Contents, node->genre);

	if (node->players > 0)
		snprintf(tmpStr, sizeof(tmpStr), "%d", node->players);
	else
		tmpStr[0] = '\0';
	set(app->TX_SB_Players, MUIA_Text_Contents, tmpStr);

	set(app->TX_SB_Chipset, MUIA_Text_Contents, node->chipset);
	set(app->TX_SB_Rating, MUIA_Text_Contents, node->rating);

	// igame.data: developer, links
	populate_sidebar_igamedata(node);
}

static void screenshot_load(void)
{
	screenshot_pending = FALSE;

	if (current_settings->hide_side_panel || current_settings->hide_screenshots)
		return;

	char *image_path = AllocVec(sizeof(char) * MAX_PATH_SIZE, MEMF_CLEAR);
	if (image_path == NULL)
	{
		msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
		return;
	}

	get_screenshot_path(pending_screenshot_title, image_path);
	show_screenshot(image_path);

	FreeVec(image_path);
}

void screenshot_update(void)
{
	if (!screenshot_pending)
		return;

	screenshot_load();
}

void screenshot_tick(void)
{
	if (!screenshot_pending)
		return;

	if (screenshot_cooldown > 0)
	{
		screenshot_cooldown--;
		return;
	}

	screenshot_load();
}

BOOL screenshot_is_pending(void)
{
	return screenshot_pending;
}

/*
* Adds a repository (path on the disk)
* to the list of repositories
*/
void repo_add(void)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = malloc(bufSize);
	char* repo_path = NULL;
	get(app->PA_RepoPath, MUIA_String_Contents, &repo_path);

	if (repo_path && strlen(repo_path) != 0)
	{
		item_repos = (repos_list *)calloc(1, sizeof(repos_list));
		item_repos->next = NULL;

		if(repo_path[strlen(repo_path) - 1] == '/')
		{
			repo_path[strlen(repo_path) - 1] = '\0';
		}

		getFullPath(repo_path, buf);
		strncpy(item_repos->repo, buf, sizeof(item_repos->repo) - 1);
		item_repos->repo[sizeof(item_repos->repo) - 1] = '\0';
		if (isPathOnAssign(repo_path))
		{
			strncpy(item_repos->repo, repo_path, sizeof(item_repos->repo) - 1);
			item_repos->repo[sizeof(item_repos->repo) - 1] = '\0';
		}

		if (repos == NULL)
			repos = item_repos;
		else
		{
			item_repos->next = repos;
			repos = item_repos;
		}

		DoMethod(app->LV_GameRepositories, MUIM_List_InsertSingle, item_repos->repo, 1, MUIV_List_Insert_Bottom);
	}
	free(buf);
}

void repo_remove(void)
{
	char *path = NULL;
	DoMethod(app->LV_GameRepositories, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &path);

	repos_list *prevRepo = NULL;
	for (item_repos = repos; item_repos != NULL; item_repos = item_repos->next)
	{
		if (!strcmp(path, item_repos->repo))
		{
			if (prevRepo->repo)
			{
				prevRepo->next = item_repos->next;
			}
			else
			{
				repos = item_repos->next;
			}
			free(item_repos);
			DoMethod(app->LV_GameRepositories, MUIM_List_Remove, MUIV_List_Remove_Active);
			return;
		}
		prevRepo = item_repos;
	}
}

/*
* Writes the repositories to the repo.prefs file
*/
void repo_stop(void)
{
	const BPTR fprepos = Open((CONST_STRPTR)DEFAULT_REPOS_FILE, MODE_NEWFILE);
	if (!fprepos)
	{
		msg_box((const char*)GetMBString(MSG_CouldNotCreateReposFile));
	}
	else
	{
		CONST_STRPTR repo_path = NULL;
		for (int i = 0;; i++)
		{
			DoMethod(app->LV_GameRepositories, MUIM_List_GetEntry, i, &repo_path);
			if (!repo_path)
				break;

			FPuts(fprepos, repo_path);
			FPutC(fprepos, '\n');
		}
		Close(fprepos);
	}
}

// Shows the Properties window populating the information fields
void slaveProperties(void)
{
	char *title = NULL;
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &title);
	if(isStringEmpty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if (node = slavesListSearchByTitle(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		setSlavesListBuffer(node);

		set(app->TX_PropertiesGameTitle, MUIA_Text_Contents, node->title);
		if(!isStringEmpty(node->user_title))
		{
			set(app->TX_PropertiesGameTitle, MUIA_Text_Contents, node->user_title);
		}
		set(app->PA_PropertiesSlavePath, MUIA_String_Contents, node->path);

		updateToolTypesText(node->path);

		set(app->WI_Properties, MUIA_Window_Open, TRUE);
	}
}

// Save item properties when the user clicks on Save button
void saveItemProperties(void)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);
	ULONG tooltypesEnabled = 0;
	slavesList *node = NULL;
	node = getSlavesListBuffer(); // cppcheck-suppress memleak

	get(app->PA_PropertiesSlavePath, MUIA_String_Contents, &buf);
	strncpy(node->path, buf, sizeof(node->path));

	get(app->TX_PropertiesTooltypes, MUIA_Disabled, &tooltypesEnabled);
	if (IconBase && (IconBase->lib_Version >= 44) && (tooltypesEnabled == 0))
	{
		// Save the tooltypes
		showDefaultScreenshot();
		STRPTR tooltypesBuffer = (STRPTR)DoMethod(app->TX_PropertiesTooltypes, MUIM_TextEditor_ExportText);
		snprintf(buf, sizeof(node->path), "%s", substring(node->path, 0, -6));
		setIconTooltypes(buf, tooltypesBuffer);
		game_click();
	}
}

void updateToolTypesText(const char *buf)
{
	int pathSize = sizeof(char) * MAX_PATH_SIZE;
	char *pathBuffer = calloc(1, pathSize);
	char *tooltypesBuffer = calloc(1, sizeof(char) * 1024);
	if (pathBuffer == NULL || tooltypesBuffer == NULL)
	{
		msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
		return;
	}

	strlcpy(pathBuffer, buf, pathSize);
	if (strcasestr(buf, ".slave"))
	{
		strlcpy(pathBuffer, substring(pathBuffer, 0, -6), pathSize); // Remove the .slave extension
		getIconTooltypes(pathBuffer, tooltypesBuffer);
		set(app->TX_PropertiesTooltypes, MUIA_Disabled, FALSE);
		set(app->TX_PropertiesTooltypes, MUIA_TextEditor_Contents, tooltypesBuffer);
	}
	else
	{
		set(app->TX_PropertiesTooltypes, MUIA_TextEditor_Contents, "");
		set(app->TX_PropertiesTooltypes, MUIA_Disabled, TRUE);
	}
	free(pathBuffer);
	free(tooltypesBuffer);
}

void app_stop(void)
{
	if (current_settings->save_stats_on_exit)
		save_list();

	memset(&fname[0], 0, sizeof fname);
	emptySlavesList();
	emptyGenresList();

	if (repos)
	{
		free(repos);
		repos = NULL;
	}
}

void save_list(void)
{
	slavesListSaveToCSV(DEFAULT_GAMESLIST_FILE);
}

void import_ratings(void)
{
	const int lineSize = 256;
	int count = 0;
	char buf[MAX_SLAVE_TITLE_SIZE];

	if (!check_path_exists(DEFAULT_RATINGS_FILE))
	{
		msg_box((const char *)GetMBString(MSG_ImportRatingsFileNotFound));
		return;
	}

	const BPTR fp = Open(DEFAULT_RATINGS_FILE, MODE_OLDFILE);
	if (!fp)
	{
		msg_box((const char *)GetMBString(MSG_ImportRatingsFileNotFound));
		return;
	}

	set(app->WI_MainWindow, MUIA_Window_Sleep, TRUE);

	char *line = malloc(lineSize);
	if (!line)
	{
		Close(fp);
		set(app->WI_MainWindow, MUIA_Window_Sleep, FALSE);
		msg_box((const char *)GetMBString(MSG_NotEnoughMemory));
		return;
	}

	while (FGets(fp, line, lineSize) != NULL)
	{
		int len = strlen(line);
		if (len > 0 && line[len - 1] == '\n')
			line[--len] = '\0';

		if (len == 0 || line[0] == ';')
			continue;

		char *sep = strchr(line, ';');
		if (!sep)
			continue;

		*sep = '\0';
		char *title = line;
		char *rating = sep + 1;

		if (isStringEmpty(title) || isStringEmpty(rating))
			continue;

		slavesList *currPtr = getSlavesListHead();
		while (currPtr != NULL)
		{
			int matched = 0;
			if (strlen(currPtr->title) == strlen(title) &&
				strcasestr(currPtr->title, title) != NULL)
				matched = 1;
			else if (!isStringEmpty(currPtr->user_title) &&
				strlen(currPtr->user_title) == strlen(title) &&
				strcasestr(currPtr->user_title, title) != NULL)
				matched = 1;

			if (matched)
			{
				strncpy(currPtr->rating, rating, sizeof(currPtr->rating) - 1);
				currPtr->rating[sizeof(currPtr->rating) - 1] = '\0';
				count++;
			}
			currPtr = currPtr->next;
		}
	}

	free(line);
	Close(fp);

	filter_change();

	snprintf(buf, sizeof(buf), (const char *)GetMBString(MSG_ImportRatingsComplete), count);
	setStatusText(buf);

	set(app->WI_MainWindow, MUIA_Window_Sleep, FALSE);
}

// TODO: Make this work
void save_list_as(void)
{
	// TODO: Check if file exists, warn the user about overwriting it
	// if (get_filename("Save List As...", "Save", TRUE))
	// 	save_to_csv(fname, 0);
}

// TODO: Make this work
void open_list(void)
{
	if (get_filename("Open List", "Open", FALSE))
	{
		clear_gameslist();
		// load_games_list(fname);
	}
}

//function to return the dec eq of a hex no.
int hex2dec(char *hexin)
{
	unsigned int dec;
	//lose the first $ character
	hexin++;
	sscanf(hexin, "%x", &dec);
	return dec;
}

static LONG xget(Object* obj, ULONG attribute)
{
	LONG x;
	get(obj, attribute, &x);
	return x;
}

static int get_cycle_index(Object* obj)
{
	int index = 0;
	get(obj, MUIA_Cycle_Active, &index);
	return index;
}

static int get_radio_index(Object* obj)
{
	int index = 0;
	get(obj, MUIA_Radio_Active, &index);
	return index;
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

void settingUseIgameDataTitleChanged(void)
{
	current_settings->useIgameDataTitle = (BOOL)xget(app->CH_UseIgameDataTitle, MUIA_Selected);
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
		applySidePanelChange();

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
	pos += snprintf(file_line + pos, buffer_size - pos, "use_igame.data_title=%d\n", current_settings->useIgameDataTitle);
	pos += snprintf(file_line + pos, buffer_size - pos, "last_scan_setup=%d\n", current_settings->lastScanSetup);
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

void toggle_side_panel(void)
{
	current_settings->hide_side_panel = !current_settings->hide_side_panel;

	set(app->BA_Balance, MUIA_ShowMe, !current_settings->hide_side_panel);
	set(app->GR_sidepanel, MUIA_ShowMe, !current_settings->hide_side_panel);

	if (!current_settings->hide_side_panel)
	{
		game_click();
	}

	nnset(app->CH_HideSidepanel, MUIA_Selected, current_settings->hide_side_panel);
}

void toggle_filter_bar(void)
{
	current_settings->hide_filter_bar = !current_settings->hide_filter_bar;
	set(app->GR_FilterBar, MUIA_ShowMe, !current_settings->hide_filter_bar);
}


void setting_start_with_favorites_changed(void)
{
	current_settings->start_with_favorites = (BOOL)xget(app->CH_StartWithFavorites, MUIA_Selected);
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

void setting_short_year_changed(void)
{
	current_settings->short_year = (BOOL)xget(app->CH_ShortYear, MUIA_Selected);
	apply_column_settings();
}

void msg_box(const char* msg)
{
	struct EasyStruct msgbox;

	msgbox.es_StructSize = sizeof msgbox;
	msgbox.es_Flags = 0;
	msgbox.es_Title = (UBYTE*)GetMBString(MSG_WI_MainWindow);
	msgbox.es_TextFormat = (unsigned char*)msg;
	msgbox.es_GadgetFormat = (UBYTE*)GetMBString(MSG_BT_AboutOK);

	EasyRequest(NULL, &msgbox, NULL);
}

static int msg_box_confirm(const char *msg)
{
	struct EasyStruct msgbox;
	msgbox.es_StructSize = sizeof msgbox;
	msgbox.es_Flags = 0;
	msgbox.es_Title = (UBYTE*)GetMBString(MSG_WI_MainWindow);
	msgbox.es_TextFormat = (unsigned char*)msg;
	msgbox.es_GadgetFormat = (UBYTE*)"Yes|No";
	return EasyRequest(NULL, &msgbox, NULL);
}

void add_non_whdload(void)
{
	set(app->STR_AddTitle, MUIA_String_Contents, NULL);
	set(app->PA_AddGame, MUIA_String_Contents, NULL);
	set(app->WI_AddNonWHDLoad, MUIA_Window_Open, TRUE);
}

void non_whdload_ok(void)
{
	int genreId = 0;
	char *path, *title;
	ULONG newpos;
	slavesList *node = malloc(sizeof(slavesList));

	node->instance = 0;
	node->title[0] = '\0';
	node->user_title[0] = '\0';
	node->genre[0] = '\0';
	node->arguments[0] = '\0';
	node->chipset[0] = '\0';
	node->rating[0] = '\0';
	node->times_played = 0;
	node->favourite = 0;
	node->last_played = 0;
	node->exists = 1;
	node->hidden = 0;
	node->deleted = 0;
	node->year = 0;
	node->players = 0;
	node->path[0] = '\0';

	get(app->PA_AddGame, MUIA_String_Contents, &path);
	getFullPath(path, node->path);

	get(app->STR_AddTitle, MUIA_String_Contents, &title);
	strncpy(node->title, title, sizeof(node->title));

	get(app->CY_AddGameGenre, MUIA_Cycle_Active, &genreId);
	strncpy(node->genre, app->GenresContent[genreId], sizeof(node->genre));

	if (isStringEmpty(title))
	{
		msg_box((const char*)GetMBString(MSG_NoTitleSpecified));
		free(node);
		return;
	}
	if (isStringEmpty(path))
	{
		msg_box((const char*)GetMBString(MSG_NoExecutableSpecified));
		free(node);
		return;
	}

	slavesListAddTail(node);

	set(app->LV_GamesList, MUIA_NList_Quiet, TRUE);
	DoMethod(app->LV_GamesList,
		MUIM_NList_InsertSingle, node,
		MUIV_NList_Insert_Bottom);
	get(app->LV_GamesList, MUIA_NList_InsertPosition, &newpos);
	set(app->LV_GamesList, MUIA_NList_Active, newpos);
	DoMethod(app->LV_GamesList, MUIM_NList_Sort);
	set(app->LV_GamesList, MUIA_NList_Quiet, FALSE);
}

static void joy_left(void)
{
  char *curr_game = NULL, *prev_game = NULL, *last_game = NULL;
  int ind;

  DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &curr_game);
  get(app->LV_GamesList, MUIA_NList_Active, &ind);

  if (curr_game == NULL || strlen(curr_game) == 0)
    {
      set(app->LV_GamesList, MUIA_NList_Active, MUIV_NList_Active_Top);
      return;
    }

  DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, ind--, &prev_game);
  if (prev_game == NULL)
	return;

  while (toupper(curr_game[0]) == toupper(prev_game[0]))
    {
      DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, ind--, &prev_game);
      if (prev_game == NULL)
	return;
    }

  last_game = prev_game;

  while (toupper(last_game[0]) == toupper(prev_game[0]))
    {
      DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, ind--, &prev_game);
      if (prev_game == NULL)
	return;
    }

  set(app->LV_GamesList, MUIA_NList_Active, ind+2);
}

static void joy_right(void)
{
  char *curr_game = NULL, *next_game = NULL;
  int ind;

  DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &curr_game);
  get(app->LV_GamesList, MUIA_NList_Active, &ind);

  if (curr_game == NULL || strlen(curr_game) == 0)
    {
      set(app->LV_GamesList, MUIA_NList_Active, MUIV_NList_Active_Top);
      return;
    }

  DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, ind++, &next_game);
  if (next_game == NULL)
	return;

  while (toupper(curr_game[0]) == toupper(next_game[0]))
    {
      DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, ind++, &next_game);
      if (next_game == NULL)
	return;
    }

  set(app->LV_GamesList, MUIA_NList_Active, ind-1);

}

ULONG get_wb_version(void)
{
	static ULONG ver = 0UL;

	if (ver != 0UL)
	{
		return ver;
	}

	if (WorkbenchBase == NULL)
	{
		//Really Workbench library should be already opened
		// Somehow we're running without Workbench.
		// Nothing to do since the version variable inits with 0.
		return ver;
	}

	// Save workbench.library version for later calls
	ver = WorkbenchBase->lib_Version;

	return ver;
}


static void joystick_buttons(ULONG val)
{
	//if (val & JPF_BUTTON_PLAY) printf("[PLAY/MMB]\n");
	//if (val & JPF_BUTTON_REVERSE) printf("[REVERSE]\n");
	//if (val & JPF_BUTTON_FORWARD) printf("[FORWARD]\n");
	//if (val & JPF_BUTTON_GREEN) printf("[SHUFFLE]\n");
	if (val & JPF_BUTTON_RED)
	{
		launch_game();
	}
	//if (val & JPF_BUTTON_BLUE) printf("[STOP/RMB]\n");
}

static void joystick_directions(ULONG val)
{
	if (val & JPF_JOY_UP)
		set(app->LV_GamesList, MUIA_NList_Active, MUIV_NList_Active_Up);

	if (val & JPF_JOY_DOWN)
		set(app->LV_GamesList, MUIA_NList_Active, MUIV_NList_Active_Down);

	if (val & JPF_JOY_LEFT)
	  joy_left();

	if (val & JPF_JOY_RIGHT)
	  joy_right();
}

void joystick_input(ULONG val)
{
	if ((val & JP_TYPE_MASK) == JP_TYPE_NOTAVAIL)
		return;
	if ((val & JP_TYPE_MASK) == JP_TYPE_UNKNOWN)
		return;
	if ((val & JP_TYPE_MASK) == JP_TYPE_MOUSE)
		return;

	if ((val & JP_TYPE_MASK) == JP_TYPE_JOYSTK)
	{
		joystick_directions(val);
		joystick_buttons(val);
	}

	if ((val & JP_TYPE_MASK) == JP_TYPE_GAMECTLR)
	{
		joystick_directions(val);
		joystick_buttons(val);
	}
}

void joystick_direction_repeat(ULONG val)
{
	joystick_directions(val);
}

// Shows the Properties window populating the information fields
void getItemInformation(void)
{
	char *title = NULL;
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &title);
	if(isStringEmpty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if (node = slavesListSearchByTitle(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		char tmpStr[6];

		char *igameDataPath = malloc(sizeof(char) * MAX_PATH_SIZE);
		if(igameDataPath == NULL)
		{
			msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
			return;
		}

		// This is used when the item data change and gets saved
		setSlavesListBuffer(node);

		set(app->STR_WI_Information_Title, MUIA_String_Contents, node->title);
		if(!isStringEmpty(node->user_title))
		{
			set(app->STR_WI_Information_Title, MUIA_String_Contents, node->user_title);
		}

		snprintf(tmpStr, sizeof(tmpStr), "%d", node->year);
		set(app->TX_WI_Information_Year, MUIA_Text_Contents, tmpStr);

		snprintf(tmpStr, sizeof(tmpStr), "%d", node->players);
		set(app->TX_WI_Information_Players, MUIA_Text_Contents, tmpStr);

		snprintf(tmpStr, sizeof(tmpStr), "%d", node->times_played);
		set(app->TX_WI_Information_TimesPlayed, MUIA_Text_Contents, tmpStr);

		set(app->TX_WI_Information_Rating, MUIA_Text_Contents, node->rating);

		set(app->TX_WI_Information_Chipset, MUIA_Text_Contents, node->chipset);

		set(app->CH_WI_Information_Favourite, MUIA_Selected, node->favourite);

		set(app->CH_WI_Information_Hidden, MUIA_Selected, node->hidden);


		// Clear the igame.data fields first
		set(app->TX_WI_Information_ReleasedBy, MUIA_Text_Contents, '\0');
		set(app->URL_WI_Information_Lemonamiga, MUIA_Urltext_Url, '\0');
		set(app->URL_WI_Information_Lemonamiga, MUIA_ShowMe, FALSE);
		set(app->URL_WI_Information_HOL, MUIA_Urltext_Url, '\0');
		set(app->URL_WI_Information_HOL, MUIA_ShowMe, FALSE);
		set(app->URL_WI_Information_Pouet, MUIA_Urltext_Url, '\0');
		set(app->URL_WI_Information_Pouet, MUIA_ShowMe, FALSE);

		if (current_settings->show_url_links)
		{
			set(app->GR_WI_Information_Links, MUIA_ShowMe, FALSE);
		}

		// Get the info from the igame.data file if that exists
		getParentPath(node->path, igameDataPath, sizeof(char) * MAX_PATH_SIZE);
		snprintf(igameDataPath, sizeof(char) * MAX_PATH_SIZE, "%s/%s", igameDataPath, DEFAULT_IGAMEDATA_FILE); // cppcheck-suppress sprintfOverlappingData
		if (check_path_exists(igameDataPath))
		{
			const BPTR fpigamedata = Open(igameDataPath, MODE_OLDFILE);
			if (fpigamedata)
			{
				int lineSize = 64;
				char *line = malloc(lineSize * sizeof(char));
				while (FGets(fpigamedata, line, lineSize) != NULL)
				{
					char **tokens = str_split(line, '=');
					if (tokens)
					{
						if (tokens[1] != NULL)
						{
							int tokenValueLen = strlen(tokens[1]);
							if (tokens[1][tokenValueLen - 1] == '\n')
							{
								tokens[1][tokenValueLen - 1] = '\0';
							}
							else
							{
								tokens[1][tokenValueLen] = '\0';
							}

							if(!strcmp(tokens[0], "by"))
							{
								set(app->TX_WI_Information_ReleasedBy, MUIA_Text_Contents, tokens[1]);
							}

							if(!strcmp(tokens[0], "rating"))
							{
								set(app->TX_WI_Information_Rating, MUIA_Text_Contents, tokens[1]);
							}

							// Check if the MCC_URLText is available and disable the following code
							if (current_settings->show_url_links)
							{
								int urlBufferSize = sizeof(char) * 64;
								char *urlBuffer = malloc(urlBufferSize);
								int linksCnt = 0;

								if(!strcmp(tokens[0], "lemon") && !isStringEmpty(tokens[1]) && isNumeric(tokens[1]))
								{
									snprintf(urlBuffer, urlBufferSize, DEFAULT_LEMONAMIGA_URL, tokens[1]);
									set(app->URL_WI_Information_Lemonamiga, MUIA_Urltext_Url, urlBuffer);
									set(app->URL_WI_Information_Lemonamiga, MUIA_ShowMe, TRUE);
									linksCnt++;
								}

								if(!strcmp(tokens[0], "hol") && !isStringEmpty(tokens[1]) && isNumeric(tokens[1]))
								{
									snprintf(urlBuffer, urlBufferSize, DEFAULT_HOL_URL, tokens[1]);
									set(app->URL_WI_Information_HOL, MUIA_Urltext_Url, urlBuffer);
									set(app->URL_WI_Information_HOL, MUIA_ShowMe, TRUE);
									linksCnt++;
								}

								if(!strcmp(tokens[0], "pouet") && !isStringEmpty(tokens[1]) && isNumeric(tokens[1]))
								{
									snprintf(urlBuffer, urlBufferSize, DEFAULT_POUET_URL, tokens[1]);
									set(app->URL_WI_Information_Pouet, MUIA_Urltext_Url, urlBuffer);
									set(app->URL_WI_Information_Pouet, MUIA_ShowMe, TRUE);
									linksCnt++;
								}

								if (linksCnt > 0)
								{
									set(app->GR_WI_Information_Links, MUIA_ShowMe, TRUE);
								}

								free(urlBuffer);
								urlBuffer = NULL;
							}
						}

						for (int i = 0; *(tokens + i); i++)
						{
							free(*(tokens + i));
						}
						free(tokens);
					}
				}

				free(line);
				Close(fpigamedata);
			}
		}
		free(igameDataPath);

		//set the genre
		set(app->STR_WI_Information_Genre, MUIA_String_Contents, node->genre);

		set(app->WI_Information, MUIA_Window_Open, TRUE);
	}
}

void saveItemInformation(void)
{
	int genreId, hideSelection;
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);
	ULONG newpos;
	slavesList *node = NULL;
	node = getSlavesListBuffer(); // cppcheck-suppress memleak

	// Update the title
	get(app->STR_WI_Information_Title, MUIA_String_Contents, &buf);
	if (strncmp(node->title, buf, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		strncpy(node->user_title, buf, sizeof(node->user_title));
	}
	else if (strncmp(node->user_title, buf, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		node->user_title[0] = '\0';
	}

	if (strncmp(node->title, buf, sizeof(char) * MAX_SLAVE_TITLE_SIZE) ||
		strncmp(node->user_title, buf, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		set(app->LV_GamesList, MUIA_NList_Quiet, TRUE);
		DoMethod(app->LV_GamesList, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		DoMethod(app->LV_GamesList,
			MUIM_NList_InsertSingle, node,
			MUIV_NList_Insert_Sorted);
		get(app->LV_GamesList, MUIA_NList_InsertPosition, &newpos);
		set(app->LV_GamesList, MUIA_NList_Active, newpos);
		set(app->LV_GamesList, MUIA_NList_Quiet, FALSE);
	}

	// Update the genre and if it is a new one, add it to the genres list
	get(app->STR_WI_Information_Genre, MUIA_String_Contents, &buf);
	strlcpy(node->genre, buf, sizeof(node->genre));
	genresList *newGenrePtr = addGenreInList(buf);
	if (newGenrePtr != NULL)
	{
		if (!current_settings->hide_filter_bar)
		{
			DoMethod(app->LV_GenresList, MUIM_List_InsertSingle, newGenrePtr->title, MUIV_List_Insert_Sorted);
		}
		DoMethod(app->LV_WI_Information_Genre, MUIM_List_InsertSingle, newGenrePtr->title, MUIV_List_Insert_Sorted);
	}

	// Update favourite selection
	get(app->CH_WI_Information_Favourite, MUIA_Selected, &node->favourite);

	// Update hidden selection
	get(app->CH_WI_Information_Hidden, MUIA_Selected, &hideSelection);
	if (node->hidden != hideSelection)
	{
		node->hidden = hideSelection;
		if (hideSelection)
		{
			blacklistAdd(node->path);
		}
		else
		{
			blacklistRemove(node->path);
		}
		blacklistSave(DEFAULT_BLACKLIST_FILE);
		set(app->LV_GamesList, MUIA_NList_Quiet, TRUE);
		DoMethod(app->LV_GamesList, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		set(app->LV_GamesList, MUIA_NList_Quiet, FALSE);
	}
}

void game_toggle_favourite(void)
{
	char *title = NULL;
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &title);
	if (isStringEmpty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if ((node = slavesListSearchByTitle(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE)))
	{
		node->favourite = !node->favourite;
		set(app->LV_GamesList, MUIA_NList_Quiet, TRUE);
		DoMethod(app->LV_GamesList, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		DoMethod(app->LV_GamesList, MUIM_NList_InsertSingle, node, MUIV_NList_Insert_Sorted);
		set(app->LV_GamesList, MUIA_NList_Quiet, FALSE);
	}
}

void game_hide(void)
{
	char *title = NULL;
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &title);
	if (isStringEmpty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if ((node = slavesListSearchByTitle(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE)))
	{
		node->hidden = 1;
		blacklistAdd(node->path);
		blacklistSave(DEFAULT_BLACKLIST_FILE);
		set(app->LV_GamesList, MUIA_NList_Quiet, TRUE);
		DoMethod(app->LV_GamesList, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		set(app->LV_GamesList, MUIA_NList_Quiet, FALSE);
	}
}

void game_delete(void)
{
	char *title = NULL;
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &title);
	if (isStringEmpty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if ((node = slavesListSearchByTitle(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE)))
	{
		if (!msg_box_confirm((const char*)GetMBString(MSG_ConfirmDelete)))
			return;

		int bufSize = sizeof(char) * MAX_PATH_SIZE;
		char *parentDir = AllocVec(bufSize, MEMF_CLEAR);
		getParentPath(node->path, parentDir, bufSize);

		if (parentDir[0] != '\0')
		{
			deleteDirectory(parentDir);
		}
		FreeVec(parentDir);

		set(app->LV_GamesList, MUIA_NList_Quiet, TRUE);
		DoMethod(app->LV_GamesList, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		set(app->LV_GamesList, MUIA_NList_Quiet, FALSE);

		slavesListRemoveByPath(node->path, sizeof(char) * MAX_PATH_SIZE);
	}
}
