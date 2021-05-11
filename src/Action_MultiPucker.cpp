#include "Action_MultiPucker.h"
#include "CpptrajStdio.h"
#include "TorsionRoutines.h"

using namespace Cpptraj;

const double Action_MultiPucker::PERIOD_ = 360.0;

/** CONSTRUCTOR */
Action_MultiPucker::Action_MultiPucker() :
  outfile_(0),
  masterDSL_(0),
  defaultMethod_(Pucker::ALTONA_SUNDARALINGAM),
  puckerMin_(0),
  puckerMax_(0),
  offset_(0)
{}

// Action_MultiPucker::Help()
void Action_MultiPucker::Help() const {
  mprintf("\t[<name>] [<type> ...] [out <filename>] [altona|cremer]\n"
          "\t[range360] [offset <offset>]\n");
}

// Action_MultiPucker::Init()
Action::RetType Action_MultiPucker::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
  // Get Keywords
  outfile_ = init.DFL().AddDataFile( actionArgs.GetStringKey("out"), actionArgs);
  if      (actionArgs.hasKey("altona")) defaultMethod_ = Pucker::ALTONA_SUNDARALINGAM;
  else if (actionArgs.hasKey("cremer")) defaultMethod_ = Pucker::CREMER_POPLE;
  else                                  defaultMethod_ = Pucker::UNSPECIFIED;
  offset_ = actionArgs.getKeyDouble("offset",0.0);
  if (actionArgs.hasKey("range360"))
    puckerMin_ = 0.0;
  else
    puckerMin_ = -180.0;
  puckerMax_ = puckerMin_ + PERIOD_;
  std::string resrange_arg = actionArgs.GetStringKey("resrange");
  if (!resrange_arg.empty())
    if (resRange_.SetRange( resrange_arg )) return Action::ERR;
  // Search for known pucker keywords
  if (puckerSearch_.SearchForArgs( actionArgs )) return Action::ERR;
  // Get custom pucker args
  if (puckerSearch_.SearchForNewTypeArgs( actionArgs )) return Action::ERR;
  // If no pucker types are yet selected, this will select all.
  puckerSearch_.SearchForAll();

  // Setup DataSet(s) name
  dsetname_ = actionArgs.GetStringNext();

  mprintf("    MULTIPUCKER: Calculating");
  puckerSearch_.PrintTypes();
  if (!resRange_.Empty())
    mprintf(" puckers for residues in range %s\n", resRange_.RangeArg());
  else
    mprintf(" puckers for all solute residues.\n");
  if (defaultMethod_ == Pucker::ALTONA_SUNDARALINGAM) 
    mprintf("\tUsing Altona & Sundaralingam method.\n");
  else if (defaultMethod_ == Pucker::CREMER_POPLE)
    mprintf("\tUsing Cremer & Pople method.\n");
  if (offset_!=0)
    mprintf("\tOffset: %f degrees will be added to values.\n", offset_);
  if (puckerMin_ > -180.0)
    mprintf("\tOutput range is 0 to 360 degrees.\n");
  else
    mprintf("\tOutput range is -180 to 180 degrees.\n");
  if (!dsetname_.empty())
    mprintf("\tDataSet name: %s\n", dsetname_.c_str());
  if (outfile_ != 0) mprintf("\tOutput to %s\n", outfile_->DataFilename().base());

  init.DSL().SetDataSetsPending(true);
  masterDSL_ = init.DslPtr();
  return Action::OK;
}

