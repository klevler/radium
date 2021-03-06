/* Copyright 2012 Kjetil S. Matheussen

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


#ifndef QT_MYQSLIDER_H
#define QT_MYQSLIDER_H

#include <QSlider>
#include <QScrollBar>
#include <QMouseEvent>
#include <QPainter>
#include <QVector>
#include <QApplication>
#include <QFont>

#include "EditorWidget.h"

#include "../common/instruments_proc.h"
#include "../common/vector_proc.h"
#include "../common/settings_proc.h"
#include "../common/placement_proc.h"
#include "../common/seqtrack_proc.h"
#include "../common/seqtrack_automation_proc.h"

#include "../audio/undo_audio_effect_proc.h"
#include "../audio/SoundPlugin.h"
#include "../audio/SoundPlugin_proc.h"
#include "../audio/Pd_plugin_proc.h"
#include "../audio/Modulator_plugin_proc.h"

#include "Qt_instruments_proc.h"

#include "Qt_SliderPainter_proc.h"

struct MyQSlider;

static inline int scale_int(int x, int x1, int x2, int y1, int y2){
  return (int)scale((float)x,(float)x1,(float)x2,(float)y1,(float)y2);
}


#ifdef COMPILING_RADIUM
//extern QVector<MyQSlider*> g_all_myqsliders;
extern struct Root *root;
#else
//QVector<MyQSlider*> g_all_myqsliders;
#endif

extern struct TEvent tevent;

static int g_minimum_height = 0;

struct MyQSlider : public QSlider, public radium::MouseCycleFix {

 public:
  radium::GcHolder<struct Patch> _patch;
  int _effect_num;
  bool _is_recording;
  
  SliderPainter *_painter;

  bool _minimum_size_set;

  bool _is_a_pd_slider;

  void init(){
    _has_mouse=false;

    _patch.set(NULL);
    _effect_num = 0;
    _is_recording = false;
    
    _is_a_pd_slider = false;

    if(g_minimum_height==0){
      QFontMetrics fm(QApplication::font());
      QRect r =fm.boundingRect("In Out 234234 dB");
      g_minimum_height = r.height()+4;
      printf("Minimum height: %d, family: %s, font pixelsize: %d, font pointsize: %d\n",g_minimum_height,QApplication::font().family().toUtf8().constData(),QApplication::font().pixelSize(),QApplication::font().pointSize());
    }

    _minimum_size_set = false; // minimumSize must be set later. I think ui generated code overwrites it when set here.

    _painter = SLIDERPAINTER_create(this);

    //g_all_myqsliders.push_back(this);
  }

  MyQSlider ( QWidget * parent_ = 0 ) : QSlider(parent_) {init();}
  MyQSlider ( Qt::Orientation orientation_, QWidget * parent_ = 0 ) : QSlider(orientation_,parent_) { init();}

  void prepare_for_deletion(void){
    SLIDERPAINTER_prepare_for_deletion(_painter);
  }
  
  ~MyQSlider(){
    prepare_for_deletion();
    
    //R_ASSERT(false);
    //g_all_myqsliders.remove(g_all_myqsliders.indexOf(this));
    SLIDERPAINTER_delete(_painter);
    
    _painter = NULL; // quicker to discover memory corruption
    _patch.set(NULL);
  }

  void calledRegularlyByParent(void){
    R_ASSERT_RETURN_IF_FALSE(_patch.data() != NULL);
      
    SoundPlugin *plugin = (SoundPlugin*)_patch->patchdata;

    if (plugin != NULL) {
        bool is_recording_now = PLUGIN_is_recording_automation(plugin, _effect_num);
        if (is_recording_now != _is_recording){
          SLIDERPAINTER_set_recording_color(_painter, is_recording_now);
          update();
          _is_recording = is_recording_now;
        }
    }

    SLIDERPAINTER_call_regularly(_painter, -1);
  }
  
  void hideEvent ( QHideEvent * event_ ) override {
    SLIDERPAINTER_became_invisible(_painter);
  }

  void showEvent ( QShowEvent * event_ ) override {
    SLIDERPAINTER_became_visible(_painter);
  }

#if 0
  void handle_system_delay(bool down){
    SoundPlugin *plugin = (SoundPlugin*)_patch->patchdata;
    const SoundPluginType *type = plugin->type;

    if(_effect_num==type->num_effects+EFFNUM_DELAY_TIME){
      if(down==true)
        plugin->delay.is_on = false;
      else
        plugin->delay.is_on = true;
    }
  }
#endif

  float last_value;
  float last_pos;
  bool _has_mouse;

  void handle_mouse_event (radium::MouseCycleEvent &event_){
    //printf("Got mouse press event_ %d / %d\n",(int)event_.x(),(int)event_.y());

    float slider_length;
    float new_pos;

    if (orientation() == Qt::Vertical) {
      new_pos = event_.y();
      slider_length = height();
    } else {
      new_pos = event_.x();
      slider_length = width();
    }

    float dx = new_pos - last_pos;
    float per_pixel = (float)(maximum() - minimum()) / (float)slider_length;

    //printf("***** last_pos: %d, new_pos: %d, dx: %d\n",(int)last_pos,(int)new_pos,(int)dx);
    
    if ((event_.modifiers() & Qt::ControlModifier))
      per_pixel /= 10.0f;

    last_pos = new_pos;
    last_value += dx*per_pixel;

    if (last_value < minimum())
      last_value = minimum();
    if (last_value > maximum())
      last_value = maximum();

    setValue(last_value);

    //printf("dx: %f, per_pixel: %f, min/max: %f / %f, value: %f\n",dx,per_pixel,(float)minimum(),(float)maximum(),(float)value());

    event_.accept();
  }

  void handle_mouse_event ( QMouseEvent * event_ ){
    if (event_==NULL)
      return;

    radium::MouseCycleEvent event2(event_);
    handle_mouse_event(event2);
  }
  
  void show_slider_popup_menu(void){
    bool is_audio_instrument = _patch->instrument==get_audio_instrument() ;

    SoundPlugin *plugin = (SoundPlugin*)_patch->patchdata;
      
    vector_t options = {}; // c++ way of zero-initialization without getting missing-field-initializers warning.

    bool has_midi_learn = is_audio_instrument && PLUGIN_has_midi_learn(plugin, _effect_num);
    bool is_recording_automation = is_audio_instrument && PLUGIN_is_recording_automation(plugin, _effect_num);
    bool doing_random_change = is_audio_instrument && PLUGIN_get_random_behavior(plugin, _effect_num);
    int64_t modulator_id = MODULATOR_get_id(_patch.data(), _effect_num);
      
    int pd_delete=-10;
    int reset=-10;
      
    int remove_midi_learn=-10;
    int midi_relearn=-10;
    int midi_learn=-10;
      
    int remove_modulator=-10;
    int replace_modulator=-10;
    int add_modulator=-10;
      
    int record=-10;
    int add_automation_to_current_editor_track=-10;
    int add_automation_to_current_sequencer_track=-10;
    int add_random = -10;
    int remove_random = -10;
      
    if (is_audio_instrument) {

      if(_is_a_pd_slider){
        /*
          VECTOR_push_back(&options, "Set Symbol Name");
          VECTOR_push_back(&options, "Set Type");
          VECTOR_push_back(&options, "Set Minimum Value");
          VECTOR_push_back(&options, "Set Maximum Value");
        */
        pd_delete = VECTOR_push_back(&options, "Delete");
      } else {
          
        reset=VECTOR_push_back(&options, "Reset");
          
        //VECTOR_push_back(&options, "Set Value");
      }
        
      VECTOR_push_back(&options, "--------------");
        
      if (has_midi_learn){
        remove_midi_learn = VECTOR_push_back(&options, "Remove MIDI Learn");
        midi_relearn = VECTOR_push_back(&options, "MIDI relearn");
      }else{
        midi_learn = VECTOR_push_back(&options, "MIDI Learn");
      }

      VECTOR_push_back(&options, "--------------");

    }

    if(modulator_id >= 0){
      remove_modulator=VECTOR_push_back(&options, talloc_format("Remove Modulator (%s)", getModulatorDescription2(_patch->id, _effect_num)));
      replace_modulator=VECTOR_push_back(&options, talloc_format("Replace Modulator (%s)", getModulatorDescription2(_patch->id, _effect_num)));
    } else {
      add_modulator=VECTOR_push_back(&options, "Assign Modulator");
    }

    if (is_audio_instrument) {

      VECTOR_push_back(&options, "--------------");
        
      if (!is_recording_automation)
        record = VECTOR_push_back(&options, "Record");
        
      VECTOR_push_back(&options, "--------------");
        
      add_automation_to_current_editor_track = VECTOR_push_back(&options, "Add automation to current editor track");
      add_automation_to_current_sequencer_track = VECTOR_push_back(&options, "Add automation to current sequencer track");
        
      VECTOR_push_back(&options, "--------------");
        
      if (_effect_num < plugin->type->num_effects){
        if (doing_random_change)
          remove_random = VECTOR_push_back(&options, "Don't change value when pressing \"Random\"");
        else
          add_random = VECTOR_push_back(&options, "Change value when pressing \"Random\"");
      }
    }
      
    //VECTOR_push_back(&options, "");
      
    //VECTOR_push_back(&options, "Set Value");

    IsAlive is_alive(this);
    
    GFX_Menu3(options,[is_alive, this, plugin, modulator_id, pd_delete, reset, remove_midi_learn, midi_relearn, midi_learn, remove_modulator, replace_modulator, add_modulator, record, remove_random, add_random, add_automation_to_current_editor_track, add_automation_to_current_sequencer_track](int command, bool onoff){

        if (!is_alive || _patch->patchdata==NULL)
          return;

        //printf("command: %d, _patch: %p, is_audio: %d\n",command, _patch, _patch!=NULL && _patch->instrument==get_audio_instrument());
        
        if (command==pd_delete)
          PD_delete_controller(plugin, _effect_num);
        
        else if (command==reset)
          PLUGIN_reset_one_effect(plugin,_effect_num);
        
        else if (command==remove_midi_learn)
          PLUGIN_remove_midi_learn(plugin, _effect_num, true);
        
        else if (command==midi_relearn)
          {
            PLUGIN_remove_midi_learn(plugin, _effect_num, true);
            PLUGIN_add_midi_learn(plugin, _effect_num);
          }
        
        else if (command==midi_learn)
          PLUGIN_add_midi_learn(plugin, _effect_num);
        
        else if (command==remove_modulator){
          MODULATOR_remove_target(modulator_id, _patch.data(), _effect_num);
          
        }else if (command==replace_modulator){
          MODULATOR_maybe_create_and_add_target(_patch.data(), _effect_num, true);
          
        }else if (command==add_modulator){
          MODULATOR_maybe_create_and_add_target(_patch.data(), _effect_num, false);
          
        }else if (command==record)
          PLUGIN_set_recording_automation(plugin, _effect_num, true);
        
        else if (command==remove_random)
          PLUGIN_set_random_behavior(plugin, _effect_num, false);
        
        else if (command==add_random)
          PLUGIN_set_random_behavior(plugin, _effect_num, true);
        
        else if (command==add_automation_to_current_editor_track) {
          
          addAutomationToCurrentEditorTrack(_patch->id, getInstrumentEffectName(_effect_num, _patch->id));
          
        } else if (command==add_automation_to_current_sequencer_track) {
          
          addAutomationToCurrentSequencerTrack(_patch->id, getInstrumentEffectName(_effect_num, _patch->id));
          
        }
      
       
        GFX_update_instrument_widget(_patch.data());
        
#if 0
        else if(command==1){
          char *s = GFX_GetString(root->song->tracker_windows,NULL, (char*)"new value");
          if(s!=NULL){
            float value = OS_get_double_from_string(s);
            printf("value: %f\n",value);
            setValue(value);
          }
        }
        
#else
#if 0
        else if(command==1 && _patch.data()!=NULL && _patch->instrument==get_audio_instrument()){
          SoundPlugin *plugin = (SoundPlugin*)_patch->patchdata;
          char *s = GFX_GetString(root->song->tracker_windows,NULL, (char*)"new value");
          if(s!=NULL){
            float value = OS_get_double_from_string(s);
            ADD_UNDO(AudioEffect_CurrPos(_patch.data(), _effect_num));
            PLUGIN_set_effect_value(plugin, -1, _effect_num, value, PLUGIN_STORED_TYPE, PLUGIN_STORE_VALUE, FX_single);
            GFX_update_instrument_widget(_patch.data());
          }
        }
#endif
#endif

      });
  }
  
  // mousePressEvent 
  void fix_mousePressEvent ( QMouseEvent * event_ ) override
  {
    if(_patch.data()!=NULL && _patch->instrument==get_audio_instrument() && _patch->patchdata == NULL) // temp fix
      return;
    
    //printf("Got mouse pres event_ %d / %d\n",(int)event_->x(),(int)event_->y());
    if (event_->button() == Qt::LeftButton){

#ifdef COMPILING_RADIUM
      if(_patch.data()!=NULL && _patch->instrument==get_audio_instrument()){
        ADD_UNDO(AudioEffect_CurrPos(_patch.data(), _effect_num));
        //handle_system_delay(true);
      }
#endif

      last_value = value();
      if (orientation() == Qt::Vertical)
        last_pos = event_->y();
      else
        last_pos = event_->x();
      //handle_mouse_event_(event_);
      _has_mouse = true;

    }else{

      if(_patch.data()==NULL || _patch->patchdata == NULL) //  _patch->instrument!=get_audio_instrument() 
        return;
      
#ifdef COMPILING_RADIUM
      show_slider_popup_menu();
#endif // COMPILING_RADIUM

      event_->accept();
    }
  }

  void fix_mouseMoveEvent ( QMouseEvent * event_ ) override
  {
    if (_has_mouse){
      handle_mouse_event(event_);
    }else
      QSlider::mouseMoveEvent(event_);
  }

  void fix_mouseReleaseEvent (radium::MouseCycleEvent &event_ ) override
  {
    //printf("Got mouse release event %d / %d\n",(int)event_->x(),(int)event_->y());
    if (_has_mouse){
#if 0
      if(_patch.data()!=NULL && _patch->instrument==get_audio_instrument()){
        handle_system_delay(false);
      }
#endif
      handle_mouse_event(event_);
      _has_mouse=false;
    }else {

      if (event_.is_real_event()){
        QSlider::mouseReleaseEvent(event_.get_qtevent());
      } else {
        //R_ASSERT_NON_RELEASE(false);
      }
    }
  }

  MOUSE_CYCLE_CALLBACKS_FOR_QT;
    
  void paintEvent ( QPaintEvent * ev ) override {
    TRACK_PAINT();
    
    if(_minimum_size_set==false){
      _minimum_size_set=true;
      setMinimumHeight(g_minimum_height);
    }

#if 0
    {
      QFontMetrics fm(QApplication::font());
      //QRect r =fm.boundingRect(SLIDERPAINTER_get_string(_painter));
      int width = fm.width(SLIDERPAINTER_get_string(_painter)) + 20;
      if(minimumWidth() < width)
        setMinimumWidth(width);
    }

#endif

    QPainter p(this);
    SLIDERPAINTER_paint(_painter,&p);
  }

};



#endif // QT_MYQSLIDER_H
