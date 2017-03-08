
#if !defined(_RADIUM_COMMON_DYNVEC_PROC_H)
#define _RADIUM_COMMON_DYNVEC_PROC_H

extern LANGSPEC bool DYNVEC_equal(dynvec_t *v1, dynvec_t *v2);

static inline void DYNVEC_ensure_space_for_one_more_element(dynvec_t *v){
  const int num_elements = v->num_elements;

  if(num_elements==v->num_elements_allocated){
    if(num_elements==0){
#ifdef TEST_VECTOR
      const int num_init = 2;
#else
      const int num_init = 8;
#endif
      v->num_elements_allocated = num_init;
      v->elements = (dyn_t*)talloc(num_init*(int)sizeof(dyn_t));
    }else{
      const int num_elements_allocated = num_elements * 2;
      v->num_elements_allocated = num_elements_allocated;
      v->elements = (dyn_t*)talloc_realloc(v->elements,num_elements_allocated*(int)sizeof(dyn_t));
    }
  }
  
}

static inline int DYNVEC_push_back(dynvec_t *v, const dyn_t element){
#if 0 //ifndef RELEASE
  R_ASSERT(element!=NULL);
#endif

  DYNVEC_ensure_space_for_one_more_element(v);
  
  const int num_elements = v->num_elements;

  v->elements[num_elements] = element;
  v->num_elements = num_elements+1;
  
  return num_elements;
}


#endif
