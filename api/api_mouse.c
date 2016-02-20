/* Copyright 2014 Kjetil S. Matheussen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. */

#include "../common/includepython.h"

#include "../common/nsmtracker.h"
#include "../common/placement_proc.h"
#include "../common/vector_proc.h"
#include "../common/list_proc.h"
#include "../common/undo.h"
#include "../common/undo_reltemposlider_proc.h"
#include "../common/gfx_wblocks_reltempo_proc.h"
#include "../common/time_proc.h"
#include "../common/trackreallines2_proc.h"
#include "../common/common_proc.h"
#include "../common/temponodes_proc.h"
#include "../common/undo_temponodes_proc.h"
#include "../common/realline_calc_proc.h"
#include "../common/notes_proc.h"
#include "../common/pitches_proc.h"
#include "../common/undo_notes_proc.h"
#include "../common/gfx_subtrack_proc.h"
#include "../common/velocities_proc.h"
#include "../common/visual_proc.h"
#include "../common/undo_fxs_proc.h"
#include "../common/fxlines_proc.h"
#include "../common/instruments_proc.h"
#include "../common/wblocks_proc.h"
#include "../common/OS_Player_proc.h"
#include "../common/player_proc.h"
#include "../common/undo_trackheader_proc.h"
#include "../common/patch_proc.h"
#include "../common/nodelines_proc.h"
#include "../common/wtracks_proc.h"
#include "../common/undo_blocks_proc.h"

#include "../OpenGL/Render_proc.h"

#include "api_common_proc.h"

#include "radium_proc.h"


extern struct Root *root;

extern volatile float g_scroll_pos;


extern const struct ListHeader3 *current_node;
extern const struct ListHeader3 *indicator_node;
extern int indicator_velocity_num;
extern int indicator_pitch_num;


// various
///////////////////////////////////////////////////

static void setCurrentNode(struct ListHeader3 *new_current_node){
  if (current_node != new_current_node){
    current_node = new_current_node;
    root->song->tracker_windows->must_redraw_editor = true;
    //printf("current node dirty\n");
  }
}

void cancelCurrentNode(void){
  setCurrentNode(NULL);
}

static void setIndicatorNode(const struct ListHeader3 *new_indicator_node){
  if (indicator_node != new_indicator_node){
    indicator_node = new_indicator_node;
    root->song->tracker_windows->must_redraw_editor = true;
    //printf("indicator node dirty\n");
  }
}

void cancelIndicatorNode(void){
  setIndicatorNode(NULL);
  indicator_velocity_num = -1;
  indicator_pitch_num = -1;
}


float getHalfOfNodeWidth(void){
  return root->song->tracker_windows->fontheight / 1.5; // if changing 1.5 here, also change 1.5 in Render.cpp
}

static float get_scroll_pos(void){
  return safe_volatile_float_read(&g_scroll_pos);
}

static float get_mouse_realline_y1(const struct Tracker_Windows *window, int realline){
  //printf("fontheight: %f\n",(float)window->fontheight);
  //printf("wblock->t.y1: %f. scroll_pos: %f\n",(float)window->wblock->t.y1,(float)scroll_pos);
  return window->fontheight*realline - get_scroll_pos() + window->wblock->t.y1;
}

static float get_mouse_realline_y2(const struct Tracker_Windows *window, int realline){
  return window->fontheight*(realline+1) - get_scroll_pos() + window->wblock->t.y1;
}

float getTopVisibleY(int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if(wblock==NULL)
    return 0;

  return get_mouse_realline_y1(window, R_MAX(0, wblock->top_realline));
}

float getBotVisibleY(int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if(wblock==NULL)
    return 0;

  return get_mouse_realline_y2(window, R_MIN(wblock->num_reallines-1, wblock->bot_realline));
}

void setMouseTrack(int tracknum){
  struct Tracker_Windows *window = root->song->tracker_windows;
  struct WBlocks *wblock = window->wblock;

  if(tracknum != wblock->mouse_track){
    wblock->mouse_track = tracknum;
    window->must_redraw_editor = true;
  }
}

void setNoMouseTrack(void){
  setMouseTrack(NOTRACK);
}

void setMouseTrackToReltempo(void){
  setMouseTrack(TEMPONODETRACK);
}


// placement (block time)
///////////////////////////////////////////////////

float getPlaceFromY(float y, int blocknum, int windownum) {
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if (wblock==NULL)
    return 0.0;

  Place place;

  GetReallineAndPlaceFromY(window,
                           wblock,
                           y,
                           &place,
                           NULL,
                           NULL
                           );
  
  return GetFloatFromPlace(&place);
}


static double get_gridded_abs_y(struct Tracker_Windows *window, float abs_y){
  double grid = (double)root->grid_numerator / (double)root->grid_denominator;

  float abs_realline = abs_y / window->fontheight;
  
  double rounded = round(abs_realline / grid);
    
  return rounded * grid * window->fontheight;
}

float getPlaceInGridFromY(float y, int blocknum, int windownum) {
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if (wblock==NULL)
    return 0.0;

  // This is a hack, but it's a relatively clean one, and modifying GetReallineAndPlaceFromY is a quite major work (old code, hard to understand, it should be rewritten).
  float abs_y = (y-wblock->t.y1) + wblock->top_realline*window->fontheight;
  float gridded_abs_y = get_gridded_abs_y(window, abs_y);
  float gridded_y = gridded_abs_y - wblock->top_realline*window->fontheight + wblock->t.y1;
  
  Place place;
  
  GetReallineAndPlaceFromY(window,
                           wblock,
                           gridded_y,
                           &place,
                           NULL,
                           NULL
                           );
  
  return GetFloatFromPlace(&place);
}


static double get_next_gridded_abs_y(struct Tracker_Windows *window, float abs_y){
  double grid = (double)root->grid_numerator / (double)root->grid_denominator;

  float abs_realline = abs_y / window->fontheight;
  
  double rounded = round(abs_realline / grid) + 1.0;
    
  return rounded * grid * window->fontheight;
}

float getNextPlaceInGridFromY(float y, int blocknum, int windownum) {
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if (wblock==NULL)
    return 0.0;

  // This is a hack, but it's a relatively clean one, and modifying GetReallineAndPlaceFromY is a quite major work (old code, hard to understand, it should be rewritten).
  float abs_y = (y-wblock->t.y1) + wblock->top_realline*window->fontheight;
  float gridded_abs_y = get_next_gridded_abs_y(window, abs_y);
  float gridded_y = gridded_abs_y - wblock->top_realline*window->fontheight + wblock->t.y1;
  
  Place place;
  
  GetReallineAndPlaceFromY(window,
                           wblock,
                           gridded_y,
                           &place,
                           NULL,
                           NULL
                           );
  
  return GetFloatFromPlace(&place);
}




// reltempo
///////////////////////////////////////////////////

float getReltempoSliderX1(void){
  return root->song->tracker_windows->wblock->reltempo.x1;
}
float getReltempoSliderY1(void){
  return root->song->tracker_windows->wblock->reltempo.y1;
}
float getReltempoSliderX2(void){
  return root->song->tracker_windows->wblock->reltempo.x2;
}
float getReltempoSliderY2(void){
  return root->song->tracker_windows->wblock->reltempo.y2;
}

float getReltempo(int blocknum, int windownum){
  struct WBlocks *wblock = getWBlockFromNum(windownum, blocknum);
  if (wblock==NULL)
    return 0.0f;
  else
    return wblock->block->reltempo;
}

void undoReltempo(void){
  struct Tracker_Windows *window = root->song->tracker_windows;
  Undo_RelTempoSlider(window,window->wblock);
}

void setReltempo(float reltempo){
  struct Tracker_Windows *window = root->song->tracker_windows;
  struct WBlocks *wblock = window->wblock;
  wblock->block->reltempo=R_BOUNDARIES(
    MINBLOCKRELTIME,
    reltempo,
    MAXBLOCKRELTIME
  );

  //update_statusbar(window);
  //DrawBlockRelTempo(window,wblock);

  window->must_redraw = true;
}


float getMinReltempo(void){
  return MINBLOCKRELTIME;
}

float getMaxReltempo(void){
  return MAXBLOCKRELTIME;
}



// Set track patch
///////////////////////////////////////////////////
void setTrackPatch(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  PATCH_select_patch_for_track(window,wtrack,true);
}



// track panning on/off
///////////////////////////////////////////////////

float getTrackPanOnOffX1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->panonoff.x1;
}
float getTrackPanOnOffY1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->panonoff.y1;
}
float getTrackPanOnOffX2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->panonoff.x2;
}
float getTrackPanOnOffY2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->panonoff.y2;
}

bool getTrackPanOnOff(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->track->panonoff;
}

void undoTrackPanOnOff(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  Undo_TrackHeader(window, wblock->block, wtrack->track, wblock->curr_realline);
}

void setTrackPanOnOff(bool onoff, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  wtrack->track->panonoff = onoff;
        
  window->must_redraw = true;
}


// track volumening on/off
///////////////////////////////////////////////////

float getTrackVolumeOnOffX1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->volumeonoff.x1;
}
float getTrackVolumeOnOffY1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->volumeonoff.y1;
}
float getTrackVolumeOnOffX2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->volumeonoff.x2;
}
float getTrackVolumeOnOffY2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->volumeonoff.y2;
}

bool getTrackVolumeOnOff(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->track->volumeonoff;
}

void undoTrackVolumeOnOff(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  Undo_TrackHeader(window, wblock->block, wtrack->track, wblock->curr_realline);
}

void setTrackVolumeOnOff(bool onoff, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  wtrack->track->volumeonoff = onoff;
  
  window->must_redraw = true;
}


// track panning slider
///////////////////////////////////////////////////

float getTrackPanSliderX1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->pan.x1;
}
float getTrackPanSliderY1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->pan.y1;
}
float getTrackPanSliderX2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->pan.x2;
}
float getTrackPanSliderY2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->pan.y2;
}

float getTrackPan(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return scale(wtrack->track->pan, -MAXTRACKPAN, MAXTRACKPAN, -1.0, 1.0);
}

void undoTrackPan(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  Undo_TrackHeader(window, wblock->block, wtrack->track, wblock->curr_realline);
}

