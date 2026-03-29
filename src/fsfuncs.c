/*
  fsfuncs.c
  Filesystem functions source for iGame

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


/* MUI */
#include <libraries/mui.h>
#include <mui/NListview_mcc.h>

/* Prototypes */
#include <clib/alib_protos.h>
#include <proto/wb.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/muimaster.h>

#include <workbench/icon.h>

/* System */
#if defined(__amigaos4__)
#define ASL_PRE_V38_NAMES
#define DeleteFile(name) Delete(name)
#endif
#include <libraries/asl.h>

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
#include "slavesList.h"
#include "genresList.h"
#include "chipsetList.h"
#include "funcs.h"
#include "fsfuncs.h"

extern struct ObjApp* app;
extern char* executable_name;

/* structures */
struct FileRequester* request;

/* global variables */
extern char fname[255];
extern igame_settings *current_settings;

/*
 * Get the path of the parent folder
 */
void get_parent_path(char *filename, char *result, int resultSize)
{
	BPTR fileLock = Lock(filename, SHARED_LOCK);
	if (fileLock)
	{
		BPTR folderLock = ParentDir(fileLock);
		NameFromLock(folderLock, result, resultSize);

		UnLock(folderLock);
		UnLock(fileLock);
	}
}

/*
 * Get the filename of a folder/file from a path
 */
void get_name_from_path(char *path, char *result, unsigned int resultSize)
{
	BPTR pathLock = Lock(path, SHARED_LOCK);
	if (pathLock) {

	#if defined(__amigaos4__)
		struct ExamineData *data = ExamineObjectTags(EX_LockInput, pathLock, TAG_END);
		strncpy(result, data->Name, resultSize);
	#else
		struct FileInfoBlock *FIblock = (struct FileInfoBlock *)AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);

		if (Examine(pathLock, FIblock))
		{
			strncpy(result, FIblock->fib_FileName, resultSize);
			FreeVec(FIblock);
		}
	#endif
		UnLock(pathLock);
	}
}

void get_full_path(const char *path, char *result)
{
	char buf[MAX_PATH_SIZE];
	snprintf(buf, sizeof(buf), "%s", path);
	if (path[0] == '/')
	{
		snprintf(buf, sizeof(buf), "PROGDIR:%s", path);
	}

	BPTR pathLock = Lock(buf, SHARED_LOCK);
	if (pathLock)
	{
		NameFromLock(pathLock, result, sizeof(char) * MAX_PATH_SIZE);
		UnLock(pathLock);
		return;
	}
}

/*
 * Check if a path actually exists in hard disk.
 * - Return True if exists
 * - Return False if it doesn't exist
 */
BOOL check_path_exists(char *path)
{
	const BPTR lock = Lock(path, SHARED_LOCK);
	if (lock) {
		UnLock(lock);
		return TRUE;
	}

	return FALSE;
}

BOOL get_filename(const char *title, const char *positive_text, const BOOL save_mode)
{
	BOOL result = FALSE;
	if ((request = MUI_AllocAslRequest(ASL_FileRequest, NULL)) != NULL)
	{
		if (MUI_AslRequestTags(request,
						ASLFR_TitleText, title,
						ASLFR_PositiveText, positive_text,
						ASLFR_DoSaveMode, save_mode,
						ASLFR_InitialDrawer, PROGDIR,
						TAG_DONE))
		{
			memset(&fname[0], 0, sizeof fname);
			snprintf(fname, sizeof(fname), "%s", request->fr_Drawer);
			if (fname[strlen(fname) - 1] != (UBYTE)58) /* Check for : */
				strncat(fname, "/", sizeof(fname) - strlen(fname) - 1);
			strncat(fname, request->fr_File, sizeof(fname) - strlen(fname) - 1);

			result = TRUE;
		}

		if (request)
			MUI_FreeAslRequest(request);
	}

	return result;
}

