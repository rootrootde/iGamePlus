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
#include "screenshot.h"
#include "settings.h"
#include "columns.h"

extern struct ObjApp* app;
extern struct Library *IconBase;
extern char* executable_name;

/* global variables */
char fname[255];
BOOL sidepanelChanged = FALSE; // This is temporary until settings are revamped

/* function definitions */
static void show_slaves_list(void);
static LONG xget(Object *, ULONG);
static int get_cycle_index(Object *);
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

void set_status_text(char *text)
{
	set(app->TX_Status, MUIA_Text_Contents, text);
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

			get_full_path(file_line, repo_path);
			strcpy(item_repos->repo, repo_path);

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

static void populate_genres_lists(void)
{
	if (!current_settings->hide_filter_bar)
	{
		nnset(app->LV_GenresList, MUIA_List_Quiet, TRUE);

		// Add Show All in genres list
		DoMethod(app->LV_GenresList, MUIM_List_InsertSingle, GetMBString(MSG_FilterShowAll), MUIV_List_Insert_Bottom);
	}

	load_genres_from_file();
	add_genre_in_list(GetMBString(MSG_UnknownGenre));
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

static void populate_chipset_list(void)
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
	set_status_text(GetMBString(MSG_LoadingSavedList));
	set(app->WI_MainWindow, MUIA_Window_Sleep, TRUE);
	slaves_list_load_from_csv(csvFilename);
	blacklist_load(DEFAULT_BLACKLIST_FILE);

	populate_genres_lists(); // This calls the filter_change()
	if (!current_settings->hide_filter_bar)
	{
		populate_chipset_list();
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

	show_slaves_list();
}

static BOOL createRunGameScript(slavesList *node)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);

	get_parent_path(node->path, buf, bufSize);
	FILE* runGameScript = fopen("T:rungame", "we");
	if (!runGameScript)
	{
		printf("Could not open rungame file!");
		FreeVec(buf);
		return FALSE;
	}
	get_parent_path(node->path, buf, bufSize);
	fprintf(runGameScript, "Cd \"%s\"\n", buf);
	get_name_from_path(node->path, buf, bufSize);
	fprintf(runGameScript, "Run >NIL: \"%s\"\n", buf);
	fclose(runGameScript);

	FreeVec(buf);
	return TRUE;
}

static void launchSlave(slavesList *node)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);

	get_parent_path(node->path, buf, bufSize);
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
					get_full_path(FIblock->fib_FileName, infoPath);
					{
						STRPTR tmp = substring(infoPath, 0, -5);
						if (tmp) { snprintf(infoPath, bufSize, "%s", tmp); free(tmp); }
					}

					// Get the slave filename
					get_name_from_path(node->path, buf, bufSize);
#if !defined(__morphos__)
					if (check_slave_in_tooltypes(infoPath, buf))
					{
#endif
						int execSize = sizeof(char) * MAX_EXEC_SIZE;
						char *exec = AllocVec(execSize, MEMF_CLEAR);

#if defined(__morphos__)
						snprintf(exec, execSize, "open %s", buf);
#else
						prepare_whd_execution(infoPath, exec);
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
		sprintf(exec, "C:WBRun \"%s\"", node->path);
	}

	strncpy(buf, "C:WBLoad", bufSize);
	if (check_path_exists(buf))
	{
		sprintf(exec, "C:WBLoad \"%s\"", node->path);
	}

	if (is_string_empty(exec))
	{
		if (createRunGameScript(node))
		{
			sprintf(exec, "Execute T:rungame");
		}
	}


	if (!is_string_empty(exec))
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
	if ((existingNode = slaves_list_search_by_title(game_title, sizeof(char) * MAX_SLAVE_TITLE_SIZE)) == NULL)
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	if(!check_path_exists(existingNode->path)) {
		msg_box((const char*)GetMBString(MSG_slavePathDoesntExist));
		return;
	}

	get_name_from_path(existingNode->path, buf, bufSize);

	// Slave file found
	if (strcasestr(buf, ".slave"))
	{
		launchSlave(existingNode);
	}
	else
	{
		launchFromWB(existingNode);
	}

	sprintf(buf, (const char *)GetMBString(MSG_TotalNumberOfGames), slaves_list_node_count(-1));
	set_status_text(buf);

	FreeVec(buf);
}

