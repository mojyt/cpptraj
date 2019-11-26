#include "Analysis_EvalEquilibration.h"
#include "CpptrajStdio.h"
#include "DataSet_1D.h"
#include "DataSet_Mesh.h"
#include "CurveFit.h"

Analysis_EvalEquilibration::Analysis_EvalEquilibration() :
  Analysis(HIDDEN),
  statsout_(0),
  tolerance_(0),
  valaCut_(0),
  chisqCut_(0),
  slopeCut_(0),
  maxIt_(0),
  debug_(0)
{}

const char* Analysis_EvalEquilibration::OdataStr_[NDATA] = {
  "A0",
  "A1",
  "A2",
  "F",
  "corr",
  "vala",
  "chisq",
  "eqtime",
  "name",
  "result"
};

DataSet::DataType Analysis_EvalEquilibration::OdataType_[NDATA] = {
  DataSet::DOUBLE,
  DataSet::DOUBLE,
  DataSet::DOUBLE,
  DataSet::DOUBLE,
  DataSet::DOUBLE,
  DataSet::DOUBLE,
  DataSet::DOUBLE,
  DataSet::DOUBLE,
  DataSet::STRING,
  DataSet::STRING
};

// Analysis_EvalEquilibration::Help()
void Analysis_EvalEquilibration::Help() const {
  mprintf("\n");
}

// Analysis_EvalEquilibration::Setup()
Analysis::RetType Analysis_EvalEquilibration::Setup(ArgList& analyzeArgs, AnalysisSetup& setup, int debugIn)
{
  debug_ = debugIn;

  dsname_ = analyzeArgs.GetStringKey("name");
  if (dsname_.empty())
    dsname_ = setup.DSL().GenerateDefaultName("EvalEquil");

  tolerance_ = analyzeArgs.getKeyDouble("tol", 0.00001);
  if (tolerance_ < 0.0) {
    mprinterr("Error: Tolerance must be greater than or equal to 0.0\n");
    return Analysis::ERR;
  }
  valaCut_ = analyzeArgs.getKeyDouble("valacut", 0.01);
  if (valaCut_ <= 0) {
    mprinterr("Error: valacut must be > 0\n");
    return Analysis::ERR;
  }
  chisqCut_ = analyzeArgs.getKeyDouble("chisqcut", 0.05);
  if (chisqCut_ <= 0) {
    mprinterr("Error: chisqcut must be > 0\n");
    return Analysis::ERR;
  }
  slopeCut_ = analyzeArgs.getKeyDouble("slopecut", 0.000001);
  if (slopeCut_ <= 0) {
    mprinterr("Error: slopecut must be > 0\n");
    return Analysis::ERR;
  }

  maxIt_ = analyzeArgs.getKeyInt("maxit", 500);
  if (maxIt_ < 1) {
    mprinterr("Error: Max iterations must be greater than or equal to 1.\n");
    return Analysis::ERR;
  }

  // Curves out
  DataFile* outfile = setup.DFL().AddDataFile( analyzeArgs.GetStringKey("out"), analyzeArgs );
  // Results out
  DataFile* resultsOut = setup.DFL().AddDataFile( analyzeArgs.GetStringKey("resultsout"), analyzeArgs );

  // Allocate output stats file. Allow STDOUT.
  statsout_ = setup.DFL().AddCpptrajFile( analyzeArgs.GetStringKey("statsout"),
                                          "EvalEquil stats",
                                          DataFileList::TEXT, true );
  if (statsout_ == 0) return Analysis::ERR;

  // get input data sets
  if (inputSets_.AddSetsFromArgs( analyzeArgs.RemainingArgs(), setup.DSL() ))
    return Analysis::ERR;

  // Create output sets.
  int idx = 0;
  for (Array1D::const_iterator it = inputSets_.begin(); it != inputSets_.end(); ++it, ++idx)
  {
    DataSet* setOut = setup.DSL().AddSet( DataSet::XYMESH, MetaData( dsname_, idx ) );
    if (setOut == 0) return Analysis::ERR;
    outputSets_.push_back( setOut );
    if (outfile != 0) {
      outfile->AddDataSet( *it );
      outfile->AddDataSet( setOut );
    }
  }
  // Results data sets
  data_.reserve( inputSets_.size() );
  DataSet::SizeArray nData(1, inputSets_.size());
  for (int idx = 0; idx != (int)NDATA; idx++)
  {
    DataSet* ds = setup.DSL().AddSet( OdataType_[idx], MetaData( dsname_, OdataStr_[idx] ) );
    if (ds == 0) return Analysis::ERR;
    ds->Allocate( nData );
    if (resultsOut != 0)
      resultsOut->AddDataSet( ds );
    data_.push_back( ds );
  }

  mprintf("    EVALEQUILIBRATION: Evaluate equilibration of %zu sets.\n", inputSets_.size());
  mprintf("\tOutput set name: %s\n", dsname_.c_str());
  mprintf("\tTolerance for curve fit: %g\n", tolerance_);
  mprintf("\tMax iterations for curve fit: %i\n", maxIt_);
  if (outfile != 0)
    mprintf("\tFit curve output to '%s'\n", outfile->DataFilename().full());
  mprintf("\tStatistics output to '%s'\n", statsout_->Filename().full());
  if (resultsOut != 0)
    mprintf("\tResults output to '%s'\n", resultsOut->DataFilename().full());
  mprintf("\tCutoff for last half average vs estimated long term value: %g\n", valaCut_);
  mprintf("\tCutoff for non-linear fit chi^2: %g\n", chisqCut_);
  mprintf("\tCutoff for slope: %g\n", slopeCut_);

  return Analysis::OK;
}

