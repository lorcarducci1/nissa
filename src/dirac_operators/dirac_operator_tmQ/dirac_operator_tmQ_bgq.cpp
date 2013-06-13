#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include "../../base/global_variables.h"
#include "../../base/vectors.h"
#include "../../bgq/bgq_macros.h"
#include "../../bgq/hopping_matrix_bgq.h"
#include "../../new_types/complex.h"
#include "../../routines/thread.h"

/*
  application of hopping matrix goes among the following steps:
   1) hopping matrix on the surf
   2) start communications
   3) hopping matrix on the bulk
   4) finish communications
  then the application of Q requires to expand the halfspincolor into full spincolor and summ the diagonal part
*/

#define DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_tmp,piece)			\
  REG_LOAD_BI_COMPLEX_AFTER_ADVANCING(NAME2(reg_tmp,s0_c1),piece);	\
  REG_LOAD_BI_COMPLEX_AFTER_ADVANCING(NAME2(reg_tmp,s0_c2),piece);	\
  REG_LOAD_BI_COMPLEX_AFTER_ADVANCING(NAME2(reg_tmp,s1_c0),piece);	\
  REG_LOAD_BI_COMPLEX_AFTER_ADVANCING(NAME2(reg_tmp,s1_c1),piece);	\
  REG_LOAD_BI_COMPLEX_AFTER_ADVANCING(NAME2(reg_tmp,s1_c2),piece);	\
  REG_BI_HALFSPINCOLOR_SUMM(reg_out,reg_out,reg_temp);

THREADABLE_FUNCTION_4ARG(hopping_matrix_expand_to_Q_and_summ_diag_term_bgq_binded, bi_spincolor*,out, double,kappa, double,mu, bi_spincolor*,in)
{
  GET_THREAD_ID();
  
  double A=-1/kappa,B=-2*mu;
  bi_complex diag[2]={{{+A,B},{+A,B}},{{-A,B},{-A,B}}};
  
#ifdef BGQ
  DECLARE_REG_BI_COMPLEX(reg_diag_0);
  DECLARE_REG_BI_COMPLEX(reg_diag_1);
  REG_LOAD_BI_COMPLEX(reg_diag_0,diag[0]);
  REG_LOAD_BI_COMPLEX(reg_diag_1,diag[1]);
  
  bi_complex mone_half={{-0.5,-0.5},{-0.5,-0.5}};
  DECLARE_REG_BI_COMPLEX(reg_mone_half);
  REG_LOAD_BI_COMPLEX(reg_mone_half,mone_half);
#endif
  
  //wait that all the terms are put in place
  THREAD_BARRIER();
  
  NISSA_PARALLEL_LOOP(i,0,loc_volh)
    {
#ifdef BGQ
      DECLARE_REG_BI_SPINCOLOR(reg_out);

      //multiply in by the diagonal part
      {
	DECLARE_REG_BI_SPINCOLOR(reg_in);
	REG_LOAD_BI_HALFSPINCOLOR(reg_in,in[i]);
	REG_BI_COLOR_PROD_COMPLEX(reg_out_s0,reg_in_s0,reg_diag_0);
	REG_BI_COLOR_PROD_COMPLEX(reg_out_s1,reg_in_s1,reg_diag_0);
	REG_BI_COLOR_PROD_COMPLEX(reg_out_s2,reg_in_s2,reg_diag_1);
	REG_BI_COLOR_PROD_COMPLEX(reg_out_s3,reg_in_s3,reg_diag_1);
      }

      //8 pieces
      {
	void *piece=bgq_hopping_matrix_output_data+i*8;      
	DECLARE_REG_BI_HALFSPINCOLOR(reg_temp);
	
	//TFW
	DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_temp,piece);
	REG_BI_COLOR_SUBT(reg_out_s2,reg_out_s2,reg_temp_s0);
	REG_BI_COLOR_SUBT(reg_out_s3,reg_out_s3,reg_temp_s1);
	
	//XFW
	DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_temp,piece);
	REG_BI_COLOR_ISUMM(reg_out_s2,reg_out_s2,reg_temp_s1);
	REG_BI_COLOR_ISUMM(reg_out_s3,reg_out_s3,reg_temp_s0);
	
	//YFW
	DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_temp,piece);
	REG_BI_COLOR_SUMM(reg_out_s2,reg_out_s2,reg_temp_s1);
	REG_BI_COLOR_SUBT(reg_out_s3,reg_out_s3,reg_temp_s0);
	
	//ZFW
	DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_temp,piece);
	REG_BI_COLOR_ISUMM(reg_out_s2,reg_out_s2,reg_temp_s0);
	REG_BI_COLOR_ISUBT(reg_out_s3,reg_out_s3,reg_temp_s1);
	
	//TBW
	DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_temp,piece);
	REG_BI_COLOR_SUMM(reg_out_s2,reg_out_s2,reg_temp_s0);
	REG_BI_COLOR_SUMM(reg_out_s3,reg_out_s3,reg_temp_s1);
	
	//XBW
	DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_temp,piece);
	REG_BI_COLOR_ISUBT(reg_out_s2,reg_out_s2,reg_temp_s1);
	REG_BI_COLOR_ISUBT(reg_out_s3,reg_out_s3,reg_temp_s0);
	
	//YBW
	DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_temp,piece);
	REG_BI_COLOR_SUBT(reg_out_s2,reg_out_s2,reg_temp_s1);
	REG_BI_COLOR_SUMM(reg_out_s3,reg_out_s3,reg_temp_s0);
	
	//ZBW
	DER_TMQ_EXP_BGQ_HEADER(reg_out,reg_temp,piece);
	REG_BI_COLOR_ISUBT(reg_out_s2,reg_out_s2,reg_temp_s0);
	REG_BI_COLOR_ISUMM(reg_out_s3,reg_out_s3,reg_temp_s1);
      }
      
      //put final -0.5
      REG_BI_SPINCOLOR_PROD_4DOUBLE(reg_out,reg_out,reg_mone_half);
      STORE_REG_BI_SPINCOLOR(&(out[i]),reg_out);
