#include "Ewald_Recip.h"
#include "ErfcFxn.h" // erfc_func
#include "EwaldParams.h"
#include "../Constants.h"
#include "../CpptrajStdio.h"
#include "../Matrix_3x3.h"
#include "../Vec3.h"
#include <cmath> // sqrt
#include <algorithm> // std::max 

using namespace Cpptraj::Energy;

// Absolute value functions
static inline int IABS(   int    xIn) { if (xIn < 0  ) return -xIn; else return xIn; }
static inline double DABS(double xIn) { if (xIn < 0  ) return -xIn; else return xIn; }

/** CONSTRUCTOR */
Ewald_Recip::Ewald_Recip() :
# ifdef _OPENMP
  multCut_(0),
# endif
  maxexp_(0.0),
  rsumTol_(0.0),
  maxmlim_(0)
{
  mlimit_[0] = 0;
  mlimit_[1] = 0;
  mlimit_[2] = 0;
}

/** \return maxexp value based on mlimits */
double Ewald_Recip::FindMaxexpFromMlim(const int* mlimit, Matrix_3x3 const& recip) {
  double maxexp = DABS( (double)mlimit[0] * recip[0] );
  double z2     = DABS( (double)mlimit[1] * recip[4] );
  maxexp = std::max(maxexp, z2);
  double z3     = DABS( (double)mlimit[2] * recip[8] );
  maxexp = std::max(maxexp, z3);
  return maxexp;
}

/** \return maxexp value based on Ewald coefficient and reciprocal sum tolerance. */
double Ewald_Recip::FindMaxexpFromTol(double ewCoeff, double rsumTol) {
  double xval = 0.5;
  int nloop = 0;
  double term = 0.0;
  do {
    xval = 2.0 * xval;
    nloop++;
    double yval = Constants::PI * xval / ewCoeff;
    term = 2.0 * ewCoeff * ErfcFxn::erfc_func(yval) * EwaldParams::INVSQRTPI();
  } while (term >= rsumTol);

  // Binary search tolerance is 2^-60
  int ntimes = nloop + 60;
  double xlo = 0.0;
  double xhi = xval;
  for (int i = 0; i != ntimes; i++) {
    xval = (xlo + xhi) / 2.0;
    double yval = Constants::PI * xval / ewCoeff;
    double term = 2.0 * ewCoeff * ErfcFxn::erfc_func(yval) * EwaldParams::INVSQRTPI();
    if (term > rsumTol)
      xlo = xval;
    else
      xhi = xval;
  }
  mprintf("\tMaxExp for Ewald coefficient %g, direct sum tol %g is %g\n",
          ewCoeff, rsumTol, xval);
  return xval;
}


/** Get mlimits. */
void Ewald_Recip::GetMlimits(int* mlimit, double maxexp, double eigmin, 
                       Vec3 const& reclng, Matrix_3x3 const& recip)
{
  //mprintf("DEBUG: Recip lengths %12.4f%12.4f%12.4f\n", reclng[0], reclng[1], reclng[2]);

  int mtop1 = (int)(reclng[0] * maxexp / sqrt(eigmin));
  int mtop2 = (int)(reclng[1] * maxexp / sqrt(eigmin));
  int mtop3 = (int)(reclng[2] * maxexp / sqrt(eigmin));

  int nrecvecs = 0;
  mlimit[0] = 0;
  mlimit[1] = 0;
  mlimit[2] = 0;
  double maxexp2 = maxexp * maxexp;
  for (int m1 = -mtop1; m1 <= mtop1; m1++) {
    for (int m2 = -mtop2; m2 <= mtop2; m2++) {
      for (int m3 = -mtop3; m3 <= mtop3; m3++) {
        Vec3 Zvec = recip.TransposeMult( Vec3(m1,m2,m3) );
        if ( Zvec.Magnitude2() <= maxexp2 ) {
          nrecvecs++;
          mlimit[0] = std::max( mlimit[0], IABS(m1) );
          mlimit[1] = std::max( mlimit[1], IABS(m2) );
          mlimit[2] = std::max( mlimit[2], IABS(m3) );
        }
      }
    }
  }
  mprintf("\tNumber of reciprocal vectors: %i\n", nrecvecs);
}

