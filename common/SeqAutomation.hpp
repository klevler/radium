/* Copyright 2016 Kjetil S. Matheussen

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



#ifndef _RADIUM_COMMON_SEQAUTOMATION_HPP
#define _RADIUM_COMMON_SEQAUTOMATION_HPP

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#include <QVector> // Shortening warning in the QVector header. Temporarily turned off by the surrounding pragmas.
#pragma clang diagnostic pop


#include <QPainter>

#include "../Qt/Qt_mix_colors.h"


namespace radium{

// Default T struct for SeqAutomation
struct AutomationNode{
  double time; // seqtime format
  double value;
  int logtype;
};

static inline bool nodes_are_not_equal(const radium::AutomationNode &node1, const radium::AutomationNode &node2){
  return node1.time!=node2.time || node1.value!=node2.value || node1.logtype!=node2.logtype;
}

static inline bool nodes_are_equal(const radium::AutomationNode &node1, const radium::AutomationNode &node2){
  return node1.time==node2.time && node1.value==node2.value && node1.logtype==node2.logtype;
}

  
template <typename T> struct NodeFromStateProvider{
  virtual T create_node_from_state(hash_t *state, double state_samplerate) const = 0;
  virtual ~NodeFromStateProvider() = default; // Crazy c++ stuff. https://www.securecoding.cert.org/confluence/display/cplusplus/OOP52-CPP.+Do+not+delete+a+polymorphic+object+without+a+virtual+destructor
};


enum class SeqAutomationReturnType{
  VALUE_OK,
  NO_VALUES_YET,
  NO_VALUES,
  NO_MORE_VALUES,
};



template <typename T> 
struct SeqAutomationPainter : AutomationPainter{
  const QVector<T> &_automation;
  int _size = 0;

  bool _can_paint = false;
  bool _paint_lines = false;

  const QColor _color;

  float _x1, _y1, _x2, _y2;

  int _start_i;

  int _curr_nodenum;
  bool _paint_nodes;

  int _fill_x1, _fill_x2;
  const QColor _fill_color;
  
  QPointF *_points;

  SeqAutomationPainter(const SeqAutomationPainter&) = delete;
  SeqAutomationPainter& operator=(const SeqAutomationPainter&) = delete;

  SeqAutomationPainter(const QVector<T> &automation,
                       int curr_nodenum, bool paint_nodes,
                       float x1, float y1, float x2, float y2,
                       double start_time, double end_time,
                       const QColor &color,                       
                       float (*get_y)(const T &node, float y1, float y2, void *data),
                       float (*get_x)(const T &node, double start_time, double end_time, float x1, float x2, void *data) = NULL,
                       void *data = NULL,
                       const QColor fill_color = QColor(),
                       float fill_x1 = -1, float fill_x2 = -1                                       
                       )
      
    : _automation(automation)
    , _color(color)
    , _x1(x1)
    , _y1(y1)
    , _x2(x2)
    , _y2(y2)
    , _curr_nodenum(curr_nodenum)
    , _paint_nodes(paint_nodes)
    , _fill_x1(fill_x1)
    , _fill_x2(fill_x2)
    , _fill_color(fill_color)
    , _points(new QPointF[4 + _automation.size()*2])
      
  {
    if (_automation.size()==0){
      R_ASSERT_NON_RELEASE(false);
      return;
    }
      
    _start_i = -1;
    int num_after_end = 0;
    bool next_is_hold = false;
      
    // 1. find x+y _points in the gfx coordinate system
    for(int i = 0; i < _automation.size() ; i++){
      const T &node1 = _automation.at(i);
        
      float x_a;
        
      if (get_x != NULL)
        x_a = get_x(node1, start_time, end_time, x1, x2, data);
      else
        x_a = scale(node1.time, start_time, end_time, x1, x2);
        
      if (x_a >= x2)
        num_after_end++;
        
      if (num_after_end == 2)
        break;
        
      if (_start_i < 0)
        _start_i = i;
        
      float y_a = get_y(node1, y1, y2, data);
        
      if (next_is_hold && x_a>=x1){
        _points[_size] = QPointF(x_a, _points[_size-1].y());
        _size++;
      }
        
      //printf("   %d: %f, %f  (x1: %f)\n", _size, x_a, y_a, x1);
      if (_size > 0 && x_a < x1)
        _points[_size-1] = QPointF(x_a, y_a);
      else
        _points[_size++] = QPointF(x_a, y_a);
        
        
      next_is_hold = node1.logtype==LOGTYPE_HOLD;
    }
      
    //printf("---------------------------. x1: %f\n", x1);
      
    if (_size==0)
      return;

    _paint_lines = _size >= 2;
      
    if (_points[_size-1].x() < x1){
        
      if (_points[_size-1].x() < x1-get_min_node_size())
        return;
        
      _paint_lines = false;
    }
      
    if (_points[0].x() >= x2){
        
      if (_points[0].x() >= x2+get_min_node_size())
        return;
        
      _paint_lines = false;
    }

    _can_paint = true;
  }

  ~SeqAutomationPainter(){
    delete[] _points;
  }
                         
  QColor get_color(QColor col1, QColor col2, int mix, float alpha) const {
    QColor ret = mix_colors(col1, col2, (float)mix/1000.0);
    ret.setAlphaF(alpha);
    return ret;
  }

  void paint_node(QPainter *p, float x, float y, int nodenum, QColor color) const {
    static float penwidth_d2 = 1.2;
    
    static QPen pen1,pen2,pen3,pen4;
    static QBrush fill_brush;
    static bool has_inited = false;

    if(has_inited==false){

      const float penwidth = (float)root->song->tracker_windows->systemfontheight / 10.0f;
      penwidth_d2 = penwidth/2.01;

      fill_brush = QBrush(get_color(color, Qt::white, 300, 0.7));
      
      pen1 = QPen(get_color(color, Qt::white, 100, 0.3));
      pen1.setWidthF(penwidth);
      
      pen2 = QPen(get_color(color, Qt::black, 300, 0.3));
      pen2.setWidthF(penwidth);
      
      pen3 = QPen(get_color(color, Qt::black, 400, 0.3));
      pen3.setWidthF(penwidth);
      
      pen4 = QPen(get_color(color, Qt::white, 300, 0.3));
      pen4.setWidthF(penwidth);
      
      has_inited=true;
    }

    float minnodesize = get_min_node_size() - 1;

    //printf("penwidth: %f\n", penwidth);
    
    float x1 = x-minnodesize+penwidth_d2;
    float x2 = x+minnodesize-penwidth_d2;
    float y1 = y-minnodesize+penwidth_d2;
    float y2 = y+minnodesize-penwidth_d2;

    if (nodenum == _curr_nodenum) {
      p->setBrush(fill_brush);
      p->setPen(Qt::NoPen);
      QRectF rect(x1,y1,x2-x1-1,y2-y1);
      p->drawRect(rect);
    }
    
    // vertical left
    {
      p->setPen(pen1);
      QLineF line(x1+1, y1+1,
                  x1+2,y2-1);
      p->drawLine(line);
    }
    
    // horizontal bottom
    {
      p->setPen(pen2);
      QLineF line(x1+2,y2-1,
                  x2-1,y2-2);
      p->drawLine(line);
    }
    
    // vertical right
    {
      p->setPen(pen3);
      QLineF line(x2-1,y2-2,
                  x2-2,y1+2);
      p->drawLine(line);
    }
    
    // horizontal top
    {
      p->setPen(pen4);
      QLineF line(x2-2,y1+2,
                  x1+1,y1+1);
      p->drawLine(line);
    }
  }

  void paint_fill(QPainter *p) const override {
    if (_can_paint==false)
      return;
      
    float first_x = _points[0].x();
    float first_y = _points[0].y();
    float last_x = _points[_size-1].x();
    float last_y = _points[_size-1].y();
      
    int fill_size;
      
    if (_fill_x1 == -1){
      _points[_size]   = QPointF(last_x, _y2);
      _points[_size+1] = QPointF(first_x, _y2);
      fill_size = _size+2;
    } else {
      _points[_size]   = QPointF(_x2, last_y);
      _points[_size+1] = QPointF(_x2, _y2);
      _points[_size+2] = QPointF(_x1, _y2);
      _points[_size+3] = QPointF(_x1, first_y);
      fill_size = _size+4;
    }
        
    //for(int i=0 ; i < _size+2 ; i++)
    //  printf("  %d/%d: %d , %d  (y1: %f, y2: %f)\n", i, _size, (int)_points[i].x(), (int)_points[i].y(), y1, y2);
      
    if (_fill_color.isValid()) {

      // 2. Background fill
        
      p->setPen(Qt::NoPen);
      p->setBrush(_fill_color);
        
      p->drawPolygon(_points, fill_size);
        
      p->setBrush(Qt::NoBrush);

    } else {

      double org_opacity = p->opacity();
        
      // 3. stipled line.
      p->setOpacity(0.35*org_opacity);

#if 0
      QPen pen(_color);
      pen.setWidthF(_paint_nodes ? root->song->tracker_windows->systemfontheight / 3 : root->song->tracker_windows->systemfontheight / 6);
      p->setPen(pen);

      p->drawPolygon(_points, fill_size);
#else
      p->setPen(Qt::NoPen);
      p->setBrush(_color);
        
      p->drawPolygon(_points, fill_size);
        
      p->setBrush(Qt::NoBrush);
#endif
      p->setOpacity(org_opacity);

    }

  }

  void paint_lines(QPainter *p) const override {
    if (_can_paint==false || _paint_lines==false)
      return;
      
    QPen pen(_color);
    pen.setWidthF(_paint_nodes ? root->song->tracker_windows->systemfontheight / 3 : root->song->tracker_windows->systemfontheight / 6);
    p->setPen(pen);
    p->drawPolyline(_points, _size);
  }

  void paint_nodes(QPainter *p) const override {
    if (_can_paint==false || _paint_nodes==false)
      return;

    int node_pos = _start_i;
    for(int i = _start_i; i < _size ; i++){
      const T &node = _automation.at(node_pos);
        
      float x_a = _points[i].x();
      float y_a = _points[i].y();
      paint_node(p, x_a, y_a, node_pos, _color);
        
      if (node.logtype==LOGTYPE_HOLD)
        i++;
        
      node_pos++;
    }

  }
};
  

template <typename T> class SeqAutomation{
  
private:
  
  QVector<T> _automation;

  struct RT{
    int num_nodes;
    T nodes[];
  };

  int _curr_nodenum = -1;
  bool _paint_nodes = false;

  mutable const SeqAutomationPainter<T> *_last_painter = NULL;
  
  AtomicPointerStorage _rt;

  // Double free / using data after free, if trying to copy data.
  SeqAutomation(const SeqAutomation&) = delete;
  SeqAutomation& operator=(const SeqAutomation&) = delete;


public:

  SeqAutomation()
    : _rt(V_free_function)
  {
  }

  ~SeqAutomation(){
    delete _last_painter;
  }
      
  void *new_rt_data_has_been_created_data = NULL;
  void (*new_rt_data_has_been_created)(void *data) = NULL;

  const QVector<T> &get_qvector(void) const {
    return _automation;
  }
  
private:
  
  int get_size(int num_nodes) const {
    return sizeof(struct RT) + num_nodes*sizeof(T);
  }

  const struct RT *create_rt(void) const {
    struct RT *rt = (struct RT*)V_malloc(get_size(_automation.size()));
  
    rt->num_nodes=_automation.size();
    
    for(int i=0 ; i<_automation.size() ; i++)
      rt->nodes[i] = _automation.at(i);
    
    return (const struct RT*)rt;
  }

  void create_new_rt_data(void){
    const struct RT *new_rt_tempo_automation = create_rt();

    _rt.set_new_pointer((void*)new_rt_tempo_automation);

    if (new_rt_data_has_been_created != NULL)
      new_rt_data_has_been_created(new_rt_data_has_been_created_data);
  }


public:

  bool do_paint_nodes(void) const {
    return _paint_nodes;
  }

  // called very often.
  bool set_do_paint_nodes(bool do_paint_nodes){
    if (do_paint_nodes != _paint_nodes){
      _paint_nodes = do_paint_nodes;
      return true;
    } else {
      return false;
    }
  }

  int size(void) const {
    return _automation.size();
  }

  T* begin() {
    return _automation.begin();
    //return &_automation[0];
  }

  T* end() {
    return _automation.end();
  }

  const T &at(int n) const {
    return _automation.at(n);
  }

  const T &last(void) const {
    R_ASSERT(size()>0);
    return at(size()-1);
  }
  
  double get_value(double time, double time1, double time2, int das_logtype1_das, double value1, double value2) const {
    if (das_logtype1_das==LOGTYPE_LINEAR) {
      if (time1==time2)        
        return (value1 + value2) / 2.0;
      else
        return scale_double(time, time1, time2, value1, value2);
    } else
      return value1;
  }

  double get_value(double time, const T *node1, const T *node2, double (*custom_get_value)(double time, const T *node1, const T *node2) = NULL) const {
    const double time1 = node1->time;
    const double time2 = node2->time;
    
    const int logtype1 = node1->logtype;

    R_ASSERT_NON_RELEASE(time >= time1);
    R_ASSERT_NON_RELEASE(time <= time2);

    if (custom_get_value!=NULL && logtype1==LOGTYPE_LINEAR && time1!=time2)
      return custom_get_value(time, node1, node2);
    else
      return get_value(time, time1, time2, logtype1, node1->value, node2->value);
  }

  double get_value(double time, double (*custom_get_value)(double time, const T *node1, const T *node2) = NULL) const {
    int size = _automation.size();

    if (size==0)
      return 0;

    if (size==1)
      return _automation.at(0).value;

    int nodenum = get_node_num(time);
    if (nodenum==-1)
      return _automation.at(0).value;

    if (nodenum >= size-1)
      return _automation.at(nodenum-1).value;

    return get_value((double)time, &_automation.at(nodenum), &_automation.at(nodenum+1), custom_get_value);
  }

  bool get_value(double time, const T *node1, const T *node2, double &value, double (*custom_get_value)(double time, const T *node1, const T *node2)) const {
    const double time1 = node1->time;
    const double time2 = node2->time;
    
    if (time >= time1 && time < time2){
      value = get_value(time, node1, node2, custom_get_value);
      return true;
    } else {
      return false;
    }
  }


private:

  // Based on pseudocode for the function BinarySearch_Left found at https://rosettacode.org/wiki/Binary_search
  int BinarySearch_Left(const struct RT *rt, double value, int low, int high) const {   // initially called with low = 0, high = N - 1
      // invariants: value  > A[i] for all i < low
      //             value <= A[i] for all i > high
      if (high < low)
        return low;
      
      int mid = (low + high) / 2;
        
      if (rt->nodes[mid].time >= value)
        return BinarySearch_Left(rt, value, low, mid-1);
      else
        return BinarySearch_Left(rt, value, mid+1, high);
  }
  
  int _RT_last_search_pos = 1;

  /*
  bool get_value(double time, double &value, double (*custom_get_value)(double time, const T *node1, const T *node2) = NULL, bool always_set_value = false){
  }
  */

