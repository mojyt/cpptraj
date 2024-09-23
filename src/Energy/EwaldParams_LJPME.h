#ifndef INC_ENERGY_EWALDPARAMS_LJPME_H
#define INC_ENERGY_EWALDPARAMS_LJPME_H
#include "EwaldParams_PME.h"
namespace Cpptraj {
namespace Energy {
class EwaldParams_LJPME : public EwaldParams_PME {
  public:
    EwaldParams_LJPME();

    int InitEwald(Box const&, EwaldOptions const&, int);
    int SetupEwald(Topology const&, AtomMask const&);

    /// \return LJ Ewald coefficient
    double LJ_EwaldCoeff() const { return lw_coeff_; }

    /// \return LJPME self energy
    double Self6() const { return ljpme_self_; }
    /// \return C6 parameter pair for specified selected atoms
    double CalcCij(int idx0, int idx1) const { return (Cparam_[idx0] * Cparam_[idx1]); }
  private:
   Darray Cparam_;   ///< Hold selected atomic C6 coefficients for LJ PME
   double lw_coeff_; ///< LJ Ewald coefficient
   double ljpme_self_; ///< LJ PME self energy calculated from C6 parameters and LJ Ewald coeff.
};
}
}
#endif
