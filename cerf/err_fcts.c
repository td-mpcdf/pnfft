/* Library libcerf:
 *   Compute complex error functions, based on a new implementation of
 *   Faddeeva's w_of_z. Also provide Dawson and Voigt functions.
 *
 * File err_fcts.c:
 *   Computate Dawson, Voigt, and several error functions,
 *   based on erfcx, im_w_of_x, w_of_z as implemented in separate files.
 *
 *   Given w(z), the error functions are mostly straightforward
 *   to compute, except for certain regions where we have to
 *   switch to Taylor expansions to avoid cancellation errors
 *   [e.g. near the origin for erf(z)].
 * 
 * Copyright:
 *   (C) 2012 Massachusetts Institute of Technology
 *   (C) 2013 Forschungszentrum Jülich GmbH
 * 
 * Licence:
 *   Permission is hereby granted, free of charge, to any person obtaining
 *   a copy of this software and associated documentation files (the
 *   "Software"), to deal in the Software without restriction, including
 *   without limitation the rights to use, copy, modify, merge, publish,
 *   distribute, sublicense, and/or sell copies of the Software, and to
 *   permit persons to whom the Software is furnished to do so, subject to
 *   the following conditions:
 * 
 *   The above copyright notice and this permission notice shall be
 *   included in all copies or substantial portions of the Software.
 * 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 *
 * Authors:
 *   Steven G. Johnson, Massachusetts Institute of Technology, 2012, core author
 *   Joachim Wuttke, Forschungszentrum Jülich, 2013, package maintainer
 *
 * Website:
 *   http://apps.jcns.fz-juelich.de/libcerf
 *
 * Revision history:
 *   ../CHANGELOG
 *
 * Man pages:
 *   cerf(3), dawson(3), voigt(3)
 */

#include "cerf.h"

#define _GNU_SOURCE // enable GNU libc NAN extension if possible

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "defs.h" // defines cmplx, CMPLX, NaN

const R spi2 = 0.8862269254527580136490837416705725913990; // sqrt(pi)/2
const R s2pi = 2.5066282746310005024157652848110; // sqrt(2*pi)
const R pi   = 3.141592653589793238462643383279503;

/******************************************************************************/
/*  Simple wrappers: cerfcx, cerfi, erfi, dawson                              */
/******************************************************************************/

cmplx cerfcx(cmplx z)
{
    // Compute erfcx(z) = exp(z^2) erfc(z),
    // the complex underflow-compensated complementary error function,
    // trivially related to Faddeeva's w_of_z.

    return w_of_z(CNUM(-pnfft_cimag(z), pnfft_creal(z)));
}

cmplx cerfi(cmplx z)
{
    // Compute erfi(z) = -i erf(iz),
    // the rotated complex error function.

    cmplx e = cerf(CNUM(-pnfft_cimag(z),pnfft_creal(z)));
    return CNUM(pnfft_cimag(e), -pnfft_creal(e));
}

R erfi(R x)
{
    // Compute erfi(x) = -i erf(ix),
    // the imaginary error function.

    return x*x > 720 ? (x > 0 ? Inf : -Inf) : pnfft_exp(x*x) * im_w_of_x(x);
}

R dawson(R x)
{

    // Compute dawson(x) = sqrt(pi)/2 * exp(-x^2) * erfi(x),
    // Dawson's integral for a real argument.

    return spi2 * im_w_of_x(x);
}

R re_w_of_z( R x, R y )
{
    return pnfft_creal( w_of_z( CNUM(x,y) ) );
}

R im_w_of_z( R x, R y )
{
    return pnfft_cimag( w_of_z( CNUM(x,y) ) );
}

/******************************************************************************/
/*  voigt                                                                     */
/******************************************************************************/