public:

  // Note: Value is not set if rt->num_nodes==0, even if always_set_value==true.  
  SeqAutomationReturnType RT_get_value(double time, double &value, double (*custom_get_value)(double time, const T *node1, const T *node2) = NULL, bool always_set_value = false) {

    R_ASSERT_NON_RELEASE(_RT_last_search_pos > 0);

    RT_AtomicPointerStorage_ScopedUsage rt_pointer(&_rt);
      
    const struct RT *rt = (const struct RT*)rt_pointer.get_pointer();

    if (rt!=NULL) {

      const int num_nodes = rt->num_nodes;
      
      if (num_nodes==0) {
        
        return SeqAutomationReturnType::NO_VALUES;
                
      } else if (time < rt->nodes[0].time){
        
        if (always_set_value)
          value = rt->nodes[0].value;
        
        return SeqAutomationReturnType::NO_VALUES_YET;
        
      } else if (time == rt->nodes[0].time) {
        
        value = rt->nodes[0].value;
        
        return SeqAutomationReturnType::VALUE_OK;

      } else if (num_nodes==1) {
        
        if (always_set_value)
          value = rt->nodes[0].value;
        
        return SeqAutomationReturnType::NO_MORE_VALUES;
        
      } else if (time > rt->nodes[num_nodes-1].time){

        if (always_set_value)
          value = rt->nodes[num_nodes-1].value;
        
        return SeqAutomationReturnType::NO_MORE_VALUES;
        
      } else {
      
        const T *node1;
        const T *node2;
        int i = _RT_last_search_pos;
        
        R_ASSERT_NON_RELEASE(i >= 0);
        
        if (i<num_nodes){
          node1 = &rt->nodes[i-1];
          node2 = &rt->nodes[i];
          if (time >= node1->time && time <= node2->time) // Same position in array as last time. No need to do binary search. This is the path we usually take.
            goto gotit;
        }
        
        i = BinarySearch_Left(rt, time, 0, num_nodes-1);
        R_ASSERT_NON_RELEASE(i>0);
        
        _RT_last_search_pos = i;
        node1 = &rt->nodes[i-1];
        node2 = &rt->nodes[i];
        
      gotit:
        
        value = get_value(time, node1, node2, custom_get_value);
        return SeqAutomationReturnType::VALUE_OK;

      }
    }

    return SeqAutomationReturnType::NO_VALUES;
  }

  /*
  double RT_get_value(double time, double (*custom_get_value)(double time, const T *node1, const T *node2) = NULL){
    double ret;
    if(RT_get_value(time, ret, custom_get_value))
      return ret;
    else
      return 1.0;
  }
  */

  int get_curr_nodenum(void) const {
    return _curr_nodenum;
  }
  
  bool set_curr_nodenum(int nodenum){
    if(nodenum != _curr_nodenum){
      _curr_nodenum = nodenum;
      return true;
    } else {
      return false;
    }
  }

  int get_node_num(double time) const {
    int size = _automation.size();

    for(int i=0;i<size;i++)
      if (time < at(i).time)
        return i-1;

    return size-1;
  }

  int add_node(const T &node){
    double time = node.time;

    R_ASSERT(time >= 0);

    if (time < 0)
      time = 0;
    
    int nodenum = get_node_num(time)+1;
    
    _automation.insert(nodenum, node);

    create_new_rt_data();
    
    return nodenum;
  }


  void delete_node(int nodenum){
    R_ASSERT_RETURN_IF_FALSE(nodenum >= 0);
    R_ASSERT_RETURN_IF_FALSE(nodenum < _automation.size());
    
    _automation.remove(nodenum);

    create_new_rt_data();
  }

  void replace_node(int nodenum, const T &new_node){
    R_ASSERT_RETURN_IF_FALSE(nodenum >= 0);
    R_ASSERT_RETURN_IF_FALSE(nodenum < _automation.size());

    T *node = &_automation[nodenum];

    if (false && node->time != new_node.time){
      _automation.remove(nodenum);
      add_node(new_node);
    } else {
      *node = new_node;
      create_new_rt_data();
    }
            
  }

  void cut_after(double time){
    R_ASSERT_RETURN_IF_FALSE(_automation.size() > 0);
    
    if (time >= _automation.last().time)
      return;
        
    QVector<T> new_automation;
    
    if (time <= 0){
      
      RError("radium::SeqAutomation::cut_after: Time<=0. Time: %f\n", time);
      T node = _automation.at(0);
      node.time = 0;      
      new_automation.push_back(node);
      node.time = 10;
      new_automation.push_back(node);
            
    } else if (_automation.at(0).time >= time || _automation.size()==1) {
      
      T node = _automation.at(0);
      
      node.time = 0;      
      new_automation.push_back(node);
      
      node.time = time;
      new_automation.push_back(node);
      
    } else {
    
      T node1;
      T node2;
      bool got_node1 = false;
      bool got_node2 = false;

      for(const auto &node : _automation){
        
        if (node.time <= time) {
          node1 = node;
          new_automation.push_back(node);
          got_node1 = true;
          
        } else {
          R_ASSERT(got_node1);
          node2 = node;
          got_node2 = true;
          goto gotit;
          
        }
        
      }

      R_ASSERT(false);
      
    gotit:
      if(got_node1==false || got_node2==false){
        R_ASSERT(false);        
      } else {

        T node = node1;
        node.time = time;
        
        if (node.logtype == LOGTYPE_LINEAR)        
          node.value = scale(time, node1.time, node2.time, node1.value, node2.value);
        
        new_automation.push_back(node);
      }
    }

    _automation = new_automation;
    
    create_new_rt_data();    
  }

  void extend_last_node(double new_duration, radium::PlayerLockOnlyIfNeeded *lock){
    R_ASSERT_RETURN_IF_FALSE(_automation.size() > 0);
    
    T &last_node = _automation.last();

    R_ASSERT_RETURN_IF_FALSE(last_node.time < new_duration);
                             
    last_node.time = new_duration;

    {
      radium::PlayerLockOnlyIfNeeded::ScopedLockPause pause(lock);
      create_new_rt_data();
    }
  }
  
  void duration_has_changed(double new_duration, radium::PlayerLockOnlyIfNeeded *lock){
    int size = _automation.size();
    
    R_ASSERT_RETURN_IF_FALSE(size > 0);
      
    T &last_node = _automation.last();
    
    if (new_duration==last_node.time)
      return;
    
    for(T &node : _automation){
      if (node.time > new_duration){
        radium::PlayerLockOnlyIfNeeded::ScopedLockPause pause(lock);
        return cut_after(new_duration);
      }
    }

    if (size > 1){
      
      T second_last_node = _automation.at(size-2);
      
      if (second_last_node.value == last_node.value)
        return extend_last_node(new_duration, lock);
        
    }
    
    {
      radium::PlayerLockOnlyIfNeeded::ScopedLockPause pause(lock);
      
      T new_node = last_node;
      new_node.time = new_duration;
      
      add_node(new_node);
    }
  }
  
  void reset(void){
    _automation.clear();
    create_new_rt_data();
  }

