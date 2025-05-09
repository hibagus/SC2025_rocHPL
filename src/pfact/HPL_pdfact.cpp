/* ---------------------------------------------------------------------
 * -- High Performance Computing Linpack Benchmark (HPL)
 *    HPL - 2.2 - February 24, 2016
 *    Antoine P. Petitet
 *    University of Tennessee, Knoxville
 *    Innovative Computing Laboratory
 *    (C) Copyright 2000-2008 All Rights Reserved
 *
 *    Modified by: Noel Chalmers
 *    (C) 2018-2025 Advanced Micro Devices, Inc.
 *    See the rocHPL/LICENCE file for details.
 *
 *    SPDX-License-Identifier: (BSD-3-Clause)
 * ---------------------------------------------------------------------
 */

#include "hpl.hpp"
#include <assert.h>

void HPL_pdfact(HPL_T_panel* PANEL) {
  /*
   * Purpose
   * =======
   *
   * HPL_pdfact recursively factorizes a  1-dimensional  panel of columns.
   * The  RPFACT  function pointer specifies the recursive algorithm to be
   * used, either Crout, Left- or Right looking.  NBMIN allows to vary the
   * recursive stopping criterium in terms of the number of columns in the
   * panel, and  NDIV allows to specify the number of subpanels each panel
   * should be divided into. Usuallly a value of 2 will be chosen. Finally
   * PFACT is a function pointer specifying the non-recursive algorithm to
   * to be used on at most NBMIN columns. One can also choose here between
   * Crout, Left- or Right looking.  Empirical tests seem to indicate that
   * values of 4 or 8 for NBMIN give the best results.
   *
   * Bi-directional  exchange  is  used  to  perform  the  swap::broadcast
   * operations  at once  for one column in the panel.  This  results in a
   * lower number of slightly larger  messages than usual.  On P processes
   * and assuming bi-directional links,  the running time of this function
   * can be approximated by (when N is equal to N0):
   *
   *    N0 * log_2( P ) * ( lat + ( 2*N0 + 4 ) / bdwth ) +
   *    N0^2 * ( M - N0/3 ) * gam2-3
   *
   * where M is the local number of rows of  the panel, lat and bdwth  are
   * the latency and bandwidth of the network for  double  precision  real
   * words, and  gam2-3  is  an estimate of the  Level 2 and Level 3  BLAS
   * rate of execution. The  recursive  algorithm  allows indeed to almost
   * achieve  Level 3 BLAS  performance  in the panel factorization.  On a
   * large  number of modern machines,  this  operation is however latency
   * bound,  meaning  that its cost can  be estimated  by only the latency
   * portion N0 * log_2(P) * lat.  Mono-directional links will double this
   * communication cost.
   *
   * Arguments
   * =========
   *
   * PANEL   (local input/output)          HPL_T_panel *
   *         On entry,  PANEL  points to the data structure containing the
   *         panel information.
   *
   * ---------------------------------------------------------------------
   */

  int jb = PANEL->jb;
  PANEL->n -= jb;
  PANEL->ja += jb;

  if((PANEL->grid->mycol != PANEL->pcol) || (jb <= 0)) return;

#ifdef HPL_DETAILED_TIMING
  HPL_ptimer(HPL_TIMING_RPFACT);
#endif

  /*Copy current panel into workspace*/
  HPL_dlacpy(
      PANEL->mp, PANEL->jb, PANEL->A, PANEL->lda, PANEL->A0, PANEL->lda0);

  /*
   * Factor the panel - Update the panel pointers
   */
  HPL_TracingPush("pdfact");

  hipStream_t stream;
  CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
  CHECK_HIP_ERROR(hipEventRecord(pfactStart, stream));

  PANEL->algo->rffun(PANEL, PANEL->mp, jb, 0);

  CHECK_HIP_ERROR(hipEventRecord(pfactStop, stream));

  HPL_TracingPop();

  /*Copy L1 back into A*/
  if(PANEL->grid->myrow == PANEL->prow) {
    if(PANEL->algo->L1notran) {
      HPL_dlacpy(jb, jb, PANEL->L1, jb, PANEL->A, PANEL->lda);
    } else {
      HPL_dlatcpy(jb, jb, PANEL->L1, jb, PANEL->A, PANEL->lda);
    }
  }

  PANEL->A = Mptr(PANEL->A, 0, jb, PANEL->lda);
  PANEL->nq -= jb;
  PANEL->jj += jb;

#ifdef HPL_DETAILED_TIMING
  HPL_ptimer(HPL_TIMING_RPFACT);
#endif
}
