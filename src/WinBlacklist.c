/*
  WinBlacklist.c
  Blacklist management window source for iGame

  Copyright (c) 2025, Emmanuel Vasilakis and contributors

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

#ifdef __morphos__
#define SDI_TRAP_LIB
#endif
#include <SDI_hook.h>

/* Prototypes */
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <exec/memory.h>

#include <string.h>

#define iGame_NUMBERS
#include "iGame_strings.h"

#include "iGameExtern.h"
#include "strfuncs.h"
#include "iGameGUI.h"
#include "slavesList.h"
#include "blacklist.h"
#include "fsfuncs.h"
#include "funcs.h"
#include "WinBlacklist.h"

extern struct ObjApp *app;

void blacklistWindowPopulate(void)
{
	DoMethod(app->LV_Blacklist, MUIM_List_Clear);

	blacklistNode *curr = blacklistGetHead();
	while (curr != NULL)
	{
		DoMethod(app->LV_Blacklist, MUIM_List_InsertSingle,
			curr->path, MUIV_List_Insert_Bottom);
		curr = curr->next;
	}
}

void blacklistWindowUnhide(void)
{
	char *path = NULL;
	DoMethod(app->LV_Blacklist, MUIM_List_GetEntry, MUIV_List_GetEntry_Active, &path);
	if (path == NULL || path[0] == '\0')
		return;

	/* Find matching node in slaves list and unhide it */
	slavesList *node = slavesListSearchByPath(path, sizeof(char) * MAX_PATH_SIZE);
	if (node)
	{
		node->hidden = 0;
	}

	blacklistRemove(path);
	blacklistSave(DEFAULT_BLACKLIST_FILE);

	DoMethod(app->LV_Blacklist, MUIM_List_Remove, MUIV_List_Remove_Active);

	/* Refresh the main game list */
	filter_change();
}

void blacklistWindowUnhideAll(void)
{
	/* Unhide all nodes in the slaves list */
	slavesList *currPtr = getSlavesListHead();
	while (currPtr != NULL)
	{
		if (currPtr->hidden)
		{
			currPtr->hidden = 0;
		}
		currPtr = currPtr->next;
	}

	blacklistRemoveAll();
	blacklistSave(DEFAULT_BLACKLIST_FILE);

	DoMethod(app->LV_Blacklist, MUIM_List_Clear);

	/* Refresh the main game list */
	filter_change();
}

void blacklistWindowDeleteAll(void)
{
	struct EasyStruct confirm;
	confirm.es_StructSize = sizeof confirm;
	confirm.es_Flags = 0;
	confirm.es_Title = (UBYTE *)GetMBString(MSG_WI_Blacklist);
	confirm.es_TextFormat = (unsigned char *)GetMBString(MSG_ConfirmDeleteAllHidden);
	confirm.es_GadgetFormat = (UBYTE *)"Yes|No";
	if (!EasyRequest(NULL, &confirm, NULL))
		return;

	/* Delete files and remove all blacklisted entries */
	blacklistNode *curr = blacklistGetHead();
	while (curr != NULL)
	{
		int bufSize = sizeof(char) * MAX_PATH_SIZE;
		char *parentDir = AllocVec(bufSize, MEMF_CLEAR);
		if (parentDir)
		{
			getParentPath(curr->path, parentDir, bufSize);
			if (parentDir[0] != '\0')
				deleteDirectory(parentDir);
			FreeVec(parentDir);
		}
		slavesListRemoveByPath(curr->path, sizeof(char) * MAX_PATH_SIZE);
		curr = curr->next;
	}

	blacklistRemoveAll();
	blacklistSave(DEFAULT_BLACKLIST_FILE);

	DoMethod(app->LV_Blacklist, MUIM_List_Clear);

	filter_change();
}

MakeStaticHook(BlacklistPopulateHook, blacklistWindowPopulate);
MakeStaticHook(BlacklistUnhideHook, blacklistWindowUnhide);
MakeStaticHook(BlacklistUnhideAllHook, blacklistWindowUnhideAll);
MakeStaticHook(BlacklistDeleteAllHook, blacklistWindowDeleteAll);

APTR getBlacklistWindow(struct ObjApp *object)
{
	object->LV_Blacklist = ListObject,
		MUIA_Frame, MUIV_Frame_InputList,
		End;

	object->BT_BlacklistUnhide = SimpleButton(GetMBString(MSG_BT_BlacklistUnhide));
	object->BT_BlacklistUnhideAll = SimpleButton(GetMBString(MSG_BT_BlacklistUnhideAll));
	object->BT_BlacklistDeleteAll = SimpleButton(GetMBString(MSG_BT_BlacklistDeleteAll));
	object->BT_BlacklistClose = SimpleButton(GetMBString(MSG_BT_CloseRepoWindow));

	return WindowObject,
		MUIA_Window_Title, GetMBString(MSG_WI_Blacklist),
		MUIA_Window_ID, MAKE_ID('5', 'I', 'G', 'A'),
		WindowContents, VGroup,
			Child, ListviewObject,
				MUIA_Listview_List, object->LV_Blacklist,
			End,
			Child, HGroup,
				Child, object->BT_BlacklistUnhide,
				Child, object->BT_BlacklistUnhideAll,
				Child, object->BT_BlacklistDeleteAll,
				Child, object->BT_BlacklistClose,
			End,
		End,
	End;
}

void setBlacklistWindowMethods(struct ObjApp *object)
{
	DoMethod(object->WI_Blacklist,
		MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
		object->WI_Blacklist, 3,
		MUIM_Set, MUIA_Window_Open, FALSE
	);

	DoMethod(object->BT_BlacklistClose,
		MUIM_Notify, MUIA_Pressed, FALSE,
		object->WI_Blacklist, 3,
		MUIM_Set, MUIA_Window_Open, FALSE
	);

	DoMethod(object->WI_Blacklist,
		MUIM_Notify, MUIA_Window_Open, TRUE,
		object->App, 2,
		MUIM_CallHook, &BlacklistPopulateHook
	);

	DoMethod(object->BT_BlacklistUnhide,
		MUIM_Notify, MUIA_Pressed, FALSE,
		object->App, 2,
		MUIM_CallHook, &BlacklistUnhideHook
	);

	DoMethod(object->BT_BlacklistUnhideAll,
		MUIM_Notify, MUIA_Pressed, FALSE,
		object->App, 2,
		MUIM_CallHook, &BlacklistUnhideAllHook
	);

	DoMethod(object->BT_BlacklistDeleteAll,
		MUIM_Notify, MUIA_Pressed, FALSE,
		object->App, 2,
		MUIM_CallHook, &BlacklistDeleteAllHook
	);

	DoMethod(object->WI_Blacklist,
		MUIM_Window_SetCycleChain,
		object->LV_Blacklist,
		object->BT_BlacklistUnhide,
		object->BT_BlacklistUnhideAll,
		object->BT_BlacklistDeleteAll,
		object->BT_BlacklistClose,
		0
	);
}
