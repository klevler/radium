/* Copyright 2000 Kjetil S. Matheussen

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



#include "nsmtracker.h"
#include "list_proc.h"
#include "notes_proc.h"
#include "wtracks_proc.h"

#include "gfx_subtrack_proc.h"



/************************************************************************
  FUNCTION
    Returns the left X coordinate relative to track->fx.y for the subtrack
    'subtrack'.
************************************************************************/
static int GetPolyX1(
	const struct WTracks *wtrack,
	int polyphony_num
){
  R_ASSERT_RETURN_IF_FALSE2(wtrack, 100);

  return (wtrack->fxwidth*polyphony_num/wtrack->track->polyphony) + (polyphony_num>0 ? 1 : 0 ) ;
}

/************************************************************************
  FUNCTION
    Returns the absolute left X coordinate to track->fx.y for the subtrack
    'subtrack'.
************************************************************************/
int GetXSubTrack1(
	const struct WTracks *wtrack,
	int subtrack
){
  R_ASSERT_RETURN_IF_FALSE2(wtrack, 100);

  if(subtrack==-1)
    return wtrack->notearea.x;

  int fontwidth = (wtrack->veltextarea.x2 - wtrack->veltextarea.x) /  3;
  
  int sn = 0;

  if (wtrack->centtext_on){
    if (subtrack == sn)
      return wtrack->centtextarea.x;
    if (subtrack == sn+1)
      return wtrack->centtextarea.x + fontwidth;

    sn += 2;
  }
  
  if (wtrack->veltext_on){
    if (subtrack == sn)
      return wtrack->veltextarea.x;
    if (subtrack == sn+1)
      return wtrack->veltextarea.x + fontwidth;
    if (subtrack == sn+2)
      return wtrack->veltextarea.x + (fontwidth*2);

    sn += 3;
  }

  if (wtrack->fxtext_on){
    int numfxs = ListFindNumElements1(&wtrack->track->fxs->l);
    int fxnum;
    for(fxnum=0 ; fxnum < numfxs ; fxnum++){
      
      if (subtrack == sn)
        return wtrack->fxtextarea.x + fxnum*WTRACK_fxtrack_width(fontwidth);
      if (subtrack == sn+1)
        return wtrack->fxtextarea.x + fxnum*WTRACK_fxtrack_width(fontwidth) + fontwidth;
      if (subtrack == sn+2)
        return wtrack->fxtextarea.x + fxnum*WTRACK_fxtrack_width(fontwidth) + (fontwidth*2);
      
      sn += 3;
    }
  }
  
  int polyphony_num = subtrack - sn;
  return wtrack->fxarea.x + GetPolyX1(wtrack,polyphony_num);
}


/************************************************************************
  FUNCTION
    Returns the right X coordinate relative to track->fx.y for the subtrack
    'subtrack'.
************************************************************************/
static int GetPolyX2(
	const struct WTracks *wtrack,
	int polyphony_num
){
    R_ASSERT_RETURN_IF_FALSE2(wtrack, 200);
    
    int num_subtracks = GetNumSubtracks(wtrack);
    return
      (wtrack->fxwidth*(polyphony_num+1)/wtrack->track->polyphony) -
      (polyphony_num == num_subtracks-1 ? 0 : 1)
      ;
}

