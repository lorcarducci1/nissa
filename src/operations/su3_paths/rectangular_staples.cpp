#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../../base/communicate.h"
#include "../../base/debug.h"
#include "../../base/global_variables.h"
#include "../../base/vectors.h"
#include "../../new_types/su3.h"
#include "../../routines/openmp.h"

#ifdef BGQ
 #include "../../bgq/spi.h"
#endif

#define COMPUTE_RECT_FW_STAPLE(OUT,A,B,C,TEMP)	\
  unsafe_su3_prod_su3(TEMP,A,B);		\
  unsafe_su3_prod_su3_dag(OUT,TEMP,C);
#define SUMM_RECT_FW_STAPLE(OUT,A,B,C,TEMP)	\
  unsafe_su3_prod_su3(TEMP,A,B);		\
  su3_summ_the_prod_su3_dag(OUT,TEMP,C);

#define COMPUTE_POINT_RECT_FW_STAPLES(out,conf,sq_staples,A,B,F,imu,mu,inu,nu,temp) \
  COMPUTE_RECT_FW_STAPLE(out[A][mu][3+inu],sq_staples[A][nu][imu],conf[B][mu],conf[F][nu],temp); /*bw sq staple*/ \
  SUMM_RECT_FW_STAPLE(out[A][mu][3+inu],conf[A][nu],sq_staples[B][mu][3+inu],conf[F][nu],temp);  /*fw sq staple*/ \
  SUMM_RECT_FW_STAPLE(out[A][mu][3+inu],conf[A][nu],conf[B][mu],sq_staples[F][nu][3+imu],temp);  /*fw sq staple*/

#define COMPUTE_RECT_BW_STAPLE(OUT,A,B,C,TEMP)	\
  unsafe_su3_dag_prod_su3(TEMP,A,B);		\
  unsafe_su3_prod_su3(OUT,TEMP,C);
#define SUMM_RECT_BW_STAPLE(OUT,A,B,C,TEMP)	\
  unsafe_su3_dag_prod_su3(TEMP,A,B);		\
  su3_summ_the_prod_su3(OUT,TEMP,C);

#define COMPUTE_POINT_RECT_BW_STAPLES(out,conf,sq_staples,A,D,E,imu,mu,inu,nu,temp) \
  COMPUTE_RECT_BW_STAPLE(out[A][mu][inu],sq_staples[D][nu][imu],conf[D][mu],conf[E][nu],temp); /*bw sq staple*/ \
  SUMM_RECT_BW_STAPLE(out[A][mu][inu],conf[D][nu],sq_staples[D][mu][inu],conf[E][nu],temp);    /*bw sq staple*/ \
  SUMM_RECT_BW_STAPLE(out[A][mu][inu],conf[D][nu],conf[D][mu],sq_staples[E][nu][3+imu],temp);  /*fw sq staple*/

// 0) allocate or take from SPI the buffer where to exchange fw border and back staples
void rectangular_staples_lx_conf_allocate_buffers(quad_su3 **send_buf,quad_su3 **recv_buf)
{
#ifdef BGQ
  *send_buf=(quad_su3*)(spi_lx_quad_su3_comm.send_buf);
  *recv_buf=(quad_su3*)(spi_lx_quad_su3_comm.recv_buf);
#else
  *send_buf=nissa_malloc("Wforce_send_buf",bord_vol,quad_su3);
  *recv_buf=nissa_malloc("Wforce_recv_buf",bord_vol,quad_su3);
#endif
}

