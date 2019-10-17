#include "Traj_GmxDump.h"
#include "CpptrajStdio.h"
#include "Constants.h" // ANG_TO_NM
#include "Topology.h"
#include "ArgList.h"

/// CONSTRUCTOR
Traj_GmxDump::Traj_GmxDump() :
  natoms_(0),
  outfmt_(0),
  longFormat_(false)
{}

/** Identify trajectory format. File should be setup for READ */
bool Traj_GmxDump::ID_TrajFormat(CpptrajFile& fileIn) {

  return false;
}

/** Print trajectory info to stdout. */
void Traj_GmxDump::Info() {
  mprintf("is a Gromacs text dump file");
}

/** Close file. */
void Traj_GmxDump::closeTraj() {

}

// -----------------------------------------------------------------------------
/** Open trajectory for reading. */
int Traj_GmxDump::openTrajin() {

  return 1;
}

/** Read help */
void Traj_GmxDump::ReadHelp() {

}

/** Process read arguments. */
int Traj_GmxDump::processReadArgs(ArgList& argIn) {

  return 0;
}

/** Set up trajectory for reading.
  * \return Number of frames in trajectory.
  */
int Traj_GmxDump::setupTrajin(FileName const& fname, Topology* trajParm)
{

  return TRAJIN_ERR;
}

/** Read specified trajectory frame. */
int Traj_GmxDump::readFrame(int set, Frame& frameIn) {

  return 1;
}

/** Read velocities from specified frame. */
int Traj_GmxDump::readVelocity(int set, Frame& frameIn) {

  return 1;
}

/** Read forces from specified frame. */
int Traj_GmxDump::readForce(int set, Frame& frameIn) {

  return 1;
}

// -----------------------------------------------------------------------------
/** Write help. */
void Traj_GmxDump::WriteHelp() {
  mprintf("\tlongformat : If specified, use output format with increased width/precision.\n");
}

/** Process write arguments. */
int Traj_GmxDump::processWriteArgs(ArgList& argIn, DataSetList const& DSLin) {
  longFormat_ = argIn.hasKey("longformat");
  return 0;
}

/** Set up trajectory for write. */
int Traj_GmxDump::setupTrajout(FileName const& fname, Topology* trajParm,
                                   CoordinateInfo const& cInfoIn, 
                                   int NframesToWrite, bool append)
{
  SetCoordInfo( cInfoIn );

  // Set up title. Default to filename.
  std::string title = Title();
  if (title.empty()) {
    title.assign( fname.Full() );
    SetTitle( title );
  }

  if (append) {
    // Appending.
    if (file_.SetupAppend( fname, debug_ )) return 1;
    if (file_.OpenFile()) return 1;
  } else {
    // Not appending.
    if (file_.SetupWrite( fname, debug_ )) return 1;
    if (file_.OpenFile()) return 1;
  }
  natoms_ = trajParm->Natom();
  if (longFormat_)
    outfmt_ = "%15.8e";
  else
    outfmt_ = "%12.5e";
  return 0;
}

void Traj_GmxDump::writeVectorArray(const double* array, const char* title, int Nlines, int Ncols)
{
  // Print title, indent 4.
  file_.Printf("    %s (%dx%d):\n", title, Nlines, Ncols);
  // Print each line, indent of 8
  int idx = 0;
  for (int line = 0; line != Nlines; line++)
  {
    file_.Printf("        %s[%5d]={", title, line);
    for (int col = 0; col != Ncols; col++)
    {
      if (col != 0) file_.Printf(", ");
      file_.Printf(outfmt_, array[idx++]);
    }
    file_.Printf("}\n");
  } // END loop over lines
}

/** Write specified trajectory frame. */
int Traj_GmxDump::writeFrame(int set, Frame const& frameOut) {
  // Write file name and frame (starts from 0). No indent.
  file_.Printf("%s frame %d:\n", Title().c_str(), set);
  // Write number of atoms, step, time, lambda. Indent of 4.
  file_.Printf("    natoms=%10d  step=%10i  time=%12.7e  lambda=%10g\n",
               natoms_, 0, frameOut.Time(), 0);
  if (CoordInfo().HasBox()) {
    Matrix_3x3 Ucell = frameOut.BoxCrd().UnitCell( Constants::ANG_TO_NM );
    writeVectorArray( Ucell.Dptr(), "box", 3, 3 );
  }

    
  return 0;
}

// =============================================================================
#ifdef MPI
/** Open trajectory for reading in parallel. */
int Traj_GmxDump::parallelOpenTrajin(Parallel::Comm const& commIn) {
  return 1;
}

/** Open trajectory for writing in parallel. */
int Traj_GmxDump::parallelOpenTrajout(Parallel::Comm const& commIn) {
  return 1;
}

/** Set up trajectory for write in parallel. */
int Traj_GmxDump::parallelSetupTrajout(FileName const& fname, Topology* trajParm,
                                           CoordinateInfo const& cInfoIn,
                                           int NframesToWrite, bool append,
                                           Parallel::Comm const& commIn)
{

  return 1;
}

/** Read frame in parallel. */
int Traj_GmxDump::parallelReadFrame(int set, Frame& frameIn) {

  return 1;
}

/** Write frame in parallel. */
int Traj_GmxDump::parallelWriteFrame(int set, Frame const& frameOut) {

  return 1;
}

/** Close trajectory in parallel. */
void Traj_GmxDump::parallelCloseTraj() {

}
#endif