// void setTrackPan(float pan, int tracknum, int blocknum, int windownum)
// was already (more correctly) implemented in api_various.c



// track volumening slider
///////////////////////////////////////////////////

float getTrackVolumeSliderX1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->volume.x1;
}
float getTrackVolumeSliderY1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->volume.y1;
}
float getTrackVolumeSliderX2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->volume.x2;
}
float getTrackVolumeSliderY2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return wtrack->volume.y2;
}

float getTrackVolume(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0.0f;

  return scale(wtrack->track->volume, 0, MAXTRACKVOL, 0.0, 1.0);
}

void undoTrackVolume(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  Undo_TrackHeader(window, wblock->block, wtrack->track, wblock->curr_realline);
}

// void setTrackVolume(float volume, int tracknum, int blocknum, int windownum)
// was already (more correctly) implemented in api_various.c



// block positions
///////////////////////////////////////////////////

float getBlockHeaderY2(int blocknum, int windownum){
  struct WBlocks *wblock = getWBlockFromNum(windownum, blocknum);
  return wblock==NULL ? 0 : wblock->t.y1;
}


// tracks positions
///////////////////////////////////////////////////

float getTrackX1(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if(wblock==NULL)
    return 0;

  return WTRACK_getx1(window, wblock, tracknum);
}

float getTrackY1(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->y;
}

float getTrackX2(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if(wblock==NULL)
    return 0;

  return WTRACK_getx2(window, wblock, tracknum);
}

float getTrackY2(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->y2;
}

float getTrackPianorollX1(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->pianoroll_area.x;
}

float getTrackPianorollY1(int tracknum, int blocknum, int windownum){
  return getBlockHeaderY2(blocknum, windownum);
}

float getTrackPianorollX2(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->pianoroll_area.x2;
}

float getTrackPianorollY2(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->y2;
}

float getTrackNotesX1(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->notearea.x;
}

float getTrackNotesY1(int tracknum, int blocknum, int windownum){
  return getBlockHeaderY2(blocknum, windownum);
}

float getTrackNotesX2(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->notearea.x2;
}

float getTrackNotesY2(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->y2;
}

float getTrackFxX1(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->fxarea.x;
}

float getTrackFxY1(int tracknum, int blocknum, int windownum){
  return getBlockHeaderY2(blocknum, windownum);
}

float getTrackFxX2(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->fxarea.x2;
}

float getTrackFxY2(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0 : wtrack->y2;
}

int getLeftmostTrackNum(void){
  return LEFTMOSTTRACK;
}

int getRelTempoTrackNum(void){
  return TEMPONODETRACK;
}
int getTempoTrackNum(void){
  return TEMPOTRACK;
}
int getLPBTrackNum(void){
  return LPBTRACK;
}
int getSignatureTrackNum(void){
  return SIGNATURETRACK;
}
int getTempoColorTrackNum(void){
  return TEMPOCOLORTRACK;
}
int getLinenumTrackNum(void){
  return LINENUMBTRACK;
}


// temponodearea
//////////////////////////////////////////////////

int getTemponodeAreaX1(void){
  return root->song->tracker_windows->wblock->temponodearea.x;
}
int getTemponodeAreaY1(void){
  return root->song->tracker_windows->wblock->t.y1;
}
int getTemponodeAreaX2(void){
  return root->song->tracker_windows->wblock->temponodearea.x2;
}
int getTemponodeAreaY2(void){
  return root->song->tracker_windows->wblock->t.y2;
}

float getTemponodeMax(int blocknum, int windownum){
  struct WBlocks *wblock = getWBlockFromNum(windownum, blocknum);
  if (wblock==NULL)
    return 0.0f;
  else
    return wblock->reltempomax;
}

static const struct Node *get_temponode(int boxnum){
  return VECTOR_get(GetTempoNodes(root->song->tracker_windows, root->song->tracker_windows->wblock),
                    boxnum,
                    "temponode"
                    );
}

float getTemponodeX(int num){
  const struct Node *nodeline = get_temponode(num);
  return nodeline==NULL ? 0 : nodeline->x;
}

float getTemponodeY(int num){
  const struct Node *nodeline = get_temponode(num);
  return nodeline==NULL ? 0 : nodeline->y-get_scroll_pos();
}

float getTemponodeValue(int num, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if (wblock==NULL) {
    RWarning("getTemponodeValue: No block %d in window %d",blocknum,windownum);
    return 0.0;
  }

  struct Blocks *block = wblock->block;
  struct TempoNodes *temponode = ListFindElement3_num(&block->temponodes->l, num);
  if (temponode==NULL) {
    RWarning("No temponode %d in block %d%s",num,blocknum,blocknum==-1?" (i.e. current block)":"");
    return 0.0;
  }

  return temponode->reltempo;
}

void undoTemponodes(void){
  struct Tracker_Windows *window = root->song->tracker_windows;
  Undo_TempoNodes_CurrPos(window);
}

void setCurrentTemponode(int num, int blocknum){
  struct Blocks *block = blocknum==-1 ? root->song->tracker_windows->wblock->block : getBlockFromNum(blocknum);
  if (block==NULL) {
    RWarning("setCurrentTemponode: No block %d",blocknum);
    return;
  }
  
  struct TempoNodes *temponode = ListFindElement3_num(&block->temponodes->l, num);

  setCurrentNode(&temponode->l);
}

void setIndicatorTemponode(int num, int blocknum){
  struct Blocks *block = blocknum==-1 ? root->song->tracker_windows->wblock->block : getBlockFromNum(blocknum);
  if (block==NULL) {
    RWarning("setCurrentTemponode: No block %d",blocknum);
    return;
  }
  
  struct TempoNodes *temponode = ListFindElement3_num(&block->temponodes->l, num);

  setIndicatorNode(&temponode->l);
}

void setTemponode(int num, float value, float floatplace, int blocknum, int windownum){

  PlayStop();

  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if (wblock==NULL) {
    RWarning("setTemponode: No block %d in window %d",blocknum,windownum);
    return;
  }

  //printf("Set temponode. value: %f. Place: %f\n",value,floatplace);
  
  struct Blocks *block = wblock->block;

  struct TempoNodes *temponode;

  const vector_t *tempo_nodes = GetTempoNodes(window, wblock);
  
  if (num==0)
    temponode = block->temponodes; // don't want to set placement for the first node. It's always at top.
  
  else if (num==tempo_nodes->num_elements-1)
    temponode = ListLast3(&block->temponodes->l); // don't want to set placement for the last node. It's always at bottom.

  else if (num>=tempo_nodes->num_elements) {
    RWarning("No temponode %d in block %d%s",num,blocknum,blocknum==-1?" (i.e. current block)":"");
    return;
    
  } else if (floatplace < 0) {
    temponode = ListFindElement3_num(&block->temponodes->l, num);

  } else {
    Place place;
    Float2Placement(floatplace, &place);
    temponode = (struct TempoNodes *)ListMoveElement3_FromNum_ns(&block->temponodes, num, &place, NULL, NULL);
  }
  
  if ( (value+1) > wblock->reltempomax) {
    wblock->reltempomax = value+1;      
  } else if ( (value-1) < -wblock->reltempomax) {
    wblock->reltempomax = -1*(value -1);
  }

  temponode->reltempo = value;

  UpdateSTimes(wblock->block);    

  //printf("before: %f, now: %f\n",floatplace, GetfloatFromPlace(&temponode->l.p));

  window->must_redraw_editor = true;
}

int getNumTemponodes(int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if (wblock==NULL)
    return 0;
  
  const vector_t *tempo_nodes = GetTempoNodes(window, wblock);
  return tempo_nodes->num_elements;
}

void deleteTemponode(int num, int blocknum){
  PlayStop();

  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(-1, &window, blocknum);
  if (wblock==NULL)
    return;

  const vector_t *tempo_nodes = GetTempoNodes(window, wblock);

  if (num >= tempo_nodes->num_elements){
    RWarning("deleteTemponode: No temponode %d in block %d",num,blocknum);
    return;
  }

  if (num==0){
    wblock->block->temponodes->reltempo = 0.0f;
  } else if (num==tempo_nodes->num_elements-1) {
    struct TempoNodes *last = ListLast3(&wblock->block->temponodes->l);
    last->reltempo = 0.0f;
  } else {
    ListRemoveElement3_fromNum(&wblock->block->temponodes,num);
  }

  UpdateSTimes(wblock->block);    

  window->must_redraw_editor = true;
}

int createTemponode(float value, float floatplace, int blocknum, int windownum){
  PlayStop();

  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if (wblock==NULL) {
    RWarning("createTemponode: No block %d in window %d",blocknum,windownum);
    return -1;
  }

  struct Blocks *block = wblock->block;
  
  if (floatplace<=0.0f)
    return -1;

  if (floatplace>=block->num_lines)
    return -1;
  
  Place place;
  Float2Placement(floatplace, &place);

  if ( (value+1) > wblock->reltempomax) {
    wblock->reltempomax = value+1;      
  } else if ( (value-1) < -wblock->reltempomax) {
    wblock->reltempomax = -1*(value -1);
  }

  struct TempoNodes *temponode = AddTempoNode(window,wblock,&place,value);

  if (temponode==NULL)
    return -1;
  
  //GFX_SetChangeFloat(window,wblock,"Reltempo",RelTempo2RealRelTempo(Gfx2RelTempo(wblock,dx)));
  //UpdateSTimes(wblock->block);
  //GFX_DrawStatusBar(window,wblock);

  UpdateSTimes(block);

  window->must_redraw_editor = true;

  return ListFindElementPos3(&block->temponodes->l, &temponode->l);
}


// pianoroll
//////////////////////////////////////////////////

bool pianorollVisible(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(-1, blocknum, tracknum);

  if (wtrack==NULL)
    return false;

  return wtrack->pianoroll_on;
}

int getPioanorollLowKey(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(-1, blocknum, tracknum);

  if (wtrack==NULL)
    return 0;

  return wtrack->pianoroll_lowkey;
}

int getPioanorollHighKey(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(-1, blocknum, tracknum);

  if (wtrack==NULL)
    return 127;

  return wtrack->pianoroll_highkey;
}


