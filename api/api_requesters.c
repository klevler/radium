/* Copyright 2001-2012 Kjetil S. Matheussen

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


#include "Python.h"
#include "radium_proc.h"

#include "../common/nsmtracker.h"

#include "../common/visual_proc.h"
#include "../common/window_config_proc.h"
#include "../common/block_properties_proc.h"
#include "../common/OS_visual_input.h"
#include "../common/OS_string_proc.h"
#include "../embedded_scheme/s7extra_proc.h"

#include "../midi/midi_i_plugin_proc.h"

#ifdef _AMIGA
#include "Amiga_colors_proc.h"
#endif

#include "api_common_proc.h"

extern struct Root *root;


void setDefaultColors1(void){
  GFX_SetDefaultColors1(getWindowFromNum(-1));
}

void setDefaultColors2(void){
  GFX_SetDefaultColors2(getWindowFromNum(-1));
}

void configSystemFont(void){
  GFX_ConfigSystemFont();
}

void configFonts(int windownum){
  struct Tracker_Windows *window=getWindowFromNum(windownum);if(window==NULL) return;
  GFX_ConfigFonts(window);
}

void setDefaultEditorFont(void){
  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return;
  GFX_SetDefaultFont(window);
}

void setDefaultSystemFont(void){
  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return;
  //RWarning("Warning! (?)"); // warning window test
  GFX_SetDefaultSystemFont(window);
}

void configWindow(int windownum){
  struct Tracker_Windows *window=getWindowFromNum(windownum);if(window==NULL) return;
  Window_config(window);
}

void configBlock(int windownum){
  struct Tracker_Windows *window=getWindowFromNum(windownum);if(window==NULL) return;
  Block_Properties_CurrPos(window);
}

void configVST(int windownum){
  struct Tracker_Windows *window=getWindowFromNum(windownum);if(window==NULL) return;
  OS_VST_config(window);
}

const char *getLoadFilename(const_char *text, const_char *filetypes, const_char *dir, const_char *type){
  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return "";
  if (!strcmp(type,""))
    type = NULL;
  const wchar_t *ret = GFX_GetLoadFileName(window, NULL, text, STRING_create(dir), filetypes, type);
  if(ret==NULL)
    return "";
  else
    return STRING_get_chars(ret);
}

const char *getSaveFilename(const_char *text, const_char *filetypes, const_char *dir, const_char *type){
  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return "";
  if (!strcmp(type,""))
    type = NULL;
  const wchar_t *ret = GFX_GetSaveFileName(window, NULL, text, STRING_create(dir), filetypes, type);
  if(ret==NULL)
    return "";
  else
    return STRING_get_chars(ret);
}


static ReqType requester = NULL;

void openRequester(char *text, int width, int height){
  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return;

  requester = GFX_OpenReq(window,width,height,text);
}

void closeRequester(void){
  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return;

  if(requester!=NULL){
    GFX_CloseReq(window, requester);
    requester = NULL;
  }
}

int requestInteger(char *text, int min, int max, bool standalone){
  if (standalone)
    return GFX_GetInteger(NULL, requester, text, min, max);

  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return min-1;
  return GFX_GetInteger(window, requester, text, min, max);
}

float requestFloat(char *text, float min, float max, bool standalone){
  if (standalone)
    return GFX_GetFloat(NULL, requester, text, min, max);

  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return min-1.0f;
  return GFX_GetFloat(window, requester, text, min, max);
}

char* requestString(char *text, bool standalone){
  char *ret;

  if (standalone)
    ret = GFX_GetString(NULL, requester, text);
  else {
    struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return "";
    ret = GFX_GetString(window, requester, text);
  }

  if(ret==NULL)
    ret="";
  return ret;
}

int requestMenu(char *text, PyObject* arguments){
  handleError("requestMenu not implemented");
  return 0;
}

int popupMenu(const char *texts){
  struct Tracker_Windows *window=getWindowFromNum(-1);
  vector_t *vec = GFX_MenuParser(texts, "%");
  return GFX_Menu(window, NULL,"",vec);
}

int popupMenu2(const char *texts, func_t* callback){
  struct Tracker_Windows *window=getWindowFromNum(-1);
  vector_t *vec = GFX_MenuParser(texts, "%");
  return GFX_Menu2(window, NULL, "", vec, callback, false);
}

void asyncPopupMenu(const char *texts, func_t* callback){
  struct Tracker_Windows *window=getWindowFromNum(-1);
  vector_t *vec = GFX_MenuParser(texts, "%");
  GFX_Menu2(window, NULL, "", vec, callback, true);
}


void colorDialog(const char *initial_color, int64_t parentguinum, func_t* callback){
  GFX_color_dialog(initial_color, parentguinum, callback);
}

void callFunc_void_int_bool(func_t* callback, int arg1, bool arg2){
  s7extra_callFunc_void_int_bool(callback, arg1, arg2);
}

char* requestMidiPort(void){
  struct Tracker_Windows *window=getWindowFromNum(-1);if(window==NULL) return "";
  char *ret = MIDIrequestPortName(window, requester, false);
  if(ret==NULL)
    ret="";
  return ret;
}

const_char* showMessage(char *text, dyn_t buttons){
  if (buttons.type==UNINITIALIZED_TYPE){
    GFX_Message(NULL, text);
    return "Ok";
  }

  if (buttons.type!=ARRAY_TYPE){
    handleError("showMessage: Argument 1: Expected ARRAY_TYPE, found %s", DYN_type_name(buttons.type));
    return "";
  }
  
  vector_t v={0};
  for(int i=0;i<buttons.array->num_elements;i++){
    dyn_t button = buttons.array->elements[i];
    if (button.type!=STRING_TYPE){
      handleError("showMessage: Button #%d: Expected STRING_TYPE, found %s", DYN_type_name(button.type));
      return "";
    }
    VECTOR_push_back(&v, STRING_get_chars(button.string));
  }

  int ret = GFX_Message(&v, text);
  if (ret<0 || ret>= buttons.array->num_elements) // don't this can happen though.
    return "";

  return STRING_get_chars(buttons.array->elements[ret].string);
}

void showWarning(char *text){
  RWarning(text);
}

void showError(char *text){
  RError(text);
}

void openProgressWindow(const char *message){
  GFX_OpenProgress(message);
}
void showProgressWindowMessage(const char *message){
  GFX_ShowProgressMessage(message);
}
void closeProgressWindow(void){
  GFX_CloseProgress();
}

void showMixerHelpWindow(void){
  GFX_showMixerHelpWindow();
}

void showChanceHelpWindow(void){
  GFX_showChanceHelpWindow();
}
void showVelocityHelpWindow(void){
  GFX_showVelocityHelpWindow();
}
void showFXHelpWindow(void){
  GFX_showFXHelpWindow();
}
void showSwingHelpWindow(void){
  GFX_showSwingHelpWindow();
}
void showKeybindingsHelpWindow(void){
  GFX_showKeybindingsHelpWindow();
}
