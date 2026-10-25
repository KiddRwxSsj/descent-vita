#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fix.h"
#include "object.h"
#include "mono.h"
#include "timer.h"
#include "digi.h"
#include "sounds.h"
#include "args.h"
#include "newdemo.h"
#include "game.h"
#include "error.h"
#include "wall.h"
#include "piggy.h"
#include "kconfig.h"
#include "mixer.h"
#include "cfile.h"

#define _DIGI_MAX_VOLUME	16384

#define CENTER_PAN		(0xffff/2)

static int	Digi_initialized 		= 0;
static int	digi_atexit_called	= 0;

int digi_driver_board				= 0;
int digi_driver_port					= 0;
int digi_driver_irq					= 0;
int digi_driver_dma					= 0;
int digi_midi_type					= 0;
int digi_midi_port					= 0;
static int digi_max_channels		= 8;
int digi_timer_rate					= 9943;
int digi_lomem 						= 0;
static int digi_volume				= _DIGI_MAX_VOLUME;
static int midi_volume				= 128/2;
char digi_last_midi_song[16] = "";
char digi_last_melodic_bank[16] = "";
char digi_last_drum_bank[16] = "";

static ubyte *SongData = NULL;
static int SongSize;
static int music_ready = 0;

#define SOF_USED				1
#define SOF_PLAYING			2
#define SOF_LINK_TO_OBJ		4
#define SOF_LINK_TO_POS		8
#define SOF_PLAY_FOREVER	16

typedef struct sound_object {
	short			signature;
	ubyte			flags;
	fix			max_volume;
	fix			max_distance;
	int			volume;
	int			pan;
	int			handle;
	short			soundnum;
	union {
		struct {
			short			segnum;
			short			sidenum;
			vms_vector	position;
		};
		struct {
			short			objnum;
			short			objsignature;
		};
	};
} sound_object;

#define MAX_SOUND_OBJECTS 16
sound_object SoundObjects[MAX_SOUND_OBJECTS];
short next_signature=0;

int digi_sounds_initialized=0;

static int next_handle = 0;
static int SampleHandles[32];
static int SoundNums[32];
static int SoundVolumes[32];

int sound_paused = 0;

int digi_xlat_sound(int soundno)
{
	if ( soundno < 0 ) return -1;

	if ( digi_lomem )	{
		soundno = AltSounds[soundno];
		if ( soundno == 255 ) return -1;
	}
	// the shareware tables leave some slots at 255 (weapon select is one). DOS read
	// past the end of GameSounds and played junk, here the mixer faults on it
	if ( Sounds[soundno] == 255 ) return -1;
	return Sounds[soundno];
}

void digi_close()
{
	if (!Digi_initialized) return;
	Digi_initialized = 0;
	mix_close();
}

static void digi_reset_digi_sounds()
{
	int i;

	for (i=0; i<32; i++ )	{
		if (SampleHandles[i] >= 0)	{
			mix_stop(SampleHandles[i]);
			SampleHandles[i] = -1;
		}
		SoundNums[i] = -1;
	}
}

int digi_init()
{
	int i;

	for (i=0; i<32; i++ ) {
		SampleHandles[i] = -1;
		SoundNums[i] = -1;
	}

	if (!mix_init())
		return 0;

	// there is no SETUP.EXE here, so any board id means "use the vita output"
	if ( digi_driver_board < 1 )
		digi_driver_board = 1;

	Digi_initialized = 1;

	if (!digi_atexit_called)	{
		atexit( digi_close );
		digi_atexit_called = 1;
	}

	digi_init_sounds();

	// stands in for the SOS MIDI driver; no soundfont and no .dxa soundtrack in the data folder, no music
	if ( !music_ready )	{
		music_ready = music_init( "music.sf2" );
		music_ready |= dxa_init();
	}
	digi_set_midi_volume( midi_volume );
	digi_reset_digi_sounds();

	return 0;
}

