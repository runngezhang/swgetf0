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
 * Written by:  David Talkin
 * Checked by:
 * Revised by:
 *
 * Brief description:
 *
 */

#include "get_cands.h"

#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <stdlib.h>
#include "f0_structs.h"
#include "f0.h"
#include "spsassert.h"
#include "sigproc.h"

#define TRUE 1
#define FALSE 0

static void peak(float* y, float* xp, float* yp);

static int downsamp(const float* in, float* out, int samples, int* outsamps,
                    int state_idx, int decimate, int ncoef, float fc[],
                    int init);

static void do_ffir(const float* buf, int in_samps, float* bufo, int* out_samps,
                    int idx, int ncoef, float* fc, int invert, int skip,
                    int init);

static int lc_lin_fir(float fc, int* nf, float* coef);


static void get_cand(Cross* const cross, float* peak, int* loc, int nlags,
                     int* ncand, float cand_thresh);


static int clamp_min(int input, int minvalue)
{
    return input < minvalue ? minvalue : input;
}

/* ----------------------------------------------------------------------- */
void get_fast_cands(const float* const fdata, const float* const fdsdata,
                    const int ind, const int step, const int size,
                    const int dec, const int start, const int nlags,
                    float* engref, int* maxloc, float* maxval, Cross* cp,
                    float* peaks, int* locs, int* ncand,
                    const F0_params* const par)
{
  const float lag_wt = par->lag_weight/nlags;
  const int decnlags = 1 + (nlags/dec);
  const int decstart = clamp_min(start/dec, 1);
  const int decind = (ind * step)/dec;
  const int decsize = 1 + (size/dec);

  crossf(fdsdata + decind, decsize, decstart, decnlags, engref, maxloc,
	maxval, cp->correl);
  cp->maxloc = *maxloc;	/* location of maximum in correlation */
  cp->maxval = *maxval;	/* max. correlation value (found at maxloc) */
  cp->rms = sqrt(*engref/size); /* rms in reference window */
  cp->firstlag = decstart;

  get_cand(cp,peaks,locs,decnlags,ncand,par->cand_thresh); /* return high peaks in xcorr */

  /* Interpolate to estimate peak locations and values at high sample rate. */
  {
    int i, j, *lp;
    float xp, yp;
    float *pe;
    for(i = *ncand, lp = locs, pe = peaks; i--; pe++, lp++) {
      j = *lp - decstart - 1;
      peak(&cp->correl[j],&xp,&yp);
      *lp = (*lp * dec) + (int)(0.5+(xp*dec)); /* refined lag */
      *pe = yp*(1.0 - (lag_wt* *lp)); /* refined amplitude */
    }
  }
  
  if(*ncand >= par->n_cands) {	/* need to prune candidates? */
    register int *loc, *locm, lt;
    register float smaxval, *pem;
    register int outer, inner, lim;
    float *pe;
    for(outer=0, lim = par->n_cands-1; outer < lim; outer++)
      for(inner = *ncand - 1 - outer,
	  pe = peaks + (*ncand) -1, pem = pe-1,
	  loc = locs + (*ncand) - 1, locm = loc-1;
	  inner--;
	  pe--,pem--,loc--,locm--)
	if((smaxval = *pe) > *pem) {
	  *pe = *pem;
	  *pem = smaxval;
	  lt = *loc;
	  *loc = *locm;
	  *locm = lt;
	}
    *ncand = par->n_cands-1;  /* leave room for the unvoiced hypothesis */
  }
  crossfi(fdata + (ind * step), size, start, nlags, 7, engref, maxloc,
	  maxval, cp->correl, locs, *ncand);

  cp->maxloc = *maxloc;	/* location of maximum in correlation */
  cp->maxval = *maxval;	/* max. correlation value (found at maxloc) */
  cp->rms = sqrt(*engref/size); /* rms in reference window */
  cp->firstlag = start;
  get_cand(cp,peaks,locs,nlags,ncand,par->cand_thresh); /* return high peaks in xcorr */
    if(*ncand >= par->n_cands) {	/* need to prune candidates again? */
    register int *loc, *locm, lt;
    register float smaxval, *pe, *pem;
    register int outer, inner, lim;
    for(outer=0, lim = par->n_cands-1; outer < lim; outer++)
      for(inner = *ncand - 1 - outer,
	  pe = peaks + (*ncand) -1, pem = pe-1,
	  loc = locs + (*ncand) - 1, locm = loc-1;
	  inner--;
	  pe--,pem--,loc--,locm--)
	if((smaxval = *pe) > *pem) {
	  *pe = *pem;
	  *pem = smaxval;
	  lt = *loc;
	  *loc = *locm;
	  *locm = lt;
	}
    *ncand = par->n_cands - 1;  /* leave room for the unvoiced hypothesis */
  }
}