/************************************************************************
  FUNCTION
    Returns the absolute right X coordinate to track->fx.y for the subtrack
    'subtrack'.
************************************************************************/
int GetXSubTrack2(
	const struct WTracks *wtrack,
	int subtrack
){
    R_ASSERT_RETURN_IF_FALSE2(wtrack, 200);
    if(subtrack==-1) return wtrack->notearea.x2;

    int fontwidth = (wtrack->veltextarea.x2 - wtrack->veltextarea.x) / 3;
  
    int sn = 0;

    if (wtrack->centtext_on){
      if (subtrack == sn)
        return wtrack->centtextarea.x + fontwidth;
      if (subtrack == sn+1)
        return wtrack->centtextarea.x + (fontwidth*2);
      
      sn += 2;
    }
    
    if (wtrack->veltext_on){
      if (subtrack == sn)
        return wtrack->veltextarea.x + fontwidth;
      if (subtrack == sn+1)
        return wtrack->veltextarea.x + (fontwidth*2);
      if (subtrack == sn+2)
        return wtrack->veltextarea.x + (fontwidth*3);
      
      sn +=3;
    }

    if (wtrack->fxtext_on){
      int numfxs = ListFindNumElements1(&wtrack->track->fxs->l);
      int fxnum;
      for(fxnum=0 ; fxnum < numfxs ; fxnum++){
        
        if (subtrack == sn)
          return wtrack->fxtextarea.x + fxnum*WTRACK_fxtrack_width(fontwidth) + fontwidth;
        if (subtrack == sn+1)
          return wtrack->fxtextarea.x + fxnum*WTRACK_fxtrack_width(fontwidth) + fontwidth*2;
        if (subtrack == sn+2)
          return wtrack->fxtextarea.x + fxnum*WTRACK_fxtrack_width(fontwidth) + (fontwidth*3);
      
        sn += 3;
      }
    }
    
    int polyphony_num = subtrack - sn;
    return wtrack->fxarea.x + GetPolyX2(wtrack,polyphony_num);
}


  
/************************************************************************
  FUNCTION
    These two functions works just like GetXSubTrack1 and 2, except
    that they also accept the tracks that contains to the block. Which
    is the signature-track, lpb-track, tempo-track, and the temponode-track.
************************************************************************/
int GetXSubTrack_B1(
	const struct WBlocks *wblock,
	NInt track,
	int subtrack
){
	if(track>=0) return GetXSubTrack1(ListFindElement1(&wblock->wtracks->l,track),subtrack);
	switch(track){
		case SIGNATURETRACK:
			return wblock->signaturearea.x;
			break;
		case LPBTRACK:
			return wblock->lpbarea.x;
			break;
		case TEMPOTRACK:
			return wblock->tempoarea.x;
			break;
		case TEMPONODETRACK:
			return wblock->temponodearea.x;
			break;
	};
	RError("Error in function GetXSubTrack_B1 in file gfx_subtrack.c\n");
	return 0;
}

int GetXSubTrack_B2(
	const struct WBlocks *wblock,
	NInt track,
	int subtrack
){
	if(track>=0) return GetXSubTrack2(ListFindElement1(&wblock->wtracks->l,track),subtrack);
	switch(track){
		case SIGNATURETRACK:
			return wblock->signaturearea.x2;
			break;
		case LPBTRACK:
			return wblock->lpbarea.x2;
			break;
		case TEMPOTRACK:
			return wblock->tempoarea.x2;
			break;
		case TEMPONODETRACK:
			return wblock->temponodearea.x2;
			break;
	};
	RError("Error in function GetXSubTrack_B2 in file gfx_subtrack.c\n");
	return 0;
}


int GetNoteX1(const struct WTracks *wtrack, const struct Notes *note){
  return GetXSubTrack1(wtrack,
                       WTRACK_num_non_polyphonic_subtracks(wtrack) + note->polyphony_num
                       );
}

int GetNoteX2(const struct WTracks *wtrack, const struct Notes *note){
  return GetXSubTrack2(wtrack,
                       WTRACK_num_non_polyphonic_subtracks(wtrack) + note->polyphony_num
                       );
}


/************************************************************************
  FUNCTION
    Make shure that x is placed within the boundaries of the subtrack.
************************************************************************/
/*
int SubtrackBoundaries(const struct WTracks *wtrack,int subtrack,int x){
	int x1=GetRelXSubTrack1(wtrack,subtrack);
	int x2=GetRelXSubTrack2(wtrack,subtrack);
	if(x<x1) return x1;
	if(x>x2) return x2;
	return x;
}
*/


int GetSubTrackWidth(const struct WTracks *wtrack,int subtrack){
	return GetXSubTrack2(wtrack,subtrack)-GetXSubTrack1(wtrack,subtrack);
}



/**************************************************************
  FUNCTION
    Returns the relative X position according to X1 value
    of the wtrack 'wtrack', calculated from the vector (x,maxx).
  INPUTS
    subtrack - Start at zero.
**************************************************************/
/*
int GetSubTrackPos(
	const struct WTracks *wtrack,
	float x,
	int maxx,
	int subtrack
){
	float x2;

	x2=x*GetSubTrackWidth(wtrack,subtrack)/maxx;
	x2=x2+GetRelXSubTrack1(wtrack,subtrack);

	return (int) (x2+0.5);
}
*/


/**************************************************************
  FUNCTION
    Returns the subtrack 'x' belongs to in the wtrack 'wtrack'.
    If it doesn't belong to a subtrack. Returns -2;
**************************************************************/
/*
int GetSubTrack(
	const struct WTracks *wtrack,
	int x
){
	int lokke;
        int num_subtracks = GetNumSubtracks(wtrack);

	for(lokke= -1;lokke<num_subtracks;lokke++){
		if(x==SubtrackBoundaries(wtrack,lokke,x)) return lokke;
	}

	return -2;
}
*/



