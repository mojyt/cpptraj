#ifndef INC_ANALYSIS_TICA_H
#define INC_ANALYSIS_TICA_H
#include "Analysis.h"
#include "Array1D.h"
class DataSet_2D;
class DataSet_Modes;
/// <Enter description of Analysis_TICA here>
class Analysis_TICA : public Analysis {
  public:
    Analysis_TICA();
    DispatchObject* Alloc() const { return (DispatchObject*)new Analysis_TICA(); }
    void Help() const;

    Analysis::RetType Setup(ArgList&, AnalysisSetup&, int);
    Analysis::RetType Analyze();
  private:
    enum EvectorScaleType {
      NO_SCALING = 0, ///< Do not scale eigenvectors
      KINETIC_MAP,    ///< Scale eigenvectors by eigenvalues
      COMMUTE_MAP     ///< Scale eigenvectors by regularized time scales
    };

    typedef std::vector<DataSet_1D*> DSarray;
    typedef std::vector<double> Darray;

    /// Analyze using coordinates data set (TgtTraj_)
    Analysis::RetType analyze_crdset();
    /// Analyze using 1D data sets (sets_)
    Analysis::RetType analyze_datasets();
    /// Calculate instantaneous and lagged covariance matrices
    int calculateCovariance_C0CT(DSarray const&) const;

    Array1D sets_;                  ///< Input 1D data sets (data)
    DataSet_Coords* TgtTraj_;       ///< Input trajectory (crdset)
    AtomMask mask1_;                ///< Atoms to use in matrix calc
    AtomMask mask2_;                ///< Second atom mask for debugging full covar matrix
    int lag_;                       ///< TICA time lag
    bool useMass_;                  ///< Control whether to mass-weight
    CpptrajFile* debugC0_;          ///< Debug output for C0
    CpptrajFile* debugCT_;          ///< Debug output for CT
    EvectorScaleType evectorScale_; ///< Eigenvector scaling type
    DataSet_Modes* ticaModes_;      ///< Output TICA modes
};
#endif