R voigt( R x, R sigma, R gamma )
{
    // Joachim Wuttke, January 2013.

    // Compute Voigt's convolution of a Gaussian
    //    G(x,sigma) = 1/sqrt(2*pi)/|sigma| * exp(-x^2/2/sigma^2)
    // and a Lorentzian
    //    L(x,gamma) = |gamma| / pi / ( x^2 + gamma^2 ),
    // namely
    //    voigt(x,sigma,gamma) =
    //          \int_{-infty}^{infty} dx' G(x',sigma) L(x-x',gamma)
    // using the relation
    //    voigt(x,sigma,gamma) = Re{ w(z) } / sqrt(2*pi) / |sigma|
    // with
    //    z = (x+i*|gamma|) / sqrt(2) / |sigma|.

    // Reference: Abramowitz&Stegun (1964), formula (7.4.13).

    R gam = gamma < 0 ? -gamma : gamma;
    R sig = sigma < 0 ? -sigma : sigma;

    if ( gam==0 ) {
        if ( sig==0 ) {
            // It's kind of a delta function
            return x ? 0 : Inf;
        } else {
            // It's a pure Gaussian
            return pnfft_exp( -x*x/2/(sig*sig) ) / s2pi / sig;
        }
    } else {
        if ( sig==0 ) {
            // It's a pure Lorentzian
            return gam / pi / (x*x + gam*gam);
        } else {
            // Regular case, both parameters are nonzero
            cmplx z = CNUM(x,gam) / pnfft_sqrt(2) / sig;
            return pnfft_creal( w_of_z(z) ) / s2pi / sig;
        }
    }
}

/******************************************************************************/
/*  cerf                                                                      */
/******************************************************************************/

cmplx cerf(cmplx z)
{

    // Steven G. Johnson, October 2012.

    // Compute erf(z), the complex error function,
    // using w_of_z except for certain regions.

    R x = pnfft_creal(z), y = pnfft_cimag(z);

    if (y == 0)
        return CNUM(erf(x), y); // preserve sign of 0
    if (x == 0) // handle separately for speed & handling of y = Inf or NaN
        return CNUM(x, // preserve sign of 0
                 /* handle y -> Inf limit manually, since
                    exp(y^2) -> Inf but Im[w(y)] -> 0, so
                    IEEE will give us a NaN when it should be Inf */
                 y*y > 720 ? (y > 0 ? Inf : -Inf)
                 : pnfft_exp(y*y) * im_w_of_x(y));
  
    R mRe_z2 = (y - x) * (x + y); // Re(-z^2), being careful of overflow
    R mIm_z2 = -2*x*y; // Im(-z^2)
    if (mRe_z2 < -750) // underflow
        return (x >= 0 ? 1.0 : -1.0);

    /* Handle positive and negative x via different formulas,
       using the mirror symmetries of w, to avoid overflow/underflow
       problems from multiplying exponentially large and small quantities. */
    if (x >= 0) {
        if (x < 8e-2) {
            if (pnfft_fabs(y) < 1e-2)
                goto taylor;
            else if (pnfft_fabs(mIm_z2) < 5e-3 && x < 5e-3)
                goto taylor_erfi;
        }
        /* don't use complex exp function, since that will produce spurious NaN
           values when multiplying w in an overflow situation. */
        return 1.0 - pnfft_exp(mRe_z2) *
            (CNUM(pnfft_cos(mIm_z2), pnfft_sin(mIm_z2))
             * w_of_z(CNUM(-y,x)));
    }
    else { // x < 0
        if (x > -8e-2) { // duplicate from above to avoid pnfft_fabs(x) call
            if (pnfft_fabs(y) < 1e-2)
                goto taylor;
            else if (pnfft_fabs(mIm_z2) < 5e-3 && x > -5e-3)
                goto taylor_erfi;
        }
        else if (isnan(x))
            return CNUM(NaN, y == 0 ? 0 : NaN);
        /* don't use complex exp function, since that will produce spurious NaN
           values when multiplying w in an overflow situation. */
        return pnfft_exp(mRe_z2) *
            (CNUM(pnfft_cos(mIm_z2), pnfft_sin(mIm_z2))
             * w_of_z(CNUM(y,-x))) - 1.0;
    }

    // Use Taylor series for small |z|, to avoid cancellation inaccuracy
    //   erf(z) = 2/sqrt(pi) * z * (1 - z^2/3 + z^4/10 - z^6/42 + z^8/216 + ...)
taylor:
    {
        cmplx mz2 = CNUM(mRe_z2, mIm_z2); // -z^2
        return z * (1.1283791670955125739
                    + mz2 * (0.37612638903183752464
                             + mz2 * (0.11283791670955125739
                                      + mz2 * (0.026866170645131251760
                                               + mz2 * 0.0052239776254421878422))));
    }

    /* for small |x| and small |xy|, 
       use Taylor series to avoid cancellation inaccuracy:
       erf(x+iy) = erf(iy)
       + 2*exp(y^2)/sqrt(pi) *
       [ x * (1 - x^2 * (1+2y^2)/3 + x^4 * (3+12y^2+4y^4)/30 + ... 
       - i * x^2 * y * (1 - x^2 * (3+2y^2)/6 + ...) ]
       where:
       erf(iy) = exp(y^2) * Im[w(y)]
    */
taylor_erfi:
    {
        R x2 = x*x, y2 = y*y;
        R expy2 = pnfft_exp(y2);
        return CNUM
            (expy2 * x * (1.1283791670955125739
                          - x2 * (0.37612638903183752464
                                  + 0.75225277806367504925*y2)
                          + x2*x2 * (0.11283791670955125739
                                     + y2 * (0.45135166683820502956
                                             + 0.15045055561273500986*y2))),
             expy2 * (im_w_of_x(y)
                      - x2*y * (1.1283791670955125739 
                                - x2 * (0.56418958354775628695 
                                        + 0.37612638903183752464*y2))));
    }
} // cerf