/* ----------------------------------------------------------------------- */
const float* downsample(const float* input, int samsin, int state_idx,
                        double freq, int* samsout, int decimate, int first_time,
                        int last_time)
{
  static float	b[2048];
  static float *foutput;
  float	beta = 0.0;
  static int	ncoeff = 127, ncoefft = 0;
  int init;

  if(input && (samsin > 0) && (decimate > 0) && *samsout) {
    if(decimate == 1) {
      return(input);
    }

    if(first_time){
      int nbuff = (samsin/decimate) + (2*ncoeff);

      ncoeff = ((int)(freq * .005)) | 1;
      beta = .5/decimate;
      foutput = (float*)malloc(sizeof(float) * nbuff);
      spsassert(foutput, "Can't allocate foutput in downsample");
      for( ; nbuff > 0 ;)
	foutput[--nbuff] = 0.0;

      if( !lc_lin_fir(beta,&ncoeff,b)) {
	fprintf(stderr,"\nProblems computing interpolation filter\n");
	free(foutput);
	return(NULL);
      }
      ncoefft = (ncoeff/2) + 1;
    }		    /*  endif new coefficients need to be computed */

    if(first_time) init = 1;
    else if (last_time) init = 2;
    else init = 0;
    
    if(downsamp(input,foutput,samsin,samsout,state_idx,decimate,ncoefft,b,init)) {
      return(foutput);
    } else
      Fprintf(stderr,"Problems in downsamp() in downsample()\n");
  } else
    Fprintf(stderr,"Bad parameters passed to downsample()\n");
  
  return(NULL);
}

/* ----------------------------------------------------------------------- */
/* Get likely candidates for F0 peaks. */
static void get_cand(Cross* const cross, float* peak, int* loc, int nlags,
                     int* ncand, float cand_thresh)
{
  const float clip = cand_thresh * cross->maxval;
  const int lastl = nlags - 2;
  const int start = cross->firstlag;

  float* r = cross->correl;
  float o= *r++;			/* first point */
  float q = *r++;	                /* middle point */
  float p = *r++;
  *ncand = 0;
  int i;
  for(i=1; i < lastl; i++, o=q, q=p, p= *r++){
    if((q > clip) &&		/* is this a high enough value? */
      (q >= p) && (q >= o)){ /* NOTE: this finds SHOLDERS and PLATEAUS
				      as well as peaks (is this a good idea?) */
	*peak++ = q;		/* record the peak value */
	*loc++ = i + start;	/* and its location */
	(*ncand)++;		/* count number of peaks found */
      }
  }
}

/* ----------------------------------------------------------------------- */
/* buffer-to-buffer downsample operation */
/* This is STRICTLY a decimator! (no upsample) */
static int downsamp(const float* in, float* out, int samples, int* outsamps,
                    int state_idx, int decimate, int ncoef, float fc[],
                    int init)
{
  if(in && out) {
    do_ffir(in, samples, out, outsamps, state_idx, ncoef, fc, 0, decimate, init);
    return(TRUE);
  } else
    printf("Bad signal(s) passed to downsamp()\n");
  return(FALSE);
}

/*      ----------------------------------------------------------      */
/* fc contains 1/2 the coefficients of a symmetric FIR filter with unity
    passband gain.  This filter is convolved with the signal in buf.
    The output is placed in buf2.  If(invert), the filter magnitude
    response will be inverted.  If(init&1), beginning of signal is in buf;
    if(init&2), end of signal is in buf.  out_samps is set to the number of
    output points placed in bufo. */