void slaves_list_load_from_csv(char *filename)
{
	int lineBufSize = sizeof(char) * 512;

	if (check_path_exists(filename))
	{
		FILE *fpgames = fopen(filename, "r");
		if (fpgames)
		{
			char *lineBuf = AllocVec(lineBufSize, MEMF_CLEAR);
			char *buf = AllocVec(sizeof(char) * MAX_PATH_SIZE, MEMF_CLEAR);
			if((buf == NULL) || (lineBuf == NULL))
			{
				msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
				fclose(fpgames);
				return;
			}

			while (fgets(lineBuf, lineBufSize, fpgames) != NULL)
			{
				slavesList *node = malloc(sizeof(slavesList));
				if(node == NULL)
				{
					msg_box((const char*)GetMBString(MSG_NotEnoughMemory));
					return;
				}

				buf = strtok(lineBuf, ";");
				node->instance = atoi(buf);

				buf = strtok(NULL, ";");
				node->title[0] = '\0';
				snprintf(node->title, sizeof(node->title), "%s", buf);
				if (strcasestr(buf, "\""))
				{
					STRPTR tmp = substring(buf, 1, -2);
				if (tmp) { snprintf(node->title, sizeof(node->title), "%s", tmp); free(tmp); }
				}

				buf = strtok(NULL, ";");
				node->genre[0] = '\0';
				snprintf(node->genre, sizeof(node->genre), "%s", buf);
				if (strcasestr(buf, "\""))
				{
					STRPTR tmp = substring(buf, 1, -2);
				if (tmp) { snprintf(node->genre, sizeof(node->genre), "%s", tmp); free(tmp); }
				}
				if(is_string_empty(node->genre))
				{
					snprintf(node->genre, sizeof(node->genre), "Unknown");
				}
				add_genre_in_list(node->genre);

				buf = strtok(NULL, ";");
				node->path[0] = '\0';
				snprintf(node->path, sizeof(node->path), "%s", buf);
				if (strcasestr(buf, "\""))
				{
					STRPTR tmp = substring(buf, 1, -2);
				if (tmp) { snprintf(node->path, sizeof(node->path), "%s", tmp); free(tmp); }
				}

				buf = strtok(NULL, ";");
				node->favourite = atoi(buf);

				buf = strtok(NULL, ";");
				node->times_played = atoi(buf);

				buf = strtok(NULL, ";");
				node->last_played = atoi(buf);

				buf = strtok(NULL, ";");
				node->hidden = atoi(buf);

				buf = strtok(NULL, ";");
				node->deleted = atoi(buf);

				buf = strtok(NULL, ";");
				node->user_title[0] = '\0';
				if (buf)
				{
					snprintf(node->user_title, sizeof(node->user_title), "%s", buf);
					if (strcasestr(buf, "\""))
					{
						STRPTR tmp = substring(buf, 1, -2);
					if (tmp) { snprintf(node->user_title, sizeof(node->user_title), "%s", tmp); free(tmp); }
					}
				}

				buf = strtok(NULL, ";");
				node->year = 0;
				if (buf)
					node->year = atoi(buf);

				buf = strtok(NULL, ";");
				node->players = 0;
				if (buf)
					node->players = atoi(buf);

				buf = strtok(NULL, ";");
				node->chipset[0] = '\0';
				if (strncmp(buf, "\n", sizeof(char)))
				{
					snprintf(node->chipset, sizeof(node->chipset), "%s", buf);
				}
				if (strcasestr(buf, "\""))
				{
					STRPTR tmp = substring(buf, 1, -2);
				if (tmp) { snprintf(node->chipset, sizeof(node->chipset), "%s", tmp); free(tmp); }
				}
				add_chipset_in_list(node->chipset);

				buf = strtok(NULL, ";");
				node->rating[0] = '\0';
				if (buf && strncmp(buf, "\n", sizeof(char)))
				{
					snprintf(node->rating, sizeof(node->rating), "%s", buf);
					if (strcasestr(buf, "\""))
					{
						STRPTR tmp = substring(buf, 1, -2);
					if (tmp) { snprintf(node->rating, sizeof(node->rating), "%s", tmp); free(tmp); }
					}
				}

				slaves_list_add_tail(node);
			}
			fclose(fpgames);
			FreeVec(lineBuf);
		}
	}
}

