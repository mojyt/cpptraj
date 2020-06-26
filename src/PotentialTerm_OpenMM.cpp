#include "PotentialTerm_OpenMM.h"
#include "CpptrajStdio.h"
#ifdef HAS_OPENMM
# include "OpenMM.h"
# include "Box.h"
# include "Topology.h"
# include "CharMask.h"
# include "Constants.h"
#endif

PotentialTerm_OpenMM::PotentialTerm_OpenMM() :
  PotentialTerm(OPENMM)
#ifdef HAS_OPENMM
  ,system_(0)
  ,context_(0)
#endif
{}

PotentialTerm_OpenMM::~PotentialTerm_OpenMM() {
# ifdef HAS_OPENMM
  if (system_ != 0) delete system_;
  if (context_ != 0) delete context_;
# endif
}

#ifdef HAS_OPENMM
void PotentialTerm_OpenMM::AddBonds(OpenMM::HarmonicBondForce* bondStretch,
                                    std::vector< std::pair<int,int> >& bondPairs,
                                    BondArray const& bonds, BondParmArray const& BP,
                                    std::vector<int> const& oldToNew)
{
  for (BondArray::const_iterator bnd = bonds.begin(); bnd != bonds.end(); ++bnd)
  {
    int a1 = oldToNew[bnd->A1()];
    int a2 = oldToNew[bnd->A2()];
    if (a1 != -1 && a2 != -1)
    {
      bondPairs.push_back(std::make_pair(a1, a2));
      bondStretch->addBond( a1, a2,
                            BP[bnd->Idx()].Req() * OpenMM::NmPerAngstrom,
                            BP[bnd->Idx()].Rk() * 2 * OpenMM::KJPerKcal
                              * OpenMM::AngstromsPerNm * OpenMM::AngstromsPerNm );
    }
  }
}


/** This performs the actual openMM setup. */
int PotentialTerm_OpenMM::OpenMM_setup(Topology const& topIn, Box const& boxIn,
                                       CharMask const& maskIn, EnergyArray& earrayIn)
{
  mprintf("OpenMM setup.\n");
  system_ = new OpenMM::System();
  OpenMM::NonbondedForce* nonbond = new OpenMM::NonbondedForce();
  system_->addForce( nonbond );
  OpenMM::HarmonicBondForce* bondStretch = new OpenMM::HarmonicBondForce();
  system_->addForce( bondStretch );

  // Do periodic boundary conditions if necessary.
  if (boxIn.Type() != Box::NOBOX) {
    nonbond->setNonbondedMethod(OpenMM::NonbondedForce::CutoffPeriodic);
    nonbond->setCutoffDistance( 0.8 ); // TODO allow args
    Matrix_3x3 ucell, recip;
    boxIn.ToRecip(ucell, recip);
    system_->setDefaultPeriodicBoxVectors(
      OpenMM::Vec3( ucell[0], ucell[1], ucell[2] ),
      OpenMM::Vec3( ucell[3], ucell[4], ucell[5] ),
      OpenMM::Vec3( ucell[6], ucell[7], ucell[8] ) );
  }

  // Add atoms to the system.
  std::vector<int> oldToNew(topIn.Natom(), -1);
  int newIdx = 0;
  for (int idx = 0; idx != topIn.Natom(); idx++)
  {
    if (maskIn.AtomInCharMask(idx)) {
      oldToNew[idx] = newIdx++;
      system_->addParticle( topIn[idx].Mass() );
      if (topIn.Nonbond().HasNonbond()) {
        nonbond->addParticle(
          topIn[idx].Charge(),
          topIn.GetVDWradius(idx) * OpenMM::NmPerAngstrom * OpenMM::SigmaPerVdwRadius,
          topIn.GetVDWdepth(idx) * OpenMM::KJPerKcal );
      }
    }
  }
  // Add bonds
  // Note factor of 2 for stiffness below because Amber specifies the constant
  // as it is used in the harmonic energy term kx^2 with force 2kx; OpenMM wants 
  // it as used in the force term kx, with energy kx^2/2.
  std::vector< std::pair<int,int> >   bondPairs;
  AddBonds(bondStretch, bondPairs, topIn.Bonds(), topIn.BondParm(),  oldToNew);
  AddBonds(bondStretch, bondPairs, topIn.BondsH(), topIn.BondParm(), oldToNew);

  // Populate nonbonded exclusions TODO make args
  const double Coulomb14Scale      = 1.0;
  const double LennardJones14Scale = 1.0;
  nonbond->createExceptionsFromBonds(bondPairs, Coulomb14Scale, LennardJones14Scale);

  // Set up integrator and context.
  OpenMM::Integrator* integrator = new OpenMM::VerletIntegrator( 0.001 ); // TODO allow ars
  context_ = new OpenMM::Context( *system_, *integrator );

  std::string platformName = context_->getPlatform().getName();
  mprintf("OpenMM Platform: %s\n", platformName.c_str());

  return 0;
}
#endif
 
/** Set up openmm terms. This is the wrapper for try/catch. */
int PotentialTerm_OpenMM::SetupTerm(Topology const& topIn, Box const& boxIn,
                                    CharMask const& maskIn, EnergyArray& earrayIn)
{
  int err = 1;
# ifdef HAS_OPENMM
  try {
    err = OpenMM_setup(topIn, boxIn, maskIn, earrayIn);
  }

  catch(const std::exception& e) {
    printf("EXCEPTION: %s\n", e.what());
    err = 1;
  }
# else
  mprinterr("Error: CPPTRAJ was compiled without OpenMM support.\n");
# endif
  return err;
}

void PotentialTerm_OpenMM::CalcForce(Frame& frameIn, CharMask const& maskIn) const
{
# ifdef HAS_OPENMM
  // Set positions in nm
  std::vector<OpenMM::Vec3> posInNm;
  posInNm.reserve(maskIn.Nselected());
  for (int at = 0; at != frameIn.Natom(); at++) {
    if (maskIn.AtomInCharMask(at)) {
      const double* xyz = frameIn.XYZ(at);
      posInNm.push_back( OpenMM::Vec3(xyz[0]*OpenMM::NmPerAngstrom,
                                      xyz[1]*OpenMM::NmPerAngstrom,
                                      xyz[2]*OpenMM::NmPerAngstrom) );
    }
  }
  context_->setPositions(posInNm);
  // Do a single minimization step
  OpenMM::LocalEnergyMinimizer min;
  min.minimize(*context_, 10.0, 1);
  // Get the results
  const OpenMM::State state = context_->getState(OpenMM::State::Forces, true);
  //  timeInPs = state.getTime(); // OpenMM time is in ps already

  // Copy OpenMM positions into output array and change units from nm to Angstroms.
  const std::vector<OpenMM::Vec3>& ommForces = state.getForces();
  double* fptr = frameIn.fAddress();
  for (int i=0; i < (int)ommForces.size(); ++i, fptr += 3)
  {
    //for (int j=0; j<3; j++)
    //  xptr[j] = positionsInNm[i][j] * OpenMM::AngstromsPerNm;
    for (int j=0; j<3; j++)
      fptr[j] = ommForces[i][j] * Constants::GMX_FRC_TO_AMBER;
  }
# endif
  return;
}