static void show_slaves_list(void)
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
				!is_string_empty(filters.showGenre) &&
				strncmp(filters.showGenre, currPtr->genre, sizeof(currPtr->genre))
			)
			{
				goto nextItem;
			}

			// Filter the list by the selected chipset
			if (
				!is_string_empty(filters.showChipset) &&
				strncmp(filters.showChipset, currPtr->chipset, sizeof(currPtr->chipset))
			)
			{
				goto nextItem;
			}

			// Filter by minimum rating
			if (filters.minRatingX100 > 0)
			{
				int gameRating = 0;
				if (!is_string_empty(currPtr->rating))
					gameRating = parse_rating_x100(currPtr->rating);
				if (gameRating < filters.minRatingX100)
					goto nextItem;
			}

			// Decide where the item name will be taken from
			if(!is_string_empty(currPtr->user_title))
			{
				snprintf(buf, bufSize, "%s", currPtr->user_title);
			}
			else if(is_string_empty(currPtr->user_title) && (currPtr->instance > 0))
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
			if (!is_string_empty(filters.title) && !strcasestr(buf, filters.title))
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

	sprintf(buf, (const char *)GetMBString(MSG_TotalNumberOfGames), slaves_list_node_count(cnt));
	set_status_text(buf);
	free(buf);
}

/*
 * Generate a title name based on user settings and the path
 */
static void generateItemName(char *path, char *result, int resultSize)
{
	if (current_settings->titles_from_dirs)
	{
		get_title_from_path(path, result, resultSize);
	}
	else
	{
		get_title_from_slave(path, result);
		if (!strncmp(result, "", sizeof(char) * MAX_SLAVE_TITLE_SIZE))
		{
			get_title_from_path(path, result, resultSize);
		}
	}
}

static int calcLastScanBitfield(void)
{
	int scanBitfield = 0;
	scanBitfield = 1 * current_settings->use_igamedata_title;
	scanBitfield += 2 * current_settings->titles_from_dirs;
	if(current_settings->titles_from_dirs)
	{
		scanBitfield += 4 * current_settings->no_smart_spaces;
	}
	return scanBitfield;
}