void digi_reset()
{
	if ( Digi_initialized )	{
		digi_reset_digi_sounds();
		digi_close();
	} else {
		digi_init();
	}
}

static void reset_sounds_on_channel( int channel )
{
	int i;

	for (i=0; i<digi_max_channels; i++ )	{
		if (SampleHandles[i] == channel )	{
			SampleHandles[i] = -1;
			SoundNums[i] = -1;
		}
	}
}

void digi_set_max_channels(int n)
{
	digi_max_channels	= n;

	if ( digi_max_channels < 1 )
		digi_max_channels = 1;
	if ( digi_max_channels > (32-MAX_SOUND_OBJECTS) )
		digi_max_channels = (32-MAX_SOUND_OBJECTS);

	if ( !Digi_initialized ) return;

	digi_reset_digi_sounds();
}

int digi_get_max_channels()
{
	return digi_max_channels;
}

static int digi_start_sound(digi_sound *snd, int soundnum, int volume, int pan)
{
	int ntries;
	int handle;

	if ( !Digi_initialized ) return -1;

	ntries = 0;

TryNextChannel:
	if ( (SampleHandles[next_handle] >= 0) && (!mix_done(SampleHandles[next_handle]))  )	{
		if ( (SoundVolumes[next_handle] > digi_volume) && (ntries<digi_max_channels) )	{
			next_handle++;
			if ( next_handle >= digi_max_channels )
				next_handle = 0;
			ntries++;
			goto TryNextChannel;
		}
		mix_stop(SampleHandles[next_handle]);
		SampleHandles[next_handle] = -1;
	}

	handle = mix_start(snd->data, snd->length, volume, pan, 0, soundnum);
	if ( handle < 0 )
		return -1;

	SampleHandles[next_handle] = handle;
	SoundNums[next_handle] = soundnum;
	SoundVolumes[next_handle] = volume;
	next_handle++;
	if ( next_handle >= digi_max_channels )
		next_handle = 0;

	return handle;
}

int digi_is_sound_playing(int soundno)
{
	soundno = digi_xlat_sound(soundno);

	if (!Digi_initialized) return 0;
	if (soundno < 0 ) return 0;
	if (GameSounds[soundno].data==NULL) return 0;

	return mix_find_id(soundno) >= 0;
}

void digi_play_sample_once( int soundno, fix max_volume )
{
	int h;

#ifdef NEWDEMO
	if ( Newdemo_state == ND_STATE_RECORDING )
		newdemo_record_sound( soundno );
#endif
	soundno = digi_xlat_sound(soundno);

	if (!Digi_initialized) return;
	if (soundno < 0 ) return;

	h = mix_find_id(soundno);
	if (h >= 0)
		mix_stop(h);

	digi_start_sound( &GameSounds[soundno], soundno, fixmuldiv(max_volume,digi_volume,F1_0), CENTER_PAN );
}

void digi_play_sample( int soundno, fix max_volume )
{
#ifdef NEWDEMO
	if ( Newdemo_state == ND_STATE_RECORDING )
		newdemo_record_sound( soundno );
#endif
	soundno = digi_xlat_sound(soundno);

	if (!Digi_initialized) return;
	if (soundno < 0 ) return;

	digi_start_sound( &GameSounds[soundno], soundno, fixmuldiv(max_volume,digi_volume,F1_0), CENTER_PAN );
}

void digi_play_sample_3d( int soundno, int angle, int volume, int no_dups )
{
	no_dups = 1;

#ifdef NEWDEMO
	if ( Newdemo_state == ND_STATE_RECORDING )		{
		if ( no_dups )
			newdemo_record_sound_3d_once( soundno, angle, volume );
		else
			newdemo_record_sound_3d( soundno, angle, volume );
	}
#endif
	soundno = digi_xlat_sound(soundno);

	if (!Digi_initialized) return;
	if (soundno < 0 ) return;

	if (volume < 10 ) return;

	digi_start_sound( &GameSounds[soundno], soundno, fixmuldiv(volume,digi_volume,F1_0), angle );
}

