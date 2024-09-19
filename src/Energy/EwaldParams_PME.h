#ifndef INC_ENERGY_EWALDPARAMS_PME_H
#define INC_ENERGY_EWALDPARAMS_PME_H
#include "EwaldParams.h"
namespace Cpptraj {
namespace Energy {
class EwaldParams_PME : public EwaldParams {
  public:
    EwaldParams_PME();
    int InitEwald(Box const&, EwaldOptions const&, int);
  private:
    int nfft_[3]; ///< Number of FFT grid points in each direction
    int order_;   ///< PME B spline order
};
}
}
#endif