/******************************************************************************/
/*  cerfc                                                                     */
/******************************************************************************/

cmplx cerfc(cmplx z)
{
    // Steven G. Johnson, October 2012.

    // Compute erfc(z) = 1 - erf(z), the complex complementary error function,
    // using w_of_z except for certain regions.

    R x = pnfft_creal(z), y = pnfft_cimag(z);

    if (x == 0.)
        return CNUM(1,
                 /* handle y -> Inf limit manually, since
                    exp(y^2) -> Inf but Im[w(y)] -> 0, so
                    IEEE will give us a NaN when it should be Inf */
                 y*y > 720 ? (y > 0 ? -Inf : Inf)
                 : -pnfft_exp(y*y) * im_w_of_x(y));
    if (y == 0.) {
        if (x*x > 750) // underflow
            return CNUM(x >= 0 ? 0.0 : 2.0,
                     -y); // preserve sign of 0
        return CNUM(x >= 0 ? pnfft_exp(-x*x) * erfcx(x) 
                 : 2. - pnfft_exp(-x*x) * erfcx(-x),
                 -y); // preserve sign of zero
    }

    R mRe_z2 = (y - x) * (x + y); // Re(-z^2), being careful of overflow
    R mIm_z2 = -2*x*y; // Im(-z^2)
    if (mRe_z2 < -750) // underflow
        return (x >= 0 ? 0.0 : 2.0);

    if (x >= 0)
        return pnfft_cexp(CNUM(mRe_z2, mIm_z2))
            * w_of_z(CNUM(-y,x));
    else
        return 2.0 - pnfft_cexp(CNUM(mRe_z2, mIm_z2))
            * w_of_z(CNUM(y,-x));
} // cerfc

/******************************************************************************/
/*  cdawson                                                                   */
/******************************************************************************/

