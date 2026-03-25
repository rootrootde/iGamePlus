/*
  blacklist.c
  blacklist functions source for iGame

  Copyright (c) 2018-2025, Emmanuel Vasilakis

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <exec/types.h>
#include <proto/dos.h>

#include "iGameExtern.h"
#include "strfuncs.h"
#include "blacklist.h"

static blacklistNode *head = NULL;

void blacklistAdd(const char *path)
{
	if (isStringEmpty(path))
		return;

	if (blacklistContains(path))
		return;

	blacklistNode *node = malloc(sizeof(blacklistNode));
	if (node == NULL)
		return;

	strncpy(node->path, path, MAX_PATH_SIZE);
	node->path[MAX_PATH_SIZE - 1] = '\0';
	node->next = head;
	head = node;
}

int blacklistContains(const char *path)
{
	if (isStringEmpty(path))
		return 0;

	blacklistNode *curr = head;
	while (curr != NULL)
	{
		if (strncmp(curr->path, path, MAX_PATH_SIZE) == 0)
			return 1;
		curr = curr->next;
	}
	return 0;
}

void blacklistLoad(const char *filename)
{
	const BPTR fp = Open(filename, MODE_OLDFILE);
	if (fp)
	{
		int lineSize = MAX_PATH_SIZE;
		char *line = malloc(lineSize * sizeof(char));
		while (FGets(fp, line, lineSize) != NULL)
		{
			/* Strip trailing newline */
			size_t len = strlen(line);
			if (len > 0 && line[len - 1] == '\n')
				line[len - 1] = '\0';

			if (!isStringEmpty(line))
			{
				blacklistAdd(line);
			}
		}

		free(line);
		Close(fp);
	}
}

void blacklistSave(const char *filename)
{
	const BPTR fp = Open(filename, MODE_NEWFILE);
	if (fp)
	{
		blacklistNode *curr = head;
		while (curr != NULL)
		{
			FPuts(fp, curr->path);
			FPutC(fp, '\n');
			curr = curr->next;
		}
		Close(fp);
	}
}

void blacklistRemove(const char *path)
{
	if (isStringEmpty(path))
		return;

	blacklistNode *prev = NULL;
	blacklistNode *curr = head;
	while (curr != NULL)
	{
		if (strncmp(curr->path, path, MAX_PATH_SIZE) == 0)
		{
			if (prev == NULL)
				head = curr->next;
			else
				prev->next = curr->next;
			free(curr);
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}

void blacklistRemoveAll(void)
{
	while (head != NULL)
	{
		blacklistNode *next = head->next;
		free(head);
		head = next;
	}
}

blacklistNode *blacklistGetHead(void)
{
	return head;
}

void blacklistFree(void)
{
	while (head != NULL)
	{
		blacklistNode *next = head->next;
		free(head);
		head = next;
	}
}
