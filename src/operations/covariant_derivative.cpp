#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include "base/global_variables.hpp"
#include "base/thread_macros.hpp"
#include "base/vectors.hpp"
#include "communicate/communicate.hpp"
#include "new_types/dirac.hpp"
#include "new_types/su3.hpp"
#ifdef USE_THREADS
 #include "routines/thread.hpp"
#endif

namespace nissa
{
#define APPLY_NABLA_I(TYPE)                                             \
  THREADABLE_FUNCTION_4ARG(apply_nabla_i, TYPE*,out, TYPE*,in, quad_su3*,conf, int,mu) \
  {                                                                     \
    GET_THREAD_ID();                                                    \
                                                                        \
    NAME3(communicate_lx,TYPE,borders)(in);                             \
    communicate_lx_quad_su3_borders(conf);                              \
                                                                        \
    TYPE *temp=nissa_malloc("temp",loc_vol+bord_vol,TYPE);              \
    vector_reset(temp);                                                 \
                                                                        \
    NISSA_PARALLEL_LOOP(ix,0,loc_vol)                                   \
      {                                                                 \
        int Xup,Xdw;                                                    \
        Xup=loclx_neighup[ix][mu];					\
        Xdw=loclx_neighdw[ix][mu];					\
									\
        NAME2(unsafe_su3_prod,TYPE)(      temp[ix],conf[ix][mu] ,in[Xup]); \
        NAME2(su3_dag_subt_the_prod,TYPE)(temp[ix],conf[Xdw][mu],in[Xdw]); \
      }                                                                 \
    									\
    vector_copy(out,temp);                                              \
    nissa_free(temp);                                                   \
                                                                        \
    set_borders_invalid(out);                                           \
  }									\
  THREADABLE_FUNCTION_END
  
  //instantiate the application function
  APPLY_NABLA_I(spincolor)
  APPLY_NABLA_I(colorspinspin)
  APPLY_NABLA_I(su3spinspin)
    
