#include "nissa.hpp"

using namespace nissa;

tm_quark_info le;
double lep_energy,neu_energy;

//compute phase exponent for space part: vec{p}*\vec{x}
double get_space_arg(int ivol,momentum_t bc)
{
  double arg=0;
  for(int mu=1;mu<4;mu++)
    {
      double step=bc[mu]*M_PI/glb_size[mu];
      arg+=step*glb_coord_of_loclx[ivol][mu];
    }
  return arg;
}

//compute the phase for lepton on its sink
void get_lepton_sink_phase_factor(complex out,int ivol)
{
  //compute space and time factor
  double arg=get_space_arg(ivol,le.bc);
  int t=glb_coord_of_loclx[ivol][0];
  double ext=exp(t*lep_energy);
  
  //compute full exponential (notice the factor -1)
  out[RE]=cos(-arg)*ext;
  out[IM]=sin(-arg)*ext;
}

//set everything to a phase factor
void set_to_lepton_sink_phase_factor(spinspin *prop,int twall)
{
  GET_THREAD_ID();
  
  vector_reset(prop);
  NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
    if(twall==-1||glb_coord_of_loclx[ivol][0]==twall)
      //if(glb_coord_of_loclx[ivol][0]==0)
      {
	complex ph;
	get_lepton_sink_phase_factor(ph,ivol);
	spinspin_put_to_diag(prop[ivol],ph);
      }
  set_borders_invalid(prop);
}

//compute the phase for antineutrino - the orientation is that of the muon (as above)
void get_antineutrino_source_phase_factor(complex out,int ivol,momentum_t bc)
{
  //compute space and time factor
  double arg=get_space_arg(ivol,bc);
  int t=glb_coord_of_loclx[ivol][0];
  if(t>=glb_size[0]/2) t=glb_size[0]-t;
  double ext=exp(t*neu_energy);
  
  //compute full exponential (notice the factor +1)
  out[RE]=cos(+arg)*ext;
  out[IM]=sin(+arg)*ext;
}

//computing the lepton circle, without the contact term
void compute_lepton_circle_without_contact_term_latt_num(complex lcwct_ln)
{
  spinspin promu,pronu;
  twisted_on_shell_operator_of_imom(promu,le,0,false,-1);
  momentum_t tempbc;
  tempbc[0]=le.bc[0];
  for(int mu=1;mu<NDIM;mu++) tempbc[mu]=-le.bc[mu];
  naive_massless_on_shell_operator_of_imom(pronu,tempbc,0,-1);
  spinspin t;
  unsafe_dirac_prod_spinspin(t,base_gamma+map_mu[0],pronu);
  safe_spinspin_prod_spinspin(t,promu,t);
  trace_dirac_prod_spinspin(lcwct_ln,base_gamma+map_mu[0],t);
}

double compute_lepton_circle_without_contact_term_latt_form()
{
  double mo=le.bc[1]*M_PI/glb_size[1];
  return 4*(-3*sqr(sin(mo))+sinh(lep_energy)*sinh(neu_energy));
}

double compute_lepton_circle_cont()
{
  double summu=lep_energy+neu_energy;
  return 2*sqr(le.mass)*(1-sqr(le.mass/summu));
}

