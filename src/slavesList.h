/*
  slavesList.h
  slaves list functions header for iGame

  Copyright (c) 2018-2022, Emmanuel Vasilakis

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

#ifndef _SLAVES_LIST_H
#define _SLAVES_LIST_H


void slavesListAddHead(slavesList *);
void slaves_list_add_tail(slavesList *);
void slavesListPrint(void);
void slavesListClearTitles(void);
slavesList *slavesListSearchByPath(char *, unsigned int);
slavesList *slaves_list_search_by_title(char *, unsigned int);
void slavesListCountInstancesByTitle(slavesList *);
BOOL slavesListRemoveByPath(const char *, unsigned int);
slavesList *getSlavesListHead(void);
void set_slaves_list_buffer(slavesList *);
slavesList *get_slaves_list_buffer(void);
void emptySlavesList(void);
int slaves_list_node_count(int);

#endif
