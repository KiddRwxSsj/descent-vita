/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/main/rcs/bm.c $
 * $Revision: 2.3 $
 * $Author: john $
 * $Date: 1995/03/14 16:22:04 $
 *
 * Bitmap and palette loading functions.
 *
 * $Log: bm.c $
 * Revision 2.3  1995/03/14  16:22:04  john
 * Added cdrom alternate directory stuff.
 * 
 * Revision 2.2  1995/03/07  16:51:48  john
 * Fixed robots not moving without edtiro bug.
 * 
 * Revision 2.1  1995/03/06  15:23:06  john
 * New screen techniques.
 * 
 * Revision 2.0  1995/02/27  11:27:05  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * 
 */

#pragma off (unreferenced)
static char rcsid[] = "$Id: bm.c 2.3 1995/03/14 16:22:04 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "inferno.h"
#include "gr.h"
#include "bm.h"
#include "mem.h"
#include "cflib.h"
#include "mono.h"
#include "error.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "polyobj.h"
#include "wall.h"
#include "textures.h"
#include "game.h"
#include "multi.h"
#include "iff.h"
#include "cfile.h"
#include "hostage.h"
#include "powerup.h"
#include "sounds.h"
#include "piggy.h"
#include "aistruct.h"
#include "robot.h"
#include "weapon.h"
#include "gauges.h"
#include "player.h"
#include "fuelcen.h"
#include "endlevel.h"
#include "cntrlcen.h"

ubyte Sounds[MAX_SOUNDS];
ubyte AltSounds[MAX_SOUNDS];

int Num_total_object_types;

byte	ObjType[MAX_OBJTYPE];
byte	ObjId[MAX_OBJTYPE];
fix	ObjStrength[MAX_OBJTYPE];

//for each model, a model number for dying & dead variants, or -1 if none
int Dying_modelnums[MAX_POLYGON_MODELS];
int Dead_modelnums[MAX_POLYGON_MODELS];

//right now there's only one player ship, but we can have another by 
//adding an array and setting the pointer to the active ship.
player_ship only_player_ship,*Player_ship=&only_player_ship;

//----------------- Miscellaneous bitmap pointers ---------------
int					Num_cockpits = 0;
bitmap_index		cockpit_bitmap[N_COCKPIT_BITMAPS];

//---------------- Variables for wall textures ------------------
int 					Num_tmaps;
tmap_info 			TmapInfo[MAX_TEXTURES];

//---------------- Variables for object textures ----------------

int					First_multi_bitmap_num=-1;

bitmap_index		ObjBitmaps[MAX_OBJ_BITMAPS];
ushort				ObjBitmapPtrs[MAX_OBJ_BITMAPS];		// These point back into ObjBitmaps, since some are used twice.

//-----------------------------------------------------------------
// Initializes all bitmaps from BITMAPS.TBL file.
int bm_init()
{
	#ifdef SHAREWARE
	// shareware data has no compiled tables in the pig, they come from BITMAPS.BIN
	return bm_init_use_tbl();
	#endif

	init_polygon_models();
	piggy_init();				// This calls bm_read_all
	piggy_read_sounds();
	return 0;
}

#ifndef __WATCOMC__
// The pig tables were written straight out of Watcom's byte-packed structs.
// GCC pads these for ARM, so pull them in a field at a time instead.

static int rd_int(CFILE *fp)		{ int v = 0; cfread(&v, 4, 1, fp); return v; }
static short rd_short(CFILE *fp)	{ short v = 0; cfread(&v, 2, 1, fp); return v; }
static ubyte rd_byte(CFILE *fp)	{ ubyte v = 0; cfread(&v, 1, 1, fp); return v; }

static void rd_vecs(vms_vector *v, int n, CFILE *fp)
{
	for (; n > 0; n--, v++) {
		v->x = rd_int(fp);
		v->y = rd_int(fp);
		v->z = rd_int(fp);
	}
}

static void rd_ints(int *v, int n, CFILE *fp)
{
	while (n-- > 0)
		*v++ = rd_int(fp);
}