THREADABLE_FUNCTION_0ARG(compute_lepton_free_loop)
{
  GET_THREAD_ID();
  
  FILE *fout=open_file("corr_l_free","w");
  
  spinspin *prop=nissa_malloc("prop",loc_vol+bord_vol,spinspin);
  complex *corr=nissa_malloc("corr",glb_size[0],complex);
  
  //put it to a phase
  int twall=glb_size[0]/2;
  set_to_lepton_sink_phase_factor(prop,twall);
  
  //multiply with the prop
  multiply_from_right_by_x_space_twisted_propagator_by_fft(prop,prop,le);
  
  //get the projectors
  spinspin promu[2],pronu[2];
  twisted_on_shell_operator_of_imom(promu[0],le,0,false,-1);
  twisted_on_shell_operator_of_imom(promu[1],le,0,false,+1);
  naive_massless_on_shell_operator_of_imom(pronu[0],le.bc,0,-1);
  naive_massless_on_shell_operator_of_imom(pronu[1],le.bc,0,+1);
  
  //compute the right part of the leptonic loop: G0 G^dag
  const int nhadrolept_proj=2,hadrolept_projs[nhadrolept_proj]={9,4};
  dirac_matr hadrolept_proj_gamma[nhadrolept_proj];
  for(int ig_proj=0;ig_proj<nhadrolept_proj;ig_proj++)
    {
      int ig=hadrolept_projs[ig_proj];
      dirac_matr temp_gamma;
      dirac_herm(&temp_gamma,base_gamma+ig);
      dirac_prod(hadrolept_proj_gamma+ig_proj,base_gamma+map_mu[0],&temp_gamma);
    }
  
  //compare result of form with numerical evaluation
  complex lcwct_ln;
  compute_lepton_circle_without_contact_term_latt_num(lcwct_ln);
  master_printf("lepton circle without contact term (numerical): %+16.16lg \n",lcwct_ln[RE]);
  double lcwct_lf=compute_lepton_circle_without_contact_term_latt_form();
  master_printf("lepton circle without contact term (form):      %+16.16lg \n",lcwct_lf);
  double lc_c=compute_lepton_circle_cont();
  master_printf("lepton circle in the continuum:                 %+016.016lg\n",lc_c);
  
  const int nweak_ins=2;
  int list_weak_insl[nweak_ins]={4,9};
  for(int ins=0;ins<nweak_ins;ins++)
    {
      //define a local storage
      spinspin l_loc_corr[loc_size[0]];
      for(int i=0;i<loc_size[0];i++) spinspin_put_to_zero(l_loc_corr[i]);
      
      NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
	{
	  int t=loc_coord_of_loclx[ivol][0];
	  
	  //multiply lepton side on the right (source) side
	  spinspin l;
	  unsafe_spinspin_prod_dirac(l,prop[ivol],base_gamma+list_weak_insl[ins]);
	  
	  //add the neutrino phase
	  complex ph;
	  get_antineutrino_source_phase_factor(ph,ivol,le.bc);
	  spinspin_summ_the_complex_prod(l_loc_corr[t],l,ph);
	}
      glb_threads_reduce_double_vect((double*)l_loc_corr,loc_size[0]*sizeof(spinspin)/sizeof(double));
      
      //save projection on LO
      for(int ig_proj=0;ig_proj<nhadrolept_proj;ig_proj++)
	{
	  vector_reset(corr);
	  NISSA_PARALLEL_LOOP(loc_t,0,loc_size[0])
	    {
	      int glb_t=loc_t+glb_coord_of_loclx[0][0];
	      int ilnp=(glb_t>=glb_size[0]/2); //select the lepton/neutrino projector
	      
	      spinspin td;
	      unsafe_spinspin_prod_spinspin(td,l_loc_corr[loc_t],pronu[ilnp]);
	      //unsafe_dirac_prod_spinspin(td,base_gamma+list_weak_insl[ins],pronu[ilnp]);
	      spinspin dtd;
	      unsafe_spinspin_prod_spinspin(dtd,promu[ilnp],td);
	      trace_spinspin_with_dirac(corr[glb_t],dtd,hadrolept_proj_gamma+ig_proj);
	    }
	  THREAD_BARRIER();
	  
	  if(IS_MASTER_THREAD)
	    {
	      glb_nodes_reduce_complex_vect(corr,glb_size[0]);
	      master_fprintf(fout," # ins=%s, ig_proj=%s\n\n",gtag[list_weak_insl[ins]],gtag[hadrolept_projs[ig_proj]]);
	      for(int t=0;t<glb_size[0];t++)
		{
		  int mt=(t<glb_size[0]/2?t:glb_size[0]-t);
		  double n=exp(-mt*(lep_energy+neu_energy))/glb_vol*glb_size[0];
		  //n=1;
		  master_fprintf(fout,"%+016.016lg %+016.016lg\n",corr[t][0]*n,corr[t][1]*n);
		}
	      master_fprintf(fout,"\n");
	    }
	}
    }
  
  nissa_free(prop);
  nissa_free(corr);
  close_file(fout);
}
THREADABLE_FUNCTION_END

void in_main(int narg,char **arg)
{
  if(narg<2) crash("use %s nx",arg[0]);
  int X=atoi(arg[1]);
  int T=2*X,L=X;
  init_grid(T,L);
  glb_size[0]=T;
  for(int mu=1;mu<4;mu++) glb_size[mu]=L;
  
  start_loc_rnd_gen(1000);
  
  //testing the new fourier transform
  spinspin *test=nissa_malloc("test",loc_vol,spinspin);
  spinspin *test2=nissa_malloc("test2",loc_vol,spinspin);
  GET_THREAD_ID();
  NISSA_PARALLEL_LOOP(ivol,0,loc_vol)
    {
      momentum_t bc={0,2,0,0};
      double a=0;
      for(int mu=0;mu<NDIM;mu++) a+=bc[mu]*glb_coord_of_loclx[ivol][mu]*M_PI/glb_size[mu];
      //complex_put_to_real(test[ivol],rnd_get_unif(loc_rnd_gen+ivol,-glb_vol,glb_vol));
      complex temp;
      complex_iexp(temp,a);
      spinspin_put_to_diag(test[ivol],temp);
    }
  set_borders_invalid(test);
  vector_copy(test2,test);
  int d[4]={1,1,1,1};
  fft4d((complex*)test,(complex*)test,d,sizeof(spinspin)/sizeof(complex),1,1);
  for(int irank=0;irank<nranks;irank++)
    {
      if(rank==irank)
	for(int ivol=0;ivol<loc_vol;ivol++)
	  {
	    double a=test[ivol][0][0][RE];
	    double b=test[ivol][0][0][IM];
	    double c=test2[ivol][0][0][0];
	    //if(a!=b)
	    //crash("obtained %d while expecting %d",a,b);
	    printf("%d %lg\t %lg %lg ANNA\n",glblx_of_loclx[ivol],c,a,b);
	  }
      MPI_Barrier(MPI_COMM_WORLD);
    }
  nissa_free(test);
  nissa_free(test2);
  
  //prepare the quark info
  le.r=1;
  le.mass=0.6465/sqrt(glb_size[0]);
  le.kappa=0.125;
  le.bc[0]=1;
  for(int mu=1;mu<4;mu++) le.bc[mu]=0.03423546*sqrt(glb_size[0]);
  //compute energies
  lep_energy=tm_quark_energy(le,0);
  neu_energy=naive_massless_quark_energy(le.bc,0);
  master_printf("mcrit: %lg, Emu: %+016.016lg, Enu: %+016.016lg %lg\n",m0_of_kappa(le.kappa),lep_energy,neu_energy,lep_energy/neu_energy);
  
  compute_lepton_free_loop();
  pure_loop();
}

int main(int narg,char **arg)
{
  init_nissa_threaded(narg,arg,in_main);
  close_nissa();
  
  return 0;
}