##########################################################################
# object files (040)
##########################################################################

src/funcs_040.o: src/funcs.c src/iGame_strings.h src/strfuncs.h src/fsfuncs.h src/iGameExtern.h src/blacklist.h
	$(CC) $(CFLAGS_040) $(INCLUDES) -o $@ src/funcs.c

src/iGameGUI_040.o: src/iGameGUI.c src/iGameGUI.h src/iGame_strings.h src/fsfuncs.h src/iGameExtern.h
	$(CC) $(CFLAGS_040) $(INCLUDES) -o $@ src/iGameGUI.c

src/iGameMain_040.o: src/iGameMain.c src/iGameExtern.h
	$(CC) $(CFLAGS_040) $(INCLUDES) -o $@ src/iGameMain.c

src/strfuncs_040.o: src/strfuncs.c src/strfuncs.h
	$(CC) $(CFLAGS_040) $(INCLUDES) -o $@ src/strfuncs.c

src/fsfuncs_040.o: src/fsfuncs.c src/fsfuncs.h src/funcs.h src/iGameExtern.h
	$(CC) $(CFLAGS_040) $(INCLUDES) -o $@ src/fsfuncs.c

src/blacklist_040.o: src/blacklist.c src/blacklist.h src/iGameExtern.h src/strfuncs.h
	$(CC) $(CFLAGS_040) $(INCLUDES) -o $@ src/blacklist.c

src/WinBlacklist_040.o: src/WinBlacklist.c src/WinBlacklist.h src/iGameExtern.h src/iGameGUI.h src/iGame_strings.h src/slavesList.h src/blacklist.h src/funcs.h src/strfuncs.h
	$(CC) $(CFLAGS_040) $(INCLUDES) -o $@ src/WinBlacklist.c