enum PianoNoteWhatToGet {
  PIANONOTE_INFO_X1,
  PIANONOTE_INFO_Y1,
  PIANONOTE_INFO_X2,
  PIANONOTE_INFO_Y2,
  PIANONOTE_INFO_VALUE
};

static float get_pianonote_info(enum PianoNoteWhatToGet what_to_get, int pianonotenum, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return 0;

  if (what_to_get==PIANONOTE_INFO_VALUE){
    if (pianonotenum==0)
      return note->note;
    
    //if (pianonotenum==getNumPianonotes(notenum,tracknum,blocknum,windownum))
    //  return note->end_pitch;
    
    struct Pitches *pitch = ListFindElement3_num_r0(&note->pitches->l, pianonotenum-1);
    if (pitch==NULL){
      RWarning("There is no pianonote %d in note %d in track %d in block %d",pianonotenum,notenum,tracknum,blocknum);
      return 0;
    }
    return pitch->note;
  }
  
  const struct NodeLine *nodeline = GetPianorollNodeLines(window, wblock, wtrack, note);

  int num = -1;

  while(nodeline != NULL){
    if (nodeline->is_node)
      num++;

    if (num==pianonotenum)
      break;
    
    nodeline=nodeline->next;
  }

  if (nodeline==NULL) {
    RWarning("There is no pianonote %d in note %d in track %d in block %d",pianonotenum,notenum,tracknum,blocknum);  
    return 0;
  }

  NodelineBox box = GetPianoNoteBox(wtrack, nodeline);
  
  switch (what_to_get){
  case PIANONOTE_INFO_X1:
    return box.x1;
  case PIANONOTE_INFO_Y1:
    return wblock->t.y1 + box.y1 - get_scroll_pos();
  case PIANONOTE_INFO_X2:
    return box.x2;
  case PIANONOTE_INFO_Y2:
    return wblock->t.y1 + box.y2 - get_scroll_pos();
  default:
    RWarning("Internal error");
    return 0;
  }
}


float getPianonoteX1(int num, int notenum, int tracknum, int blocknum, int windownum){
  return get_pianonote_info(PIANONOTE_INFO_X1, num, notenum, tracknum, blocknum, windownum);
}

float getPianonoteY1(int num, int notenum, int tracknum, int blocknum, int windownum){
  return get_pianonote_info(PIANONOTE_INFO_Y1, num, notenum, tracknum, blocknum, windownum);
}

float getPianonoteX2(int num, int notenum, int tracknum, int blocknum, int windownum){
  return get_pianonote_info(PIANONOTE_INFO_X2, num, notenum, tracknum, blocknum, windownum);
}

float getPianonoteY2(int num, int notenum, int tracknum, int blocknum, int windownum){
  return get_pianonote_info(PIANONOTE_INFO_Y2, num, notenum, tracknum, blocknum, windownum);
}

float getPianonoteValue(int num, int notenum, int tracknum, int blocknum, int windownum){
  return get_pianonote_info(PIANONOTE_INFO_VALUE, num, notenum, tracknum, blocknum, windownum);
}

int getNumPianonotes(int notenum, int tracknum, int blocknum, int windownum){
  struct Notes *note=getNoteFromNum(windownum,blocknum,tracknum,notenum);
  if (note==NULL)
    return 0;

  return 1 + ListFindNumElements3(&note->pitches->l);
}

static void MOVE_PLACE(Place *place, float diff){
  float oldplace = GetfloatFromPlace(place);
  Float2Placement(oldplace+diff, place);
}

static void setPianoNoteValues(float value, int pianonotenum, struct Notes *note){

  // 1. Find delta
  //
  float old_value;
  
  if (pianonotenum==0) {
    
    old_value = note->note;
    
  } else {
  
    struct Pitches *pitch = ListFindElement3_num_r0(&note->pitches->l, pianonotenum-1);
    if (pitch==NULL){
      RWarning("There is no pianonote %d",pianonotenum);
      return;
    }

    old_value = pitch->note;
  }

  
  float delta = value - old_value;

  // 2. Apply
  //
  //note->note + note->note - value
    
  note->note = R_BOUNDARIES(1, note->note + delta, 127);

  if (note->pitch_end > 0)
    note->pitch_end = R_BOUNDARIES(1, note->pitch_end + delta, 127);

  struct Pitches *pitch = note->pitches;
  while(pitch != NULL){
    pitch->note = R_BOUNDARIES(1, pitch->note + delta, 127);
    pitch = NextPitch(pitch);
  }
  
}

static Place getPianoNotePlace(int pianonotenum, struct Notes *note){
  if (pianonotenum==0)
    return note->l.p;
  
  struct Pitches *pitch = ListFindElement3_num_r0(&note->pitches->l, pianonotenum-1);
  if (pitch==NULL){
    RWarning("There is no pianonote %d",pianonotenum);
    return note->l.p;
  }

  return pitch->l.p;
}


static int getPitchNumFromPianonoteNum(int pianonotenum, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 0;

  struct Tracks *track = wtrack->track;

  int ret = 0;
  int notenum_so_far = 0;
  
  struct Notes *note = track->notes;

  while(note!=NULL){

    if (notenum_so_far==notenum) {
      
      if (pianonotenum > 0) {
        struct Pitches *pitch = ListFindElement3_num_r0(&note->pitches->l, pianonotenum-1);
        if (pitch==NULL){
          RWarning("There is no pianonote %d in note %d in track %d in block %d",pianonotenum,notenum,tracknum,blocknum);
          return 0;
        }
      }
      
      return ret + pianonotenum;
    }
      
    ret++;

    ret += ListFindNumElements3(&note->pitches->l);

    if (note->pitch_end > 0)
      ret++;

    notenum_so_far++;
    
    note = NextNote(note);
  }

  RWarning("There is no pianonote %d in note %d in track %d in block %d",pianonotenum,notenum,tracknum,blocknum);
  return 0;
}

static void moveNote(struct Blocks *block, struct Tracks *track, struct Notes *note, float diff){
  float old_start = GetfloatFromPlace(&note->l.p);

  if (old_start + diff < 0)
    diff = -old_start;

  //printf("new_start 1: %f\n",old_start+diff);

  float old_end   = GetfloatFromPlace(&note->end);

  Place lastplace;
  PlaceSetLastPos(block, &lastplace);
  float lastplacefloat = GetfloatFromPlace(&lastplace);

  if (old_end + diff > lastplacefloat)
    diff = lastplacefloat - old_end;

  //printf("new_start 2: %f\n",old_start+diff);

  PLAYER_lock();{
    ListRemoveElement3(&track->notes, &note->l);
    
    //Float2Placement(new_start, &note->l.p);
    //Float2Placement(new_end, &note->end);
    MOVE_PLACE(&note->l.p, diff);
    MOVE_PLACE(&note->end, diff);
    
    struct Velocities *velocity = note->velocities;
    while(velocity != NULL){
      MOVE_PLACE(&velocity->l.p, diff);
      velocity = NextVelocity(velocity);
    }

    struct Pitches *pitch = note->pitches;
    while(pitch != NULL){
      MOVE_PLACE(&pitch->l.p, diff);
      pitch = NextPitch(pitch);
    }
    
    ListAddElement3(&track->notes, &note->l);

    NOTE_validate(block, track, note);
  }PLAYER_unlock();
}

int movePianonote(int pianonotenum, float value, float floatplace, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return notenum;

  struct Blocks *block = wblock->block;
  
  setPianoNoteValues(value, pianonotenum, note);

  window->must_redraw_editor = true;

  //printf("floatplace: %f\n",floatplace);
  if (floatplace < 0)
    return notenum;

  struct Tracks *track = wtrack->track;

  Place old_place = getPianoNotePlace(pianonotenum, note);
  float old_floatplace = GetfloatFromPlace(&old_place);
  float diff      = floatplace - old_floatplace;

  moveNote(block, track, note, diff);

  return ListPosition3(&track->notes->l, &note->l);
}

static int setPitch2(int num, float value, float floatplace, int tracknum, int blocknum, int windownum, bool replace_note_ends);
  
int movePianonoteStart(int pianonotenum, float value, float floatplace, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return notenum;

  struct Blocks *block = wblock->block;
  struct Tracks *track = wtrack->track;

  if (note->pitches!=NULL) {
    setPitch2(getPitchNumFromPianonoteNum(pianonotenum, notenum, tracknum, blocknum, windownum),
              value, floatplace,
              tracknum, blocknum, windownum,
              false
             );
    return ListPosition3(&track->notes->l, &note->l);
  }

  note->note = R_BOUNDARIES(1, value, 127);
    
  window->must_redraw_editor = true;
    
  if (floatplace < 0)
    return notenum;

  const float mindiff = 0.001;
    
  float lastplacefloat = GetfloatFromPlace(&note->end);
  if (floatplace+mindiff >= lastplacefloat)
    floatplace = lastplacefloat - mindiff;

  if (note->velocities != NULL) {
    float firstvelplace = GetfloatFromPlace(&note->velocities->l.p);
    if (floatplace+mindiff >= firstvelplace)
      floatplace = firstvelplace - mindiff;
  }

  // (there are no pitches here)
    
  PLAYER_lock();{
    ListRemoveElement3(&track->notes, &note->l);
    
    Float2Placement(floatplace, &note->l.p);
    
    ListAddElement3(&track->notes, &note->l);

    NOTE_validate(block, track, note);
  }PLAYER_unlock();


  return ListPosition3(&track->notes->l, &note->l);
}

static void MoveEndNote(struct Blocks *block, struct Tracks *track, struct Notes *note, Place *place, bool last_legal_may_be_next_note);
static int getPitchLogtype(int pitchnum, struct Tracks *track);
static void setPitchLogtype(bool is_holding, int pitchnum, struct Tracks *track);
  
bool isPianonoteLogtypeHolding(int pianonotenum, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return notenum;

  struct Tracks *track = wtrack->track;

  int pitchnum = getPitchNumFromPianonoteNum(pianonotenum, notenum, tracknum, blocknum, windownum);
  return getPitchLogtype(pitchnum, track) == LOGTYPE_HOLD;
}

