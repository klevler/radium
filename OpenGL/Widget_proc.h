#ifndef OPENGL_WIDGET_PROC_H
#define OPENGL_WIDGET_PROC_H

#include <stdint.h>

#ifdef __cplusplus
class QWidget;

  extern QWidget *GL_create_widget(QWidget *parent);
  extern void GL_stop_widget(QWidget *widget);

  extern "C" {
#endif

extern bool GL_should_do_modal_windows(void);
    
extern void GL_pause_gl_thread_a_short_while(void);
extern void GL_EnsureMakeCurrentIsCalled(void);
  
extern void GL_erase_estimated_vblank(void);
extern double GL_get_estimated_vblank(void);

extern void GL_set_vsync(bool onoff);
extern bool GL_get_vsync(void);

extern void GL_set_multisample(int size);
extern int GL_get_multisample(void);

extern void GL_set_safe_mode(bool onoff);
extern bool GL_get_safe_mode(void);

extern void GL_lock(void);
extern void GL_unlock(void);
extern bool GL_maybeLock(void);
    
extern volatile char *GE_vendor_string;
extern volatile char *GE_renderer_string;
extern volatile char *GE_version_string;
extern volatile uint32_t GE_opengl_version_flags;
  
extern void GE_set_curr_realline(int curr_realline);

#ifdef __cplusplus
  }
#endif

#endif // OPENGL_WIDGET_PROC_H
