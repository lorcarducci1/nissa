#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include "smooth.hpp"
#include "operations/gaugeconf.hpp"
#include "operations/su3_paths/gauge_sweeper.hpp"

namespace nissa
{
  //smooth a configuration until measure is due
  bool smooth_lx_conf_until_next_meas(quad_su3 *smoothed_conf,smooth_pars_t &sp,double &t,double &tnext_meas)
  {
    bool finished=1;
    while(t+1e-10<tnext_meas)
      switch(sp.method)
	{
	case smooth_pars_t::COOLING:cool_lx_conf(smoothed_conf,get_sweeper(sp.cool.gauge_action));t++;finished=(t>=sp.cool.nsteps);break;
	case smooth_pars_t::STOUT:stout_smear_single_level(smoothed_conf,smoothed_conf,sp.stout.rho);t++;finished=(t>=sp.stout.nlevels);break;
	case smooth_pars_t::WFLOW:Wflow_lx_conf(smoothed_conf,sp.Wflow.dt);t+=sp.Wflow.dt;finished=(t>=sp.Wflow.T);break;
	case smooth_pars_t::HYP:hyp_smear_conf(smoothed_conf,smoothed_conf,sp.hyp.alpha0,sp.hyp.alpha1,sp.hyp.alpha2);t+=1;finished=(t>=sp.hyp.nlevels);break;
	case smooth_pars_t::APE:ape_smear_conf(smoothed_conf,smoothed_conf,sp.ape.alpha,1);t+=1;finished=(t>=sp.ape.nlevels);break;
	case smooth_pars_t::UNSPEC_SMOOTH_METHOD:crash("should not arrive here");
	}
    tnext_meas+=sp.meas_each;
    
    return finished;
  }
  
  std::string smooth_pars_t::get_str(bool full)
  {
    std::ostringstream os;
    
    if(full||is_nonstandard())
      {
	if(full||method!=def_method()||
	   (method==COOLING&&cool.is_nonstandard())||
	   (method==STOUT&&stout.is_nonstandard())||
	   (method==WFLOW&&Wflow.is_nonstandard())||
	   (method==APE&&ape.is_nonstandard())||
	   (method==HYP&&hyp.is_nonstandard()))
	  {
	    os<<" /* alternatives: Cooling, Stout, WFlow, Ape, Hyp */\n";
	    os<<" SmoothMethod\t=\t";
	    switch(method)
	      {
	      case COOLING: os<<cool.get_str(full);break;
	      case STOUT: os<<stout.get_str(full);break;
	      case WFLOW: os<<Wflow.get_str(full);break;
	      case APE: os<<ape.get_str(full);break;
	      case HYP: os<<hyp.get_str(full);break;
	      case UNSPEC_SMOOTH_METHOD: crash("unspecified");
	      }
	  }
	if(full||meas_each!=def_meas_each()) os<<" MeasEach\t=\t"<<def_meas_each()<<"\n";
      }
    
    return os.str();
  }
}