void setPianonoteLogtypeHolding(bool is_holding, int pianonotenum, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  struct Tracks *track = wtrack->track;

  window->must_redraw_editor=true;
  
  int pitchnum = getPitchNumFromPianonoteNum(pianonotenum, notenum, tracknum, blocknum, windownum);
  setPitchLogtype(is_holding, pitchnum, track);
}
  
int movePianonoteEnd(int pianonotenum, float value, float floatplace, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return notenum;

  struct Blocks *block = wblock->block;
  struct Tracks *track = wtrack->track;

  if (note->pitches != NULL) {
    
    int pitchnum = getPitchNumFromPianonoteNum(pianonotenum, notenum, tracknum, blocknum, windownum);
    int logtype  = getPitchLogtype(pitchnum, track);    
             
    // 1. Change pitch value
    setPitch2(logtype==LOGTYPE_HOLD ? pitchnum : pitchnum + 1,
              value, -1,
              tracknum, blocknum, windownum,
              false
              );

    // 2. Change place of the next pianonote
    setPitch2(pitchnum+1,
              -1, floatplace,
              tracknum, blocknum, windownum,
              false
              );
    
  } else {
  
  
    window->must_redraw_editor=true;
    
    if(note->pitch_end > 0 || note->pitches!=NULL)
      note->pitch_end = R_BOUNDARIES(1, value, 127);
    else
      note->note = R_BOUNDARIES(1, value, 127);
  
    if (floatplace < 0)
      return notenum;

    const float mindiff = 0.001;
  
    float firstplacefloat = GetfloatFromPlace(&note->l.p);
    if (floatplace-mindiff <= firstplacefloat)
      floatplace = firstplacefloat + mindiff;

    if (note->velocities != NULL) {
      float lastvelplace = GetfloatFromPlace(ListLastPlace3(&note->velocities->l));
      if (floatplace-mindiff <= lastvelplace)
        floatplace = lastvelplace + mindiff;
    }

    // (there are no pitches here)
    
    PLAYER_lock();{
      Float2Placement(floatplace, &note->end);
      NOTE_validate(block, track, note);
    }PLAYER_unlock();

  }

  return notenum;
}

int createPianonote(float value, float floatplace, float endfloatplace, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return -1;

  struct Blocks *block = wblock->block;
  struct Tracks *track = wtrack->track;

  value = R_BOUNDARIES(1,value,127);

  Place lastplace;
  PlaceSetLastPos(block, &lastplace);
  float lastfloatplace = GetFloatFromPlace(&lastplace);
  
  if (floatplace < 0)
    floatplace = 0;

  if (endfloatplace > lastfloatplace)
    endfloatplace = lastfloatplace;

  if (floatplace >= endfloatplace) {
    //RWarning("Illegal parameters for createPianonote. start: %f, end: %f",floatplace, endfloatplace);
    return -1;
  }

  Place startplace;
  Float2Placement(floatplace, &startplace);

  Place endplace;
  Float2Placement(endfloatplace, &endplace);

  Undo_Notes(window,block,track,window->wblock->curr_realline);

  struct Notes *note = InsertNote(wblock, wtrack, &startplace, &endplace, value, NOTE_get_velocity(track), true);
  
  window->must_redraw_editor = true;

  return ListPosition3(&track->notes->l, &note->l);
}

void deletePianonote(int pianonotenum, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  if (pianonotenum==0) {
    window->must_redraw_editor=true;

    PLAYER_lock();{
      ListRemoveElement3(&wtrack->track->notes, &note->l);
    }PLAYER_unlock();
    
    return;
  }

  int pitchnum = getPitchNumFromPianonoteNum(pianonotenum, notenum, tracknum,  blocknum, windownum);

  deletePitch(pitchnum, tracknum, blocknum);      
}


extern struct CurrentPianoNote current_piano_note;
  
void setCurrentPianonote(int num, int notenum, int tracknum){
  if (
      current_piano_note.tracknum != tracknum ||
      current_piano_note.notenum != notenum || 
      current_piano_note.pianonotenum != num
      )
    {  
      current_piano_note.tracknum = tracknum;
      current_piano_note.notenum = notenum;
      current_piano_note.pianonotenum = num;
      root->song->tracker_windows->must_redraw_editor = true;
    }
}

void cancelCurrentPianonote(void){
  setCurrentPianonote(-1, -1, -1);
}

static int addPitch(struct Tracker_Windows *window, struct WBlocks *wblock, struct WTracks *wtrack, struct Notes *note, Place *place, float value);
  
void addPianonotePitch(float value, float floatplace, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;


  if (note->pitch_end == 0) {
    window->must_redraw_editor = true;
    note->pitch_end = note->note;
  }

  Place place;
  Float2Placement(floatplace, &place);

  addPitch(window, wblock, wtrack, note, &place, value);
}


// pitches
//////////////////////////////////////////////////

static int getPitchNum(struct Tracks *track, struct Notes *note, struct Pitches *pitch, bool is_end_pitch){
  int num = 0;
  struct Notes *note2 = track->notes;

  while(note2!=NULL){

    if (note==note2 && pitch==NULL && is_end_pitch==false)
      return num;

    num++;
    
    struct Pitches *pitch2 = note2->pitches;
    while(pitch2!=NULL){
      if (note2==note && pitch==pitch2)
        return num;

      num++;

      pitch2 = NextPitch(pitch2);
    }

    if (note2->pitch_end > 0){
      if (note==note2 && pitch==NULL && is_end_pitch==true)
        return num;
      
      num++;
    }

    if (note==note2) {
      RWarning("getPitchNum: Could not find pitch in note.");
      return 0;
    }

    note2 = NextNote(note2);
  }

  RWarning("getPitchNum: Could not find it");
  return 0;
}

int getNumPitches(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(-1, blocknum, tracknum);

  if (wtrack==NULL)
    return 0;

  struct Tracks *track = wtrack->track;
  
  int num = 0;
  struct Notes *notes = track->notes;
  
  while(notes!=NULL){

    num++;
    
    struct Pitches *pitches = notes->pitches;
    while(pitches!=NULL){
      num++;
      pitches = NextPitch(pitches);
    }

    if (notes->pitch_end > 0)
      num++;
    
    notes = NextNote(notes);
  }

  return num;
}

void deletePitch(int pitchnum, int tracknum, int blocknum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(-1, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  struct Blocks *block = wblock->block;
  struct Tracks *track = wtrack->track;
  
  int num = 0;
  struct Notes *notes = track->notes;
  
  while(notes!=NULL){

    if (pitchnum==num) {
      PLAYER_lock();{
        RemoveNote(block, track, notes);
      }PLAYER_unlock();
      goto gotit;
    }
    
    num++;
    
    struct Pitches *pitches = notes->pitches;
    while(pitches!=NULL){
      if (pitchnum==num){
        PLAYER_lock();{
          ListRemoveElement3(&notes->pitches,&pitches->l);
          NOTE_validate(block, track, notes);
        }PLAYER_unlock();
        goto gotit;
      }
      
      num++;
      pitches = NextPitch(pitches);
    }

    if (notes->pitch_end > 0) {
      if (pitchnum==num){
        struct Pitches *pitch = ListLast3(&notes->pitches->l);
        if (pitch!=NULL)
          notes->pitch_end = pitch->note;
        else
          notes->pitch_end = 0;
        goto gotit;
      }
      num++;
    }
    
    notes = NextNote(notes);
  }

  RWarning("no pitch %d in track %d in block %d\n",pitchnum,tracknum,blocknum);
  return;
  
 gotit:
  window->must_redraw_editor = true;
}



static bool getPitch(int pitchnum, struct Pitches **pitch, struct Notes **note, bool *is_end_pitch, struct Tracks *track){
  int num = 0;
  struct Notes *notes = track->notes;
  
  *is_end_pitch = false;
  
  while(notes!=NULL){

    if(num==pitchnum) {
      *note = notes;
      *pitch = NULL;
      return true;
    }

    num++;
    
    struct Pitches *pitches = notes->pitches;
    while(pitches!=NULL){
      if(num==pitchnum) {
        *note = notes;
        *pitch = pitches;
        return true;
      }

      num++;
      pitches = NextPitch(pitches);
    }

    if (notes->pitch_end > 0) {
      if(num==pitchnum) {
        *note = notes;
        *is_end_pitch = true;
        *pitch = NULL;
        return true;
      }
      num++;
    }
    
    notes = NextNote(notes);
  }

  RWarning("Pitch #%d in track #%d does not exist",pitchnum,track->l.num);
  return false;
}

static int getPitchLogtype(int pitchnum, struct Tracks *track){
  bool is_end_pitch = false;
  struct Notes *note = NULL;
  struct Pitches *pitch = NULL;
  getPitch(pitchnum, &pitch, &note, &is_end_pitch, track);

  if (is_end_pitch)
    return LOGTYPE_IRRELEVANT;
  if (pitch==NULL)
    return note->pitch_first_logtype;
  else
    return pitch->logtype;
}

static void setPitchLogtype(bool is_holding, int pitchnum, struct Tracks *track){
  bool is_end_pitch = false;
  struct Notes *note = NULL;
  struct Pitches *pitch = NULL;

  getPitch(pitchnum, &pitch, &note, &is_end_pitch, track);

  int logtype = is_holding ? LOGTYPE_HOLD : LOGTYPE_LINEAR;
  
  if (is_end_pitch) {
    RError("Can not set hold type of end pitch. pitchnum: %d, tracknum: %d",pitchnum,track->l.num);
    return;
  }
      
  if (pitch==NULL)
    note->pitch_first_logtype = logtype;
  else
    pitch->logtype = logtype;
}


void setPitchLogtypeHolding(bool is_holding, int pitchnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);

  if (wtrack==NULL)
    return;

  setPitchLogtype(is_holding, pitchnum, wtrack->track);
}



static int getReallineForPitch(const struct WBlocks *wblock, struct Pitches *pitch, struct Notes *note, bool is_end_pitch){
  if( pitch!=NULL)
    return FindRealLineFor(wblock,pitch->Tline,&pitch->l.p);
  else if (is_end_pitch)
    return find_realline_for_end_pitch(wblock, &note->end);
  else
    return FindRealLineFor(wblock,note->Tline,&note->l.p);
}

