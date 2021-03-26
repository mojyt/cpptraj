#include "PmeOptions.h"
#include "CpptrajStdio.h"
#include "ArgList.h"

/** CONSTRUCTOR */
PmeOptions::PmeOptions() :
  cutoff_(0),
  dsumtol_(0),
  ewcoeff_(0),
  lwcoeff_(0),
  ljswidth_(0),
  skinnb_(0),
  erfcDx_(0),
  npoints_(0),
  nfft1_(0),
  nfft2_(0),
  nfft3_(0)
{}

/** Set whether we want to read LJ PME options. */
void PmeOptions::AllowLjPme(bool allow) {
  allowLjPme_ = allow;
}


/** Parse PME options from ArgList. */
int PmeOptions::GetOptions(ArgList& actionArgs, const char* desc) {
  cutoff_ = actionArgs.getKeyDouble("cut", 8.0);
  dsumtol_ = actionArgs.getKeyDouble("dsumtol", 1E-5);
  ewcoeff_ = actionArgs.getKeyDouble("ewcoeff", 0.0);
  lwcoeff_ = -1.0;
  lwcoeff_ = 0;
  if (actionArgs.hasKey("ljpme")) {
    if (!allowLjPme_) {
      mprinterr("Error: LJ PME not allowed for '%s'\n", desc);
      return 1;
    }
    lwcoeff_ = 0.4;
    lwcoeff_ = actionArgs.getKeyDouble("ewcoefflj", lwcoeff_);
  }
  ljswidth_ = actionArgs.getKeyDouble("ljswidth", 0.0);
  skinnb_ = actionArgs.getKeyDouble("skinnb", 2.0);
  erfcDx_ = actionArgs.getKeyDouble("erfcdx", 0.0);
  npoints_ = actionArgs.getKeyInt("order", 6);
  std::string marg = actionArgs.GetStringKey("nfft");
  if (!marg.empty()) {
    ArgList mlim(marg, ",");
    if (mlim.Nargs() != 3) {
      mprinterr("Error: Need 3 integers in comma-separated list for 'nfft'\n");
      return 1;
    }
    nfft1_ = mlim.getNextInteger(0);
    nfft2_ = mlim.getNextInteger(0);
    nfft3_ = mlim.getNextInteger(0);
  } else {
    nfft1_ = -1;
    nfft2_ = -1;
    nfft3_ = -1;
  }
  return 0;
}