void slaves_list_save_to_csv(const char *filename)
{
	char csvFilename[MAX_PATH_SIZE];
	set(app->TX_Status, MUIA_Text_Contents, (const char*)GetMBString(MSG_SavingGamelist));

	snprintf(csvFilename, sizeof(csvFilename), "%s.csv", filename);

	FILE *fpgames = fopen(csvFilename, "w");
	if (!fpgames)
	{
		msg_box((const char*)GetMBString(MSG_FailedOpeningGameslist));
		return;
	}

	slavesList *currPtr = getSlavesListHead();

	while (currPtr != NULL)
	{
		fprintf(
			fpgames,
			"%d;\"%s\";\"%s\";\"%s\";%d;%d;%d;%d;%d;\"%s\";%d;%d;\"%s\";\"%s\";\n",
			currPtr->instance, currPtr->title,
			currPtr->genre, currPtr->path, currPtr->favourite,
			currPtr->times_played, currPtr->last_played, currPtr->hidden,
			currPtr->deleted, currPtr->user_title,
			currPtr->year, currPtr->players, currPtr->chipset,
			currPtr->rating
		);
		currPtr = currPtr->next;
	}

	fclose(fpgames);
}

/*
* Gets title from a slave file
* returns 0 on success, 1 on fail
*/
int get_title_from_slave(char* slave, char* title)
{
	char slave_title[MAX_SLAVE_TITLE_SIZE];

	struct slave_info
	{
		unsigned long security; // cppcheck-suppress unusedStructMember
		char id[8]; // cppcheck-suppress unusedStructMember
		unsigned short version;
		unsigned short flags; // cppcheck-suppress unusedStructMember
		unsigned long base_mem_size; // cppcheck-suppress unusedStructMember
		unsigned long exec_install; // cppcheck-suppress unusedStructMember
		unsigned short game_loader; // cppcheck-suppress unusedStructMember
		unsigned short current_dir; // cppcheck-suppress unusedStructMember
		unsigned short dont_cache; // cppcheck-suppress unusedStructMember
		char keydebug; // cppcheck-suppress unusedStructMember
		char keyexit; // cppcheck-suppress unusedStructMember
		unsigned long exp_mem; // cppcheck-suppress unusedStructMember
		unsigned short name;
		unsigned short copy; // cppcheck-suppress unusedStructMember
		unsigned short info; // cppcheck-suppress unusedStructMember
	};

	struct slave_info sl;

	FILE* fp = fopen(slave, "rbe");
	if (fp == NULL)
	{
		return 1;
	}

	//seek to +0x20
	fseek(fp, 32, SEEK_SET);
	fread(&sl, 1, sizeof sl, fp);

	//sl.Version = (sl.Version>>8) | (sl.Version<<8);
	//sl.name = (sl.name>>8) | (sl.name<<8);

	//printf ("[%s] [%d]\n", sl.ID, sl.Version);

	//sl.name holds the offset for the slave name
	fseek(fp, sl.name + 32, SEEK_SET);
	//title = calloc (1, 100);
	//fread (title, 1, 100, fp);

	if (sl.version < 10)
	{
		fclose(fp);
		return 1;
	}

	for (int i = 0; i <= 99; i++)
	{
		slave_title[i] = fgetc(fp);
		if (slave_title[i] == '\n')
		{
			slave_title[i] = '\0';
			break;
		}
	}

	snprintf(title, MAX_SLAVE_TITLE_SIZE, "%s", slave_title);
	fclose(fp);

	return 0;
}

/*
 * Set the item title name based on the path on disk
 */
void get_title_from_path(char *path, char *result, int resultSize)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);
	char *itemFolderPath = AllocVec(bufSize, MEMF_CLEAR);

	get_parent_path(path, itemFolderPath, bufSize);
	if (itemFolderPath)
	{
		get_name_from_path(itemFolderPath, buf, bufSize);

		if (current_settings->no_smart_spaces)
		{
			strncpy(result, buf, resultSize);
		}
		else
		{
			add_spaces_to_string(buf, result, resultSize);
		}
	}
	FreeVec(itemFolderPath);
	FreeVec(buf);
}

