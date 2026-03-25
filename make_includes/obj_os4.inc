##########################################################################
# object files (AOS4)
##########################################################################

src/funcs_OS4.o: src/funcs.c src/iGame_strings.h src/strfuncs.h src/fsfuncs.h src/iGameExtern.h src/blacklist.h
	$(CC_PPC) $(CFLAGS_OS4) $(INCLUDES_OS4) -o $@ src/funcs.c

src/iGameGUI_OS4.o: src/iGameGUI.c src/iGameGUI.h src/iGame_strings.h src/fsfuncs.h src/iGameExtern.h
	$(CC_PPC) $(CFLAGS_OS4) $(INCLUDES_OS4) -o $@ src/iGameGUI.c

src/iGameMain_OS4.o: src/iGameMain.c src/iGameExtern.h
	$(CC_PPC) $(CFLAGS_OS4) $(INCLUDES_OS4) -o $@ src/iGameMain.c

src/strfuncs_OS4.o: src/strfuncs.c src/strfuncs.h
	$(CC_PPC) $(CFLAGS_OS4) $(INCLUDES_OS4) -o $@ src/strfuncs.c

src/fsfuncs_OS4.o: src/fsfuncs.c src/fsfuncs.h src/funcs.h src/iGameExtern.h
	$(CC_PPC) $(CFLAGS_OS4) $(INCLUDES_OS4) -o $@ src/fsfuncs.c

src/blacklist_OS4.o: src/blacklist.c src/blacklist.h src/iGameExtern.h src/strfuncs.h
	$(CC_PPC) $(CFLAGS_OS4) $(INCLUDES_OS4) -o $@ src/blacklist.c

src/WinBlacklist_OS4.o: src/WinBlacklist.c src/WinBlacklist.h src/iGameExtern.h src/iGameGUI.h src/iGame_strings.h src/slavesList.h src/blacklist.h src/funcs.h src/strfuncs.h
	$(CC_PPC) $(CFLAGS_OS4) $(INCLUDES_OS4) -o $@ src/WinBlacklist.c