// 1) start communicating lower surface forward staples
void rectangular_staples_lx_conf_start_communicating_lower_surface_fw_squared_staples(quad_su3 *send_buf,quad_su3 *recv_buf,squared_staples_t *sq_staples,int (*nrequest),MPI_Request *request,int thread_id)
{
  //copy lower surface into sending buf to be sent to dw nodes
  //obtained scanning on first half of the border, and storing them
  //in the first half of sending buf
  for(int nu=0;nu<4;nu++) //border and staple direction
    if(paral_dir[nu])
      for(int imu=0;imu<3;imu++) //link direction
        {
          int mu=perp_dir[nu][imu];
          int inu=(nu<mu)?nu:nu-1;
	  
          NISSA_PARALLEL_LOOP(ibord,bord_offset[nu],bord_offset[nu]+bord_dir_vol[nu])
            su3_copy(send_buf[ibord][mu],sq_staples[loc_vol+ibord][mu][3+inu]); //one contribution per link in the border
        }

  //start communication of lower surf to backward nodes
  if(IS_MASTER_THREAD) tot_nissa_comm_time-=take_time();
#ifdef BGQ
  int dir_comm[8]={0,0,0,0,1,1,1,1},tot_size=bord_volh*sizeof(quad_su3);
  spi_start_comm(&spi_lx_quad_su3_comm,dir_comm,tot_size);
#else
  int imessage=654325;  
  //wait that all threads filled their part
  thread_barrier(WILSON_STAPLE_BARRIER);
  if(IS_MASTER_THREAD)
    {
      (*nrequest)=0;
      for(int mu=0;mu<4;mu++)
        if(paral_dir[mu]!=0)
          {
            //exchanging the lower surface, from the first half of sending node to the second half of receiving node
            MPI_Irecv(recv_buf+bord_offset[mu]+bord_volh,bord_dir_vol[mu],MPI_QUAD_SU3,rank_neighup[mu],imessage,cart_comm,request+(*nrequest)++);
            MPI_Isend(send_buf+bord_offset[mu],bord_dir_vol[mu],MPI_QUAD_SU3,rank_neighdw[mu],imessage++,cart_comm,request+(*nrequest)++);
          }     
    }
#endif
}

// 2) compute non_fwsurf fw staples that are always local
void rectangular_staples_lx_conf_compute_non_fw_surf_fw_staples(rectangular_staples_t *out,quad_su3 *conf,squared_staples_t *sq_staples,int thread_id)
{
  for(int mu=0;mu<4;mu++) //link direction
    for(int inu=0;inu<3;inu++) //staple direction
      {
	int nu=perp_dir[mu][inu];
	int imu=(mu<nu)?mu:mu-1;

	NISSA_PARALLEL_LOOP(ibulk,0,non_fw_surf_vol)
	  {
	    su3 temp; //three staples in clocwise order
	    int A=loclx_of_non_fw_surflx[ibulk],B=loclx_neighup[A][nu],F=loclx_neighup[A][mu];
	    COMPUTE_POINT_RECT_FW_STAPLES(out,conf,sq_staples,A,B,F,imu,mu,inu,nu,temp);
	  }
      }
}

// 3) finish communication of lower surface fw squared staples
void rectangular_staples_lx_conf_finish_communicating_lower_surface_fw_squared_staples(squared_staples_t *sq_staples,quad_su3 *recv_buf,int *nrequest,MPI_Request *request,int thread_id)
{
  if(IS_MASTER_THREAD) tot_nissa_comm_time+=take_time();
#ifdef BGQ
  spi_comm_wait(&spi_lx_quad_su3_comm);
#else
  if(IS_MASTER_THREAD) MPI_Waitall((*nrequest),request,MPI_STATUS_IGNORE);
#endif
  
  //copy the received forward border (stored in the second half of receiving buf) on its destination
  for(int nu=0;nu<4;nu++) //border and staple direction
    if(paral_dir[nu])
      for(int imu=0;imu<3;imu++) //link direction
        {
          int mu=perp_dir[nu][imu];
          int inu=(nu<mu)?nu:nu-1;

          NISSA_PARALLEL_LOOP(ibord,bord_volh+bord_offset[nu],bord_volh+bord_offset[nu]+bord_dir_vol[nu])
            su3_copy(sq_staples[loc_vol+ibord][mu][inu],recv_buf[ibord][mu]); //one contribution per link in the border
        }

  thread_barrier(WILSON_STAPLE_BARRIER);
}