#else
      bi_spincolor temp;
      
      //multiply in by the diagonal part
      DIAG_TMQ(temp,diag,in[i]);

      //summ 8 contributions
      TFW_DER_TMQ_EXP(temp,piece[0]);
      XFW_DER_TMQ_EXP(temp,piece[1]);
      YFW_DER_TMQ_EXP(temp,piece[2]);
      ZFW_DER_TMQ_EXP(temp,piece[3]);
      TBW_DER_TMQ_EXP(temp,piece[4]);
      XBW_DER_TMQ_EXP(temp,piece[5]);
      YBW_DER_TMQ_EXP(temp,piece[6]);
      ZBW_DER_TMQ_EXP(temp,piece[7]);

      //put final -0.5
      BI_SPINCOLOR_PROD_DOUBLE(out[i],temp,-0.5);
#endif
    }
}}

THREADABLE_FUNCTION_5ARG(apply_tmQ_bgq, bi_spincolor*,out, bi_oct_su3*,conf, double,kappa, double,mu, bi_spincolor*,in)
{
  //compute on the surface and start communications
  apply_Wilson_hopping_matrix_bgq_binded_nocomm_nobarrier(conf,0,bgq_vsurf_vol,in);
  start_Wilson_hopping_matrix_bgq_binded_communications();
  
  //compute on the bulk and finish communications
  apply_Wilson_hopping_matrix_bgq_binded_nocomm_nobarrier(conf,bgq_vsurf_vol,loc_volh,in);
  finish_Wilson_hopping_matrix_bgq_binded_communications();
  
  //put together all the 8 pieces
  hopping_matrix_expand_to_Q_and_summ_diag_term_bgq_binded(out,kappa,mu,in);
}}