// Action_MultiPucker::Setup()
Action::RetType Action_MultiPucker::Setup(ActionSetup& setup)
{
  Range actualRange;
  // If range is empty (i.e. no resrange arg given) look through all 
  // solute residues.
  if (resRange_.Empty())
    actualRange = setup.Top().SoluteResidues();
  else {
    // If user range specified, create new range shifted by -1 since internal
    // resnums start from 0.
    actualRange = resRange_;
    actualRange.ShiftBy(-1);
  }
  // Exit if no residues specified
  if (actualRange.Empty()) {
    mprinterr("Error: No residues specified for %s\n",setup.Top().c_str());
    return Action::ERR;
  }
  // Search for specified puckers in each residue in the range
  if (puckerSearch_.FindPuckers(setup.Top(), actualRange))
    return Action::SKIP;
  mprintf("\tResults of search in residue range [%s] for types", resRange_.RangeArg());
  puckerSearch_.PrintTypes();
  mprintf(", %u puckers found.\n", puckerSearch_.Npuckers());

  // Print selected puckers, set up DataSets
  data_.clear();
  puckerMethods_.clear();

  if (dsetname_.empty())
    dsetname_ = masterDSL_->GenerateDefaultName("MPUCKER");
  for (Pucker::PuckerSearch::mask_it pucker = puckerSearch_.begin();
                                     pucker != puckerSearch_.end(); ++pucker)
  {
    // setup/check the method
    Pucker::Method methodToUse = defaultMethod_;
    if (methodToUse == Pucker::UNSPECIFIED) {
      if (pucker->Natoms() > 5)
        methodToUse = Pucker::CREMER_POPLE;
      else
        methodToUse = Pucker::ALTONA_SUNDARALINGAM;
    } else if (methodToUse == Pucker::ALTONA_SUNDARALINGAM) {
      if (pucker->Natoms() > 5) {
        mprinterr("Error: Pucker '%s' has too many atoms for Altona-Sundaralingam method.\n");
        return Action::ERR;
      }
    }
    puckerMethods_.push_back( methodToUse );

    int resNum = pucker->ResNum() + 1;
    // See if Dataset already present. FIXME should AddSet do this?
    MetaData md( dsetname_, pucker->Name(), resNum );
    DataSet* ds = masterDSL_->CheckForSet(md);
    if (ds == 0) {
      // Create new DataSet
      md.SetScalarMode( MetaData::M_PUCKER );
      md.SetScalarType( MetaData::PUCKER   ); // TODO pucker types
      ds = masterDSL_->AddSet( DataSet::DOUBLE, md );
      if (ds == 0) return Action::ERR;
      // Add to outfile
      if (outfile_ != 0)
        outfile_->AddDataSet( ds );
    }
    data_.push_back( ds ); 
    //if (debug_ > 0) {
      static const char* methodStr[] = { "Altona", "Cremer", "Unspecified" };
      mprintf("\tPUCKER [%s]: %s (%s)\n", ds->legend(), pucker->PuckerMaskString(setup.Top()).c_str(),
              methodStr[puckerMethods_.back()]);
    //}
  }
  return Action::OK;
}

// Action_MultiPucker::DoAction()
Action::RetType Action_MultiPucker::DoAction(int frameNum, ActionFrame& frm)
{
  const double* XYZ[6];
  for (int i = 0; i != 6; i++)
    XYZ[i] = 0;
  double pval, aval, tval;

  std::vector<DataSet*>::const_iterator ds = data_.begin();
  std::vector<Pucker::Method>::const_iterator method = puckerMethods_.begin();
  for (Pucker::PuckerSearch::mask_it pucker = puckerSearch_.begin();
                                     pucker != puckerSearch_.end(); ++pucker, ++ds)
  {
    // Since puckers are always at least 5 atoms, just reinit the 6th coord
    XYZ[5] = 0;
    // Get pucker coordinates
    unsigned int idx = 0;
    for (Pucker::PuckerMask::atom_it atm = pucker->begin(); atm != pucker->end(); ++atm, ++idx)
      XYZ[idx] = frm.Frm().XYZ( *atm );
    // Do pucker calculation
    switch (*method) {
      case Pucker::ALTONA_SUNDARALINGAM:
        pval = Pucker_AS( XYZ[0], XYZ[1], XYZ[2], XYZ[3], XYZ[4], aval );
        break;
      case Pucker::CREMER_POPLE:
        pval = Pucker_CP( XYZ[0], XYZ[1], XYZ[2], XYZ[3], XYZ[4], XYZ[5],
                          pucker->Natoms(), aval, tval );
        break;
      case Pucker::UNSPECIFIED : // Sanity check
        return Action::ERR;
    }
    
    pval = (pval * Constants::RADDEG) + offset_;
    if (pval > puckerMax_)
      pval -= PERIOD_;
    else if (pval < puckerMin_)
      pval += PERIOD_;
    (*ds)->Add(frameNum, &pval);
  }
  return Action::OK;
}