static void do_ffir(const float* buf, int in_samps, float* bufo, int* out_samps,
                    int idx, int ncoef, float* fc, int invert, int skip,
                    int init)
{
  register float *dp1, *dp2, sum, integral;
  register const float *dp3;
  static float *co=NULL, *mem=NULL;
  static float state[1000];
  static int fsize=0, resid=0;
  register int i, j, k, l;
  register float *sp;
  register const float *buf1;

  buf1 = buf;
  if(ncoef > fsize) {/*allocate memory for full coeff. array and filter memory */
    if(co)
      free(co);
    if(mem)
      free(mem);
    fsize = 0;
    i = (ncoef+1)*2;
    if(!((co = (float *)malloc(sizeof(float)*i)) &&
	 (mem = (float *)malloc(sizeof(float)*i)))) {
      fprintf(stderr,"allocation problems in do_fir()\n");
      exit(-1);
    }
    fsize = ncoef;
  }

  /* fill 2nd half with data */
  for(i=ncoef, dp1=mem+ncoef-1; i-- > 0; )  *dp1++ = *buf++;  

  if(init & 1) {	/* Is the beginning of the signal in buf? */
    /* Copy the half-filter and its mirror image into the coefficient array. */
    for(i=ncoef-1, dp3=fc+ncoef-1, dp2=co, dp1 = co+((ncoef-1)*2),
	integral = 0.0; i-- > 0; )
      if(!invert) *dp1-- = *dp2++ = *dp3--;
      else {
	integral += (sum = *dp3--);
	*dp1-- = *dp2++ = -sum;
      }
    if(!invert)  *dp1 = *dp3;	/* point of symmetry */
    else {
      integral *= 2;
      integral += *dp3;
      *dp1 = integral - *dp3;
    }

    for(i=ncoef-1, dp1=mem; i-- > 0; ) *dp1++ = 0;
  }
  else
    for(i=ncoef-1, dp1=mem, sp=state; i-- > 0; ) *dp1++ = *sp++;

  i = in_samps;
  resid = 0;

  k = (ncoef << 1) -1;	/* inner-product loop limit */

  if(skip <= 1) {       /* never used */
/*    *out_samps = i;	
    for( ; i-- > 0; ) {	
      for(j=k, dp1=mem, dp2=co, dp3=mem+1, sum = 0.0; j-- > 0;
	  *dp1++ = *dp3++ )
	sum += *dp2++ * *dp1;

      *--dp1 = *buf++;	
      *bufo++ = (sum < 0.0)? sum -0.5 : sum +0.5; 
    }
    if(init & 2) {	
      for(i=ncoef; i-- > 0; ) {
	for(j=k, dp1=mem, dp2=co, dp3=mem+1, sum = 0.0; j-- > 0;
	    *dp1++ = *dp3++ )
	  sum += *dp2++ * *dp1;
	*--dp1 = 0.0;
	*bufo++ = (sum < 0)? sum -0.5 : sum +0.5; 
      }
      *out_samps += ncoef;
    }
    return;
*/
  } 
  else {			/* skip points (e.g. for downsampling) */
    /* the buffer end is padded with (ncoef-1) data points */
    for( l=0 ; l < *out_samps; l++ ) {
      for(j=k-skip, dp1=mem, dp2=co, dp3=mem+skip, sum=0.0; j-- >0;
	  *dp1++ = *dp3++)
	sum += *dp2++ * *dp1;
      for(j=skip; j-- >0; *dp1++ = *buf++) /* new data to memory */
	sum += *dp2++ * *dp1;
      *bufo++ = (sum<0.0) ? sum -0.5 : sum +0.5;
    }
    if(init & 2){
      resid = in_samps - *out_samps * skip;
      for(l=resid/skip; l-- >0; ){
	for(j=k-skip, dp1=mem, dp2=co, dp3=mem+skip, sum=0.0; j-- >0;
	    *dp1++ = *dp3++)
	    sum += *dp2++ * *dp1;
	for(j=skip; j-- >0; *dp1++ = 0.0)
	  sum += *dp2++ * *dp1;
	*bufo++ = (sum<0.0) ? sum -0.5 : sum +0.5;
	(*out_samps)++;
      }
    }
    else
      for(dp3=buf1+idx-ncoef+1, l=ncoef-1, sp=state; l-- >0; ) *sp++ = *dp3++;
  }
}

/*      ----------------------------------------------------------      */
/* create the coefficients for a symmetric FIR lowpass filter using the
   window technique with a Hanning window. */
static int lc_lin_fir(float fc, int* nf, float* coef)
{
    register int	i, n;
    register double	twopi, fn, c;

    if(((*nf % 2) != 1))
	*nf = *nf + 1;
    n = (*nf + 1)/2;

    /*  Compute part of the ideal impulse response (the sin(x)/x kernel). */
    twopi = M_PI * 2.0;
    coef[0] = 2.0 * fc;
    c = M_PI;
    fn = twopi * fc;
    for(i=1;i < n; i++) coef[i] = sin(i * fn)/(c * i);

    /* Now apply a Hanning window to the (infinite) impulse response. */
    /* (Probably should use a better window, like Kaiser...) */
    fn = twopi/(double)(*nf);
    for(i=0;i<n;i++) 
	coef[n-i-1] *= (.5 - (.5 * cos(fn * ((double)i + 0.5))));
    
    return(TRUE);
}


/* ----------------------------------------------------------------------- */
/* Use parabolic interpolation over the three points defining the peak
 * vicinity to estimate the "true" peak. */
static void peak(
    float* y,  /* vector of length 3 defining peak */
    float* xp, /* x,y values of parabolic peak fitting the input points. */
    float* yp  /* x,y values of parabolic peak fitting the input points. */
    )
{
  register float a, c;
  
  a = (y[2]-y[1])+(.5*(y[0]-y[2]));
  if(fabs(a) > .000001) {
    *xp = c = (y[0]-y[2])/(4.0*a);
    *yp = y[1] - (a*c*c);
  } else {
    *xp = 0.0;
    *yp = y[1];
  }
}

