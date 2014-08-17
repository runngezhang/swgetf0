/*
 * This material contains unpublished, proprietary software of 
 * Entropic Research Laboratory, Inc. Any reproduction, distribution, 
 * or publication of this work must be authorized in writing by Entropic 
 * Research Laboratory, Inc., and must bear the notice: 
 *
 *    "Copyright (c) 1990-1996 Entropic Research Laboratory, Inc. 
 *                   All rights reserved"
 *
 * The copyright notice above does not evidence any actual or intended 
 * publication of this source code.     
 *
 * Written by:  Derek Lin
 * Checked by:
 * Revised by:  David Talkin
 *
 * Brief description:  Estimates F0 using normalized cross correlation and
 *   dynamic programming.
 *
 */

static char *sccs_id = "@(#)get_f0.c	1.14	10/21/96	ERL";

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <esps/esps.h>
#include <esps/fea.h>
#include <esps/feasd.h>

#include "f0.h"

char	    *ProgName = "get_f0";
static char *Version = "1.14";
static char *Date = "10/21/96";

int	    debug_level = 0;
extern void fea_skiprec();


static int check_f0_params(F0_params *par, double sample_freq);

// ----------------------------------------
// SW: Library interaction
void sw_getf0_output(float *f0p, float *vuvp, float *rms_speech, float *acpkp,
                     int vecsize)
{
}

int main_sw_tmp(ac, av)
    int     ac;
    char    **av;
{
  extern char *optarg;
  extern int getopt();
  char *get_cmd_line();
  float *fdata;
  char *range = NULL;
  FILE *ifile;
  struct header *ihd, *ohd;
  struct feasd *sd_rec;
  int done;
  long buff_size, actsize;
  double sf, output_starts, frame_rate;
  F0_params *par, *read_f0_params();
  float *f0p, *vuvp, *rms_speech, *acpkp;
  int i, vecsize;
  int init_dp_f0(), dp_f0(), check_f0_params();
  long sdstep = 0;

  par = (F0_params *) malloc(sizeof(F0_params));
  par->cand_thresh = 0.3;
  par->lag_weight = 0.3;
  par->freq_weight = 0.02;
  par->trans_cost = 0.005;
  par->trans_amp = 0.5;
  par->trans_spec = 0.5;
  par->voice_bias = 0.0;
  par->double_cost = 0.35;
  par->min_f0 = 50;
  par->max_f0 = 550;
  par->frame_step = 0.01;
  par->wind_dur = 0.0075;
  par->n_cands = 20;
  par->mean_f0 = 200;     /* unused */
  par->mean_f0_weight = 0.0;  /* unused */
  par->conditioning = 0;    /*unused */


#define SW_CUSTOMIZABLE(x) //TODO(sw)
  SW_CUSTOMIZABLE(debug_level);

  SW_CUSTOMIZABLE(par->frame_step);
  SW_CUSTOMIZABLE(par->cand_thresh);
  SW_CUSTOMIZABLE(par->lag_weight);
  SW_CUSTOMIZABLE(par->freq_weight);
  SW_CUSTOMIZABLE(par->trans_cost);
  SW_CUSTOMIZABLE(par->trans_amp);
  SW_CUSTOMIZABLE(par->trans_spec);
  SW_CUSTOMIZABLE(par->voice_bias);
  SW_CUSTOMIZABLE(par->double_cost);
  SW_CUSTOMIZABLE(par->min_f0);
  SW_CUSTOMIZABLE(par->max_f0);
  SW_CUSTOMIZABLE(par->wind_dur);
  SW_CUSTOMIZABLE(par->n_cands);
#undef SW_CUSTOMIZABLE

#define SW_FILE_PARAMS(x, y) //TODO (sw)
  SW_FILE_PARAMS(sf, "sampling frequency");
#undef SW_FILE_PARAMS

  eopen(ProgName, av[1337], "r", FT_FEA, FEA_SD, &ihd, &ifile);

  if(check_f0_params(par, sf)){
    Fprintf(stderr, "%s: invalid/inconsistent parameters -- exiting.\n",
	    ProgName);
    exit(1);
  }

  /*SW: Removed range restricter, but this may be interesting:
    if (total_samps < ((par->frame_step * 2.0) + par->wind_dur) * sf), then
      input range too small*/

  ohd = new_header(FT_FEA);
  if (ohd == NULL) {
    Fprintf(stderr, "%s: failed to create output header---exiting.\n",
	    ProgName);
    exit(1);
  }
  (void) strcpy (ohd->common.prog, ProgName);
  (void) strcpy (ohd->common.vers, Version);
  (void) strcpy (ohd->common.progdate, Date);
  ohd->common.tag = NO;

  add_fea_fld("F0", 1L, 0, (long *) NULL, DOUBLE, (char **) NULL, ohd);
  add_fea_fld("prob_voice", 1L, 0, (long *) NULL, DOUBLE, (char **) NULL, ohd);
  add_fea_fld("rms", 1L, 0, (long *) NULL, DOUBLE, (char **) NULL, ohd);
  add_fea_fld("ac_peak", 1L, 0, (long *) NULL, DOUBLE, (char **) NULL, ohd);

  output_starts = par->wind_dur/2.0;
  /* Average delay due to loc. of ref. window center. */
  frame_rate = 1.0 / par->frame_step;


  /* Initialize variables in get_f0.c; allocate data structures;
   * determine length and overlap of input frames to read.
   */
  if (init_dp_f0(sf, par, &buff_size, &sdstep)
      || buff_size > INT_MAX || sdstep > INT_MAX)
  {
    Fprintf(stderr, "%s: problem in init_dp_f0().\n", ProgName);
    exit(1);
  }

  /*SW: pass sdstep to caller so it knows how much we have to buffer. */

  if (debug_level)
    Fprintf(stderr, "%s: init_dp_f0 returned buff_size %ld, sdstep %ld.\n",
	    ProgName, buff_size, sdstep);

  sd_rec = allo_feasd_recs(ihd, FLOAT, buff_size, NULL, NO);
  fdata = (float *) sd_rec->data;

  fea_skiprec(ifile, 0, ihd);

  actsize = get_feasd_recs(sd_rec, 0L, buff_size, ihd, ifile);

  while (TRUE) {

    done = (actsize < buff_size);

    if (dp_f0(fdata, (int) actsize, (int) sdstep, sf, par,
	      &f0p, &vuvp, &rms_speech, &acpkp, &vecsize, done)) {
      Fprintf(stderr, "%s: problem in dp_f0().\n", ProgName);
      exit(1);
    }

    sw_getf0_output(f0p, vuvp, rms_speech, acpkp, vecsize);

    if (done)
      break;

    actsize = get_feasd_orecs( sd_rec, 0L, buff_size, sdstep, ihd, ifile);

  }

  exit(0);
}