static void read_tmap_info(tmap_info *t, CFILE *fp)
{
	cfread(t->filename, 13, 1, fp);
	t->flags = rd_byte(fp);
	t->lighting = rd_int(fp);
	t->damage = rd_int(fp);
	t->eclip_num = rd_int(fp);
}

static void read_vclip(vclip *vc, CFILE *fp)
{
	int i;

	vc->play_time = rd_int(fp);
	vc->num_frames = rd_int(fp);
	vc->frame_time = rd_int(fp);
	vc->flags = rd_int(fp);
	vc->sound_num = rd_short(fp);
	for (i=0; i<VCLIP_MAX_FRAMES; i++)
		vc->frames[i].index = rd_short(fp);
	vc->light_value = rd_int(fp);
}

static void read_eclip(eclip *ec, CFILE *fp)
{
	read_vclip(&ec->vc, fp);
	ec->time_left = rd_int(fp);
	ec->frame_count = rd_int(fp);
	ec->changing_wall_texture = rd_short(fp);
	ec->changing_object_texture = rd_short(fp);
	ec->flags = rd_int(fp);
	ec->crit_clip = rd_int(fp);
	ec->dest_bm_num = rd_int(fp);
	ec->dest_vclip = rd_int(fp);
	ec->dest_eclip = rd_int(fp);
	ec->dest_size = rd_int(fp);
	ec->sound_num = rd_int(fp);
	ec->segnum = rd_int(fp);
	ec->sidenum = rd_int(fp);
}

static void read_wclip(wclip *wc, CFILE *fp)
{
	int i;

	wc->play_time = rd_int(fp);
	wc->num_frames = rd_short(fp);
	for (i=0; i<MAX_CLIP_FRAMES; i++)
		wc->frames[i] = rd_short(fp);
	wc->open_sound = rd_short(fp);
	wc->close_sound = rd_short(fp);
	wc->flags = rd_short(fp);
	cfread(wc->filename, 13, 1, fp);
	wc->pad = rd_byte(fp);
}

static void read_robot_info(robot_info *r, CFILE *fp)
{
	int i, j;

	r->model_num = rd_int(fp);
	r->n_guns = rd_int(fp);
	rd_vecs(r->gun_points, MAX_GUNS, fp);
	cfread(r->gun_submodels, MAX_GUNS, 1, fp);
	r->exp1_vclip_num = rd_short(fp);
	r->exp1_sound_num = rd_short(fp);
	r->exp2_vclip_num = rd_short(fp);
	r->exp2_sound_num = rd_short(fp);
	r->weapon_type = rd_short(fp);
	r->contains_id = rd_byte(fp);
	r->contains_count = rd_byte(fp);
	r->contains_prob = rd_byte(fp);
	r->contains_type = rd_byte(fp);
	r->score_value = rd_int(fp);
	r->lighting = rd_int(fp);
	r->strength = rd_int(fp);
	r->mass = rd_int(fp);
	r->drag = rd_int(fp);
	rd_ints(r->field_of_view, NDL, fp);
	rd_ints(r->firing_wait, NDL, fp);
	rd_ints(r->turn_time, NDL, fp);
	rd_ints(r->fire_power, NDL, fp);
	rd_ints(r->shield, NDL, fp);
	rd_ints(r->max_speed, NDL, fp);
	rd_ints(r->circle_distance, NDL, fp);
	cfread(r->rapidfire_count, NDL, 1, fp);
	cfread(r->evade_speed, NDL, 1, fp);
	r->cloak_type = rd_byte(fp);
	r->attack_type = rd_byte(fp);
	r->boss_flag = rd_byte(fp);
	r->see_sound = rd_byte(fp);
	r->attack_sound = rd_byte(fp);
	r->claw_sound = rd_byte(fp);
	for (i=0; i<MAX_GUNS+1; i++)
		for (j=0; j<N_ANIM_STATES; j++) {
			r->anim_states[i][j].n_joints = rd_short(fp);
			r->anim_states[i][j].offset = rd_short(fp);
		}
	r->always_0xabcd = rd_int(fp);
}