void digi_set_midi_volume( int mvolume )
{
	int old_volume = midi_volume;

	if ( mvolume > 127 )
		midi_volume = 127;
	else if ( mvolume < 0 )
		midi_volume = 0;
	else
		midi_volume = mvolume;

	if ( music_ready )	{
		if ( (old_volume < 1) && ( midi_volume > 1 ) )	{
			if ( SongData == NULL )
				digi_play_midi_song( digi_last_midi_song, digi_last_melodic_bank, digi_last_drum_bank, 1 );
		}
		music_set_volume( midi_volume );
	}
}

void digi_set_digi_volume( int dvolume )
{
	dvolume = fixmuldiv( dvolume, _DIGI_MAX_VOLUME, 0x7fff);
	if ( dvolume > _DIGI_MAX_VOLUME )
		digi_volume = _DIGI_MAX_VOLUME;
	else if ( dvolume < 0 )
		digi_volume = 0;
	else
		digi_volume = dvolume;

	if ( !Digi_initialized ) return;

	digi_sync_sounds();
}

void digi_set_volume( int dvolume, int mvolume )
{
	digi_set_midi_volume( mvolume );
	digi_set_digi_volume( dvolume );
}

void digi_stop_current_song()
{
	dxa_stop();
	music_stop();
	if (SongData) 	{
		free(SongData);
		SongData = NULL;
	}
}

void digi_play_midi_song( char * filename, char * melodic_bank, char * drum_bank, int loop )
{
	CFILE *fp;

	if (!Digi_initialized) return;
	if (!music_ready) return;

	digi_stop_current_song();

	if ( filename == NULL )	return;

	if ( filename != digi_last_midi_song )	{
		strncpy( digi_last_midi_song, filename, sizeof(digi_last_midi_song)-1 );
		strncpy( digi_last_melodic_bank, melodic_bank, sizeof(digi_last_melodic_bank)-1 );
		strncpy( digi_last_drum_bank, drum_bank, sizeof(digi_last_drum_bank)-1 );
	}

	if ( midi_volume < 1 )
		return;				// Don't play song if volume == 0;

	if ( dxa_play( filename, loop ) )
		return;

	fp = cfopen( filename, "rb" );
	if (!fp) {
 		mprintf( (1, "Error opening midi file, '%s'", filename ));
 		return;
	}
	SongSize = cfilelength( fp );
	SongData	= malloc( SongSize );
	if (SongData==NULL)	{
		cfclose(fp);
		return;
	}
	if ( cfread (  SongData, SongSize, 1, fp )!=1)	{
		cfclose(fp);
		free(SongData);
		SongData=NULL;
		return;
	}
	cfclose(fp);

	if ( !music_start( SongData, SongSize, loop ) )	{
		free(SongData);
		SongData=NULL;
	}
}

void digi_get_sound_loc( vms_matrix * listener, vms_vector * listener_pos, int listener_seg, vms_vector * sound_pos, int sound_seg, fix max_volume, int *volume, int *pan, fix max_distance )
{
	vms_vector	vector_to_sound;
	fix angle_from_ear, cosang,sinang;
	fix distance;
	fix path_distance;

	*volume = 0;
	*pan = 0;

	max_distance = (max_distance*5)/4;		// Make all sounds travel 1.25 times as far.

	distance = vm_vec_normalized_dir_quick( &vector_to_sound, sound_pos, listener_pos );

	if (distance < max_distance )	{
		int num_search_segs = f2i(max_distance/20);
		if ( num_search_segs < 1 ) num_search_segs = 1;

		path_distance = find_connected_distance(listener_pos, listener_seg, sound_pos, sound_seg, num_search_segs, WID_RENDPAST_FLAG );
		if ( path_distance > -1 )	{
			*volume = max_volume - fixdiv(path_distance,max_distance);
			if (*volume > 0 )	{
				angle_from_ear = vm_vec_delta_ang_norm(&listener->rvec,&vector_to_sound,&listener->uvec);
				fix_sincos(angle_from_ear,&sinang,&cosang);
				if (Config_channels_reversed) cosang *= -1;
				*pan = (cosang + F1_0)/2;
			} else {
				*volume = 0;
			}
		}
	}
}

