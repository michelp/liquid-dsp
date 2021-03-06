/*
 * Copyright (c) 2007 - 2014 Joseph Gaeddert
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
// Binary pre-demod synchronizer
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "liquid.internal.h"

struct BPRESYNC(_s) {
    unsigned int n;     // sequence length
    unsigned int m;     // number of binary synchronizers
    
    bsequence rx_i;     // received pattern (in-phase)
    bsequence rx_q;     // received pattern (quadrature)
    
    float * dphi;       // array of frequency offsets [size: m x 1]
    bsequence * sync_i; // synchronization pattern (in-phase)
    bsequence * sync_q; // synchronization pattern (quadrature)

    float * rxy;        // output correlation [size: m x 1]

    float n_inv;        // 1/n (pre-computed for speed)
};

/* create binary pre-demod synchronizer                     */
/*  _v          :   baseband sequence                       */
/*  _n          :   baseband sequence length                */
/*  _dphi_max   :   maximum absolute frequency deviation    */
/*  _m          :   number of correlators                   */
BPRESYNC() BPRESYNC(_create)(TC *         _v,
                             unsigned int _n,
                             float        _dphi_max,
                             unsigned int _m)
{
    // validate input
    if (_n < 1) {
        fprintf(stderr, "error: bpresync_%s_create(), invalid input length\n", EXTENSION_FULL);
        exit(1);
    } else if (_m == 0) {
        fprintf(stderr, "error: bpresync_%s_create(), number of correlators must be at least 1\n", EXTENSION_FULL);
        exit(1);
    }

    // allocate main object memory and initialize
    BPRESYNC() _q = (BPRESYNC()) malloc(sizeof(struct BPRESYNC(_s)));
    _q->n = _n;
    _q->m = _m;

    _q->n_inv = 1.0f / (float)(_q->n);

    unsigned int i;

    // create internal receive buffers
    _q->rx_i = bsequence_create(_q->n);
    _q->rx_q = bsequence_create(_q->n);

    // create internal array of frequency offsets
    _q->dphi = (float*) malloc( _q->m*sizeof(float) );

    // create internal synchronizers
    _q->sync_i = (bsequence*) malloc( _q->m*sizeof(bsequence) );
    _q->sync_q = (bsequence*) malloc( _q->m*sizeof(bsequence) );

    for (i=0; i<_q->m; i++) {

        _q->sync_i[i] = bsequence_create(_q->n);
        _q->sync_q[i] = bsequence_create(_q->n);

        // generate signal with frequency offset
        _q->dphi[i] = (float)i / (float)(_q->m-1)*_dphi_max;
        unsigned int k;
        for (k=0; k<_q->n; k++) {
            TC v_prime = _v[k] * cexpf(-_Complex_I*k*_q->dphi[i]);
            bsequence_push(_q->sync_i[i], crealf(v_prime)>0);
            bsequence_push(_q->sync_q[i], cimagf(v_prime)>0);
        }
    }

    // allocate memory for cross-correlation
    _q->rxy = (float*) malloc( _q->m*sizeof(float) );

    // reset object
    BPRESYNC(_reset)(_q);

    return _q;
}

void BPRESYNC(_destroy)(BPRESYNC() _q)
{
    unsigned int i;

    // free received symbol buffers
    free(_q->rx_i);
    free(_q->rx_q);

    // free internal syncrhonizer objects
    for (i=0; i<_q->m; i++) {
        bsequence_destroy(_q->sync_i[i]);
        bsequence_destroy(_q->sync_q[i]);
    }
    free(_q->sync_i);
    free(_q->sync_q);

    // free internal frequency offset array
    free(_q->dphi);

    // free internal cross-correlation array
    free(_q->rxy);

    // free main object memory
    free(_q);
}

void BPRESYNC(_print)(BPRESYNC() _q)
{
    printf("bpresync_%s: %u samples\n", EXTENSION_FULL, _q->n);
}

void BPRESYNC(_reset)(BPRESYNC() _q)
{
    unsigned int i;
    for (i=0; i<_q->n; i++) {
        bsequence_push(_q->rx_i, (i+0) % 2);
        bsequence_push(_q->rx_q, (i+1) % 2);
    }
}

// correlate input sequence with particular 
//  _q          :   pre-demod synchronizer object
//  _id         :   ...
void BPRESYNC(_correlatex)(BPRESYNC()      _q,
                           unsigned int    _id,
                           float complex * _rxy0,
                           float complex * _rxy1)
{
    // validate input...
    if (_id >= _q->m) {
        fprintf(stderr,"error: bpresync_%s_correlatex(), invalid id\n", EXTENSION_FULL);
        exit(1);
    }

    // compute correlations
    int rxy_ii = 2*bsequence_correlate(_q->sync_i[_id], _q->rx_i) - (int)(_q->n);
    int rxy_qq = 2*bsequence_correlate(_q->sync_q[_id], _q->rx_q) - (int)(_q->n);
    int rxy_iq = 2*bsequence_correlate(_q->sync_i[_id], _q->rx_q) - (int)(_q->n);
    int rxy_qi = 2*bsequence_correlate(_q->sync_q[_id], _q->rx_i) - (int)(_q->n);

    // non-conjugated
    int rxy_i0 = rxy_ii - rxy_qq;
    int rxy_q0 = rxy_iq + rxy_qi;
    *_rxy0 = (rxy_i0 + rxy_q0 * _Complex_I) * _q->n_inv;

    // conjugated
    int rxy_i1 = rxy_ii + rxy_qq;
    int rxy_q1 = rxy_iq - rxy_qi;
    *_rxy1 = (rxy_i1 + rxy_q1 * _Complex_I) * _q->n_inv;
}

/* push input sample into pre-demod synchronizer            */
/*  _q          :   pre-demod synchronizer object           */
/*  _x          :   input sample                            */
void BPRESYNC(_push)(BPRESYNC() _q,
                     TI         _x)
{
    // push symbol into buffers
    bsequence_push(_q->rx_i, REAL(_x)>0);
    bsequence_push(_q->rx_q, IMAG(_x)>0);
}

/* correlate input sequence                                 */
/*  _q          :   pre-demod synchronizer object           */
/*  _rxy        :   output cross correlation                */
/*  _dphi_hat   :   output frequency offset estimate        */
void BPRESYNC(_correlate)(BPRESYNC() _q,
                          TO *       _rxy,
                          float *    _dphi_hat)
{
    unsigned int i;
    float complex rxy_max = 0;  // maximum cross-correlation
    float abs_rxy_max = 0;      // absolute value of rxy_max
    float complex rxy0;
    float complex rxy1;
    float dphi_hat = 0.0f;
    for (i=0; i<_q->m; i++)  {

        BPRESYNC(_correlatex)(_q, i, &rxy0, &rxy1);

        // check non-conjugated value
        if ( ABS(rxy0) > abs_rxy_max ) {
            rxy_max     = rxy0;
            abs_rxy_max = ABS(rxy0);
            dphi_hat    = _q->dphi[i];
        }

        // check conjugated value
        if ( ABS(rxy1) > abs_rxy_max ) {
            rxy_max     = rxy1;
            abs_rxy_max = ABS(rxy1);
            dphi_hat    = -_q->dphi[i];
        }
    }

    *_rxy      = rxy_max;
    *_dphi_hat = dphi_hat;
}

