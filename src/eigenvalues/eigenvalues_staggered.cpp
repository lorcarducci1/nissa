#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include <dirac_operators/stD/dirac_operator_stD.hpp>
#include <eigenvalues/eigenvalues.hpp>
#include <geometry/geometry_eo.hpp>
#include <geometry/geometry_mix.hpp>
#include <hmc/backfield.hpp>

namespace nissa
{
  //computes the spectrum of the staggered operator
  void find_eigenvalues_staggered_D2ee(color **eigvec,complex *eigval,int neigs,bool min_max,quad_su3 **conf,quad_u1 **u1b,double mass2,double residue,int wspace_size)
  {
    add_backfield_with_stagphases_to_conf(conf,u1b);
    
    color *temp=nissa_malloc("temp",loc_volh+bord_volh,color);
    
    //Application of the staggered Operator
    const auto imp_mat=[conf,&temp,mass2](complex *out_e,complex *in_e)
      {
	apply_stD2ee_m2((color*)out_e,conf,temp,mass2,(color*)in_e);
      };
    
    const auto filler=[](complex *out_e)
      {
	generate_fully_undiluted_eo_source((color*)out_e,RND_GAUSS,-1,EVN);
      };
    
    //parameters of the finder
    const int mat_size=loc_volh*NCOL;
    const int mat_size_to_allocate=(loc_volh+bord_volh)*NCOL;
    const int niter_max=100000000;
    master_printf("mat_size=%d, mat_size_to_allocate=%d\n",mat_size,mat_size_to_allocate);
    
    //precision of the eigenvalues
    double maxerr=sqrt(residue);
    
    verbosity_lv1_master_printf("Starting to search for %d %s eigenvalues of the Staggered operator DD^+ even-projected, with a precision of %lg, and Krylov space size of %d\n",neigs,(min_max?"max":"min"),maxerr,wspace_size);
    
    //find eigenvalues and eigenvectors of the staggered
    eigenvalues_find((complex**)eigvec,eigval,neigs,min_max,mat_size,mat_size_to_allocate,imp_mat,maxerr,niter_max,filler,wspace_size);
    
    rem_backfield_with_stagphases_from_conf(conf,u1b);
    
    nissa_free(temp);
  }
  
  
  //computes the spectrum of the staggered Adams operator (iD_st - Gamma5 m_Adams
  void find_eigenvalues_staggered_Adams(color **eigvec,complex *eigval,int neigs,bool min_max,quad_su3 **conf,quad_u1 **u1b,double mass,double m_Adams,double residue,int wspace_size)
  {
    
    color *temp[2]={nissa_malloc("temp_EVN",loc_volh+bord_volh,color),nissa_malloc("temp_ODD",loc_volh+bord_volh,color)};
    color *temp_in_eo[2] = {nissa_malloc("temp_in_eo_EVN",loc_volh+bord_volh,color),nissa_malloc("temp_in_eo_ODD",loc_volh+bord_volh,color)};
    color *temp_out_eo[2] = {nissa_malloc("temp_out_eo_EVN",loc_volh+bord_volh,color),nissa_malloc("temp_out_eo_ODD",loc_volh+bord_volh,color)};

    //Application of the staggered Operator
    const auto imp_mat=[conf,u1b,&temp,&temp_in_eo,&temp_out_eo,mass,m_Adams](complex *out,complex *in)
      {
        split_lx_vector_into_eo_parts(temp_in_eo,(color*)in);
        
        apply_Adams(temp_out_eo,conf,u1b,mass,m_Adams,temp,temp_in_eo);
	
        paste_eo_parts_into_lx_vector((color*)out,temp_out_eo);
      };
    
    const auto filler=[](complex *out)
      {
	generate_fully_undiluted_eo_source((color*)out,RND_GAUSS,-1,EVN);
      };
    
    //parameters of the finder
    const int mat_size=2*loc_volh*NCOL;
    const int mat_size_to_allocate=2*(loc_volh+bord_volh)*NCOL;
    const int niter_max=100000000;
    master_printf("mat_size=%d, mat_size_to_allocate=%d\n",mat_size,mat_size_to_allocate);
    
    //precision of the eigenvalues
    double maxerr=sqrt(residue);
    
    verbosity_lv1_master_printf("Starting to search for %d %s eigenvalues of the Staggered Adams operator with a precision of %lg, and Krylov space size of %d\n",neigs,(min_max?"max":"min"),maxerr,wspace_size);
    
    //find eigenvalues and eigenvectors of the staggered
    eigenvalues_find((complex**)eigvec,eigval,neigs,min_max,mat_size,mat_size_to_allocate,imp_mat,maxerr,niter_max,filler,wspace_size);
    
    
    nissa_free(temp[EVN]);
    nissa_free(temp[ODD]);
    nissa_free(temp_in_eo[EVN]);
    nissa_free(temp_in_eo[ODD]);
    nissa_free(temp_out_eo[EVN]);
    nissa_free(temp_out_eo[ODD]);
  }
}