void digi_init_sounds()
{
	int i;

	if (!Digi_initialized) return;

	digi_reset_digi_sounds();

	for (i=0; i<MAX_SOUND_OBJECTS; i++ )	{
		if (digi_sounds_initialized) {
			if ( SoundObjects[i].flags & SOF_PLAYING )
				mix_stop( SoundObjects[i].handle );
		}
		SoundObjects[i].flags = 0;
	}
	digi_sounds_initialized = 1;
}

static void digi_start_sound_object(int i)
{
	digi_sound *snd;

	if (!Digi_initialized) return;

	snd = &GameSounds[SoundObjects[i].soundnum];

	SoundObjects[i].signature=next_signature++;
	SoundObjects[i].handle = mix_start(snd->data, snd->length,
								fixmuldiv(SoundObjects[i].volume,digi_volume,F1_0),
								SoundObjects[i].pan,
								(SoundObjects[i].flags & SOF_PLAY_FOREVER) != 0,
								SoundObjects[i].soundnum);
	if (SoundObjects[i].handle >= 0)	{
		SoundObjects[i].flags |= SOF_PLAYING;
		reset_sounds_on_channel( SoundObjects[i].handle );
	}
}

int digi_link_sound_to_object2( int org_soundnum, short objnum, int forever, fix max_volume, fix  max_distance )
{
	int i,volume,pan;
	object * objp;
	int soundnum;

	soundnum = digi_xlat_sound(org_soundnum);

	if ( max_volume < 0 ) return -1;

	if (!Digi_initialized) return -1;
	if (soundnum < 0 ) return -1;
	if (GameSounds[soundnum].data==NULL) {
		Int3();
		return -1;
	}
	if ((objnum<0)||(objnum>Highest_object_index))
		return -1;

	if ( !forever )	{
		// Hack to keep sounds from building up...
		digi_get_sound_loc( &Viewer->orient, &Viewer->pos, Viewer->segnum, &Objects[objnum].pos, Objects[objnum].segnum, max_volume,&volume, &pan, max_distance );
		digi_play_sample_3d( org_soundnum, pan, volume, 0 );
		return -1;
	}

	for (i=0; i<MAX_SOUND_OBJECTS; i++ )
		if (SoundObjects[i].flags==0)
			break;

	if (i==MAX_SOUND_OBJECTS)
		return -1;

	SoundObjects[i].signature=next_signature++;
	SoundObjects[i].flags = SOF_USED | SOF_LINK_TO_OBJ;
	if ( forever )
		SoundObjects[i].flags |= SOF_PLAY_FOREVER;
	SoundObjects[i].objnum = objnum;
	SoundObjects[i].objsignature = Objects[objnum].signature;
	SoundObjects[i].max_volume = max_volume;
	SoundObjects[i].max_distance = max_distance;
	SoundObjects[i].volume = 0;
	SoundObjects[i].pan = 0;
	SoundObjects[i].soundnum = soundnum;

	objp = &Objects[SoundObjects[i].objnum];
	digi_get_sound_loc( &Viewer->orient, &Viewer->pos, Viewer->segnum,
                       &objp->pos, objp->segnum, SoundObjects[i].max_volume,
                       &SoundObjects[i].volume, &SoundObjects[i].pan, SoundObjects[i].max_distance );

	digi_start_sound_object(i);

	return SoundObjects[i].signature;
}

int digi_link_sound_to_object( int soundnum, short objnum, int forever, fix max_volume )
{
	return digi_link_sound_to_object2( soundnum, objnum, forever, max_volume, 256*F1_0  );
}

