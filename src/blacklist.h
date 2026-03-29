/*
  blacklist.h
  blacklist functions header for iGame

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

#ifndef _BLACKLIST_H
#define _BLACKLIST_H

typedef struct blacklistNode
{
	char path[256]; /* MAX_PATH_SIZE */
	struct blacklistNode *next;
} blacklistNode;

void blacklist_load(const char *filename);
void blacklistSave(const char *filename);
void blacklistAdd(const char *path);
int blacklist_contains(const char *path);
void blacklistRemove(const char *path);
void blacklistRemoveAll(void);
blacklistNode *blacklistGetHead(void);
void blacklistFree(void);

#endif