public:

  void print(void){
    for(int i = 0 ; i < _automation.size()-1 ; i++){
      const T &node1 = _automation.at(i);
      const T &node2 = _automation.at(i+1);
      printf("%d: %f -> %f. (%f -> %f)\n", i, node1.value, node2.value, node1.time, node2.time);
    }
  }

  

  const SeqAutomationPainter<T> *get_painter(float x1, float y1, float x2, float y2,
                                       double start_time, double end_time,
                                       const QColor &color,
                                       float (*get_y)(const T &node, float y1, float y2, void *data),
                                       float (*get_x)(const T &node, double start_time, double end_time, float x1, float x2, void *data) = NULL,
                                       void *data = NULL,
                                       const QColor fill_color = QColor(),
                                       float fill_x1 = -1, float fill_x2 = -1                                       
                                       ) const
  {
    const SeqAutomationPainter<T> *painter = new SeqAutomationPainter<T>(_automation,
                                                                         _curr_nodenum, _paint_nodes,
                                                                         x1, y1, x2, y2,
                                                                         start_time, end_time,
                                                                         color,
                                                                         get_y,
                                                                         get_x,
                                                                         data,
                                                                         fill_color,
                                                                         fill_x1, fill_x2);
    if (_last_painter != NULL){
      delete _last_painter;
      _last_painter = painter;
    }

    return painter;
  }

  void paint(QPainter *p,
             float x1, float y1, float x2, float y2,
             double start_time, double end_time,
             const QColor &color,
             float (*get_y)(const T &node, float y1, float y2, void *data),
             float (*get_x)(const T &node, double start_time, double end_time, float x1, float x2, void *data) = NULL,
             void *data = NULL,
             const QColor fill_color = QColor(),
             float fill_x1 = -1, float fill_x2 = -1
             ) const
  {
    const auto *painter = get_painter(x1, y1, x2, y2,
                                      start_time, end_time,
                                      color,
                                      get_y,
                                      get_x,
                                      data,
                                      fill_color,
                                      fill_x1, fill_x2);
    
    painter->paint_fill(p);
    painter->paint_lines(p);
    painter->paint_nodes(p);
  }


  void create_from_state(const dyn_t &dynstate, const NodeFromStateProvider<T> *nsp, double state_samplerate){
    _automation.clear();

    if (dynstate.type==HASH_TYPE) {

      // Old format. When loading old songs.
      
      R_ASSERT(g_is_loading==true);
      
      const hash_t *state = dynstate.hash;
      int size = HASH_get_array_size(state, "node");
      
      for(int i = 0 ; i < size ; i++)
        add_node(nsp->create_node_from_state(HASH_get_hash_at(state, "node", i), state_samplerate));

    } else if (dynstate.type==ARRAY_TYPE) {

      const dynvec_t *vec = dynstate.array;
      
      for(const dyn_t &dyn : vec)
        add_node(nsp->create_node_from_state(dyn.hash, state_samplerate));

    } else {
      
      R_ASSERT(false);
      
    }
  }
  
  void create_from_state(const dyn_t &dynstate, T (*create_node_from_state_func)(hash_t *,double), double state_samplerate){

    struct MyProvider : public NodeFromStateProvider<T> {
      T (*_create_node_from_state_func)(hash_t *,double);
      
      MyProvider(T (*create_node_from_state_func)(hash_t *,double))
        : _create_node_from_state_func(create_node_from_state_func)
      {}
      
      T create_node_from_state(hash_t *state, double state_samplerate) const {
        return _create_node_from_state_func(state, state_samplerate);
      }      
    };

    MyProvider myprovider(create_node_from_state_func);
    
    create_from_state(dynstate, &myprovider, state_samplerate);
  }
  
  dyn_t get_state(hash_t *(*get_node_state)(const T &node, void*), void *data) const {
    int size = _automation.size();
    
    dynvec_t ret = {};
    
    for(int i = 0 ; i < size ; i++)
      DYNVEC_push_back(ret, DYN_create_hash(get_node_state(_automation.at(i), data)));
    
    return DYN_create_array(ret);
  }

};