int digi_link_sound_to_pos2( int org_soundnum, short segnum, short sidenum, vms_vector * pos, int forever, fix max_volume, fix max_distance )
{
	int i, volume, pan;
	int soundnum;

	soundnum = digi_xlat_sound(org_soundnum);

	if ( max_volume < 0 ) return -1;

	if (!Digi_initialized) return -1;
	if (soundnum < 0 ) return -1;
	if (GameSounds[soundnum].data==NULL) {
		Int3();
		return -1;
	}

	if ((segnum<0)||(segnum>Highest_segment_index))
		return -1;

	if ( !forever )	{
		// Hack to keep sounds from building up...
		digi_get_sound_loc( &Viewer->orient, &Viewer->pos, Viewer->segnum, pos, segnum, max_volume, &volume, &pan, max_distance );
		digi_play_sample_3d( org_soundnum, pan, volume, 0 );
		return -1;
	}

	for (i=0; i<MAX_SOUND_OBJECTS; i++ )
		if (SoundObjects[i].flags==0)
			break;

	if (i==MAX_SOUND_OBJECTS)
		return -1;

	SoundObjects[i].signature=next_signature++;
	SoundObjects[i].flags = SOF_USED | SOF_LINK_TO_POS;
	if ( forever )
		SoundObjects[i].flags |= SOF_PLAY_FOREVER;
	SoundObjects[i].segnum = segnum;
	SoundObjects[i].sidenum = sidenum;
	SoundObjects[i].position = *pos;
	SoundObjects[i].soundnum = soundnum;
	SoundObjects[i].max_volume = max_volume;
	SoundObjects[i].max_distance = max_distance;
	SoundObjects[i].volume = 0;
	SoundObjects[i].pan = 0;
	digi_get_sound_loc( &Viewer->orient, &Viewer->pos, Viewer->segnum,
                       &SoundObjects[i].position, SoundObjects[i].segnum, SoundObjects[i].max_volume,
                       &SoundObjects[i].volume, &SoundObjects[i].pan, SoundObjects[i].max_distance );

	digi_start_sound_object(i);

	return SoundObjects[i].signature;
}

int digi_link_sound_to_pos( int soundnum, short segnum, short sidenum, vms_vector * pos, int forever, fix max_volume )
{
	return digi_link_sound_to_pos2( soundnum, segnum, sidenum, pos, forever, max_volume, F1_0 * 256 );
}

void digi_kill_sound_linked_to_segment( int segnum, int sidenum, int soundnum )
{
	int i;

	soundnum = digi_xlat_sound(soundnum);

	if (!Digi_initialized) return;

	for (i=0; i<MAX_SOUND_OBJECTS; i++ )	{
		if ( (SoundObjects[i].flags & SOF_USED) && (SoundObjects[i].flags & SOF_LINK_TO_POS) )	{
			if ((SoundObjects[i].segnum == segnum) && (SoundObjects[i].soundnum==soundnum ) && (SoundObjects[i].sidenum==sidenum) )	{
				if ( SoundObjects[i].flags & SOF_PLAYING )
					mix_stop( SoundObjects[i].handle );
				SoundObjects[i].flags = 0;
			}
		}
	}
}

void digi_kill_sound_linked_to_object( int objnum )
{
	int i;

	if (!Digi_initialized) return;

	for (i=0; i<MAX_SOUND_OBJECTS; i++ )	{
		if ( (SoundObjects[i].flags & SOF_USED) && (SoundObjects[i].flags & SOF_LINK_TO_OBJ ) )	{
			if (SoundObjects[i].objnum == objnum)	{
				if ( SoundObjects[i].flags & SOF_PLAYING )
					mix_stop( SoundObjects[i].handle );
				SoundObjects[i].flags = 0;
			}
		}
	}
}

