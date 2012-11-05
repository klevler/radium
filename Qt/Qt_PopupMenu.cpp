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

#if USE_QT_MENU

#include <QAction>
#include <QMenu>

#include "../common/nsmtracker.h"
#include "../common/visual_proc.h"

#include "EditorWidget.h"

int GFX_Menu(
             struct Tracker_Windows *tvisual,
             ReqType reqtype,
             const char *seltext,
             vector_t *v
             )
{
  if(reqtype==NULL || v->num_elements>20){
   
    QMenu menu(0);

    for(int i=0;i<v->num_elements;i++)
      menu.addAction(new QAction((const char*)v->elements[i],&menu));

    QAction *action = menu.exec(QCursor::pos());
    if(action==NULL)
      return -1;

    for(int i=0;i<v->num_elements;i++)
      if(action->text() == (const char*)v->elements[i])
        return i;

    RWarning("Got unknown action %p %s\n",action,action->text().toAscii().constData());

    return -1;

  }else{

    return GFX_ReqTypeMenu(tvisual,reqtype,seltext,v);

  }
}

#endif // USE_QT_MENU
