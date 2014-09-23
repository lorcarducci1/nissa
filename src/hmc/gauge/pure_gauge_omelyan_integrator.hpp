#ifndef _PURE_GAUGE_OMELYAN_INTEGRATOR_HPP
#define _PURE_GAUGE_OMELYAN_INTEGRATOR_HPP

namespace nissa
{
  void evolve_momenta_with_pure_gauge_force(quad_su3 *H,quad_su3 *conf,theory_pars_t *theory_pars,double dt,quad_su3 *ext_F=NULL);
  void omelyan_pure_gauge_evolver(quad_su3 *H,quad_su3 *conf,theory_pars_t *theory_pars,hmc_evol_pars_t *simul);
}

#endif