enum PitchInfoWhatToGet {
  PITCH_INFO_Y1,
  PITCH_INFO_Y2,
  PITCH_INFO_VALUE,
};

static float getPitchInfo(enum PitchInfoWhatToGet what_to_get, int pitchnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);

  if (wtrack==NULL)
    return 0;

  struct Notes *note;
  struct Pitches *pitch;
  bool is_end_pitch;
  
  if (getPitch(pitchnum, &pitch, &note, &is_end_pitch, wtrack->track)==false)
    return 0;
  
  switch (what_to_get){
  case PITCH_INFO_Y1:
    return get_mouse_realline_y1(window, getReallineForPitch(wblock, pitch, note, is_end_pitch));
  case PITCH_INFO_Y2:
    return get_mouse_realline_y2(window, getReallineForPitch(wblock, pitch, note, is_end_pitch));
  case PITCH_INFO_VALUE:
    {
      if (pitch!=NULL)
        return pitch->note;
      else if (is_end_pitch)
        return note->pitch_end;
      else
        return note->note;
    }
  }

  RWarning("internal error (getPitchInfo)\n");
  return 0;
}

float getPitchY1(int pitchnum, int tracknum, int blocknum, int windownum){
  return getPitchInfo(PITCH_INFO_Y1, pitchnum, tracknum, blocknum, windownum);
}

float getPitchY2(int pitchnum, int tracknum, int blocknum, int windownum){
  return getPitchInfo(PITCH_INFO_Y2, pitchnum, tracknum, blocknum, windownum);
}

float getPitchX1(int pitchnum, int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0.0f : wtrack->notearea.x;
}

float getPitchX2(int pitchnum, int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  return wtrack==NULL ? 0.0f : wtrack->notearea.x2;
}

static struct Node *get_pitchnodeline(int pitchnum, int tracknum, int blocknum, int windownum, bool *is_end_pitch){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return NULL;

  struct Notes *note;
  struct Pitches *pitch;  
  
  if (getPitch(pitchnum, &pitch, &note, is_end_pitch, wtrack->track)==false)
    return NULL;

  int note_pitchnum;

  if (pitch==NULL) {
    if (*is_end_pitch) {
      note_pitchnum = note->pitches==NULL ? 1 : ListFindNumElements3(&note->pitches->l) + 1;
    } else {
      note_pitchnum = 0;
    }
  } else
    note_pitchnum = ListPosition3(&note->pitches->l, &pitch->l) + 1;

  const vector_t *nodes = GetPitchNodes(window, wblock, wtrack, note);

  return nodes->elements[note_pitchnum];
}


float getPitchX(int num,  int tracknum, int blocknum, int windownum){
  bool is_end_pitch;
  struct Node *nodeline = get_pitchnodeline(num, tracknum, blocknum, windownum, &is_end_pitch);
  if (nodeline==NULL)
    return 0;

  return nodeline->x;
}

float getPitchY(int num, int tracknum, int blocknum, int windownum){
  bool is_end_pitch;
  struct Node *nodeline = get_pitchnodeline(num, tracknum, blocknum, windownum, &is_end_pitch);
  if (nodeline==NULL)
    return 0;

  if (is_end_pitch)
    return nodeline->y-get_scroll_pos();
  else
    return nodeline->y-get_scroll_pos();
}


float getPitchValue(int pitchnum, int tracknum, int blocknum, int windownum){
  return getPitchInfo(PITCH_INFO_VALUE, pitchnum, tracknum, blocknum, windownum);
}

void setCurrentPitch(int num, int tracknum, int blocknum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(-1, &window, blocknum, &wblock, tracknum);
  if(wtrack==NULL)
    return;

  struct Notes *note;
  struct Pitches *pitch;
  bool is_end_pitch;
  if (getPitch(num, &pitch, &note, &is_end_pitch, wtrack->track)==false)
    return;

  struct ListHeader3 *listHeader3 = pitch!=NULL ? &pitch->l : &note->l;
  setCurrentNode(listHeader3);
}

void setIndicatorPitch(int num, int tracknum, int blocknum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(-1, &window, blocknum, &wblock, tracknum);
  if(wtrack==NULL)
    return;

  struct Notes *note;
  struct Pitches *pitch;
  bool is_end_pitch;
  
  if (getPitch(num, &pitch, &note, &is_end_pitch, wtrack->track)==false)
    return;

  setIndicatorNode(&note->l);

  if (pitch==NULL) {
    if (is_end_pitch)
      indicator_pitch_num = 1 + ListFindNumElements3(&note->pitches->l);
    else
      indicator_pitch_num = 0;
  } else {
    int pitchnum = ListPosition3(&note->pitches->l, &pitch->l);
    indicator_pitch_num = pitchnum + 1;
  }
}

static Place *getPrevLegalNotePlace(struct Tracks *track, struct Notes *note){
  Place *end = PlaceGetFirstPos(); // small bug here, cant move pitch to first position, only almost to first position.

  struct Notes *prev = FindPrevNoteOnSameSubTrack(track, note);
  printf("prev: %p. next(prev): %p, note: %p, next(note): %p\n",prev,prev!=NULL?NextNote(prev):NULL,note,NextNote(note));
  
  if (prev != NULL) {
    end = &prev->l.p;
    if (prev->velocities!=NULL)
      end = ListLastPlace3(&prev->velocities->l);
    if (prev->pitches!=NULL)
      end = PlaceMax(end, ListLastPlace3(&prev->pitches->l));
  }
  
  return end;
}

static Place *getNextLegalNotePlace(struct Notes *note){
  Place *end = &note->end;

  if (note->velocities != NULL)
    end = PlaceMin(end, &note->velocities->l.p);

  if (note->pitches != NULL)
    end = PlaceMin(end, &note->pitches->l.p);

  return end;
}

static void MoveEndNote(struct Blocks *block, struct Tracks *track, struct Notes *note, Place *place, bool last_legal_may_be_next_note){
  Place firstLegal, lastLegal;

  if (last_legal_may_be_next_note && !ctrlPressed()){
    
    struct Notes *next = FindNextNoteOnSameSubtrack(note);
  
    if (next!=NULL)
      PlaceCopy(&lastLegal, &next->l.p);
    else
      PlaceSetLastPos(block, &lastLegal);

  }else{
    
    PlaceSetLastPos(block, &lastLegal);
    
  }

  
  Place *last_pitch = ListLastPlace3(&note->pitches->l);
  Place *last_velocity = ListLastPlace3(&note->velocities->l);
  Place *startPlace = &note->l.p;

  if (last_pitch==NULL)
    last_pitch = startPlace;
  if (last_velocity==NULL)
    last_velocity = startPlace;

  Place *firstLegalConst = PlaceMax(last_pitch, last_velocity);
  PlaceFromLimit(&firstLegal, firstLegalConst);

  PLAYER_lock();{
    note->end = *PlaceBetween(&firstLegal, place, &lastLegal);
    NOTE_validate(block, track, note);
  }PLAYER_unlock();
    
  R_ASSERT(PlaceLessOrEqual(&note->end, &lastLegal));
}

static int MoveNote(struct Blocks *block, struct Tracks *track, struct Notes *note, Place *place, bool replace_note_ends){
  Place old_place = note->l.p;

  if (!PlaceEqual(&old_place, place)) {

    printf("MoveNote. old: %f, new: %f\n", GetfloatFromPlace(&old_place), GetfloatFromPlace(place));
         
    if (PlaceLessThan(place, &old_place)) {
      Place *prev_legal = getPrevLegalNotePlace(track, note);
      printf("prev_legal: %f\n",GetfloatFromPlace(prev_legal));
      if (PlaceLessOrEqual(place, prev_legal))
        PlaceFromLimit(place, prev_legal);
    } else {
      Place *next_legal = getNextLegalNotePlace(note);
      if (PlaceGreaterOrEqual(place, next_legal))
        PlaceTilLimit(place, next_legal);
    }
    
    PLAYER_lock();{
      ListRemoveElement3(&track->notes, &note->l);
      note->l.p = *place;
      ListAddElement3(&track->notes, &note->l);
      if (replace_note_ends && !ctrlPressed())
        ReplaceNoteEnds(block, track, &old_place, place, note->subtrack);
      NOTE_validate(block, track, note);
    }PLAYER_unlock();

  }
  
  return ListPosition3(&track->notes->l, &note->l);
}

static int setPitch2(int num, float value, float floatplace, int tracknum, int blocknum, int windownum, bool replace_note_ends){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);

  if (wtrack==NULL)
    return num;

  struct Blocks *block = wblock->block;  
  struct Tracks *track = wtrack->track;

  float clamped_value = R_BOUNDARIES(1,value,127);

  struct Notes *note;
  struct Pitches *pitch;
  bool is_end_pitch;
  
  if (getPitch(num, &pitch, &note, &is_end_pitch, track)==false)
    return num;

  window->must_redraw_editor = true;

  if (pitch != NULL) {

    if (value > 0)
      pitch->note = clamped_value;

    if (floatplace >= 0.0f) {
      Place firstLegalPlace,lastLegalPlace;
      PlaceFromLimit(&firstLegalPlace, &note->l.p);
      PlaceTilLimit(&lastLegalPlace, &note->end);

      Place place;
      Float2Placement(floatplace, &place);

      PLAYER_lock();{
        ListMoveElement3_ns(&note->pitches, &pitch->l, &place, &firstLegalPlace, &lastLegalPlace);
        NOTE_validate(block, track, note);
      }PLAYER_unlock();
    }
                        
  } else if (is_end_pitch){

    if (value > 0)
      note->pitch_end = clamped_value;
    
    if (floatplace >= 0) {
      MoveEndNote(block, track, note, PlaceCreate2(floatplace), true);
      return getPitchNum(track, note, NULL, true);
    }
    
  } else {

    if (value > 0)
      note->note = clamped_value;

    if (floatplace >= 0) {
      MoveNote(block, track, note, PlaceCreate2(floatplace), replace_note_ends);
      return getPitchNum(track, note, NULL, false);
    }
  }

  return num;
}

