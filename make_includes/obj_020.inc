##########################################################################
# object files (020)
##########################################################################

src/funcs.o: src/funcs.c src/iGame_strings.h src/strfuncs.h src/fsfuncs.h src/iGameExtern.h src/blacklist.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/funcs.c

src/iGameGUI.o: src/iGameGUI.c src/iGameGUI.h src/iGame_strings.h src/fsfuncs.h src/iGameExtern.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/iGameGUI.c

src/iGameMain.o: src/iGameMain.c src/iGameExtern.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/iGameMain.c

src/strfuncs.o: src/strfuncs.c src/strfuncs.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/strfuncs.c

src/fsfuncs.o: src/fsfuncs.c src/fsfuncs.h src/funcs.h src/iGameExtern.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/fsfuncs.c

src/blacklist.o: src/blacklist.c src/blacklist.h src/iGameExtern.h src/strfuncs.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/blacklist.c

src/WinBlacklist.o: src/WinBlacklist.c src/WinBlacklist.h src/iGameExtern.h src/iGameGUI.h src/iGame_strings.h src/slavesList.h src/blacklist.h src/funcs.h src/strfuncs.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/WinBlacklist.c