// Get the application's executable name
char *get_executable_name(int argc, char **argv)
{
	// argc is zero when run from the Workbench,
	// positive when run from the CLI
	if (argc == 0)
	{
		// in AmigaOS, argv is a pointer to the WBStartup message
		// when argc is zero (run under the Workbench)
		struct WBStartup* argmsg = (struct WBStartup *)argv;
		struct WBArg* wb_arg = argmsg->sm_ArgList; /* head of the arg list */

		executable_name = wb_arg->wa_Name;
	}
	else
	{
		// Run from the CLI
		executable_name = argv[0];
	}

	return executable_name;
}

// This reveals the item window on Workbench
void open_current_dir(void)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *buf = AllocVec(bufSize, MEMF_CLEAR);
	char *game_title = NULL;

#if !defined(__morphos__)
	if (get_wb_version() < 44)
	{
		// workbench.library doesn't support OpenWorkbenchObjectA yet
		return;
	}
#endif

	// Get the selected item from the list
	DoMethod(app->LV_GamesList, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &game_title);
	if (game_title == NULL || strlen(game_title) == 0)
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

	get_parent_path(existingNode->path, buf, bufSize);
	if(!buf)
	{
		msg_box((const char*)GetMBString(MSG_DirectoryNotFound));
		return;
	}

	// Open path directory on workbench
#if defined(__morphos__)
	int execSize = sizeof(char) * MAX_EXEC_SIZE;
	char *exec = AllocVec(execSize, MEMF_CLEAR);
	snprintf(exec, execSize, "open %s", buf);
	Execute(exec, 0, 0);
	FreeVec(exec);
#else
	OpenWorkbenchObject(buf);
#endif
	FreeVec(buf);
}

// Check if the path is a folder or a partition
BOOL is_path_folder(char *path)
{
	if (path[strlen(path)-1] == ':')
		return FALSE;

	return TRUE;
}

/*
* Get the icon tooltypes and copies them in result
* The path needs to be the full file path without .info at the end
*/
void get_icon_tooltypes(char *path, char *result)
{
	if (IconBase)
	{
		struct DiskObject *diskObj = GetDiskObjectNew(path);
		if(diskObj)
		{
			if (diskObj->do_ToolTypes == NULL)
			{
				FreeDiskObject(diskObj);
				return;
			}
			char *buf = AllocVec(sizeof(char) * MAX_TOOLTYPE_SIZE, MEMF_CLEAR);

			// cppcheck-suppress redundantInitialization
			for (STRPTR *tool_types = diskObj->do_ToolTypes; (buf = *tool_types); ++tool_types)
			{
				if (!strncmp(buf, "*** DON'T EDIT", 14) || !strncmp(buf, "IM", 2))
					continue;

				strncat(result, buf, 1024 - strlen(result) - 1);
				strncat(result, "\n", 1024 - strlen(result) - 1);
			}
			FreeVec(buf);
			FreeDiskObject(diskObj);
		}
	}
}

void set_icon_tooltypes(char *path, char *tooltypes)
{
	if (IconBase && (IconBase->lib_Version >= 44))
	{
		struct DiskObject *diskObj = GetIconTags(path, TAG_DONE);
		if(diskObj)
		{
			int toolTypesCnt = 0;

			char *buf = AllocVec(sizeof(char) * MAX_TOOLTYPE_SIZE, MEMF_CLEAR);

			// Get the number of the new tooltypes
			char **table = my_split(tooltypes, "\n");
			BOOL isLastLineEmpty = FALSE;

			for (table; (buf = *table); ++table) // cppcheck-suppress redundantInitialization
			{
				toolTypesCnt++;
				isLastLineEmpty = FALSE;
				if (buf[0] == '\0')
				{
					isLastLineEmpty = TRUE;
				}
			}
			if (!isLastLineEmpty)
			{
				toolTypesCnt++;
			}

			unsigned char **newToolTypes = AllocVec(sizeof(char *) * toolTypesCnt, MEMF_CLEAR);
			if (newToolTypes)
			{
				char **table2 = my_split(tooltypes, "\n");
				int table2Cnt = 0;
				for (table2; (buf = *table2); ++table2) // cppcheck-suppress redundantInitialization
				{
					newToolTypes[table2Cnt] = buf;
					table2Cnt++;
				}

				newToolTypes[toolTypesCnt-1] = NULL;

				diskObj->do_ToolTypes = newToolTypes;

				LONG errorCode;
				BOOL success;
				success = PutIconTags(path, diskObj,
					ICONPUTA_DropNewIconToolTypes, TRUE,
					ICONA_ErrorCode, &errorCode,
				TAG_DONE);

				if(success == FALSE)
				{
					msg_box((const char*)GetMBString(MSG_ICONPICTURESTORE_FAILED));
				}

				FreeVec(newToolTypes);
			}

			FreeDiskObject(diskObj);
		}
	}
}