void set_last_scan_bitfield(void)
{
	current_settings->last_scan_setup = calcLastScanBitfield();
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
						strcpy(buf, FIblock->fib_FileName);

						if(!is_path_folder(path))
						{
							sprintf(buf, "%s%s", path, FIblock->fib_FileName);
						}
						else
						{
							sprintf(buf, "%s/%s", path, FIblock->fib_FileName);
						}

						get_full_path(buf, buf);
						set_status_text(buf);
						if (!(success = examineFolder(buf))) break;
					}
				}

				if(FIblock->fib_DirEntryType < 0)
				{

					// igame.data found
					if (current_settings->use_igamedata_title && !strcmp(FIblock->fib_FileName, DEFAULT_IGAMEDATA_FILE))
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
						if(!is_path_folder(path))
						{
							sprintf(igameDataPath, "%s%s", path, FIblock->fib_FileName);
						}
						else
						{
							sprintf(igameDataPath, "%s/%s", path, FIblock->fib_FileName);
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

						get_igame_data_info(igameDataPath, node);
						free(igameDataPath);

						strncpy(buf, node->path, bufSize);
						if(!is_path_folder(path))
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
							is_string_empty(buf) || !check_path_exists(node->path) ||
							blacklist_contains(node->path) ||
							(slavesListSearchByPath(node->path, sizeof(char) * MAX_PATH_SIZE) != NULL)
						) {
							free(node);
						}
						else
						{
							slaves_list_add_tail(node);
							add_genre_in_list(node->genre);
							add_chipset_in_list(node->chipset);
						}
					}

					// Slave file found
					if (strcasestr(FIblock->fib_FileName, ".slave"))
					{
						slavesList *existingNode = NULL;

						if(!is_path_folder(path))
						{
							sprintf(buf, "%s%s", path, FIblock->fib_FileName);
						}
						else
						{
							sprintf(buf, "%s/%s", path, FIblock->fib_FileName);
						}

						// Find if already exists in the list or is blacklisted, and ignore it
						if (!(existingNode = slavesListSearchByPath(buf, bufSize)) && !blacklist_contains(buf))
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
							sprintf(node->genre, "Unknown");
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

							get_full_path(buf, buf);
							strncpy(node->path, buf, sizeof(node->path));

							// if igame.data is enabled in settings use it
							if (current_settings->use_igamedata_title)
							{
								char *igameDataPath = malloc(sizeof(char) * MAX_PATH_SIZE);
								snprintf(igameDataPath, sizeof(char) * MAX_PATH_SIZE, "%s/%s", path, DEFAULT_IGAMEDATA_FILE);
								if (check_path_exists(igameDataPath))
								{
									get_igame_data_info(igameDataPath, node);
								}
								free(igameDataPath);
							}

							// Generate title and add in the list
							// Run the following if the node->title is empty
							if (is_string_empty(node->title))
							{
								generateItemName(node->path, node->title, sizeof(node->title));
							}

							// Scan how many others with same title exist and increase a number at the end of the list (alt)
							slavesListCountInstancesByTitle(node);

							// TODO: IDEA - Instead of adding at the tail insert it in sorted position
							slaves_list_add_tail(node);
							add_genre_in_list(node->genre);
							add_chipset_in_list(node->chipset);
						}
						else
						{
							if (current_settings->use_igamedata_title)
							{
								char *igameDataPath = malloc(sizeof(char) * MAX_PATH_SIZE);
								if(igameDataPath == NULL)
								{
									msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
									return FALSE;
								}

								get_parent_path(existingNode->path, igameDataPath, sizeof(char) * MAX_PATH_SIZE);
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
									sprintf(node->genre,"Unknown");
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

									get_igame_data_info(igameDataPath, node);

									strncpy(existingNode->title, node->title, MAX_SLAVE_TITLE_SIZE);
									if (is_string_empty(existingNode->title))
									{
										// Fallback generation of the title and addition in the list
										generateItemName(existingNode->path, existingNode->title, sizeof(existingNode->title));
									}

									if (!is_string_empty(node->genre))
									{
										strncpy(existingNode->genre, node->genre, MAX_GENRE_NAME_SIZE);
										add_genre_in_list(node->genre);
									}
									if (!is_string_empty(node->chipset))
									{
										strncpy(existingNode->chipset, node->chipset, MAX_CHIPSET_SIZE);
										add_chipset_in_list(node->chipset);
									}
									free(node);
								}

								free(igameDataPath);
							}

							// If the current scan settings are different from last scan settings and
							// the igame.data file is not used, get the title from the file
							if (!current_settings->use_igamedata_title && current_settings->last_scan_setup != calcLastScanBitfield())
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
						const char *name = !is_string_empty(staleEntries[i]->user_title)
							? staleEntries[i]->user_title
							: staleEntries[i]->title;
						strncat(titleList, name, MSG_BUF_SIZE - strlen(titleList) - 2);
						strcat(titleList, "\n");
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

		set_status_text(GetMBString(MSG_ScanCompletedUpdatingList));
		save_list();
		show_slaves_list();

		// Clear the genres lists before populating them again
		if (!current_settings->hide_filter_bar)
		{
			DoMethod(app->LV_GenresList, MUIM_List_Clear);
		}
		DoMethod(app->LV_WI_Information_Genre, MUIM_List_Clear);
		populate_genres_lists();

		populate_chipset_list();
		set_last_scan_bitfield();
		set(app->WI_MainWindow, MUIA_Window_Sleep, FALSE);
	}
}