template <typename T> 
class SeqAutomationIterator{
  const SeqAutomation<T> &_automation;

#if !defined(RELEASE)
  double _prev_time = -1;
#endif

  int _size;
  int _n;
  const T *_node2 = NULL;
  double _time1;
  double _time2;
  double _value1;
  double _value2;
  int _logtype1;

public:
  SeqAutomationIterator(const SeqAutomation<T> &automation)
    : _automation(automation)
    , _size(automation.size())
  {
    R_ASSERT_RETURN_IF_FALSE(_size > 0);

    const T &_node1 = _automation.at(0);
    _time1 = _node1.time;
    _value1 = _node1.value;
    _logtype1 = _node1.logtype;

    _n = 1;

    if (_size >= 2){
      _node2 = &_automation.at(_n);
      _time2 = _node2->time;
      _value2 = _node2->value;
    } else {
      _value2 = _value1; // Used in case there is only one node.
    }
  }

  double get_value(double time){
#if !defined(RELEASE)
    if(time<0)
      abort();
    if(time<=_prev_time)
      abort();
    _prev_time = time;
#endif

    if (_n==_size)
      return _value2;

    if (time < _time1)
      return _value1;

    while(time > _time2){
      _n++;

      if (_n==_size)
        return _value2;

      _time1 = _time2;
      _value1 = _value2;
      _logtype1 = _node2->logtype;

      _node2 = &_automation.at(_n);
      _time2 = _node2->time;
      _value2 = _node2->value;
    }

    return _automation.get_value(time, _time1, _time2, _logtype1, _value1, _value2);
  }
};

}

#endif
