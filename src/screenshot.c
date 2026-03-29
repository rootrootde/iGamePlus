/*
  screenshot.c
  Screenshot functions for iGame

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

/* Prototypes */
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/muimaster.h>

/* ANSI C */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define iGame_NUMBERS
#include "iGame_strings.h"

#include "iGameExtern.h"
#include "iGameGUI.h"
#include "fsfuncs.h"
#include "slavesList.h"
#include "strfuncs.h"
#include "funcs.h"
#include "screenshot.h"

extern struct ObjApp* app;
extern igame_settings *current_settings;

/* screenshot debounce state */
static BOOL screenshot_pending = FALSE;
static char pending_screenshot_title[MAX_SLAVE_TITLE_SIZE];
static int screenshot_cooldown = 0;
#define SCREENSHOT_COOLDOWN_TICKS 5 /* ~100ms at 50Hz (Delay(1) per tick) */

static LONG xget(Object* obj, ULONG attribute)
{
	LONG x;
	get(obj, attribute, &x);
	return x;
}

static void replace_screenshot(void)
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
			replace_screenshot();
		}

		strcpy(prvScreenshot, screenshot_path);
	}
}

static void get_screenshot_path(char *game_title, char *result)
{
	int bufSize = sizeof(char) * MAX_PATH_SIZE;
	char buf[MAX_PATH_SIZE];

	slavesList *existingNode = NULL;
	if (existingNode = slaves_list_search_by_title(game_title, sizeof(char) * MAX_SLAVE_TITLE_SIZE))
	{
		get_parent_path(existingNode->path, buf, bufSize);

		// Return the igame.iff from the game folder, if exists
		snprintf(result, sizeof(char) * MAX_PATH_SIZE, "%s/igame.iff", buf);
		if(check_image_datatype(result))
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
		if(check_image_datatype(result))
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

void show_default_screenshot(void)
{
	if (current_settings->hide_side_panel || current_settings->hide_screenshots)
		return;

	show_screenshot(DEFAULT_SCREENSHOT_FILE);
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

void screenshot_set_pending(const char *title)
{
	strncpy(pending_screenshot_title, title,
			MAX_SLAVE_TITLE_SIZE - 1);
	pending_screenshot_title[MAX_SLAVE_TITLE_SIZE - 1] = '\0';
	screenshot_pending = TRUE;
	screenshot_cooldown = SCREENSHOT_COOLDOWN_TICKS;
}
