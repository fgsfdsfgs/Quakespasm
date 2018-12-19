/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include <switch.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/iosupport.h>

#define SDL_MIN_X	2
#define SDL_MIN_Y	0
#define SDL_MIN_Z	0
#define SDL_REQUIREDVERSION	(SDL_VERSIONNUM(SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z))
#define SDL_NEW_VERSION_REJECT	(SDL_VERSIONNUM(3,0,0))

static void Sys_AtExit (void)
{
	socketExit ();
	SDL_Quit();
}

static void Sys_InitSDL (void)
{
	SDL_version v;
	SDL_version *sdl_version = &v;
	SDL_GetVersion(&v);

	Sys_Printf("Found SDL version %i.%i.%i\n",sdl_version->major,sdl_version->minor,sdl_version->patch);
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) < SDL_REQUIREDVERSION)
	{	/*reject running under older SDL versions */
		Sys_Error("You need at least v%d.%d.%d of SDL to run this game.", SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z);
	}
	if (SDL_VERSIONNUM(sdl_version->major,sdl_version->minor,sdl_version->patch) >= SDL_NEW_VERSION_REJECT)
	{	/*reject running under newer (1.3.x) SDL */
		Sys_Error("Your version of SDL library is incompatible with me.\n"
			  "You need a library version in the line of %d.%d.%d\n", SDL_MIN_X,SDL_MIN_Y,SDL_MIN_Z);
	}

	if (SDL_Init(0) < 0)
	{
		Sys_Error("Couldn't init SDL: %s", SDL_GetError());
	}
	atexit(Sys_AtExit);
}

#define DEFAULT_MEMORY (256 * 1024 * 1024) // ericw -- was 72MB (64-bit) / 64MB (32-bit)

static quakeparms_t	parms;

/*
 * ===========================================================================
 * Launcher
 * ===========================================================================
 */

#define QBASEDIR "/switch/quakespasm"

static inline int IsDir(const char *path)
{
	DIR *dir = opendir(path);
	if (dir) { closedir(dir); return 1; }
	return 0;
}

static void SelectModRedraw(int selected, int nmods, char mods[][128])
{
	int i;

	consoleClear();
	printf("\n	Select game directory");
	printf("\n	=====================");
	printf("\n\n");

	for (i = 0; i < nmods; ++i) {
		if (selected == i)
			printf(" >  %s\n", mods[i]);
		else
			printf("	%s\n", mods[i]);
	}

	printf("\n\nDPAD to select, A to confirm\n");
}

static int SelectMod(int nmods, char mods[][128])
{
	int i, selected;
	u64 keys, oldkeys;

	gfxInitDefault();
	consoleInit(NULL);

	oldkeys = keys = 0;
	selected = 0;

	SelectModRedraw(selected, nmods, mods);

	while (appletMainLoop()) {
		hidScanInput();
		keys = hidKeysDown(CONTROLLER_P1_AUTO);

		if (keys & KEY_A) {
			break;
		} else if ((keys & KEY_DOWN) && !(oldkeys & KEY_DOWN)) {
			selected = (selected == nmods - 1) ? 0 : selected + 1;
			SelectModRedraw(selected, nmods, mods);
		} else if ((keys & KEY_UP) && !(oldkeys & KEY_UP)) {
			selected = (selected == 0) ? nmods - 1 : selected - 1;
			SelectModRedraw(selected, nmods, mods);
		}

		gfxFlushBuffers();
		gfxSwapBuffers();

		oldkeys = keys;
	}

	consoleExit(NULL);
	gfxExit();

	return selected;
}

int Q_main(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	static char *args[16];
	static char fullpath[1024];
	static char mods[20][128];
	int nargs, i, nmods, havebase;
	DIR *dir;
	struct dirent *d;

	// just in case
	if (argc <= 0) {
		nargs = 1;
		args[0] = "quakespasmnx.nro";
	} else {
		nargs = argc;
		for (i = 0; i < argc && i < 8; ++i)
			args[i] = argv[i];
	}

	// check for mods

	nmods = 0;

	dir = opendir(QBASEDIR);
	if (!dir) Sys_Error("could not open `" QBASEDIR "`");

	havebase = 0;
	while ((d = readdir(dir))) {
		if (!d->d_name || d->d_name[0] == '.') continue;

		snprintf(fullpath, sizeof(fullpath)-1, QBASEDIR "/%s", d->d_name);
		if (!IsDir(fullpath)) continue;

		if (!havebase && !strncasecmp(d->d_name, "id1", 3))
			havebase = 1;

		strncpy(mods[nmods], d->d_name, 127);
		if (++nmods >= 20) break; 
	}

	closedir(dir);

	if (!havebase)
		Sys_Error("base game directory `id1` not found in `%s`", QBASEDIR);

	// show a simple select menu if there's multiple mods

	if (nmods > 1) {
		i = SelectMod(nmods, mods);
		if (strncasecmp(mods[i], "id1", 3)) {
			args[nargs++] = "-game";
			args[nargs++] = mods[i];
		}
	}

	args[nargs] = NULL;

	return Q_main(nargs, (const char **)args);
}