static void read_weapon_info(weapon_info *w, CFILE *fp)
{
	w->render_type = rd_byte(fp);
	w->model_num = rd_byte(fp);
	w->model_num_inner = rd_byte(fp);
	w->persistent = rd_byte(fp);
	w->flash_vclip = rd_byte(fp);
	w->flash_sound = rd_short(fp);
	w->robot_hit_vclip = rd_byte(fp);
	w->robot_hit_sound = rd_short(fp);
	w->wall_hit_vclip = rd_byte(fp);
	w->wall_hit_sound = rd_short(fp);
	w->fire_count = rd_byte(fp);
	w->ammo_usage = rd_byte(fp);
	w->weapon_vclip = rd_byte(fp);
	w->destroyable = rd_byte(fp);
	w->matter = rd_byte(fp);
	w->bounce = rd_byte(fp);
	w->homing_flag = rd_byte(fp);
	w->dum1 = rd_byte(fp);
	w->dum2 = rd_byte(fp);
	w->dum3 = rd_byte(fp);
	w->energy_usage = rd_int(fp);
	w->fire_wait = rd_int(fp);
	w->bitmap.index = rd_short(fp);
	w->blob_size = rd_int(fp);
	w->flash_size = rd_int(fp);
	w->impact_size = rd_int(fp);
	rd_ints(w->strength, NDL, fp);
	rd_ints(w->speed, NDL, fp);
	w->mass = rd_int(fp);
	w->drag = rd_int(fp);
	w->thrust = rd_int(fp);
	w->po_len_to_width_ratio = rd_int(fp);
	w->light = rd_int(fp);
	w->lifetime = rd_int(fp);
	w->damage_radius = rd_int(fp);
	w->picture.index = rd_short(fp);
}

static void read_polymodel(polymodel *pm, CFILE *fp)
{
	pm->n_models = rd_int(fp);
	pm->model_data_size = rd_int(fp);
	rd_int(fp);			// the DOS model_data pointer, meaningless here
	pm->model_data = NULL;
	rd_ints(pm->submodel_ptrs, MAX_SUBMODELS, fp);
	rd_vecs(pm->submodel_offsets, MAX_SUBMODELS, fp);
	rd_vecs(pm->submodel_norms, MAX_SUBMODELS, fp);
	rd_vecs(pm->submodel_pnts, MAX_SUBMODELS, fp);
	rd_ints(pm->submodel_rads, MAX_SUBMODELS, fp);
	cfread(pm->submodel_parents, MAX_SUBMODELS, 1, fp);
	rd_vecs(pm->submodel_mins, MAX_SUBMODELS, fp);
	rd_vecs(pm->submodel_maxs, MAX_SUBMODELS, fp);
	rd_vecs(&pm->mins, 1, fp);
	rd_vecs(&pm->maxs, 1, fp);
	pm->rad = rd_int(fp);
	pm->n_textures = rd_byte(fp);
	pm->first_texture = rd_short(fp);
	pm->simpler_model = rd_byte(fp);
}

#define READ_TABLE(arr, n, fn)	do { int _i; for (_i=0; _i<(n); _i++) fn(&(arr)[_i], fp); } while (0)

#endif