cmplx cdawson(cmplx z)
{

    // Steven G. Johnson, October 2012.

    // Compute Dawson(z) = sqrt(pi)/2  *  exp(-z^2) * erfi(z),
    // Dawson's integral for a complex argument,
    // using w_of_z except for certain regions.

    R x = pnfft_creal(z), y = pnfft_cimag(z);

    // handle axes separately for speed & proper handling of x or y = Inf or NaN
    if (y == 0)
        return CNUM(spi2 * im_w_of_x(x),
                 -y); // preserve sign of 0
    if (x == 0) {
        R y2 = y*y;
        if (y2 < 2.5e-5) { // Taylor expansion
            return CNUM(x, // preserve sign of 0
                     y * (1.
                          + y2 * (0.6666666666666666666666666666666666666667
                                  + y2 * 0.26666666666666666666666666666666666667)));
        }
        return CNUM(x, // preserve sign of 0
                 spi2 * (y >= 0 
                         ? pnfft_exp(y2) - erfcx(y)
                         : erfcx(-y) - pnfft_exp(y2)));
    }

    R mRe_z2 = (y - x) * (x + y); // Re(-z^2), being careful of overflow
    R mIm_z2 = -2*x*y; // Im(-z^2)
    cmplx mz2 = CNUM(mRe_z2, mIm_z2); // -z^2

    /* Handle positive and negative x via different formulas,
       using the mirror symmetries of w, to avoid overflow/underflow
       problems from multiplying exponentially large and small quantities. */
    if (y >= 0) {
        if (y < 5e-3) {
            if (pnfft_fabs(x) < 5e-3)
                goto taylor;
            else if (pnfft_fabs(mIm_z2) < 5e-3)
                goto taylor_realaxis;
        }
        cmplx res = pnfft_cexp(mz2) - w_of_z(z);
        return spi2 * CNUM(-pnfft_cimag(res), pnfft_creal(res));
    }
    else { // y < 0
        if (y > -5e-3) { // duplicate from above to avoid pnfft_fabs(x) call
            if (pnfft_fabs(x) < 5e-3)
                goto taylor;
            else if (pnfft_fabs(mIm_z2) < 5e-3)
                goto taylor_realaxis;
        }
        else if (isnan(y))
            return CNUM(x == 0 ? 0 : NaN, NaN);
        cmplx res = w_of_z(-z) - pnfft_cexp(mz2);
        return spi2 * CNUM(-pnfft_cimag(res), pnfft_creal(res));
    }

    // Use Taylor series for small |z|, to avoid cancellation inaccuracy
    //     dawson(z) = z - 2/3 z^3 + 4/15 z^5 + ...
taylor:
    return z * (1.
                + mz2 * (0.6666666666666666666666666666666666666667
                         + mz2 * 0.2666666666666666666666666666666666666667));

    /* for small |y| and small |xy|, 
       use Taylor series to avoid cancellation inaccuracy:
       dawson(x + iy)
       = D + y^2 (D + x - 2Dx^2)
       + y^4 (D/2 + 5x/6 - 2Dx^2 - x^3/3 + 2Dx^4/3)
       + iy [ (1-2Dx) + 2/3 y^2 (1 - 3Dx - x^2 + 2Dx^3)
       + y^4/15 (4 - 15Dx - 9x^2 + 20Dx^3 + 2x^4 - 4Dx^5) ] + ...
       where D = dawson(x) 

       However, for large |x|, 2Dx -> 1 which gives cancellation problems in
       this series (many of the leading terms cancel).  So, for large |x|,
       we need to substitute a continued-fraction expansion for D.

       dawson(x) = 0.5 / (x-0.5/(x-1/(x-1.5/(x-2/(x-2.5/(x...))))))

       The 6 terms shown here seems to be the minimum needed to be
       accurate as soon as the simpler Taylor expansion above starts
       breaking down.  Using this 6-term expansion, factoring out the
       denominator, and simplifying with Maple, we obtain:

       Re dawson(x + iy) * (-15 + 90x^2 - 60x^4 + 8x^6) / x
       = 33 - 28x^2 + 4x^4 + y^2 (18 - 4x^2) + 4 y^4
       Im dawson(x + iy) * (-15 + 90x^2 - 60x^4 + 8x^6) / y
       = -15 + 24x^2 - 4x^4 + 2/3 y^2 (6x^2 - 15) - 4 y^4

       Finally, for |x| > 5e7, we can use a simpler 1-term continued-fraction
       expansion for the real part, and a 2-term expansion for the imaginary
       part.  (This avoids overflow problems for huge |x|.)  This yields:
     
       Re dawson(x + iy) = [1 + y^2 (1 + y^2/2 - (xy)^2/3)] / (2x)
       Im dawson(x + iy) = y [ -1 - 2/3 y^2 + y^4/15 (2x^2 - 4) ] / (2x^2 - 1)

    */
taylor_realaxis:
    {
        R x2 = x*x;
        if (x2 > 1600) { // |x| > 40
            R y2 = y*y;
            if (x2 > 25e14) {// |x| > 5e7
                R xy2 = (x*y)*(x*y);
                return CNUM((0.5 + y2 * (0.5 + 0.25*y2
                                      - 0.16666666666666666667*xy2)) / x,
                         y * (-1 + y2 * (-0.66666666666666666667
                                         + 0.13333333333333333333*xy2
                                         - 0.26666666666666666667*y2))
                         / (2*x2 - 1));
            }
            return (1. / (-15 + x2*(90 + x2*(-60 + 8*x2)))) *
                CNUM(x * (33 + x2 * (-28 + 4*x2)
                       + y2 * (18 - 4*x2 + 4*y2)),
                  y * (-15 + x2 * (24 - 4*x2)
                       + y2 * (4*x2 - 10 - 4*y2)));
        }
        else {
            R D = spi2 * im_w_of_x(x);
            R y2 = y*y;
            return CNUM
                (D + y2 * (D + x - 2*D*x2)
                 + y2*y2 * (D * (0.5 - x2 * (2 - 0.66666666666666666667*x2))
                            + x * (0.83333333333333333333
                                   - 0.33333333333333333333 * x2)),
                 y * (1 - 2*D*x
                      + y2 * 0.66666666666666666667 * (1 - x2 - D*x * (3 - 2*x2))
                      + y2*y2 * (0.26666666666666666667 -
                                 x2 * (0.6 - 0.13333333333333333333 * x2)
                                 - D*x * (1 - x2 * (1.3333333333333333333
                                                    - 0.26666666666666666667 * x2)))));
        }
    }
} // cdawson