#ifndef INC_PAIRLISTENGINE_EWALD_LJPME_H
#define INC_PAIRLISTENGINE_EWALD_LJPME_H
#include "Energy/EwaldParams_LJPME.h"
#include "Energy/Ene_LJPME_6_12.h"
#include "Energy/Kernel_EwaldAdjust.h"
#include "PairList.h"
namespace Cpptraj {
/// Direct space nonbond calculation using pairlist with Ewald and LJPME for VDW
template <typename T>
class PairListEngine_Ewald_LJPME {
  public:
    PairListEngine_Ewald_LJPME() {}
    // -------------------------------------------
    /// Call at the beginning of the frame calculation
    void FrameBeginCalc() { Evdw_ = 0; Eelec_ = 0; Eadjust_ = 0;
                            Eljpme_correction_ = 0; Eljpme_correction_excl_ = 0; }
    /// Call for atom 0 when looping over atoms of thisCell
    void SetupAtom0( PairList::AtmType const& atom0 ) {
      q0_ = EW_.Charge(atom0.Idx());
    }
    /// Call for atom 1 when looping over interaction atoms of this/other cell
    void SetupAtom1( PairList::AtmType const& atom1 ) {
      q1_ = EW_.Charge(atom1.Idx());
    }
    /// Call when cutoff is satisfied
    void CutoffSatisfied(T const& rij2,
                         PairList::AtmType const& atom0,
                         PairList::AtmType const& atom1)
    {
      T rij = sqrt( rij2 );
      T qiqj = q0_ * q1_;
      //double erfc = erfc_func(ew_coeff_ * rij);
      T erfcval = EW_.ErfcEW( rij );
      T e_elec = qiqj * erfcval / rij;
      Eelec_ += e_elec;

      int nbindex = EW_.NbIndex(atom0.Idx(), atom1.Idx());
      if (nbindex > -1) {
        double vswitch = EW_.Switch_Fn(rij2);
        NonbondType const& LJ = EW_.GetLJ( nbindex );
        T e_vdw, e_pmevdw;
        Cpptraj::Energy::
        Ene_LJPME_6_12<T>( e_vdw, e_pmevdw,
                           rij2, LJ.A(), LJ.B(), EW_.LJ_EwaldCoeff(),
                           EW_.CalcCij(atom0.Idx(), atom1.Idx()) );
        Evdw_ += (e_vdw * vswitch);
        Eljpme_correction_ += (e_pmevdw * vswitch);
/*
        T r2    = 1.0 / rij2;
        T r6    = r2 * r2 * r2;
        T r12   = r6 * r6;
        T f12   = LJ.A() * r12;  // A/r^12
        T f6    = LJ.B() * r6;   // B/r^6
        T e_vdw = f12 - f6;      // (A/r^12)-(B/r^6)
        Evdw_ += (e_vdw * vswitch);
        //mprintf("PVDW %8i%8i%20.6f%20.6f\n", ta0+1, ta1+1, e_vdw, r2);
        // LJ PME direct space correction
        T kr2 = EW_.LJ_EwaldCoeff() * EW_.LJ_EwaldCoeff() * rij2;
        T kr4 = kr2 * kr2;
        //double kr6 = kr2 * kr4;
        T expterm = exp(-kr2);
        T Cij = EW_.CalcCij(atom0.Idx(), atom1.Idx()); //Cparam_[it0->Idx()] * Cparam_[it1->Idx()];
        Eljpme_correction_ += (1.0 - (1.0 +  kr2 + kr4/2.0)*expterm) * r6 * vswitch * Cij;*/
      }
    }
    /// Call when cutoff is not satisfied
    void AtomPairExcluded(T const& rij2,
                            PairList::AtmType const& atom0,
                            PairList::AtmType const& atom1)
    {
      T rij = sqrt(rij2);
      T erfcval = EW_.ErfcEW( rij );
      Eadjust_ += Cpptraj::Energy::Kernel_EwaldAdjust<T>( q0_, q1_, rij, erfcval );
      // LJ PME direct space exclusion correction
      // NOTE: Assuming excluded pair is within cutoff
      T kr2 = EW_.LJ_EwaldCoeff() * EW_.LJ_EwaldCoeff() * rij2;
      T kr4 = kr2 * kr2;
      //double kr6 = kr2 * kr4;
      T expterm = exp(-kr2);
      T r4 = rij2 * rij2;
      T r6 = rij2 * r4;
      T Cij = EW_.CalcCij(atom0.Idx(), atom1.Idx()); //Cparam_[it0->Idx()] * Cparam_[it1->Idx()];
      Eljpme_correction_excl_ += (1.0 - (1.0 +  kr2 + kr4/2.0)*expterm) / r6 * Cij;
    }
    // -------------------------------------------
    Cpptraj::Energy::EwaldParams_LJPME& ModifyEwaldParams() { return EW_; }
    Cpptraj::Energy::EwaldParams_LJPME const& EwaldParams() const { return EW_; }

    T Evdw() const { return Evdw_ + Eljpme_correction_ + Eljpme_correction_excl_; }
    T Eelec() const { return Eelec_; }
    T Eadjust() const { return Eadjust_; }
#   ifdef _OPENMP
    /// To allow reduction of the energy terms
    void operator+=(PairListEngine_Ewald_LJPME const& rhs) {
      Evdw_ += rhs.Evdw_;
      Eelec_ += rhs.Eelec_;
      Eadjust_ += rhs.Eadjust_;
      Eljpme_correction_ += rhs.Eljpme_correction_;
      Eljpme_correction_excl_ += rhs.Eljpme_correction_excl_;
    }
#   endif
  private:
    T q0_;                  ///< Charge on atom 0
    T q1_;                  ///< Charge on atom 1
    T Evdw_;                ///< VDW sum for current frame
    T Eelec_;               ///< Coulomb sum for current frame
    T Eadjust_;             ///< Adjust energy sum for current frame
    T Eljpme_correction_;   ///< LJ PME correction for VDW
    T Eljpme_correction_excl_; ///< LJ PME correction for adjust

    Cpptraj::Energy::EwaldParams_LJPME EW_;          ///< Hold Ewald parameters for LJPME
};
#ifdef _OPENMP
#pragma omp declare reduction( + : PairListEngine_Ewald_LJPME<double> : omp_out += omp_in ) initializer( omp_priv = omp_orig )
#endif
} // END namespace Cpptraj
#endif