void bm_read_all(CFILE * fp)
{
	int i;

	cfread( &NumTextures, sizeof(int), 1, fp );
	cfread( Textures, sizeof(bitmap_index), MAX_TEXTURES, fp );
#ifdef __WATCOMC__
	cfread( TmapInfo, sizeof(tmap_info), MAX_TEXTURES, fp );
#else
	READ_TABLE(TmapInfo, MAX_TEXTURES, read_tmap_info);
#endif
	
	cfread( Sounds, sizeof(ubyte), MAX_SOUNDS, fp );
	cfread( AltSounds, sizeof(ubyte), MAX_SOUNDS, fp );

	cfread( &Num_vclips, sizeof(int), 1, fp );
#ifdef __WATCOMC__
	cfread( Vclip, sizeof(vclip), VCLIP_MAXNUM, fp );
#else
	READ_TABLE(Vclip, VCLIP_MAXNUM, read_vclip);
#endif

	cfread( &Num_effects, sizeof(int), 1, fp );
#ifdef __WATCOMC__
	cfread( Effects, sizeof(eclip), MAX_EFFECTS, fp );
#else
	READ_TABLE(Effects, MAX_EFFECTS, read_eclip);
#endif

	cfread( &Num_wall_anims, sizeof(int), 1, fp );
#ifdef __WATCOMC__
	cfread( WallAnims, sizeof(wclip), MAX_WALL_ANIMS, fp );
#else
	READ_TABLE(WallAnims, MAX_WALL_ANIMS, read_wclip);
#endif

	cfread( &N_robot_types, sizeof(int), 1, fp );
#ifdef __WATCOMC__
	cfread( Robot_info, sizeof(robot_info), MAX_ROBOT_TYPES, fp );
#else
	READ_TABLE(Robot_info, MAX_ROBOT_TYPES, read_robot_info);
#endif

	cfread( &N_robot_joints, sizeof(int), 1, fp );
	cfread( Robot_joints, sizeof(jointpos), MAX_ROBOT_JOINTS, fp );

	cfread( &N_weapon_types, sizeof(int), 1, fp );
#ifdef __WATCOMC__
	cfread( Weapon_info, sizeof(weapon_info), MAX_WEAPON_TYPES, fp );
#else
	READ_TABLE(Weapon_info, MAX_WEAPON_TYPES, read_weapon_info);
#endif

	cfread( &N_powerup_types, sizeof(int), 1, fp );
	cfread( Powerup_info, sizeof(powerup_type_info), MAX_POWERUP_TYPES, fp );
	
	cfread( &N_polygon_models, sizeof(int), 1, fp );
#ifdef __WATCOMC__
	cfread( Polygon_models, sizeof(polymodel), N_polygon_models, fp );
#else
	READ_TABLE(Polygon_models, N_polygon_models, read_polymodel);
#endif

	for (i=0; i<N_polygon_models; i++ )	{
		Polygon_models[i].model_data = malloc(Polygon_models[i].model_data_size);
		Assert( Polygon_models[i].model_data != NULL );
		cfread( Polygon_models[i].model_data, sizeof(ubyte), Polygon_models[i].model_data_size, fp );
	}

	cfread( Gauges, sizeof(bitmap_index), MAX_GAUGE_BMS, fp );

	cfread( Dying_modelnums, sizeof(int), MAX_POLYGON_MODELS, fp );
	cfread( Dead_modelnums, sizeof(int), MAX_POLYGON_MODELS, fp );

	cfread( ObjBitmaps, sizeof(bitmap_index), MAX_OBJ_BITMAPS, fp );
	cfread( ObjBitmapPtrs, sizeof(ushort), MAX_OBJ_BITMAPS, fp );

	cfread( &only_player_ship, sizeof(player_ship), 1, fp );

	cfread( &Num_cockpits, sizeof(int), 1, fp );
	cfread( cockpit_bitmap, sizeof(bitmap_index), N_COCKPIT_BITMAPS, fp );

	cfread( Sounds, sizeof(ubyte), MAX_SOUNDS, fp );
	cfread( AltSounds, sizeof(ubyte), MAX_SOUNDS, fp );

	cfread( &Num_total_object_types, sizeof(int), 1, fp );
	cfread( ObjType, sizeof(byte), MAX_OBJTYPE, fp );
	cfread( ObjId, sizeof(byte), MAX_OBJTYPE, fp );
	cfread( ObjStrength, sizeof(fix), MAX_OBJTYPE, fp );

	cfread( &First_multi_bitmap_num, sizeof(int), 1, fp );

	cfread( &N_controlcen_guns, sizeof(int), 1, fp );
	cfread( controlcen_gun_points, sizeof(vms_vector), MAX_CONTROLCEN_GUNS, fp );
	cfread( controlcen_gun_dirs, sizeof(vms_vector), MAX_CONTROLCEN_GUNS, fp );
	cfread( &exit_modelnum, sizeof(int), 1, fp );
	cfread( &destroyed_exit_modelnum, sizeof(int), 1, fp );
}


