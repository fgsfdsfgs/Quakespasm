/*
	cd_sdl.c

	Copyright (C) 1996-1997  Id Software, Inc.
	Taken from the Twilight project with modifications
	to make it work with Hexen II: Hammer of Thyrion.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		51 Franklin St, Fifth Floor,
		Boston, MA  02110-1301  USA
*/

#include "SDL.h"

#ifndef	SDL_INIT_CDROM

/* SDL dropped support for
   cd audio since v1.3.0 */
#warning SDL CDAudio support disabled
#include "cd_null.c"

#else	/* SDL_INIT_CDROM */

#include "quakedef.h"

static qboolean enabled = false;
static qboolean cdValid = false;
static qboolean	playing = false;
static qboolean	wasPlaying = false;
static qboolean playLooping = false;
static byte	playTrack;
static double	endOfTrack = -1.0, pausetime = -1.0;
static SDL_CD	*cd_handle;
static int	cd_dev = -1;
static float	old_cdvolume;
static qboolean	hw_vol_works = true;


void CDAudioBackend_Eject(void)
{
	if (!cd_handle)
		return;

	if (SDL_CDEject(cd_handle) == -1)
		Con_Printf ("Unable to eject CD-ROM: %s\n", SDL_GetError ());
}

static int CDAudioBackend_GetAudioDiskInfo(void)
{
	cdValid = false;

	if (!cd_handle)
		return -1;

	if ( ! CD_INDRIVE(SDL_CDStatus(cd_handle)) )
		return -1;

	cdValid = true;

	return 0;
}

qboolean CDAudioBackend_IsPlaying()
{
	return playing;
}

void CDAudioBackend_Play(byte track, qboolean looping)
{
	int	len_m, len_s, len_f;

	if (!cd_handle || !enabled)
		return;

	if (!cdValid)
	{
		CDAudioBackend_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	if (track < 1 || track > cd_handle->numtracks)
	{
		Con_Printf ("CDAudioBackend_Play: Bad track number %d.\n", track);
		return;
	}

	if (cd_handle->track[track-1].type == SDL_DATA_TRACK)
	{
		Con_Printf ("CDAudioBackend_Play: track %d is not audio\n", track);
		return;
	}

	if (playing)
	{
		if (playTrack == track)
			return;
		CDAudioBackend_Stop ();
	}

	if (SDL_CDPlay(cd_handle, cd_handle->track[track-1].offset, cd_handle->track[track-1].length) == -1)
	{
		// ok, check for status now
		int cd_status = SDL_CDStatus(cd_handle);

		if (cd_status > 0)
			Con_Printf ("CDAudioBackend_Play: Unable to play %d: %s\n", track, SDL_GetError ());
		return;
	}

	playLooping = looping;
	playTrack = track;
	playing = true;

	FRAMES_TO_MSF(cd_handle->track[track-1].length, &len_m, &len_s, &len_f);
	endOfTrack = realtime + ((double)len_m * 60.0) + (double)len_s + (double)len_f / (double)CD_FPS;

	/*
	 * Add the pregap for the next track.  This means that disc-at-once CDs
	 * won't loop smoothly, but they wouldn't anyway so it doesn't really
	 * matter.  SDL doesn't give us pregap information anyway, so you'll
	 * just have to live with it.
	 */
	endOfTrack += 2.0;
	pausetime = -1.0;

	if (!hw_vol_works && bgmvolume.value == 0.0)
		CDAudioBackend_Pause ();
}

void CDAudioBackend_Stop(void)
{
	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDStop(cd_handle) == -1)
		Con_Printf ("CDAudioBackend_Stop: Unable to stop CD-ROM (%s)\n", SDL_GetError());

	wasPlaying = false;
	playing = false;
	pausetime = -1.0;
	endOfTrack = -1.0;
}

void CDAudioBackend_Next(void)
{
	byte track;

	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	track = playTrack + 1;	/* cd_handle->track[cd_handle->cur_track].id + 1; */
	if (track > cd_handle->numtracks)
		track = 1;

	CDAudioBackend_Play (track, playLooping);
}