/** Exponential (high relax to low)
  * A2 + (A0*(exp(A1*X)))
  */
int EQ_relax(CurveFit::Darray const& Xvals, CurveFit::Darray const& Params,
             CurveFit::Darray& Yvals)
{
  double A0 = Params[0];
  double A1 = Params[1];
  double A2 = Params[2];
  for (unsigned int n = 0; n != Xvals.size(); ++n)
    Yvals[n] = A2 + ( A0 * exp( -A1 * Xvals[n] ) );
  return 0;
}

/** Inverse exponential (low relax to high).
  * A2 - (A0*(exp(A1*X)))
  */
int EQ_invRelax(CurveFit::Darray const& Xvals, CurveFit::Darray const& Params,
                CurveFit::Darray& Yvals)
{
  double A0 = Params[0];
  double A1 = Params[1];
  double A2 = Params[2];
  for (unsigned int n = 0; n != Xvals.size(); ++n)
    Yvals[n] = A2 - ( A0 * exp( -A1 * Xvals[n] ) );
  return 0;
}


// Analysis_EvalEquilibration::Analyze()
Analysis::RetType Analysis_EvalEquilibration::Analyze() {
  std::vector<DataSet*>::const_iterator ot = outputSets_.begin();
  for (Array1D::const_iterator it = inputSets_.begin(); it != inputSets_.end(); ++it, ++ot)
  {
    mprintf("\tEvaluating: %s\n", (*it)->legend());
    if (!statsout_->IsStream())
      statsout_->Printf("# %s\n", (*it)->legend());
    DataSet_1D const& DS = static_cast<DataSet_1D const&>( *(*it) );
    // First do a linear fit.
    statsout_->Printf("\t----- Linear Fit -----\n");
    if (DS.Size() < 2) {
      mprintf("Warning: Not enough data in '%s' to evaluate.\n", DS.legend());
      continue;
    }
    double slope, intercept, correl, Fval;
    int err = DS.LinearRegression( slope, intercept, correl, Fval, statsout_ );
    if (err != 0) {
      mprinterr("Error: Could not perform linear regression fit.\n");
      return Analysis::ERR;
    }

    // Process expression to fit
    //if (calc.ProcessExpression( dsname_ + "=A0*(exp(A1*(1/X)))" )) {
    //  mprinterr("Error: Unable to process equation expression.\n");
    //  return Analysis::ERR;
    //}

    statsout_->Printf("\t----- Nonlinear Fit -----\n");
    // Determine general relaxation direction
    CurveFit::FitFunctionType fxn = 0;
    //int relaxationDir = 0;
    if (slope < 0) {
      mprintf("\tUsing relaxation form: A2 + (A0*exp(-A1*x))\n");
      //relaxationDir = -1;
      fxn = EQ_relax;
    } else if (slope > 0) {
      mprintf("\tUsing inverse relaxation form: A2 - (A0*exp(-A1*x))\n");
      //relaxationDir = 1;
      fxn = EQ_invRelax;
    } else {
      // Special case: if slope was exactly 0 (should be rare). Consider this
      // equilibrated.
      mprintf("\tSlope of linear fit is 0.\n");
      continue;
    }

    // Set up initial X and Y values.
    CurveFit::Darray Xvals, Yvals;
    Xvals.reserve( DS.Size() );
    Yvals.reserve( DS.Size() );
    for (unsigned int i = 0; i != DS.Size(); i++) {
      double xval = DS.Xcrd(i);
      if (xval < 0) {
        mprintf("Warning: Ignoring X value < 0: %g\n", xval);
      } else {
        Xvals.push_back( xval );
        Yvals.push_back( DS.Dval(i) );
      }
    }

    // Determine the average value of the last half of the data.
    unsigned int halfwayPt = (Yvals.size() / 2);
    double Yavg1half = 0;
    for (unsigned int hidx = 0; hidx < halfwayPt; hidx++)
      Yavg1half += Yvals[hidx];
    Yavg1half /= (double)halfwayPt;
    statsout_->Printf("\tFirst half <Y> = %g\n", Yavg1half);
    double Yavg2half = 0;
    for (unsigned int hidx = halfwayPt; hidx < Yvals.size(); hidx++)
      Yavg2half += Yvals[hidx];
    Yavg2half /= (double)(Yvals.size() - halfwayPt);
    statsout_->Printf("\tLast half <Y> = %g\n", Yavg2half);

    // Set initial guesses for parameters.
    CurveFit::Darray Params(3);
    Params[0] = Yavg2half - DS.Dval(0);
    if (Params[0] < 0) Params[0] = -Params[0];
    Params[1] = 0.1; // TODO absolute slope?
    Params[2] = Yavg2half;

    for (CurveFit::Darray::const_iterator ip = Params.begin(); ip != Params.end(); ++ip) {
      statsout_->Printf("\tInitial Param A%li = %g\n", ip - Params.begin(), *ip);
    }

    // Perform curve fitting
    CurveFit fit;
    int info = fit.LevenbergMarquardt( fxn, Xvals, Yvals, Params, tolerance_, maxIt_ );
    mprintf("\t%s\n", fit.Message(info));
    if (info == 0) {
      mprinterr("Error: %s\n", fit.ErrorMessage());
      return Analysis::ERR;
    }
    for (CurveFit::Darray::const_iterator ip = Params.begin(); ip != Params.end(); ++ip) {
      statsout_->Printf("\tFinal Param A%li = %g\n", ip - Params.begin(), *ip);
    }

    // Params[0] = A0 = Gap between final and initial values
    // Params[1] = A1 = Relaxation constant
    // Params[2] = A2 = Final value at long time
    // Determine the absolute difference of the long-time estimated value
    // from the average value of the last half of the data.
    double ValA = Yavg2half - Params[2];
    if (ValA < 0) ValA = -ValA;
    statsout_->Printf("\tValA = %g\n", ValA);

    // Create output curve
    DataSet_Mesh& OUT = static_cast<DataSet_Mesh&>( *(*ot) );
    for (unsigned int i = 0; i != Xvals.size(); i++)
      OUT.AddXY( Xvals[i], fit.FinalY()[i] );

    // Calculate where slope reaches slopeCut_
    DataSet_1D::Darray slopeX, slopeY;
    OUT.FiniteDifference(DataSet_1D::FORWARD, slopeX, slopeY);
    double finalx=-1, finaly=0;
    for (unsigned int sidx = 0; sidx != slopeY.size(); ++sidx) {
      //mprintf("DEBUG: slope %g %g\n", slopeX[sidx], slopeY[sidx]);
      double absSlope = slopeY[sidx];
      if (absSlope < 0) absSlope = -absSlope;
      if (absSlope < slopeCut_) {
        finalx = slopeX[sidx];
        finaly = slopeY[sidx];
        break;
      }
    }
    statsout_->Printf("\tFinal slope: %g\n", slopeY.back());
    statsout_->Printf("\tSlope cutoff satisfied at %g %g\n", finalx, finaly);

    // Statistics
    double corr_coeff, ChiSq, TheilU, rms_percent_error;
    err = fit.Statistics( Yvals, corr_coeff, ChiSq, TheilU, rms_percent_error);
    if (err != 0) mprintf("Warning: %s\n", fit.Message(err));
    statsout_->Printf("\tCorrelation coefficient: %g\n"
                      "\tChi squared: %g\n"
                      "\tUncertainty coefficient: %g\n"
                      "\tRMS percent error: %g\n",
                      corr_coeff, ChiSq, TheilU, rms_percent_error);

    long int oidx = (it - inputSets_.begin());
    data_[CHISQ]->Add(oidx, &ChiSq);
    data_[A0]->Add(oidx, &Params[0]);
    data_[A1]->Add(oidx, &Params[0] + 1);
    data_[A2]->Add(oidx, &Params[0] + 2);
    data_[FVAL]->Add(oidx, &Fval);
    data_[CORR]->Add(oidx, &corr_coeff);
    data_[VALA]->Add(oidx, &ValA);
    data_[EQTIME]->Add(oidx, &finalx);
    data_[NAME]->Add(oidx, (*it)->legend());
    if ( ValA < valaCut_ && ChiSq < chisqCut_ )
      data_[RESULT]->Add(oidx, "yes");
    else
      data_[RESULT]->Add(oidx, "no");

    statsout_->Printf("\n");
  }
  return Analysis::OK;
}