int setPitch(int num, float value, float floatplace, int tracknum, int blocknum, int windownum){
  return setPitch2(num, value, floatplace, tracknum, blocknum, windownum, true);
}
  
static struct Notes *getNoteAtPlace(struct Tracks *track, Place *place){
  struct Notes *note = track->notes;

  while(note != NULL){
    if (PlaceIsBetween3(place, &note->l.p, &note->end))
      return note;
    else
      note = NextNote(note);
  }

  return NULL;
}

static int addNote3(struct Tracker_Windows *window, struct WBlocks *wblock, struct WTracks *wtrack, Place *place, float value){

  struct Notes *note = InsertNote(wblock, wtrack, place, NULL, value, NOTE_get_velocity(wtrack->track), false);

  window->must_redraw_editor = true;

  return getPitchNum(wtrack->track, note, NULL, false);
}

static int addPitch(struct Tracker_Windows *window, struct WBlocks *wblock, struct WTracks *wtrack, struct Notes *note, Place *place, float value){

  struct Pitches *pitch = AddPitch(window, wblock, wtrack, note, place, value);

  if(pitch==NULL)
    return -1;

  if (note->pitch_end==0)
    note->pitch_end = value;
  
  window->must_redraw_editor = true;

  return getPitchNum(wtrack->track, note, pitch, false);
}

int createPitch3(float value, int line, int counter, int dividor, int tracknum, int blocknum, int windownum){

  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);

  if (wtrack==NULL)
    return -1;

  Place place = {line, counter, dividor};
  
  struct Notes *note = getNoteAtPlace(wtrack->track, &place);

  value = R_BOUNDARIES(0,value,127);

  Undo_Notes(window,wblock->block,wtrack->track,window->wblock->curr_realline);

  int ret;
  
  if(note==NULL)
    ret = addNote3(window, wblock, wtrack, &place, value);
  else
    ret = addPitch(window, wblock, wtrack, note, &place, value);

  if (ret==-1)
    Undo_CancelLastUndo();

  printf("\n\n\n\n ***** NUM: %d\n",ret);
  return ret;
}

int createPitch(float value, float floatplace, int tracknum, int blocknum, int windownum){
  Place place;
  Float2Placement(floatplace, &place);
  return createPitch3(value, place.line, place.counter, place.dividor, tracknum, blocknum, windownum);
}

bool portamentoEnabled(int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return false;

  return note->pitch_end > 0;
}

void setNoteEndPitch(float value, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  note->pitch_end = value;
}

void enablePortamento(int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  if (note->pitch_end == 0) {
    window->must_redraw_editor = true;
    note->pitch_end = note->note;
  }
}

void disablePortamento(int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  PLAYER_lock();{
    note->pitches = NULL;
    note->pitch_end = 0;
  }PLAYER_unlock();
  
  window->must_redraw_editor = true;
}

// subtracks
///////////////////////////////////////////////////
int getNumSubtracks(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return 1;

  return GetNumSubtracks(wtrack->track);
}

static struct WTracks *getSubtrackWTrack(int subtracknum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return NULL;

  if (subtracknum>=GetNumSubtracks(wtrack->track)){
    RWarning("No subtrack %d in track %d in block %d (only %d subtracks in this track)\n", subtracknum, tracknum, blocknum, GetNumSubtracks(wtrack->track));
    return 0;
  }

  return wtrack;
}

float getSubtrackX1(int subtracknum, int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getSubtrackWTrack(subtracknum, tracknum, blocknum, windownum);
  if (wtrack==NULL)
    return 0.0f;
  else
    return GetXSubTrack1(wtrack,subtracknum);
}

float getSubtrackX2(int subtracknum, int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getSubtrackWTrack(subtracknum, tracknum, blocknum, windownum);
  if (wtrack==NULL)
    return 0.0f;
  else
    return GetXSubTrack2(wtrack,subtracknum);
}

// notes
//////////////////////////////////////////////////

void undoNotes(int tracknum, int blocknum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(-1, &window, blocknum, &wblock, tracknum);
  if(wtrack==NULL)
    return;
  Undo_Notes(window,window->wblock->block,wtrack->track,window->wblock->curr_realline);
}

float getNoteStart(int notenum, int tracknum, int blocknum, int windownum){
  struct Notes *note=getNoteFromNum(windownum,blocknum,tracknum,notenum);

  if(note==NULL)
    return -1.0f;

  return GetfloatFromPlace(&note->l.p);
}

float getNoteEnd(int notenum, int tracknum, int blocknum, int windownum){
  struct Notes *note=getNoteFromNum(windownum,blocknum,tracknum,notenum);

  if(note==NULL)
    return -1.0f;

  return GetfloatFromPlace(&note->end);
}

float getNoteValue(int notenum, int tracknum, int blocknum, int windownum){
  struct Notes *note=getNoteFromNum(windownum,blocknum,tracknum,notenum);

  if(note==NULL)
    return 64.0f;

  return note->note;
}

float getNoteEndPitch(int notenum, int tracknum, int blocknum, int windownum){
  struct Notes *note=getNoteFromNum(windownum,blocknum,tracknum,notenum);

  if(note==NULL)
    return 0;

  return note->pitch_end;
}

int getNoteSubtrack(int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return 0;

  return GetNoteSubtrack(wtrack->track, note);
}

void setNoMouseNote(int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if(wblock==NULL)
    return;
  
  if (wblock->mouse_note != NULL){
    wblock->mouse_note = NULL;
    window->must_redraw_editor = true;
    //printf("no mouse note dirty\n");
  }
}

void setMouseNote(int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;
  else if (wblock->mouse_note != note){
    wblock->mouse_note = note;
    window->must_redraw_editor = true;
    //printf("mouse note dirty\n");
  }
}


// velocities
//////////////////////////////////////////////////

static struct Node *get_velocitynode(int velocitynum, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return NULL;

  const vector_t *nodes = GetVelocityNodes(window, wblock, wtrack, note);
  if (velocitynum < 0 || velocitynum>=nodes->num_elements) {
    RWarning("There is no velocity %d in note %d in track %d in block %d",velocitynum, notenum, tracknum, blocknum);
    return NULL;
  }
  
  return nodes->elements[velocitynum];
}


float getVelocityX(int num, int notenum, int tracknum, int blocknum, int windownum){
  struct Node *node = get_velocitynode(num, notenum, tracknum, blocknum, windownum);
  return node==NULL ? 0 : node->x;
}

float getVelocityY(int num, int notenum, int tracknum, int blocknum, int windownum){
  struct Node *node = get_velocitynode(num, notenum, tracknum, blocknum, windownum);
  return node==NULL ? 0 : node->y-get_scroll_pos();
}


float getVelocityValue(int velocitynum, int notenum, int tracknum, int blocknum, int windownum){
  struct Node *node = get_velocitynode(velocitynum, notenum, tracknum, blocknum, windownum);
  if (node==NULL)
    return 0;

  struct Velocities *velocity = (struct Velocities*)node->element;
  return velocity->velocity / (float)MAX_VELOCITY;
}

int getNumVelocities(int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return 0;

  const vector_t *nodes = GetVelocityNodes(window, wblock, wtrack, note);

  return nodes->num_elements;
}

int createVelocity3(float value, int line, int counter, int dividor, int notenum, int tracknum, int blocknum, int windownum){

  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return -1;

  Place place = {line, counter, dividor};

  if (PlaceLessOrEqual(&place, &note->l.p)) {
    //if (notenum>0)
    //  RWarning("createVelocity: placement before note start for note #%d", notenum);
    return -1;
  }

  if (PlaceGreaterOrEqual(&place, &note->end)) {
    //RWarning("createVelocity: placement after note end for note #%d", notenum);
    return -1;
  }

  Undo_Notes(window,wblock->block,wtrack->track,window->wblock->curr_realline);

  int ret = AddVelocity(value*MAX_VELOCITY, &place, note);

  if (ret==-1){
    //RWarning("createVelocity: Can not create new velocity with the same position as another velocity");
    return -1;
  }


  window->must_redraw_editor = true;

  return ret+1;
}

int createVelocity(float value, float floatplace, int notenum, int tracknum, int blocknum, int windownum){
  Place place;
  Float2Placement(floatplace, &place);
  return createVelocity3(value, place.line, place.counter, place.dividor, notenum, tracknum, blocknum, windownum);
}

int setVelocity3(int velocitynum, float value, int line, int counter, int dividor, int notenum, int tracknum, int blocknum, int windownum){
  
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return notenum;

  struct Blocks *block = wblock->block;
  struct Tracks *track = wtrack->track;
  
  const vector_t *nodes = GetVelocityNodes(window, wblock, wtrack, note);
  if (velocitynum < 0 || velocitynum>=nodes->num_elements) {
    RWarning("There is no velocity %d in note %d in track %d in block %d",velocitynum, notenum, tracknum, blocknum);
    return notenum;
  }

  window->must_redraw_editor = true;

  //printf("velocitynum==%d. floatplace: %f\n",velocitynum,floatplace);

  Place place = {line, counter, dividor};

  if (velocitynum==0) {
    
    note->velocity = R_BOUNDARIES(0,value*MAX_VELOCITY,MAX_VELOCITY);
    if (line>=0)
      return MoveNote(block, track, note, &place, true);
    
  } else if (velocitynum==nodes->num_elements-1) {
    
    note->velocity_end = R_BOUNDARIES(0,value*MAX_VELOCITY,MAX_VELOCITY);
    if (line>=0)
      MoveEndNote(block, track, note, &place, true);

  } else {

    struct Velocities *velocity;

    if (line < 0 ) {
      velocity = ListFindElement3_num(&note->velocities->l, velocitynum-1);
    } else {
      Place firstLegalPlace,lastLegalPlace;
      PlaceFromLimit(&firstLegalPlace, &note->l.p);
      PlaceTilLimit(&lastLegalPlace, &note->end);
      
      PLAYER_lock();{
        velocity = (struct Velocities*)ListMoveElement3_FromNum_ns(&note->velocities, velocitynum-1, &place, &firstLegalPlace, &lastLegalPlace);
        NOTE_validate(block, track, note);
      }PLAYER_unlock();
    }
    
    velocity->velocity=R_BOUNDARIES(0,value*MAX_VELOCITY,MAX_VELOCITY);
  }

  return notenum;
}

