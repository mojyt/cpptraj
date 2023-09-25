#include <vector>
#include "Zmatrix.h"
#include "../Frame.h"
#include "../CpptrajStdio.h"
#include "../Constants.h"
#include <cmath> // cos

using namespace Cpptraj::Structure;

/** CONSTRUCTOR */
Zmatrix::Zmatrix() :
  seed0_(InternalCoords::NO_ATOM),
  seed1_(InternalCoords::NO_ATOM),
  seed2_(InternalCoords::NO_ATOM)
{}

/** Add internal coords */
void Zmatrix::AddIC(InternalCoords const& ic) {
  IC_.push_back( ic );
}

/** Print to stdout */
void Zmatrix::print() const {
  mprintf("%zu internal coords.\n", IC_.size());
  for (ICarray::const_iterator it = IC_.begin(); it != IC_.end(); ++it)
    mprintf("\t%8li %8i %8i %8i %12.4f %12.4f %12.4f\n", it - IC_.begin() + 1,
            it->AtJ()+1, it->AtK()+1, it->AtL()+1,
            it->Dist(), it->Theta(), it->Phi());
}

/** Setup Zmatrix from Cartesian coordinates/topology. */
int Zmatrix::SetFromFrame(Frame const& frameIn, Topology const& topIn)
{
  IC_.clear();
  // First seed is first atom. No bonds, angles, or torsions.
  IC_.push_back( InternalCoords() );
  seed0_ = 0;

  // Choose second seed as bonded atom with lowest index. Prefer heavy atoms

  return 0;
}

/** Set Cartesian coordinates in Frame from internal coordinates. */
int Zmatrix::SetToFrame(Frame& frameOut) const {
  if ((unsigned int)frameOut.Natom() != IC_.size()) {
    mprinterr("Internal Error: Output frame size (%i) != # internal coords (%zu)\n",
              frameOut.Natom(), IC_.size());
    return 1;
  }
  // Set position of the first atom.
  if (seed0_ != InternalCoords::NO_ATOM) {
    //IC_[seed0_].ZeroXYZ();
    frameOut.SetXYZ(seed0_, Vec3(0.0));
    // Set position of the second atom.
    if (seed1_ != InternalCoords::NO_ATOM) {
      if (IC_[seed1_].AtJ() != seed0_) {
        mprinterr("Internal Error: Atom j of seed 1 is not seed 0.\n");
        return 1;
      }
      double r1 = IC_[seed1_].Dist();
      //IC_[seed1_].SetXYZ( Vec3(r1, 0, 0) );
      frameOut.SetXYZ(seed1_, Vec3(r1, 0, 0));
      // Set position of the third atom
      if (seed2_ != InternalCoords::NO_ATOM) {
        if (IC_[seed2_].AtJ() != seed1_) {
          mprinterr("Internal Error: Atom j of seed 2 is not seed 1.\n");
          return 1;
        }
        if (IC_[seed2_].AtK() != seed0_) {
          mprinterr("Internal Error: Atom k of seed 2 is not seed 0.\n");
          return 1;
        }
        double r2 = IC_[seed2_].Dist();
        double theta = IC_[seed2_].Theta();

        double x = r2 * cos(180.0 - theta) * Constants::DEGRAD;
        double y = r2 * cos(180.0 - theta) * Constants::DEGRAD;

        //IC_[seed2].SetXYZ( Vec3(r1 + x, y, 0) );
        frameOut.SetXYZ( seed2_, Vec3(r1 + x, y, 0) );
      } // END seed atom 2
    } // END seed atom 1
  } // END seed atom 0

  return 0;
}
