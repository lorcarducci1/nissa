#ifndef _BENCH_HPP
#define _BENCH_HPP

#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#ifndef EXTERN_BENCH
 #define EXTERN_BENCH extern
 #define EQUAL_ZERO
#else
 #define EQUAL_ZERO =0
#endif

namespace nissa
{
  //timings
  EXTERN_BENCH double tot_time EQUAL_ZERO;
  
  EXTERN_BENCH double tot_comm_time EQUAL_ZERO;
  EXTERN_BENCH int ntot_comm EQUAL_ZERO;
  EXTERN_BENCH double cgm_inv_over_time,cg_inv_over_time EQUAL_ZERO;
  EXTERN_BENCH int ncgm_inv,ncg_inv EQUAL_ZERO;
  EXTERN_BENCH double portable_stD_app_time EQUAL_ZERO;
  EXTERN_BENCH int nportable_stD_app EQUAL_ZERO;
  EXTERN_BENCH int nsto EQUAL_ZERO;
  EXTERN_BENCH double sto_time EQUAL_ZERO;
  EXTERN_BENCH int nsto_remap EQUAL_ZERO;
  EXTERN_BENCH double sto_remap_time EQUAL_ZERO;
  EXTERN_BENCH int ngluon_force EQUAL_ZERO;
  EXTERN_BENCH double gluon_force_time EQUAL_ZERO;
  EXTERN_BENCH double remap_time EQUAL_ZERO;
  EXTERN_BENCH int nremap EQUAL_ZERO;
  EXTERN_BENCH double conf_evolve_time EQUAL_ZERO;
  EXTERN_BENCH int nconf_evolve EQUAL_ZERO;
  EXTERN_BENCH double quark_force_over_time EQUAL_ZERO;
  EXTERN_BENCH int nquark_force_over EQUAL_ZERO;
  EXTERN_BENCH double unitarize_time EQUAL_ZERO;
  EXTERN_BENCH int nunitarize EQUAL_ZERO;
  EXTERN_BENCH int perform_benchmark;
 #define NISSA_DEFAULT_PERFORM_BENCHMARK 0
#ifdef BGQ
  EXTERN_BENCH double bgq_stdD_app_time EQUAL_ZERO;
  EXTERN_BENCH int nbgq_stdD_app EQUAL_ZERO;
#endif
  
#define UNPAUSE_TIMING(TIME) if(IS_MASTER_THREAD) TIME-=take_time()
#define RESET_TIMING(TIME,COUNTER) do{if(IS_MASTER_THREAD){TIME=0;COUNTER=0;}}while(0)
#define START_TIMING(TIME,COUNTER) do{if(IS_MASTER_THREAD){TIME-=take_time();COUNTER++;}}while(0)
#define STOP_TIMING(TIME) if(IS_MASTER_THREAD) TIME+=take_time()
  
  void bench_memory_bandwidth(int mem_size);
  void bench_memory_copy(double *out,double *in,int size);
  void bench_net_speed();
  
  const int flops_per_complex_summ=2;
  const int flops_per_complex_prod=6;
  const int flops_per_su3_prod=(NCOL*NCOL/*entries*/*(NCOL*flops_per_complex_prod+(NCOL-1)*flops_per_complex_summ));
  const int flops_per_su3_summ=(NCOL*NCOL/*entries*/*flops_per_complex_summ);
  const int flops_per_link_gauge_tlSym=((28*(NDIM-1)+2/*sq+rect*/+1/*close*/)*flops_per_su3_prod+(11+1/*TA*/)*flops_per_su3_summ);
  const int flops_per_link_gauge_Wilson=((6*(NDIM-1)+1/*close*/)*flops_per_su3_prod+(2+1/*TA*/)*flops_per_su3_summ);
}

#undef EXTERN_BENCH
#undef EQUAL_ZERO

#endif