void apply_side_panel_change(void)
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


static void populate_sidebar_igamedata(slavesList *node)
{
	char developer[64] = "";
	char devYear[80];

	char parentPath[MAX_PATH_SIZE];
	char igameDataPath[MAX_PATH_SIZE];
	get_parent_path(node->path, parentPath, sizeof(parentPath));
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

					if (!strcmp(tokens[0], "by") && !is_string_empty(tokens[1]))
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
		screenshot_set_pending(game_title);
	}

	// Sidebar game info
	if (current_settings->hide_side_panel)
		return;

	// Show sidebar game info (hidden on startup)
	set(app->GR_GameInfo, MUIA_ShowMe, TRUE);

	slavesList *node = slaves_list_search_by_title(game_title, sizeof(char) * MAX_SLAVE_TITLE_SIZE);
	if (!node)
		return;

	set_slaves_list_buffer(node);

	// Display-only fields
	char tmpStr[16];

	if (!is_string_empty(node->user_title))
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

		get_full_path(repo_path, buf);
		strcpy(item_repos->repo, buf);
		if (is_path_on_assign(repo_path))
		{
			strcpy(item_repos->repo, repo_path);
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
void slave_properties(void)
{
	char *title = NULL;
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &title);
	if(is_string_empty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if (node = slaves_list_search_by_title(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		set_slaves_list_buffer(node);

		set(app->TX_PropertiesGameTitle, MUIA_Text_Contents, node->title);
		if(!is_string_empty(node->user_title))
		{
			set(app->TX_PropertiesGameTitle, MUIA_Text_Contents, node->user_title);
		}
		set(app->PA_PropertiesSlavePath, MUIA_String_Contents, node->path);

		update_tooltypes_text(node->path);

		set(app->WI_Properties, MUIA_Window_Open, TRUE);
	}
}

// Save item properties when the user clicks on Save button
void save_item_properties(void)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);
	ULONG tooltypesEnabled = 0;
	slavesList *node = NULL;
	node = get_slaves_list_buffer(); // cppcheck-suppress memleak

	get(app->PA_PropertiesSlavePath, MUIA_String_Contents, &buf);
	strncpy(node->path, buf, sizeof(node->path));

	get(app->TX_PropertiesTooltypes, MUIA_Disabled, &tooltypesEnabled);
	if (IconBase && (IconBase->lib_Version >= 44) && (tooltypesEnabled == 0))
	{
		// Save the tooltypes
		show_default_screenshot();
		STRPTR tooltypesBuffer = (STRPTR)DoMethod(app->TX_PropertiesTooltypes, MUIM_TextEditor_ExportText);
		snprintf(buf, sizeof(node->path), "%s", substring(node->path, 0, -6));
		set_icon_tooltypes(buf, tooltypesBuffer);
		game_click();
	}
}

