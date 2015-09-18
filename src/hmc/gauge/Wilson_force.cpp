#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include "base/global_variables.hpp"
#include "base/thread_macros.hpp"
#include "base/vectors.hpp"
#include "new_types/new_types_definitions.hpp"
#include "new_types/su3.hpp"
#include "operations/su3_paths/squared_staples.hpp"
#include "routines/ios.hpp"
#ifdef USE_THREADS
 #include "routines/thread.hpp"
#endif

namespace nissa
{
  //Compute the gluonic force for the Wilson plaquette action and summ to the output
  //Passed conf must NOT contain the backfield.
  //Of the result still need to be taken the TA
  THREADABLE_FUNCTION_4ARG(Wilson_force_eo_conf, quad_su3**,F, quad_su3**,eo_conf, double,beta, bool,phase_pres)
  {
    GET_THREAD_ID();
    
    verbosity_lv1_master_printf("Computing Wilson force (eo)\n");
    
    double r=-beta/NCOL;
    if(phase_pres) r=-r; //(-1)^area
    compute_summed_squared_staples_eo_conf(F,eo_conf);
    
    for(int par=0;par<2;par++)
      {
	NISSA_PARALLEL_LOOP(ivol,0,loc_volh)
	  for(int mu=0;mu<NDIM;mu++)
	    safe_su3_hermitian_prod_double(F[par][ivol][mu],F[par][ivol][mu],r);
	set_borders_invalid(F[par]);
      }
  }
  THREADABLE_FUNCTION_END
  
  //lx version
  THREADABLE_FUNCTION_4ARG(Wilson_force_lx_conf, quad_su3*,F, quad_su3*,conf, double,beta, bool,phase_pres)
  {
    GET_THREAD_ID();
    
    verbosity_lv1_master_printf("Computing Wilson force (lx)\n");
    
    double r=-beta/NCOL;
    if(phase_pres) r=-r; //(-1)^area
    compute_summed_squared_staples_lx_conf(F,conf);
    
    NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
      for(int mu=0;mu<NDIM;mu++)
	safe_su3_hermitian_prod_double(F[ivol][mu],F[ivol][mu],r);
    
    set_borders_invalid(F);
  }
  THREADABLE_FUNCTION_END
}