// 4) compute backward staples to be sent to up nodes and send them
void rectangular_staples_lx_conf_compute_and_start_communicating_fw_surf_bw_staples(quad_su3 *send_buf,quad_su3 *recv_buf,rectangular_staples_t *out,quad_su3 *conf,squared_staples_t *sq_staples,int *nrequest,MPI_Request *request,int thread_id)
{
  //compute backward staples to be sent to up nodes
  //obtained scanning D on fw_surf and storing data as they come
  for(int inu=0;inu<3;inu++) //staple direction
    for(int mu=0;mu<4;mu++) //link direction
      {
	int nu=perp_dir[mu][inu];
        int imu=(mu<nu)?mu:mu-1;
	
	NISSA_PARALLEL_LOOP(ifw_surf,0,fw_surf_vol)
	  {
	    su3 temp;
	    int D=loclx_of_fw_surflx[ifw_surf],A=loclx_neighup[D][nu],E=loclx_neighup[D][mu];
            COMPUTE_POINT_RECT_BW_STAPLES(out,conf,sq_staples,A,D,E,imu,mu,inu,nu,temp);
	  }
      }
  
  //wait that everything is computed
  thread_barrier(WILSON_STAPLE_BARRIER);
  
  //copy in send buf, obtained scanning second half of each parallelized direction external border and
  //copying the three perpendicular links staple
  for(int nu=0;nu<4;nu++) //border and staple direction
    if(paral_dir[nu])
      for(int imu=0;imu<3;imu++) //link direction
	{
	  int mu=perp_dir[nu][imu];
	  int inu=(nu<mu)?nu:nu-1;
	  
	  NISSA_PARALLEL_LOOP(ibord,bord_volh+bord_offset[nu],bord_volh+bord_offset[nu]+bord_dir_vol[nu])
	    su3_copy(send_buf[ibord][mu],out[loc_vol+ibord][mu][inu]); //one contribution per link in the border
	}
  
  //start communication of fw surf backward staples to forward nodes
  if(IS_MASTER_THREAD) tot_nissa_comm_time-=take_time();
#ifdef BGQ
  int dir_comm[8]={1,1,1,1,0,0,0,0},tot_size=bord_volh*sizeof(quad_su3);
  spi_start_comm(&spi_lx_quad_su3_comm,dir_comm,tot_size);
#else
  //wait that all threads filled their part
  thread_barrier(WILSON_STAPLE_BARRIER);
  int imessage=653432;
  if(IS_MASTER_THREAD)
    {
      (*nrequest)=0;
      
      for(int mu=0;mu<4;mu++)
	if(paral_dir[mu]!=0)
	  {
	    //exchanging the backward staples, from the second half of sending node to the first half of receiving node
	    MPI_Irecv(recv_buf+bord_offset[mu],bord_dir_vol[mu],MPI_QUAD_SU3,rank_neighdw[mu],imessage,cart_comm,request+(*nrequest)++);
	    MPI_Isend(send_buf+bord_offset[mu]+bord_volh,bord_dir_vol[mu],MPI_QUAD_SU3,rank_neighup[mu],imessage++,cart_comm,request+(*nrequest)++);
	  }
    }
#endif
}

// 5) compute non_fw_surf bw staples
void rectangular_staples_lx_conf_compute_non_fw_surf_bw_staples(rectangular_staples_t *out,quad_su3 *conf,squared_staples_t *sq_staples,int thread_id)
{
  for(int mu=0;mu<4;mu++) //link direction
    for(int inu=0;inu<3;inu++) //staple direction
      {
	int nu=perp_dir[mu][inu];
	int imu=(mu<nu)?mu:mu-1;
	
	//obtained scanning D on fw_surf
	NISSA_PARALLEL_LOOP(inon_fw_surf,0,non_fw_surf_vol)
	  {
	    su3 temp;
	    int D=loclx_of_non_fw_surflx[inon_fw_surf],A=loclx_neighup[D][nu],E=loclx_neighup[D][mu];
            COMPUTE_POINT_RECT_BW_STAPLES(out,conf,sq_staples,A,D,E,imu,mu,inu,nu,temp);
	  }
      }
}

