#ifndef INC_ANALYSIS_CALCDIFFUSION_H
#define INC_ANALYSIS_CALCDIFFUSION_H
#include "Analysis.h"
/// Calculate diffusion from unwrapped coordinates using multiple time origins 
class Analysis_CalcDiffusion : public Analysis {
  public:
    Analysis_CalcDiffusion() {}
    DispatchObject* Alloc() const { return (DispatchObject*)new Analysis_CalcDiffusion(); }
    void Help() const;

    Analysis::RetType Setup(ArgList&, AnalysisSetup&, int);
    Analysis::RetType Analyze();
  private:
    DataSet_Coords* TgtTraj_; ///< Coordinates to calculate diffusion for
    int maxlag_;              ///< Maximum number of frames to use for each origin
    AtomMask mask1_;          ///< Atoms to track diffusion for.
};
#endif
