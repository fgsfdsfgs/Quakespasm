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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <hal/xbox.h>

#include "quakedef.h"

#include <errno.h>
#include <stdio.h>

#include "SDL.h"

qboolean		isDedicated;
qboolean	Win95, Win95old, WinNT, WinVista;
qboolean		isDedicated;
cvar_t		sys_throttle = {"sys_throttle", "0.02", CVAR_ARCHIVE};

#define	MAX_HANDLES		32	/* johnfitz -- was 10 */
static FILE		*sys_handles[MAX_HANDLES];

static int findhandle (void)
{
	int i;

	for (i = 1; i < MAX_HANDLES; i++)
	{
		if (!sys_handles[i])
			return i;
	}
	Sys_Error ("out of handles");
	return -1;
}

long Sys_filelength (FILE *f)
{
	long		pos, end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenRead (const char *path, int *hndl)
{
	FILE	*f;
	int	i, retval;

	i = findhandle ();
	f = fopen(path, "rb");

	if (!f)
	{
		*hndl = -1;
		retval = -1;
	}
	else
	{
		sys_handles[i] = f;
		*hndl = i;
		retval = Sys_filelength(f);
	}

	return retval;
}

int Sys_FileOpenWrite (const char *path)
{
	FILE	*f;
	int		i;

	i = findhandle ();
	f = fopen(path, "wb");

	if (!f)
		Sys_Error ("Error opening %s: %s", path, strerror(errno));

	sys_handles[i] = f;
	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return fread (dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite (int handle, const void *data, int count)
{
	return fwrite (data, 1, count, sys_handles[handle]);
}

int Sys_FileTime (const char *path)
{
	FILE	*f;

	f = fopen(path, "rb");

	if (f)
	{
		fclose(f);
		return 1;
	}

	return -1;
}

static char	cwd[1024];

static void Sys_GetBasedir (char *argv0, char *dst, size_t dstsize)
{
	char *tmp;

	/* basedir == pbe path (automounted) */
	memcpy(dst, "D:\\", 3 + 1);

	tmp = dst;
	while (*tmp != 0)
		tmp++;
	while (*tmp == 0 && tmp != dst)
	{
		--tmp;
		if (tmp != dst && (*tmp == '/' || *tmp == '\\'))
			*tmp = 0;
	}
}

void Sys_Init (void)
{
	memset (cwd, 0, sizeof(cwd));
	Sys_GetBasedir(NULL, cwd, sizeof(cwd));
	host_parms->basedir = cwd;

	/* userdirs not really necessary for windows guys.
	 * can be done if necessary, though... */
	host_parms->userdir = host_parms->basedir; /* code elsewhere relies on this ! */

	/* xbox is at the very least WinNT */
	WinNT = true;
	Win95 = WinVista = Win95old = false;
	/* it has one CPU */
	host_parms->numcpus = 1;

	Sys_Printf("Detected %d CPUs.\n", host_parms->numcpus);
}

void Sys_mkdir (const char *path)
{
	if (CreateDirectory(path, NULL) != 0)
		return;
	if (GetLastError() != ERROR_ALREADY_EXISTS)
		Sys_Error("Unable to create directory %s", path);
}

static const char errortxt1[] = "\nERROR-OUT BEGIN\n\n";
static const char errortxt2[] = "\nQUAKE ERROR: ";

void Sys_Error (const char *error, ...)
{
	va_list		argptr;
	char		text[1024];
	FILE		*out;

	host_parms->errstate++;

	va_start (argptr, error);
	q_vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	out = fopen("D:\\fatal.log", "w");
	if (out)
	{
		fprintf(out, "FATAL ERROR:\n%s\n", text);
		fclose(out);
	}

	exit (1);
}

void Sys_Printf (const char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	DWORD		dummy;

	va_start (argptr,fmt);
	q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	/* SDL will put these into its own stdout log,
	   so print to stdout even in graphical mode. */
	fputs (text, stdout);
}

void Sys_Quit (void)
{
	Host_Shutdown();

	exit (0);
}

double Sys_DoubleTime (void)
{
	return SDL_GetTicks() / 1000.0;
}

const char *Sys_ConsoleInput (void)
{
	/* nada */
	return NULL;
}

void Sys_Sleep (unsigned long msecs)
{
/*	Sleep (msecs);*/
	SDL_Delay (msecs);
}

void Sys_SendKeyEvents (void)
{
	IN_Commands();		//ericw -- allow joysticks to add keys so they can be used to confirm SCR_ModalMessage
	IN_SendKeyEvents();
}