int setVelocity(int velocitynum, float value, float floatplace, int notenum, int tracknum, int blocknum, int windownum){
  if (floatplace < 0)
    return setVelocity3(velocitynum, value, -1, 0, 1, notenum, tracknum, blocknum, windownum);

  Place place;
  Float2Placement(floatplace, &place);
  return setVelocity3(velocitynum, value, place.line, place.counter, place.dividor, notenum, tracknum, blocknum, windownum);
}

void deleteVelocity(int velocitynum, int notenum, int tracknum, int blocknum, int windownum){
 struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  struct Blocks *block=wblock->block;
  struct Tracks *track=wtrack->track;

  const vector_t *nodes = GetVelocityNodes(window, wblock, wtrack, note);
  if (velocitynum < 0 || velocitynum>=nodes->num_elements) {
    RWarning("There is no velocity %d in note %d in track %d in block %d",velocitynum, notenum, tracknum, blocknum);
    return;
  }

  bool is_first                      = velocitynum==0;
  bool is_last                       = velocitynum==nodes->num_elements-1;
  bool is_last_and_no_velocities     = is_last && nodes->num_elements==2;
  bool is_last_and_there_are_pitches = is_last && note->pitches!=NULL;

  if (is_first || is_last_and_no_velocities || is_last_and_there_are_pitches){
    PLAYER_lock();{
      RemoveNote(block, track, note);
    }PLAYER_unlock();

  } else if (velocitynum==nodes->num_elements-1) {
    struct Velocities *last = (struct Velocities*)ListLast3(&note->velocities->l);
    PLAYER_lock();{
      note->end = last->l.p;
      note->velocity_end = last->velocity;
      ListRemoveElement3(&note->velocities, &last->l);
      NOTE_validate(block, track, note);
    }PLAYER_unlock();

  } else {
    PLAYER_lock();{
      ListRemoveElement3_fromNum(&note->velocities, velocitynum-1);
      NOTE_validate(block, track, note);
    }PLAYER_unlock();
  }

  window->must_redraw_editor = true;
}

void setVelocityLogtypeHolding(bool is_holding, int velocitynum, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  //struct Blocks *block=wblock->block;
  //struct Tracks *track=wtrack->track;

  const vector_t *nodes = GetVelocityNodes(window, wblock, wtrack, note);
  if (velocitynum < 0 || velocitynum>=nodes->num_elements) {
    RWarning("There is no velocity %d in note %d in track %d in block %d",velocitynum, notenum, tracknum, blocknum);
    return;
  }
    
  bool is_first                      = velocitynum==0;
  bool is_last                       = velocitynum==nodes->num_elements-1;

  if (is_last) {
    GFX_Message(NULL, "Can not set logtype for last velocity (doesn't make any sense)");
    return;
  }

  int logtype = is_holding ? LOGTYPE_HOLD : LOGTYPE_LINEAR;

  if (is_first)
    note->velocity_first_logtype = logtype;
  else {
    struct Node *node = nodes->elements[velocitynum];
    struct Velocities *velocity = (struct Velocities*)node->element;

    velocity->logtype = logtype;
  }
}
  

void setCurrentVelocityNode(int velocitynum, int notenum, int tracknum, int blocknum, int windownum){
 struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  const vector_t *nodes = GetVelocityNodes(window, wblock, wtrack, note);
  if (velocitynum < 0 || velocitynum>=nodes->num_elements) {
    RWarning("There is no velocity %d in note %d in track %d in block %d",velocitynum, notenum, tracknum, blocknum);
    return;
  }

  struct Node *node = nodes->elements[velocitynum];
  struct Velocities *current = (struct Velocities*)node->element;

  setCurrentNode(&current->l);
}

void setIndicatorVelocityNode(int velocitynum, int notenum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct Notes *note = getNoteFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, notenum);
  if (note==NULL)
    return;

  const vector_t *nodes = GetVelocityNodes(window, wblock, wtrack, note);
  if (velocitynum < 0 || velocitynum>=nodes->num_elements) {
    RWarning("There is no velocity %d in note %d in track %d in block %d",velocitynum, notenum, tracknum, blocknum);
    return;
  }

  setIndicatorNode(&note->l);
  indicator_velocity_num = velocitynum;
}



// fxes
//////////////////////////////////////////////////

void addFX(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window=NULL;
  struct WTracks *wtrack;
  struct WBlocks *wblock;

  wtrack=getWTrackFromNumA(
                           windownum,
                           &window,
                           blocknum,
                           &wblock,
                           tracknum
                           );

  if(wtrack==NULL) return;

  //printf("x: %f, y: %f\n",tevent.x, tevent.y);
  
  AddFXNodeLineCurrPos(window, wblock, wtrack);
}

void addFXMousePos(int windownum){
  struct Tracker_Windows *window=getWindowFromNum(windownum);
  if(window==NULL)
    return;

  AddFXNodeLineCurrMousePos(window);
}

int createFx3(float value, int line, int counter, int dividor, const char* fx_name, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return -1;

  struct Tracks *track = wtrack->track;

  struct Patch *patch = track->patch;

  if(patch==NULL){
    RWarning("Track %d has no assigned instrument",tracknum);
    return -1;
  }

  int effect_num = 0;
  VECTOR_FOR_EACH(const char *name,patch->instrument->getFxNames(track)){
    if (!strcmp(name, fx_name))
      goto gotit;
    else
      effect_num++;
  }END_VECTOR_FOR_EACH;

  RWarning("createFx: No effect \"%s\" in this track.", fx_name);
  return -1;

 gotit:
  {
    struct FX *fx = patch->instrument->createFX(track, effect_num);

    if (fx==NULL)
      return -1;

    Place place = {line, counter, dividor};
    
    //printf("  1. p.line: %d, p.c: %d, p.d: %d\n",place.line,place.counter,place.dividor);
    
    AddFXNodeLineCustomFxAndPos(window, wblock, wtrack, fx, &place, value);

    //printf("  2. p.line: %d, p.c: %d, p.d: %d\n",place.line,place.counter,place.dividor);
        
    int num = 0;
    
    struct FXs *fxs = wtrack->track->fxs;
    while(fxs!=NULL){
      if (fxs->l.num == fx->num)
        return num;

      num++;
      fxs = NextFX(fxs);
    }

    RError("Internal error: Newly created FX not found, even though it was just created");
    
    return -1;
  }
}

int createFx(float value, float floatplace, const char* fx_name, int tracknum, int blocknum, int windownum){
  Place place;
  Float2Placement(floatplace, &place);

  return createFx3(value, place.line, place.counter, place.dividor, fx_name, tracknum, blocknum, windownum);
}

static struct Node *get_fxnode(int fxnodenum, int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fxs = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fxs==NULL)
    return NULL;
  
  const vector_t *nodes = GetFxNodes(window, wblock, wtrack, fxs);
  return VECTOR_get(nodes, fxnodenum, "fx node");
}


float getFxnodeX(int num, int fxnum, int tracknum, int blocknum, int windownum){
  struct Node *node = get_fxnode(num, fxnum, tracknum, blocknum, windownum);
  return node==NULL ? 0 : node->x;
}

float getFxnodeY(int num, int fxnum, int tracknum, int blocknum, int windownum){
  struct Node *node = get_fxnode(num, fxnum, tracknum, blocknum, windownum);
  return node==NULL ? 0 : node->y-get_scroll_pos();
}


float getFxnodeValue(int fxnodenum, int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fxs = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fxs==NULL)
    return 0.0f;

  const vector_t *nodes = GetFxNodes(window, wblock, wtrack, fxs);
  struct Node *node = VECTOR_get(nodes, fxnodenum, "fx node");
  if (node==NULL)
    return 0.0f;

  int max = fxs->fx->max;
  int min = fxs->fx->min;

  struct FXNodeLines *fxnodeline = (struct FXNodeLines*)node->element;

  return scale(fxnodeline->val, min, max, 0.0f, 1.0f);
}

const char* getFxName(int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fxs = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fxs==NULL)
    return NULL;

  return fxs->fx->name;
}
  
char* getFxString(int fxnodenum, int fxnum, int tracknum, int blocknum, int windownum){   
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fxs = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fxs==NULL)
    return NULL;

  const vector_t *nodes = GetFxNodes(window, wblock, wtrack, fxs);
  struct Node *node = VECTOR_get(nodes, fxnodenum, "fx node");
  if (node==NULL)
    return "<fxnode not found>";

  struct FXNodeLines *fxnodeline = (struct FXNodeLines *)node->element;

  float val = fxnodeline->val;

  // Turned out this was a lot of work. Fix later, hopefully.
  //return fx->getFXstring(fx, wtrack->track, val);

  // instead we just do this:
  struct FX *fx = fxs->fx;
  static char ret[512];

  if (wtrack->track->patch->instrument==get_MIDI_instrument())
    snprintf(ret, 511, "%s: %d", fx->name, (int)val);
  else
    snprintf(ret, 511, "%s: %.02f", fx->name, scale(val, fx->min, fx->max, 0.0f, 1.0f));

  return ret;
}

int getNumFxes(int tracknum, int blocknum, int windownum){
  struct WTracks *wtrack = getWTrackFromNum(windownum, blocknum, tracknum);
  if (wtrack == NULL)
    return 0;

  return ListFindNumElements1(&wtrack->track->fxs->l);
}

int getNumFxnodes(int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fxs = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fxs==NULL)
    return 0;

  return ListFindNumElements3(&fxs->fxnodelines->l);
}

float getFxMinValue(int fxnum, int tracknum, int blocknum, int windownum){
  return 0.0f;
#if 0
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fx = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fx==NULL)
    return 0;

  return fx->fx->min;
#endif
}

float getFxMaxValue(int fxnum, int tracknum, int blocknum, int windownum){
  return 1.0f;
#if 0
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fx = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fx==NULL)
    return 1;

  return fx->fx->max;
#endif
}

