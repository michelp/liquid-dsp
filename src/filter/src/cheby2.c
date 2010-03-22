/*
 * Copyright (c) 2007, 2008, 2009, 2010 Joseph Gaeddert
 * Copyright (c) 2007, 2008, 2009, 2010 Virginia Polytechnic
 *                                      Institute & State University
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
// Chebeshev type-I filter design
//

#include <stdio.h>
#include <complex.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#include "liquid.internal.h"

#define LIQUID_DEBUG_CHEBY2_PRINT   0

// Compute analog zeros, poles, gain of low-pass Chebyshev
// Type II filter, grouping complex conjugates together. If
// the filter order is odd, the single real pole is at the
// end of the array.
//  _n      :   filter order
//  _ep     :   epsilon, related to stop-band ripple
//  _za     :   output analog zeros [length: floor(_n/2)]
//  _pa     :   output analog poles [length: _n]
//  _ka     :   output analog gain
void cheby2_azpkf(unsigned int _n,
                  float _es,
                  liquid_float_complex * _za,
                  liquid_float_complex * _pa,
                  liquid_float_complex * _ka)
{
    float nf = (float) _n;

    float t0 = sqrt(1.0 + 1.0/(_es*_es));
    float tp = powf( t0 + 1.0/_es, 1.0/nf );
    float tm = powf( t0 - 1.0/_es, 1.0/nf );

    float b = 0.5*(tp + tm);    // ellipse major axis
    float a = 0.5*(tp - tm);    // ellipse minor axis

#if LIQUID_DEBUG_CHEBY2_PRINT
    printf("ep : %12.8f\n", _es);
    printf("b  : %12.8f\n", b);
    printf("a  : %12.8f\n", a);
#endif

    unsigned int r = _n%2;
    unsigned int L = (_n - r)/2;
    
    // compute poles
    unsigned int i;
    unsigned int k=0;
    for (i=0; i<L; i++) {
        float theta = (float)(2*(i+1) + _n - 1)*M_PI/(float)(2*_n);
        _pa[k++] = 1.0f / (a*cosf(theta) - _Complex_I*b*sinf(theta));
        _pa[k++] = 1.0f / (a*cosf(theta) + _Complex_I*b*sinf(theta));
    }

    if (r) _pa[k++] = -1.0f / a;

    assert(k==_n);

    // compute zeros
    k=0;
    for (i=0; i<L; i++) {
        float theta = (float)(0.5f*M_PI*(2*(i+1)-1)/(float)(_n));
        _za[k++] = -1.0f / (_Complex_I*cosf(theta));
        _za[k++] =  1.0f / (_Complex_I*cosf(theta));
    }

    assert(k==2*L);

    // compute gain
    *_ka = 1.0f;
    for (i=0; i<_n; i++)
        *_ka *= _pa[i];
    for (i=0; i<2*L; i++)
        *_ka /= _za[i];
}