/*
* Check if the needed slave file is set in .info tooltypes
* The infoFile needs to be the full file path without .info at the end
*/
BOOL check_slave_in_tooltypes(char *infoFile, char *slaveName)
{
	struct DiskObject *diskObj = GetDiskObjectNew(infoFile);
	if(diskObj)
	{
		if (MatchToolValue(FindToolType(diskObj->do_ToolTypes, "SLAVE"), slaveName))
		{
			FreeDiskObject(diskObj);
			return TRUE;
		}
		FreeDiskObject(diskObj);
	}
	return FALSE;
}

/*
* Prepare the exec command based on icon tooltypes
* The infoFile needs to be the full file path without .info at the end
*/
void prepare_whd_execution(char *infoFile, char *result)
{
	char *tooltypes = AllocVec(sizeof(char) * 1024, MEMF_CLEAR);
	get_icon_tooltypes(infoFile, tooltypes);

	char **table = my_split(tooltypes, "\n");
	char **tableBase = table;
	char *buf;
	snprintf(result, MAX_EXEC_SIZE, "whdload");
	for (table; (buf = *table); ++table)
	{
		if (buf[0] == ' ') continue;
		if (buf[0] == '(') continue;
		if (buf[0] == '*') continue;
		if (buf[0] == ';') continue;
		if (buf[0] == '\0') continue;
		if (buf[0] == -69) continue; // »
		if (buf[0] == -85) continue; // «
		if (buf[0] == 34) continue; // \"
		if (buf[0] == '.') continue;
		if (buf[0] == '=') continue;
		if (buf[0] == '#') continue;
		if (buf[0] == '!') continue;

		char tmpBuf[MAX_EXEC_SIZE];
		char** tmpTbl = my_split(buf, "=");
		if (tmpTbl[1] != NULL)
		{
			if (tmpTbl[1][0] == '$')
			{
				snprintf(tmpBuf, sizeof(tmpBuf), "%s=%d", tmpTbl[0], hex2dec((char *)tmpTbl[1]));
			}
			else if (is_numeric(tmpTbl[1]))
			{
				snprintf(tmpBuf, sizeof(tmpBuf), "%s=%s", tmpTbl[0], tmpTbl[1]);
			}
			else
			{
				snprintf(tmpBuf, sizeof(tmpBuf), "%s=\"%s\"", tmpTbl[0], tmpTbl[1]);
			}
			buf = tmpBuf;
		}

		strncat(result, " ", MAX_EXEC_SIZE - strlen(result) - 1);
		strncat(result, buf, MAX_EXEC_SIZE - strlen(result) - 1);

		// Free tmpTbl entries and array
		for (int i = 0; tmpTbl[i] != NULL; i++)
			free(tmpTbl[i]);
		free(tmpTbl);
	}

	// Free table entries and array
	for (int i = 0; tableBase[i] != NULL; i++)
		free(tableBase[i]);
	free(tableBase);

	FreeVec(tooltypes);
}