// 6) compute fw_surf fw staples
void rectangular_staples_lx_conf_compute_fw_surf_fw_staples(rectangular_staples_t *out,quad_su3 *conf,squared_staples_t *sq_staples,int thread_id)
{
  for(int mu=0;mu<4;mu++) //link direction
    for(int inu=0;inu<3;inu++) //staple direction
      {
	int nu=perp_dir[mu][inu];
	int imu=(mu<nu)?mu:mu-1;
	
	//obtained looping A on forward surface
	NISSA_PARALLEL_LOOP(ifw_surf,0,fw_surf_vol)
	  {
	    su3 temp;
	    int A=loclx_of_fw_surflx[ifw_surf],B=loclx_neighup[A][nu],F=loclx_neighup[A][mu];
            COMPUTE_POINT_RECT_FW_STAPLES(out,conf,sq_staples,A,B,F,imu,mu,inu,nu,temp);
	  }
      }
}

// 7) finish communication of fw_surf bw staples
void rectangular_staples_lx_conf_finish_communicating_fw_surf_bw_staples(rectangular_staples_t *out,quad_su3 *recv_buf,int (*nrequest),MPI_Request *request,int thread_id)
{
  if(IS_MASTER_THREAD) tot_nissa_comm_time+=take_time();
#ifdef BGQ
  spi_comm_wait(&spi_lx_quad_su3_comm);
#else
  if(IS_MASTER_THREAD) MPI_Waitall((*nrequest),request,MPI_STATUS_IGNORE);
  //wait that all threads filled their part
  thread_barrier(WILSON_STAPLE_BARRIER);
#endif

  //copy the received backward staples (stored on first half of receiving buf) on bw_surf sites
  for(int nu=0;nu<4;nu++) //staple and fw bord direction
    if(paral_dir[nu])
      for(int imu=0;imu<3;imu++) //link direction
	{
	  int mu=perp_dir[nu][imu];
	  int inu=(nu<mu)?nu:nu-1;
	  
	  NISSA_PARALLEL_LOOP(ibord,bord_offset[nu],bord_offset[nu]+bord_dir_vol[nu])
	    su3_copy(out[surflx_of_bordlx[ibord]][mu][inu],recv_buf[ibord][mu]);//one contribution per link in the border
	}
  
  thread_barrier(WILSON_STAPLE_BARRIER);
}

//free buffers
void rectangular_staples_lx_conf_free_buffers(quad_su3 *send_buf,quad_su3 *recv_buf)
{
#ifdef BGQ
#else
  nissa_free(send_buf);
  nissa_free(recv_buf);
#endif
}

//compute rectangular staples using overlap between computation and communications, and avoiding using edges
THREADABLE_FUNCTION_3ARG(compute_rectangular_staples_lx_conf, rectangular_staples_t*,out, quad_su3*,conf, squared_staples_t*,sq_staples)
{
  GET_THREAD_ID();
  
  //request for mpi
  int nrequest;
  MPI_Request request[8];
  
  //buffers
  quad_su3 *send_buf,*recv_buf;
  rectangular_staples_lx_conf_allocate_buffers(&send_buf,&recv_buf);
  
  //compute non_fw_surf fw staples
  rectangular_staples_lx_conf_start_communicating_lower_surface_fw_squared_staples
    (send_buf,recv_buf,sq_staples,&nrequest,request,thread_id);
  rectangular_staples_lx_conf_compute_non_fw_surf_fw_staples(out,conf,sq_staples,thread_id);
  rectangular_staples_lx_conf_finish_communicating_lower_surface_fw_squared_staples(sq_staples,recv_buf,&nrequest,request,thread_id);
  
  //compute fw_surf bw staples, non_fw_surf bw staples and fw_surf fw staples
  rectangular_staples_lx_conf_compute_and_start_communicating_fw_surf_bw_staples
    (send_buf,recv_buf,out,conf,sq_staples,&nrequest,request,thread_id);
  rectangular_staples_lx_conf_compute_non_fw_surf_bw_staples(out,conf,sq_staples,thread_id);
  rectangular_staples_lx_conf_compute_fw_surf_fw_staples(out,conf,sq_staples,thread_id);
  rectangular_staples_lx_conf_finish_communicating_fw_surf_bw_staples(out,recv_buf,&nrequest,request,thread_id);
  
  //free buffers (auto sync)
  rectangular_staples_lx_conf_free_buffers(send_buf,recv_buf);
}}