  void insert_external_source_handle(complex out,spin1field *aux,int ivol,int mu,void *pars)
  {if(aux) complex_copy(out,aux[ivol][mu]);else complex_put_to_real(out,1);}
  void insert_tadpole_handle(complex out,spin1field *aux,int ivol,int mu,void *pars){out[RE]=((double*)pars)[mu];out[IM]=0;}
  void insert_conserved_current_handle(complex out,spin1field *aux,int ivol,int mu,void *pars){out[RE]=((int*)pars)[mu];out[IM]=0;}
  
#define DEF_TM_GAMMA(r) dirac_matr GAMMA;dirac_prod_idouble(&GAMMA,base_gamma+5,-tau3[r])
  
#define INSERT_VECTOR_VERTEX(TYPE)					\
  /*insert the operator:  \sum_mu  [*/					\
  /* f_fw * ( GAMMA - gmu) A_{x,mu} U_{x,mu} \delta{x',x+mu} + f_bw * ( GAMMA + gmu) A_{x-mu,mu} U^+_{x-mu,mu} \delta{x',x-mu}]*/ \
  /* for tm GAMMA should be -i g5 tau3[r], defined through the macro above, for Wilson id */		\
  void insert_vector_vertex(TYPE *out,quad_su3 *conf,spin1field *curr,TYPE *in,complex fact_fw,complex fact_bw,dirac_matr *GAMMA,void(*get_curr)(complex,spin1field*,int,int,void*),int t,void *pars=NULL) \
  {									\
  GET_THREAD_ID();							\
									\
  /*reset the output and communicate borders*/				\
  vector_reset(out);							\
  NAME3(communicate_lx,TYPE,borders)(in);				\
  communicate_lx_quad_su3_borders(conf);				\
  if(curr) communicate_lx_spin1field_borders(curr);			\
									\
  NISSA_PARALLEL_LOOP(ivol,0,loc_vol)					\
    if(t==-1||glb_coord_of_loclx[ivol][0]==t)				\
    for(int mu=0;mu<NDIM;mu++)						\
      {									\
	/*find neighbors*/						\
	int ifw=loclx_neighup[ivol][mu];				\
	int ibw=loclx_neighdw[ivol][mu];				\
									\
	/*transport down and up*/					\
	TYPE fw,bw;							\
	NAME2(unsafe_su3_prod,TYPE)(fw,conf[ivol][mu],in[ifw]);		\
	NAME2(unsafe_su3_dag_prod,TYPE)(bw,conf[ibw][mu],in[ibw]);	\
									\
	/*weight the two pieces*/					\
	NAME2(TYPE,prodassign_complex)(fw,fact_fw);			\
	NAME2(TYPE,prodassign_complex)(bw,fact_bw);			\
									\
	/*insert the current*/						\
	complex fw_curr,bw_curr;					\
	get_curr(fw_curr,curr,ivol,mu,pars);				\
	get_curr(bw_curr,curr,ibw,mu,pars);				\
	NAME2(TYPE,prodassign_complex)(fw,fw_curr);			\
	NAME2(TYPE,prodassign_complex)(bw,bw_curr);			\
									\
	/*summ and subtract the two*/					\
	TYPE bw_M_fw,bw_P_fw;						\
	NAME2(TYPE,subt)(bw_M_fw,bw,fw);				\
	NAME2(TYPE,summ)(bw_P_fw,bw,fw);				\
									\
	/*put GAMMA on the summ*/					\
	TYPE GAMMA_bw_P_fw;						\
	NAME2(unsafe_dirac_prod,TYPE)(GAMMA_bw_P_fw,GAMMA,bw_P_fw);	\
	NAME2(TYPE,summassign)(out[ivol],GAMMA_bw_P_fw);		\
									\
	/*put gmu on the difference*/					\
	TYPE gmu_bw_M_fw;						\
	NAME2(unsafe_dirac_prod,TYPE)(gmu_bw_M_fw,base_gamma+map_mu[mu],bw_M_fw); \
	NAME2(TYPE,summassign)(out[ivol],gmu_bw_M_fw);			\
      }									\
  									\
  set_borders_invalid(out);						\
  }									\
  									\
  /*insert the tadpole*/						\
  THREADABLE_FUNCTION_6ARG(insert_tadpole, TYPE*,out, quad_su3*,conf, TYPE*,in, dirac_matr*,GAMMA, double*,tad, int,t) \
  {									\
    /*call with no source insertion, plus between fw and bw, and a global -0.25*/ \
    complex fw_factor={-0.25,0},bw_factor={-0.25,0};	/* see below for hte minus convention*/ \
    insert_vector_vertex(out,conf,NULL,in,fw_factor,bw_factor,GAMMA,insert_tadpole_handle,t,tad); \
  }									\
  THREADABLE_FUNCTION_END						\
  void insert_wilson_tadpole(TYPE *out,quad_su3 *conf,TYPE *in,double *tad,int t){insert_tadpole(out,conf,in,base_gamma+0,tad,t);} \
  void insert_tm_tadpole(TYPE *out,quad_su3 *conf,TYPE *in,int r,double *tad,int t){DEF_TM_GAMMA(r); insert_tadpole(out,conf,in,&GAMMA,tad,t);} \
  									\
  /*insert the external source, that is one of the two extrema of the stoch prop*/ \
  THREADABLE_FUNCTION_6ARG(insert_external_source, TYPE*,out, quad_su3*,conf, spin1field*,curr, TYPE*,in, dirac_matr*,GAMMA, int,t) \
  {									\
    /*call with source insertion, minus between fw and bw, and a global -i*0.5 - the minus comes from definition in eq.11 of 1303.4896*/ \
    complex fw_factor={0,-0.5},bw_factor={0,+0.5};			\
    insert_vector_vertex(out,conf,curr,in,fw_factor,bw_factor,GAMMA,insert_external_source_handle,t); \
  }									\
  THREADABLE_FUNCTION_END						\
  void insert_wilson_external_source(TYPE *out,quad_su3 *conf,spin1field *curr,TYPE *in,int t){insert_external_source(out,conf,curr,in,base_gamma+0,t);} \
  void insert_tm_external_source(TYPE *out,quad_su3 *conf,spin1field *curr,TYPE *in,int r,int t){DEF_TM_GAMMA(r);insert_external_source(out,conf,curr,in,&GAMMA,t);} \
									\
  /*insert the conserved time current*/ \
  THREADABLE_FUNCTION_6ARG(insert_conserved_current, TYPE*,out, quad_su3*,conf, TYPE*,in, dirac_matr*,GAMMA, int*,dirs, int,t) \
  {									\
    /*call with no source insertion, minus between fw and bw, and a global 0.5*/ \
    complex fw_factor={-0.5,0},bw_factor={+0.5,0}; /* follow eq.11.43 of Gattringer*/		\
    insert_vector_vertex(out,conf,NULL,in,fw_factor,bw_factor,GAMMA,insert_conserved_current_handle,t,dirs); \
  }									\
  THREADABLE_FUNCTION_END						\
  void insert_wilson_conserved_current(TYPE *out,quad_su3 *conf,TYPE *in,int *dirs,int t){insert_conserved_current(out,conf,in,base_gamma+0,dirs,t);} \
  void insert_tm_conserved_current(TYPE *out,quad_su3 *conf,TYPE *in,int r,int *dirs,int t){DEF_TM_GAMMA(r);insert_conserved_current(out,conf,in,&GAMMA,dirs,t);} \
									\
  /*multiply with gamma*/						\
  THREADABLE_FUNCTION_4ARG(prop_multiply_with_gamma, TYPE*,out, int,ig, TYPE*,in, int,twall) \
  {									\
    GET_THREAD_ID();							\
    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)					\
      {									\
	NAME2(safe_dirac_prod,TYPE)(out[ivol],base_gamma+ig,in[ivol]); \
	NAME2(TYPE,prodassign_double)(out[ivol],(twall==-1||glb_coord_of_loclx[ivol][0]==twall)); \
      }									\
    set_borders_invalid(out);						\
  }									\
  THREADABLE_FUNCTION_END						\
  									\
  /*multiply with an imaginary factor*/					\
  THREADABLE_FUNCTION_2ARG(prop_multiply_with_idouble, TYPE*,out, double,f) \
  {									\
    GET_THREAD_ID();							\
    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)					\
      NAME2(TYPE,prodassign_idouble)(out[ivol],f);			\
    set_borders_invalid(out);						\
  }									\
  THREADABLE_FUNCTION_END
  
  INSERT_VECTOR_VERTEX(spincolor)
  INSERT_VECTOR_VERTEX(colorspinspin)
  INSERT_VECTOR_VERTEX(su3spinspin)
}
