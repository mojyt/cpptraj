#include "CoordCovarMatrix_Half.h"
#include "AtomMask.h"
#include "CpptrajFile.h"
#include "CpptrajStdio.h"
#include "Frame.h"
#include <cmath> // sqrt

/** CONSTRUCTOR */
CoordCovarMatrix_Half::CoordCovarMatrix_Half() :
  CoordCovarMatrix(3)
{}

/** Clear the matrix */
void CoordCovarMatrix_Half::clearMat() {
  vect_.clear();
  mass_.clear();
}

/** Set up array sizess and masses. */
int CoordCovarMatrix_Half::SetupMatrix(std::vector<Atom> const& atoms,
                                       AtomMask const& maskIn, bool useMassIn)
{
  // Matrix - half
  covarMatrix_.resize( maskIn.Nselected()*nelt_, 0 );

  vect_.assign( maskIn.Nselected()*nelt_, 0 );

  set_mass_array(mass_, atoms, maskIn, useMassIn);

  return 0;
}

/** Add selected atoms in given Frame to the matrix. */
void CoordCovarMatrix_Half::AddFrameToMatrix(Frame const& frameIn, AtomMask const& maskIn)
{
  // Covariance
  MatType::iterator mat = covarMatrix_.begin();
  for (int idx2 = 0; idx2 < maskIn.Nselected(); idx2++) {
    int at2 = maskIn[idx2];
    Vec3 XYZj( frameIn.XYZ(at2) );
    unsigned int eidx2 = idx2 * nelt_;
    // Store average 
    for (unsigned int ej = 0; ej < nelt_; ej++)
      vect_[eidx2+ej] += XYZj[ej];
    //XYZj.Print("XYZj");
    // Loop over X, Y, and Z of vecj
    for (unsigned int jidx = 0; jidx < nelt_; jidx++) {
      double Vj = XYZj[jidx];
      // Diagonal
      for (unsigned int ej = jidx; ej < nelt_; ej++)
        *(mat++) += Vj * XYZj[ej]; // Vj * j{0,1,2}, Vj * j{1,2}, Vj * j{2}
      // Inner loop
      for (int idx1 = idx2 + 1; idx1 < maskIn.Nselected(); idx1++) {
        int at1 = maskIn[idx1];
        Vec3 XYZi( frameIn.XYZ(at1) );
        *(mat++) += Vj * XYZi[0];
        *(mat++) += Vj * XYZi[1];
        *(mat++) += Vj * XYZi[2];
      } // END inner loop over idx1
    } // END loop over x y z of vecj
  } // END outer loop over idx2
  nframes_++;
}

/** Add given Frame to the matrix. */
/*void CoordCovarMatrix_Half::AddFrameToMatrix(Frame const& frameIn)
{
  // Covariance
  MatType::iterator mat = covarMatrix_.begin();
  for (int idx1 = 0; idx1 < frameIn.Natom(); idx1++) {
    Vec3 XYZi( frameIn.XYZ(idx1) );
    // Store veci and veci^2
    vect_[idx1] += XYZi;
    //XYZi.Print("XYZi");
    //vect2[idx1] += XYZi.Squared();
    // Loop over X, Y, and Z of veci
    for (int iidx = 0; iidx < 3; iidx++) {
      double Vi = XYZi[iidx];
      // Diagonal
      for (int jidx = iidx; jidx < 3; jidx++)
        *(mat++) += Vi * XYZi[jidx]; // Vi * j{0,1,2}, Vi * j{1,2}, Vi * j{2}
      // Inner loop
      for (int idx2 = idx1 + 1; idx2 < frameIn.Natom(); idx2++) {
        Vec3 XYZj( frameIn.XYZ(idx2) );
        *(mat++) += Vi * XYZj[0];
        *(mat++) += Vi * XYZj[1];
        *(mat++) += Vi * XYZj[2];
      } // END inner loop over idx2
    } // END loop over x y z of veci
  } // END outer loop over idx1
  nframes_++;
}*/

/** Finish the matrix. */
int CoordCovarMatrix_Half::FinishMatrix() {
  if (nframes_ < 1) {
    mprinterr("Error: No frames in coordinate covariance matrix.\n");
    return 1;
  }
  // Normalize
  double norm = 1.0 / (double)nframes_;
  for (Darray::iterator it = vect_.begin(); it != vect_.end(); ++it)
    *it *= norm;
  for (MatType::iterator it = covarMatrix_.begin(); it != covarMatrix_.end(); ++it)
    *it *= norm;
  // Calc <riri> - <ri><ri>
  //for (int k = 0; k < mask1_.Nselected(); k++) {
  //  vect2[k][0] -= (vect[k][0] * vect[k][0]);
  //  vect2[k][1] -= (vect[k][1] * vect[k][1]);
  //  vect2[k][2] -= (vect[k][2] * vect[k][2]);
  //}
  // Calc <rirj> - <ri><rj>
  MatType::iterator mat = covarMatrix_.begin();
  for (unsigned int idx2 = 0; idx2 < mass_.size(); idx2++) {
    double mass2 = mass_[idx2];
    for (unsigned int jidx = 0; jidx < nelt_; jidx++) {
      unsigned int eidx2 = idx2*nelt_;
      double Vj = vect_[eidx2 + jidx];
      for (unsigned int idx1 = idx2; idx1 < mass_.size(); idx1++) {
        double Mass = sqrt( mass2 * mass_[idx1] );
        if (idx2 == idx1) {
          // Self
          for (unsigned int ej = jidx; ej < nelt_; ej++) {
            *mat = (*mat - (Vj * vect_[eidx2 + ej])) * Mass;
            ++mat;
          }
        } else {
          unsigned int eidx1 = idx1*nelt_;
          for (unsigned int iidx = 0; iidx < nelt_; iidx++) {
            *mat = (*mat - (Vj * vect_[eidx1 + iidx])) * Mass;
            ++mat;
          }
        }
      } // END inner loop over idx1
    } // END loop over elements of vect_[idx2]
  } // END outer loop over idx2
  return 0;
}