void digi_sync_sounds()
{
	int i;
	int oldvolume, oldpan;

	if (!Digi_initialized) return;

	for (i=0; i<MAX_SOUND_OBJECTS; i++ )	{
		if ( SoundObjects[i].flags & SOF_USED )	{
			oldvolume = SoundObjects[i].volume;
			oldpan = SoundObjects[i].pan;

			if ( !(SoundObjects[i].flags & SOF_PLAY_FOREVER) )	{
				if (SoundObjects[i].flags & SOF_PLAYING) {
					if (mix_done(SoundObjects[i].handle) )	{
						SoundObjects[i].flags = 0;
						continue;
					}
				}
			}

			if ( SoundObjects[i].flags & SOF_LINK_TO_POS )	{
				digi_get_sound_loc( &Viewer->orient, &Viewer->pos, Viewer->segnum,
                                &SoundObjects[i].position, SoundObjects[i].segnum, SoundObjects[i].max_volume,
                                &SoundObjects[i].volume, &SoundObjects[i].pan, SoundObjects[i].max_distance );

			} else if ( SoundObjects[i].flags & SOF_LINK_TO_OBJ )	{
				object * objp;

				objp = &Objects[SoundObjects[i].objnum];

				if ((objp->type==OBJ_NONE) || (objp->signature!=SoundObjects[i].objsignature))	{
					// The object that this is linked to is dead, so just end this sound if it is looping.
					if ( (SoundObjects[i].flags & SOF_PLAYING)  && (SoundObjects[i].flags & SOF_PLAY_FOREVER))
						mix_stop( SoundObjects[i].handle );
					SoundObjects[i].flags = 0;
					continue;
				} else {
					digi_get_sound_loc( &Viewer->orient, &Viewer->pos, Viewer->segnum,
	                                &objp->pos, objp->segnum, SoundObjects[i].max_volume,
                                   &SoundObjects[i].volume, &SoundObjects[i].pan, SoundObjects[i].max_distance );
				}
			}

			if (oldvolume != SoundObjects[i].volume) 	{
				if ( SoundObjects[i].volume < 1 )	{
					// Sound is too far away, so stop it from playing.
					if ((SoundObjects[i].flags & SOF_PLAYING)&&(SoundObjects[i].flags & SOF_PLAY_FOREVER))	{
						mix_stop( SoundObjects[i].handle );
						SoundObjects[i].flags &= ~SOF_PLAYING;
					}
				} else {
					if (!(SoundObjects[i].flags & SOF_PLAYING))
						digi_start_sound_object(i);
					else
						mix_set_volume( SoundObjects[i].handle, fixmuldiv(SoundObjects[i].volume,digi_volume,F1_0) );
				}
			}

			if (oldpan != SoundObjects[i].pan) 	{
				if (SoundObjects[i].flags & SOF_PLAYING)
					mix_set_pan( SoundObjects[i].handle, SoundObjects[i].pan );
			}
		}
	}
}

void digi_pause_all()
{
	int i;

	if (!Digi_initialized) return;

	if (sound_paused==0)	{
		for (i=0; i<MAX_SOUND_OBJECTS; i++ )	{
			if ( (SoundObjects[i].flags & SOF_USED) && (SoundObjects[i].flags & SOF_PLAYING)&& (SoundObjects[i].flags & SOF_PLAY_FOREVER) )	{
				mix_stop( SoundObjects[i].handle );
				SoundObjects[i].flags &= ~SOF_PLAYING;
			}
		}
	}
	sound_paused++;
}

void digi_resume_all()
{
	if (!Digi_initialized) return;

	Assert( sound_paused > 0 );

	sound_paused--;
}

void digi_stop_all()
{
	int i;

	if (!Digi_initialized) return;

	for (i=0; i<MAX_SOUND_OBJECTS; i++ )	{
		if ( SoundObjects[i].flags & SOF_USED )	{
			if ( SoundObjects[i].flags & SOF_PLAYING )
				mix_stop( SoundObjects[i].handle );
			SoundObjects[i].flags = 0;
		}
	}
}