void getIGameDataInfo(char *igameDataPath, slavesList *node)
{
	const BPTR fpigamedata = Open(igameDataPath, MODE_OLDFILE);
	if (fpigamedata)
	{
		int lineSize = 64;
		char *line = malloc(lineSize * sizeof(char));
		while (FGets(fpigamedata, line, lineSize))
		{
			char *key = strtok(line, "=");
			char *value = strtok(NULL, "\n");

			if (key && value && strlen(value) > 0)
			{
				if(current_settings->use_igamedata_title && !strcmp(key, "title"))
				{
					strncpy(node->title, value, MAX_SLAVE_TITLE_SIZE);
				}

				if(!strcmp(key, "chipset"))
				{
					strncpy(node->chipset, value, MAX_CHIPSET_SIZE);
				}

				if(!strcmp(key, "genre"))
				{
					strncpy(node->genre, value, MAX_GENRE_NAME_SIZE);
				}

				if(!strcmp(key, "year") && is_numeric(value))
				{
					node->year=atoi(value);
				}

				if(!strcmp(key, "players") && is_numeric(value))
				{
					node->players=atoi(value);
				}

				if(!strcmp(key, "rating") && !is_string_empty(value))
				{
					strncpy(node->rating, value, sizeof(node->rating) - 1);
					node->rating[sizeof(node->rating) - 1] = '\0';
				}

				if(!strcmp(key, "exe") && !is_string_empty(value) && !strcasestr(value, ".slave"))
				{
					strncpy(node->path, value, MAX_PATH_SIZE);
				}
			}
		}

		free(line);
		Close(fpigamedata);
	}
}

void load_genres_from_file(void)
{
	const BPTR fpgenres = Open(DEFAULT_GENRES_FILE, MODE_OLDFILE);
	if (fpgenres)
	{
		int lineSize = MAX_GENRE_NAME_SIZE;
		char *line = malloc(lineSize * sizeof(char));
		while (FGets(fpgenres, line, lineSize) != NULL)
		{
			line[strlen(line) - 1] = '\0';
			if (!is_string_empty(line))
			{
				add_genre_in_list(line);
			}
		}

		free(line);
		Close(fpgenres);
	}
}

BOOL delete_directory(const char *path)
{
	BPTR dirLock = Lock(path, SHARED_LOCK);
	if (!dirLock)
		return FALSE;

#if defined(__amigaos4__)
	APTR context = ObtainDirContextTags(EX_LockInput, dirLock, TAG_DONE);
	if (context)
	{
		struct ExamineData *data;
		while ((data = ExamineDir(context)) != NULL)
		{
			int bufSize = sizeof(char) * MAX_PATH_SIZE;
			char *childPath = AllocVec(bufSize, MEMF_CLEAR);
			NameFromLock(dirLock, childPath, bufSize);
			AddPart(childPath, data->Name, bufSize);

			if (EXD_IS_DIRECTORY(data))
			{
				delete_directory(childPath);
			}
			else
			{
				DeleteFile(childPath);
			}
			FreeVec(childPath);
		}
		ReleaseDirContext(context);
	}
#else
	struct FileInfoBlock *fib = AllocVec(sizeof(struct FileInfoBlock), MEMF_CLEAR);
	if (fib && Examine(dirLock, fib))
	{
		while (ExNext(dirLock, fib))
		{
			int bufSize = sizeof(char) * MAX_PATH_SIZE;
			char *childPath = AllocVec(bufSize, MEMF_CLEAR);
			NameFromLock(dirLock, childPath, bufSize);
			AddPart(childPath, fib->fib_FileName, bufSize);

			if (fib->fib_DirEntryType > 0)
			{
				delete_directory(childPath);
			}
			else
			{
				DeleteFile(childPath);
			}
			FreeVec(childPath);
		}
	}
	if (fib)
		FreeVec(fib);
#endif

	UnLock(dirLock);
	return DeleteFile(path);
}

BOOL is_path_on_assign(const char *path)
{
	if ((path == NULL) || (path[0] == '/'))
		return FALSE;

	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char *volume = malloc(bufSize);
	for (int i = 0; i < strlen(path) - 1; i++) {
		if (path[i] != ':') {
			volume[i] = path[i];
		}
		else
		{
			volume[i] = ':';
			volume[i+1] = '\0';
			break;
		}
	}

	char *buf = malloc(bufSize);
	get_full_path(volume, buf);
	if (!strcmp(volume, buf))
	{
		free(volume);
		free(buf);
		return FALSE;
	}

	free(buf);
	free(volume);
	return TRUE;
}