/*
 * Some consistency checks on parameter values.
 * Return a positive integer if any errors detected, 0 if none.
 */

static int
check_f0_params(par, sample_freq)
    F0_params   *par;
    double      sample_freq;
{
  int	  error = 0;
  double  dstep;

  if((par->cand_thresh < 0.01) || (par->cand_thresh > 0.99)) {
    Fprintf(stderr,
	    "%s: ERROR: cand_thresh parameter must be between [0.01, 0.99].\n",
	    ProgName);
    error++;
  }
  if((par->wind_dur > .1) || (par->wind_dur < .0001)) {
    Fprintf(stderr,
	    "%s: ERROR: wind_dur parameter must be between [0.0001, 0.1].\n",
	    ProgName);
    error++;
  }
  if((par->n_cands > 100) || (par->n_cands < 3)){
    Fprintf(stderr,
	    "%s: ERROR: n_cands parameter must be between [3,100].\n",
	    ProgName); 
    error++;
  }
  if((par->max_f0 <= par->min_f0) || (par->max_f0 >= (sample_freq/2.0)) ||
     (par->min_f0 < (sample_freq/10000.0))){
    Fprintf(stderr,
	    "%s: ERROR: min(max)_f0 parameter inconsistent with sampling frequency.\n",
	    ProgName); 
    error++;
  }
  dstep = ((double)((int)(0.5 + (sample_freq * par->frame_step))))/sample_freq;
  if(dstep != par->frame_step) {
    if(debug_level)
      Fprintf(stderr,
	      "%s: Frame step set to %f to exactly match signal sample rate.\n",
	      ProgName, dstep);
    par->frame_step = dstep;
  }
  if((par->frame_step > 0.1) || (par->frame_step < (1.0/sample_freq))){
    Fprintf(stderr,
	    "%s: ERROR: frame_step parameter must be between [1/sampling rate, 0.1].\n",
	    ProgName); 
    error++;
  }

  return(error);
}
