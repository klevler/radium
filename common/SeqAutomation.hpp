
#include <QVector>
#include <QPainter>


namespace radium{

template <typename T> class SeqAutomation{
  
private:
  
  QVector<T> _automation;

  struct RT{
    int num_nodes;
    T nodes[];
  };

  int _curr_nodenum = -1;
  
  const RT _rt_empty = {0};

  DEFINE_ATOMIC(const RT*, _rt) = &_rt_empty;

  const RT *_rt_gc[2] = {NULL};

  SeqAutomation(){
    GC_add_roots(&_rt_gc[0], &_rt_gc[2]);
  }
  ~SeqAutomation(){
    GC_remove_roots(&_rt_gc[0], &_rt_gc[2]);
  }

  
  int get_size(int num_nodes) const{
    return sizeof(struct RT) + num_nodes*sizeof(T);
  }

  struct RT *create_rt(void) const{
    struct RT *rt = (struct TempoAutomation*)talloc(get_size(_automation.size()));
  
    rt->num_nodes=_automation.size();
    
    for(int i=0 ; i<_automation.size() ; i++)
      rt->nodes[i] = _automation.at(i);
    
    return rt;
  }

  void set_rt_tempo_automation(void){
    const struct TempoAutomation *new_rt_tempo_automation = (const struct RT*)create_rt();
    
    _rt_gc[0] = ATOMIC_GET(_rt); // store previous _rt so it doesn't disappear.
    
    ATOMIC_SET(_rt, new_rt_tempo_automation);
  }


public:

  int size(void) const{
    return _automation.size();
  }

  const Node &get(int n) const{
    return _automation.at(n);
  }

  Node &get_mutable(int n){
    return _automation.at(n);
  }

  double RT_get_abstime(const T &node) const;
  double RT_get_value(const T &node) const;
  double RT_get_value(double abstime, const T &node1, const T &node2) const;

  float get_y(const T &node, float y1, float y2) const;
  int RT_get_logtype(const T &node) const;

  double RT_get_value(double abstime) const{

    // Ensure the tempo_automation we are working on won't be gc-ed while using it.
    // It probably never happens though, there shouldn't be time for it to
    // be both found, freed, and reused, in such a short time, but now we
    // don't have to worry about the possibility.
    _rt_gc[1] = ATOMIC_GET(_rt);
    
    const struct RT *rt = _rt_gc[1];
    
    for(int i = 0 ; i < rt->num_nodes-1 ; i++){
      const T &node1 = rt->nodes[i];
      const T &node2 = rt->nodes[i+1];
      double abstime1 = RT_get_abstime(node1);
      double abstime2 = RT_get_abstime(node2);

      if (abstime >= abstime1 && abstime < abstime2)
        return RT_get_value(abstime, node1, node2);
      /*
        if (node1.logtype==LOGTYPE_LINEAR)
          return scale(abstime, abstime1, abstime2, node1.value, node2.value);
        else
          return node1.value;
      */
    }

    return 1.0; //rt_tempo_automation->nodes[rt_tempo_automation->num-1].value;
  }


  void set_curr_nodenum(int nodenum){
    _curr_nodenum = nodenum;
  }

  int get_node_num(double abstime) const {
    double abstime1 = RT_get_abstime(get(0));
    //R_ASSERT_RETURN_IF_FALSE2(abstime1==0,1);
    
    int size = _automation.size();
    if (size==1){
      return 1;
    }
    
    for(int i=1;i<size;i++){
      double abstime2 = RT_get_abstime(get(i));
      
      if (abstime >= abstime1 && abstime < abstime2)
        return i;
      
      abstime1 = abstime2;
    }
    
    return size;
  }

  int add_node(const T &node){
    double abstime = RT_get_abstime(node);

    R_ASSERT(abstime >= 0);

    if (abstime < 0)
      abstime = 0;
    
    int nodenum = get_node_num(abstime);
    
    _automation.insert(nodenum, node);
    
    set_rt_tempo_automation();
    
    return nodenum;
  }


  void delete_node(int nodenum){
    R_ASSERT_RETURN_IF_FALSE(nodenum >= 0);
    R_ASSERT_RETURN_IF_FALSE(nodenum < _automation.size());
    
    _automation.remove(nodenum);
  
    set_rt_tempo_automation();
  }

  void replace_node(int nodenum, const T &new_node){
    R_ASSERT_RETURN_IF_FALSE(nodenum >= 0);
    R_ASSERT_RETURN_IF_FALSE(nodenum < _automation.size());
    
    _automation.remove(nodenum);

    add_node(new_node);
  }

  void reset(void){
    _automation.clear();
    set_rt_tempo_automation();
  }


private:

  QColor get_color(QColor col1, QColor col2, int mix, float alpha){
    QColor ret = mix_colors(col1, col2, (float)mix/1000.0);
    ret.setAlphaF(alpha);
    return ret;
  }

  void paint_node(QPainter *p, float x, float y, int nodenum){ //const TempoAutomationNode &node){
    float minnodesize = root->song->tracker_windows->fontheight / 1.5; // if changing 1.5 here, also change 1.5 in getHalfOfNodeWidth in api_mouse.c and OpenGL/Render.cpp
    float x1 = x-minnodesize;
    float x2 = x+minnodesize;
    float y1 = y-minnodesize;
    float y2 = y+minnodesize;
    const float width = 1.2;
    
    static QPen pen1,pen2,pen3,pen4;
    static QBrush fill_brush;
    static bool has_inited = false;
    
    if(has_inited==false){
      QColor color = QColor(80,40,40);
      
      fill_brush = QBrush(get_color(color, Qt::white, 300, 0.3));
      
      pen1 = QPen(get_color(color, Qt::white, 100, 0.3));
      pen1.setWidth(width);
      
      pen2 = QPen(get_color(color, Qt::black, 300, 0.3));
      pen2.setWidth(width);
      
      pen3 = QPen(get_color(color, Qt::black, 400, 0.3));
      pen3.setWidth(width);
      
      pen4 = QPen(get_color(color, Qt::white, 300, 0.3));
      pen4.setWidth(width);
      
      has_inited=true;
    }
    
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

public:

  void paint(QPainter *p, float x1, float y1, float x2, float y2, double start_time, double end_time) const {
  
    QPen pen(QColor(200,200,200));
    pen.setWidth(2.3);
    
    for(int i = 0 ; i < _automation.size()-1 ; i++){
      const T &node1 = _automation.at(i);
      double abstime1 = RT_get_abstime(node1);

      if (abstime1 >= end_time)
        break;
      
      const T &node2 = _automation.at(i+1);
      double abstime2 = RT_get_abstime(node2);

      if (abstime2 < start_time)
        continue;
      
      float x_a = scale(abstime1, start_time, end_time, x1, x2);
      float x_b = scale(abstime2, start_time, end_time, x1, x2);
      
      float y_a = get_y(node1, y1, y2);
      float y_b = get_y(node2, y1, y2);
      
      //printf("y_a: %f, y1: %f, y2: %f\n", 
      
      p->setPen(pen);
      
      if (RT_get_logtype(node1)==LOGTYPE_HOLD){
        QLineF line1(x_a, y_a, x_b, y_a);
        p->drawLine(line1);
        
        QLineF line2(x_b, y_a, x_b, y_b);
        p->drawLine(line2);
        
      } else {
        
        QLineF line(x_a, y_a, x_b, y_b);
        p->drawLine(line);
        
      }
      
      paint_node(p, x_a, y_a, i);
      
      if(i==_automation.size()-2)
        paint_node(p, x_b, y_b, i+1);
    }
  }

};

}