void update_tooltypes_text(const char *buf)
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
		get_icon_tooltypes(pathBuffer, tooltypesBuffer);
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
	slaves_list_save_to_csv(DEFAULT_GAMESLIST_FILE);
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

		if (is_string_empty(title) || is_string_empty(rating))
			continue;

		slavesList *currPtr = getSlavesListHead();
		while (currPtr != NULL)
		{
			int matched = 0;
			if (strlen(currPtr->title) == strlen(title) &&
				strcasestr(currPtr->title, title) != NULL)
				matched = 1;
			else if (!is_string_empty(currPtr->user_title) &&
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
	set_status_text(buf);

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
	get_full_path(path, node->path);

	get(app->STR_AddTitle, MUIA_String_Contents, &title);
	strncpy(node->title, title, sizeof(node->title));

	get(app->CY_AddGameGenre, MUIA_Cycle_Active, &genreId);
	strncpy(node->genre, app->GenresContent[genreId], sizeof(node->genre));

	if (is_string_empty(title))
	{
		msg_box((const char*)GetMBString(MSG_NoTitleSpecified));
		free(node);
		return;
	}
	if (is_string_empty(path))
	{
		msg_box((const char*)GetMBString(MSG_NoExecutableSpecified));
		free(node);
		return;
	}

	slaves_list_add_tail(node);

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
void get_item_information(void)
{
	char *title = NULL;
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &title);
	if(is_string_empty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if (node = slaves_list_search_by_title(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		char tmpStr[6];

		char *igameDataPath = malloc(sizeof(char) * MAX_PATH_SIZE);
		if(igameDataPath == NULL)
		{
			msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
			return;
		}

		// This is used when the item data change and gets saved
		set_slaves_list_buffer(node);

		set(app->STR_WI_Information_Title, MUIA_String_Contents, node->title);
		if(!is_string_empty(node->user_title))
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
		get_parent_path(node->path, igameDataPath, sizeof(char) * MAX_PATH_SIZE);
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

								if(!strcmp(tokens[0], "lemon") && !is_string_empty(tokens[1]) && is_numeric(tokens[1]))
								{
									snprintf(urlBuffer, urlBufferSize, DEFAULT_LEMONAMIGA_URL, tokens[1]);
									set(app->URL_WI_Information_Lemonamiga, MUIA_Urltext_Url, urlBuffer);
									set(app->URL_WI_Information_Lemonamiga, MUIA_ShowMe, TRUE);
									linksCnt++;
								}

								if(!strcmp(tokens[0], "hol") && !is_string_empty(tokens[1]) && is_numeric(tokens[1]))
								{
									snprintf(urlBuffer, urlBufferSize, DEFAULT_HOL_URL, tokens[1]);
									set(app->URL_WI_Information_HOL, MUIA_Urltext_Url, urlBuffer);
									set(app->URL_WI_Information_HOL, MUIA_ShowMe, TRUE);
									linksCnt++;
								}

								if(!strcmp(tokens[0], "pouet") && !is_string_empty(tokens[1]) && is_numeric(tokens[1]))
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

void save_item_information(void)
{
	int genreId, hideSelection;
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);
	ULONG newpos;
	slavesList *node = NULL;
	node = get_slaves_list_buffer(); // cppcheck-suppress memleak

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
	genresList *newGenrePtr = add_genre_in_list(buf);
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
	if (is_string_empty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if ((node = slaves_list_search_by_title(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE)))
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
	if (is_string_empty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if ((node = slaves_list_search_by_title(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE)))
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
	if (is_string_empty(title))
	{
		msg_box((const char*)GetMBString(MSG_SelectGameFromList));
		return;
	}

	slavesList *node = NULL;
	if ((node = slaves_list_search_by_title(title, sizeof(char) * MAX_SLAVE_TITLE_SIZE)))
	{
		if (!msg_box_confirm((const char*)GetMBString(MSG_ConfirmDelete)))
			return;

		int bufSize = sizeof(char) * MAX_PATH_SIZE;
		char *parentDir = AllocVec(bufSize, MEMF_CLEAR);
		get_parent_path(node->path, parentDir, bufSize);

		if (parentDir[0] != '\0')
		{
			delete_directory(parentDir);
		}
		FreeVec(parentDir);

		set(app->LV_GamesList, MUIA_NList_Quiet, TRUE);
		DoMethod(app->LV_GamesList, MUIM_NList_Remove, MUIV_NList_Remove_Active);
		set(app->LV_GamesList, MUIA_NList_Quiet, FALSE);

		slavesListRemoveByPath(node->path, sizeof(char) * MAX_PATH_SIZE);
	}
}