int createFxnode3(float value, int line, int counter, int dividor, int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fx = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fx==NULL)
    return -1;

  Place lastplace;
  PlaceSetLastPos(wblock->block, &lastplace);

  Place place = {line, counter, dividor};

  if (PlaceLessThan(&place, PlaceGetFirstPos())){
    RWarning("createFxnode: placement before top of block for fx #%d. (%s)", fxnum, PlaceToString(&place));
    place = *PlaceGetFirstPos();
  }

  if (PlaceGreaterThan(&place, &lastplace)) {    
    RWarning("createFxnode: placement after fx end for fx #%d (%s). num_lines: #%d", fxnum, PlaceToString(&place), wblock->block->num_lines);
    place = lastplace;
  }

  Undo_FXs(window, wblock->block, wtrack->track, wblock->curr_realline);

  int max = fx->fx->max;
  int min = fx->fx->min;

  int ret = AddFXNodeLine(
                          window,
                          wblock,
                          wtrack,
                          fx->l.num,
                          scale(value, 0,1, min, max),
                          &place
                          );

  if (ret==-1){
    //RWarning("createFxnode: Can not create new fx with the same position as another fx");
    Undo_CancelLastUndo();
    return -1;
  }

  window->must_redraw_editor = true;

  return ret;
}

int createFxnode(float value, float floatplace, int fxnum, int tracknum, int blocknum, int windownum){
  Place place;
  Float2Placement(floatplace, &place);

  return createFxnode3(value, place.line, place.counter, place.dividor, fxnum, tracknum, blocknum, windownum);
}
  
void setFxnode3(int fxnodenum, float value, int line, int counter, int dividor, int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fx = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fx==NULL)
    return;

  const vector_t *nodes = GetFxNodes(window, wblock, wtrack, fx);
  if (fxnodenum < 0 || fxnodenum>=nodes->num_elements) {
    RWarning("There is no fx node %d for fx %d in track %d in block %d",fxnodenum, fxnum, tracknum, blocknum);
    return;
  }

  struct Node *node = nodes->elements[fxnodenum];
  struct FXNodeLines *fxnodeline = (struct FXNodeLines *)node->element;
  
  if (line >= 0){
    Place place = {line, counter, dividor};
    Place *last_pos = PlaceGetLastPos(wblock->block);
    
    PLAYER_lock();{
      ListMoveElement3_FromNum_ns(&fx->fxnodelines, fxnodenum, &place, PlaceGetFirstPos(), last_pos);      
    }PLAYER_unlock();
  }
  
  int max = fx->fx->max;
  int min = fx->fx->min;
  
  fxnodeline->val=scale(value, 0.0f, 1.0f, min, max); //R_BOUNDARIES(min,value,max);

  window->must_redraw_editor = true;
}

void setFxnode(int fxnodenum, float value, float floatplace, int fxnum, int tracknum, int blocknum, int windownum){
  Place place;

  if (floatplace >= 0.0f)
    Float2Placement(floatplace, &place);
  else {
    place.line = -1;
    place.counter = 0;
    place.dividor = 1;
  }
  
  return setFxnode3(fxnodenum, value, place.line, place.counter, place.dividor, fxnum, tracknum, blocknum, windownum);
}

void setFxnodeLogtypeHolding(bool is_holding, int fxnodenum, int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fxs = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fxs==NULL)
    return;
  
  const vector_t *nodes = GetFxNodes(window, wblock, wtrack, fxs);
  if (fxnodenum < 0 || fxnodenum>=nodes->num_elements) {
    RWarning("There is no fx node %d for fx %d in track %d in block %d",fxnodenum, fxnum, tracknum, blocknum);
    return;
  }

  Undo_FXs(window, wblock->block, wtrack->track, wblock->curr_realline);

  struct Node *node = nodes->elements[fxnodenum];
  struct FXNodeLines *fxnodeline = (struct FXNodeLines *)node->element;

  int logtype = is_holding ? LOGTYPE_HOLD : LOGTYPE_LINEAR;
  
  fxnodeline->logtype = logtype;

  window->must_redraw_editor = true;
}

void deleteFxnode(int fxnodenum, int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fxs = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fxs==NULL)
    return;

  const vector_t *nodes = GetFxNodes(window, wblock, wtrack, fxs);
  if (fxnodenum < 0 || fxnodenum>=nodes->num_elements) {
    RWarning("There is no fx node %d for fx %d in track %d in block %d",fxnodenum, fxnum, tracknum, blocknum);
    return;
  }

  Undo_FXs(window, wblock->block, wtrack->track, wblock->curr_realline);

  struct Node *node = nodes->elements[fxnodenum];
  struct FXNodeLines *fxnodeline = (struct FXNodeLines *)node->element;
  
  DeleteFxNodeLine(wtrack, fxs, fxnodeline); // DeleteFxNodeLine locks player / stops playing

  window->must_redraw_editor = true;
}


void setCurrentFxnode(int fxnodenum, int fxnum, int tracknum, int blocknum, int windownum){
 struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fx = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fx==NULL)
    return;

  const vector_t *nodes = GetFxNodes(window, wblock, wtrack, fx);
  if (fxnodenum < 0 || fxnodenum>=nodes->num_elements) {
    RWarning("There is no fx node %d for fx %d in track %d in block %d",fxnodenum, fxnum, tracknum, blocknum);
    return;
  }

  struct Node *node = nodes->elements[fxnodenum];
  struct FXNodeLines *current = (struct FXNodeLines*)node->element;

  setCurrentNode(&current->l);
}

void setIndicatorFxnode(int fxnodenum, int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fx = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fx==NULL)
    return;

  const vector_t *nodes = GetFxNodes(window, wblock, wtrack, fx);
  if (fxnodenum < 0 || fxnodenum>=nodes->num_elements) {
    RWarning("There is no fx node %d for fx %d in track %d in block %d",fxnodenum, fxnum, tracknum, blocknum);
    return;
  }

  struct Node *node = nodes->elements[fxnodenum];
  setIndicatorNode(node->element);
}

void setNoMouseFx(int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock = getWBlockFromNumA(windownum, &window, blocknum);
  if(wblock==NULL)
    return;
  
  if (wblock->mouse_fxs != NULL){
    wblock->mouse_fxs = NULL;
    window->must_redraw_editor = true;
    //printf("no mouse fx dirty\n");
  }
}

void setMouseFx(int fxnum, int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack;
  struct FXs *fxs = getFXsFromNumA(windownum, &window, blocknum, &wblock, tracknum, &wtrack, fxnum);
  if (fxs==NULL)
    return;
  else if (wblock->mouse_fxs != fxs){
    wblock->mouse_fxs = fxs;
    window->must_redraw_editor = true;
    //printf("mouse fx dirty\n");
  }
}

void undoFxs(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
  struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
  if (wtrack==NULL)
    return;

  Undo_FXs(window, wblock->block, wtrack->track, wblock->curr_realline);
}


// track widths
//////////////////////////////////////////////////

float getTrackWidth(int tracknum, int blocknum, int windownum){
  struct Tracker_Windows *window;
  struct WBlocks *wblock;
    
  if (tracknum==-1){
    wblock = getWBlockFromNumA(windownum, &window, blocknum);
    if (wblock==NULL)
      return 0.0f;
    return wblock->temponodearea.width;
  } else {
    struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
    if (wtrack==NULL)
      return 0.0f;
    return wtrack->fxwidth;
  }
}

void undoTrackWidth(void){
  struct Tracker_Windows *window=getWindowFromNum(-1);
  if(window==NULL)
    return;
  
  Undo_Block_CurrPos(window); // can be optimized a lot
}

void setTrackWidth (float new_width, int tracknum, int blocknum, int windownum){
  if (new_width < 2) {
#if 0
    RWarning("Can not set width smaller than 2");
    return;
#else
    new_width = 2;
#endif
  }

  struct Tracker_Windows *window;
  struct WBlocks *wblock;
          
  if (tracknum==-1){
    wblock = getWBlockFromNumA(windownum, &window, blocknum);
    if (wblock==NULL)
      return;
    wblock->temponodearea.width = new_width;
  } else {
    struct WTracks *wtrack = getWTrackFromNumA(windownum, &window, blocknum, &wblock, tracknum);
    if (wtrack==NULL)
      return;
    ///printf("new width: %d, old: %d\n",(int)new_width,wtrack->fxwidth);
    wtrack->fxwidth = new_width;
  }

  UpdateWBlockCoordinates(window,wblock);
  //GL_create(window, window->wblock);
  
  window->must_redraw=true;
}



// ctrl / shift keys
//////////////////////////////////////////////////

extern struct TEvent tevent;

bool ctrlPressed(void){
  return AnyCtrl(tevent.keyswitch);
}

bool shiftPressed(void){
  return AnyShift(tevent.keyswitch);
}

/*
// Doesn't work to check right extra
bool extraPressed(void){
  return AnyExtra(tevent.keyswitch);
}
*/

bool leftExtraPressed(void){
  return LeftExtra(tevent.keyswitch);
}

bool altPressed(void){
  return AnyAlt(tevent.keyswitch);
}



// mouse pointer
//////////////////////////////////////////////////

/*
enum MousePointerType {
  MP_NORMAL,
  MP_BLANK,
  MP_DIAGONAL,
  MP_HORIZONTAL,  
};
*/

void setNormalMousePointer(int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window!=NULL)
    SetNormalPointer(window);
}
void setPointingMousePointer(int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window!=NULL)
    SetPointingPointer(window);
}
void setBlankMousePointer(int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window!=NULL)
    SetBlankPointer(window);
}
void setDiagonalResizeMousePointer(int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window!=NULL)
    SetDiagResizePointer(window);
}
void setHorizontalResizeMousePointer(int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window!=NULL)
    SetHorizResizePointer(window);
}
void setVerticalResizeMousePointer(int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window!=NULL)
    SetVerticalResizePointer(window);
}

void moveMousePointer(float x, float y, int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window!=NULL)
    MovePointer(window, x, y);
}
float getMousePointerX(int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window==NULL)
    return 0;

  WPoint ret = GetPointerPos(window);
  return ret.x;
}
float getMousePointerY(int windownum){
  struct Tracker_Windows *window = getWindowFromNum(windownum);
  if (window==NULL)
    return 0;

  WPoint ret = GetPointerPos(window);
  return ret.y;
}