// returns in megabytes
static int MemAvailable(void)
{
	// use a syscall to get available memory
	u64 avail = 0;

	// id0 = 6, id1 = 0 => TotalMemoryAvailable
	Result rc = svcGetInfo( &avail, 6, CUR_PROCESS_HANDLE, 0 );

	// applets get at least like 300 mb, so that's the minimum
	if ( R_FAILED(rc) ) avail = 304 * 1024 * 1024;

	int mb = (int)( avail / ( 1024 * 1024 ) );
	// round to the nearest 16Mb
	mb = ( mb + 8 ) & ~15;
	return mb;
}

int Q_main(int argc, char *argv[])
{
	int		t;
	double		time, oldtime, newtime;

#ifdef DEBUG
	socketInitializeDefault ();
	nxlinkStdio ();
#endif

	host_parms = &parms;
	parms.basedir = ".";

	parms.argc = argc;
	parms.argv = argv;

	parms.errstate = 0;

	COM_InitArgv(parms.argc, parms.argv);

	isDedicated = (COM_CheckParm("-dedicated") != 0);

	Sys_InitSDL ();

	Sys_Init();

	int mem_mb = MemAvailable();
	// leave at least 256 MB for stuff, or else it crashes on taxing maps in applet mode
	parms.memsize = (mem_mb > 512) ? DEFAULT_MEMORY : DEFAULT_MEMORY / 2;

	if (COM_CheckParm("-heapsize"))
	{
		t = COM_CheckParm("-heapsize") + 1;
		if (t < com_argc)
			parms.memsize = Q_atoi(com_argv[t]) * 1024;
	}

	parms.membase = malloc (parms.memsize);

	if (!parms.membase)
		Sys_Error ("Not enough memory free; check disk space\n");

	Sys_Printf("Quake %1.2f (c) id Software\n", VERSION);
	Sys_Printf("GLQuake %1.2f (c) id Software\n", GLQUAKE_VERSION);
	Sys_Printf("FitzQuake %1.2f (c) John Fitzgibbons\n", FITZQUAKE_VERSION);
	Sys_Printf("FitzQuake SDL port (c) SleepwalkR, Baker\n");
	Sys_Printf("QuakeSpasm " QUAKESPASM_VER_STRING " (c) Ozkan Sezer, Eric Wasylishen & others\n");
	Sys_Printf("Available memory: %d MB\n", mem_mb);
	Sys_Printf("Allocated %d MB for mem base\n", parms.memsize);

	Sys_Printf("Host_Init\n");
	Host_Init();

	oldtime = Sys_DoubleTime();
	if (isDedicated)
	{
		while (appletMainLoop ())
		{
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;

			while (time < sys_ticrate.value )
			{
				SDL_Delay(1);
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
			}

			Host_Frame (time);
			oldtime = newtime;
		}
	}
	else
	while (appletMainLoop ())
	{
		/* If we have no input focus at all, sleep a bit */
		if (!VID_HasMouseOrInputFocus() || cl.paused)
		{
			SDL_Delay(16);
		}
		/* If we're minimised, sleep a bit more */
		if (VID_IsMinimized())
		{
			scr_skipupdate = 1;
			SDL_Delay(32);
		}
		else
		{
			scr_skipupdate = 0;
		}
		newtime = Sys_DoubleTime ();
		time = newtime - oldtime;

		Host_Frame (time);

		if (time < sys_throttle.value && !cls.timedemo)
			SDL_Delay(1);

		oldtime = newtime;
	}

	// due to appletLockExit, main loop will terminate if user exits via HOME
	// so we clean shit up
	Sys_Quit ();

	return 0;
}