void CDAudioBackend_Prev(void)
{
	byte track;

	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	track = playTrack - 1;
	if (track < 1)
		track = cd_handle->numtracks;

	CDAudioBackend_Play (track, playLooping);
}

void CDAudioBackend_Pause(void)
{
	if (!cd_handle || !enabled)
		return;

	if (!playing)
		return;

	if (SDL_CDPause(cd_handle) == -1)
		Con_Printf ("Unable to pause CD-ROM: %s\n", SDL_GetError());

	wasPlaying = playing;
	playing = false;
	pausetime = realtime;
}

void CDAudioBackend_Resume(void)
{
	if (!cd_handle || !enabled)
		return;

	if (!cdValid)
		return;

	if (!wasPlaying)
		return;	

	if (SDL_CDResume(cd_handle) == -1)
		Con_Printf ("Unable to resume CD-ROM: %s\n", SDL_GetError());
	playing = true;
	endOfTrack += realtime - pausetime;
	pausetime = -1.0;
}

void CDAudioBackend_Info()
{
	int	current_min, current_sec, current_frame;
	int	length_min, length_sec, length_frame;

	Con_Printf ("%u tracks\n", cd_handle->numtracks);

	if (playing)
		Con_Printf ("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
	else if (wasPlaying)
		Con_Printf ("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);

	if (playing || wasPlaying)
	{
		SDL_CDStatus(cd_handle);
		FRAMES_TO_MSF(cd_handle->cur_frame, &current_min, &current_sec, &current_frame);
		FRAMES_TO_MSF(cd_handle->track[playTrack-1].length, &length_min, &length_sec, &length_frame);

		Con_Printf ("Current position: %d:%02d.%02d (of %d:%02d.%02d)\n",
					current_min, current_sec, current_frame * 60 / CD_FPS,
					length_min, length_sec, length_frame * 60 / CD_FPS);
	}
	Con_Printf ("Volume is %f\n", bgmvolume.value);

	return;
}

static qboolean CD_GetVolume (void *unused)
{
/* FIXME: write proper code in here when SDL
   supports cdrom volume control some day. */
	return false;
}

static qboolean CD_SetVolume (void *unused)
{
/* FIXME: write proper code in here when SDL
   supports cdrom volume control some day. */
	return false;
}

static qboolean CDAudioBackend_SetVolume (cvar_t *var)
{
	if (!cd_handle || !enabled)
		return false;

	if (var->value < 0.0)
		Cvar_SetValue (var->name, 0.0);
	else if (var->value > 1.0)
		Cvar_SetValue (var->name, 1.0);
	old_cdvolume = var->value;
	if (hw_vol_works)
	{
/* FIXME: write proper code in here when SDL
   supports cdrom volume control some day. */
		return CD_SetVolume (NULL);
	}
	else
	{
		if (old_cdvolume == 0.0)
			CDAudioBackend_Pause ();
		else
			CDAudioBackend_Resume();
		return false;
	}
}

void CDAudioBackend_Update(void)
{
	CDstatus	curstat;

	if (!cd_handle || !enabled)
		return;

	if (old_cdvolume != bgmvolume.value)
		CDAudioBackend_SetVolume (&bgmvolume);

	if (playing && realtime > endOfTrack)
	{
	//	curstat = cd_handle->status;
		curstat = SDL_CDStatus(cd_handle);
		if (curstat != CD_PLAYING && curstat != CD_PAUSED)
		{
			endOfTrack = -1.0;
			if (playLooping) {
				playing = false;
				CDAudioBackend_Play(playTrack, true);
			}
			else
				CDAudioBackend_Next();
		}
	}
}

static const char *get_cddev_arg (const char *arg)
{
#if defined(_WIN32)
/* arg should be like "D:\", make sure it is so,
 * but tolerate args like "D" or "D:", as well. */
	static char drive[4];
	if (!arg || ! *arg)
		return NULL;
	if (arg[1] != '\0')
	{
		if (arg[1] != ':')
			return NULL;
		if (arg[2] != '\0')
		{
			if (arg[2] != '\\' &&
			    arg[2] != '/')
				return NULL;
			if (arg[3] != '\0')
				return NULL;
		}
	}
	if (*arg >= 'A' && *arg <= 'Z')
	{
		drive[0] = *arg;
		drive[1] = ':';
		drive[2] = '\\';
		drive[3] = '\0';
		return drive;
	}
	else if (*arg >= 'a' && *arg <= 'z')
	{
	/* make it uppercase for SDL */
		drive[0] = *arg - ('a' - 'A');
		drive[1] = ':';
		drive[2] = '\\';
		drive[3] = '\0';
		return drive;
	}
	return NULL;
#else
	if (!arg || ! *arg)
		return NULL;
	return arg;
#endif
}

static void export_cddev_arg (void)
{
/* Bad ugly hack to workaround SDL's cdrom device detection.
 * not needed for windows due to the way SDL_cdrom works. */
#if !defined(_WIN32)
	int i = COM_CheckParm("-cddev");
	if (i != 0 && i < com_argc - 1 && com_argv[i+1][0] != '\0')
	{
		static char arg[64];
		q_snprintf(arg, sizeof(arg), "SDL_CDROM=%s", com_argv[i+1]);
		putenv(arg);
	}
#endif
}

int CDAudioBackend_Init(void)
{
	int	i, sdl_num_drives;

	if (safemode || COM_CheckParm("-nocdaudio"))
		return -1;

	export_cddev_arg();

	if (SDL_InitSubSystem(SDL_INIT_CDROM) == -1)
	{
		Con_Printf("Couldn't init SDL cdrom: %s\n", SDL_GetError());
		return -1;
	}

	sdl_num_drives = SDL_CDNumDrives ();
	Con_Printf ("SDL detected %d CD-ROM drive%c\n", sdl_num_drives,
					sdl_num_drives == 1 ? ' ' : 's');

	if (sdl_num_drives < 1)
		return -1;

	if ((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1)
	{
		const char *userdev = get_cddev_arg(com_argv[i+1]);
		if (!userdev)
		{
			Con_Printf("Invalid argument to -cddev\n");
			return -1;
		}
		for (i = 0; i < sdl_num_drives; i++)
		{
			if (!Q_strcasecmp(SDL_CDName(i), userdev))
			{
				cd_dev = i;
				break;
			}
		}
		if (cd_dev == -1)
		{
			Con_Printf("SDL couldn't find cdrom device %s\n", userdev);
			return -1;
		}
	}

	if (cd_dev == -1)
		cd_dev = 0;	// default drive

	cd_handle = SDL_CDOpen(cd_dev);
	if (!cd_handle)
	{
		Con_Printf ("CDAudioBackend_Init: Unable to open CD-ROM drive %s (%s)\n",
				SDL_CDName(cd_dev), SDL_GetError());
		return -1;
	}

	enabled = true;
	old_cdvolume = bgmvolume.value;

	Con_Printf("CDAudio initialized (SDL, using %s)\n", SDL_CDName(cd_dev));

	if (CDAudioBackend_GetAudioDiskInfo())
	{
		Con_Printf("CDAudioBackend_Init: No CD in drive\n");
		cdValid = false;
	}

// cd hardware volume: no SDL support at present.
	hw_vol_works = CD_GetVolume (NULL);
	if (hw_vol_works)
		hw_vol_works = CDAudioBackend_SetVolume (&bgmvolume);

	return 0;
}

void CDAudioBackend_Shutdown(void)
{
	if (!cd_handle)
		return;
	CDAudioBackend_Stop();
// cd hardware volume: no SDL support at present.
//	if (hw_vol_works)
//		CD_SetVolume (NULL);
	SDL_CDClose(cd_handle);
	cd_handle = NULL;
	cd_dev = -1;
	SDL_QuitSubSystem(SDL_INIT_CDROM);
}

#endif	/* SDL_INIT_CDROM */