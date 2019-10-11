#ifndef NO_TNGFILE
#include <cmath>   // pow
#include <cstring> // strncmp
#include <cstdlib> // free
#include "Traj_TNG.h"
#include "CpptrajStdio.h"
#include "FileName.h"
#include "Topology.h"
#include "CpptrajFile.h"

/// CONSTRUCTOR
Traj_TNG::Traj_TNG() :
  ftmp_(0),
  tngatoms_(0),
  tngframes_(-1),
  tngsets_(-1),
  current_frame_(-1),
  tngfac_(0),
  isOpen_(false)
{}

/// DESTRUCTOR
Traj_TNG::~Traj_TNG() {
  closeTraj();
  if (ftmp_ != 0) delete[] ftmp_;
}

/** Identify trajectory format. File should be setup for READ.
  * Note that as of version 1.8.2 the TNG library routines provide
  * no straightforward way to confirm that a file is indeed a TNG
  * file; providing a non-TNG file to the routines results in lots
  * of memory errors. Therefore, relying on the fact that the
  * 'GENERAL INFO' block should be present in all TNG files, and that
  * that string seems to always be in bytes 40-51.
  */
bool Traj_TNG::ID_TrajFormat(CpptrajFile& fileIn) {
  char tngheader[52];
  if (fileIn.OpenFile()) return false;
  if (fileIn.Read(&tngheader, 52) != 52) return false;
  fileIn.CloseFile();
  if (strncmp(tngheader+40, "GENERAL INFO", 12)==0) {
    mprintf("DEBUG: TNG file.\n");
    return true;
  }

  return false;
}

/** Print trajectory info to stdout. */
void Traj_TNG::Info() {
  mprintf("is a GROMACS TNG file");
}

/** Close file. */
void Traj_TNG::closeTraj() {
  mprintf("DEBUG: Calling closeTrajin() isOpen_=%1i\n", (int)isOpen_);
  if (isOpen_) {
    tng_util_trajectory_close(&traj_);
  }
  isOpen_ = false;
}

// -----------------------------------------------------------------------------
/** Open trajectory for reading. */
int Traj_TNG::openTrajin() {
  mprintf("DEBUG: Calling openTrajin() isOpen_=%1i\n", (int)isOpen_);
  if (isOpen_) 
    closeTraj();
  tng_function_status stat = tng_util_trajectory_open(filename_.full(), 'r', &traj_);
  if (stat != TNG_SUCCESS) {
    mprinterr("Error: Could not open TNG file '%s'\n", filename_.full());
    return TRAJIN_ERR;
  }
  mprintf("DEBUG: Successfully opened TNG file.\n");
  isOpen_ = true;
  current_frame_ = -1;

  return 0;
}

/** Read help */
void Traj_TNG::ReadHelp() {

}

/** Process read arguments. */
int Traj_TNG::processReadArgs(ArgList& argIn) {

  return 0;
}

/* Utility function for properly scaling coordinates according to the factor. */
void Traj_TNG::convertArray(double* out, float* in, unsigned int nvals) const {
  for (unsigned int i = 0; i != nvals; i++)
    out[i] = ((double)in[i]) * tngfac_;
}

/** Set up trajectory for reading.
  * \return Number of frames in trajectory.
  */
int Traj_TNG::setupTrajin(FileName const& fname, Topology* trajParm)
{
  filename_ = fname;
  // Open the trajectory
  if (openTrajin()) return TRAJIN_ERR;

  // Get number of atoms
  if (tng_num_particles_get(traj_, &tngatoms_) != TNG_SUCCESS) {
    mprinterr("Error: Could not get number of particles from TNG file.\n");
    return 1;
  }
  // Check number of atoms
  if (tngatoms_ != (int64_t)trajParm->Natom()) {
    mprinterr("Error: Number of atoms in TNG file (%li) does not match number\n"
              "Error:  of atoms in associated topology (%i)\n",
               tngatoms_, trajParm->Natom());
    return TRAJIN_ERR;
  }

  // Get number of frames
  tngframes_ = -1;
  if (tng_num_frames_get(traj_, &tngframes_) != TNG_SUCCESS) {
    mprinterr("Error: Could not get number of frames from TNG file.\n");
    return 1;
  }
  // Print number of frames
  mprintf("\tTNG file has %li frames.\n", tngframes_);

  // Get number of frame sets
  tngsets_ = -1;
  if (tng_num_frame_sets_get(traj_, &tngsets_) != TNG_SUCCESS) {
    mprinterr("Error: could not get number of frame sets from TNG file.\n");
    return 1;
  }
  mprintf("\tTNG file has %li frame sets.\n", tngsets_);
  int nframes = (int)tngsets_; // Could overflow here

  // Get the exponential distance scaling factor
  int64_t tngexp;
  if (tng_distance_unit_exponential_get(traj_, &tngexp) != TNG_SUCCESS) {
    mprinterr("Error: Could not get distance scaling exponential from TNG.\n");
    return 1;
  }
  mprintf("\tTNG exponential: %li\n", tngexp);
  switch (tngexp) {
    case -9  :
      mprintf("\tTNG has units of nm\n");
      // Input is in nm. Convert to Angstroms.
      tngfac_ = 10.0;
      break;
    case -10 :
      mprintf("\tTNG has units of Angstrom\n");
      // Input is in Angstroms.
      tngfac_ = 1.0;
      break;
    default :
      // Convert to Angstroms.
      tngfac_ = pow(10.0, tngexp + 10); break;
  }
  // Print the scaling factor
  mprintf("\tTNG distance scaling factor: %g\n", tngfac_);

  // Allocate coords temp memory
  if (ftmp_ != 0) delete[] ftmp_;
  ftmp_ = new float[ 3*tngatoms_ ];

  // Check if there are velocities
  int64_t stride = 0;
  bool hasVel = false;
  tng_function_status stat = tng_util_vel_read_range(traj_, 0, 0, &ftmp_, &stride);
  mprintf("DEBUG: Velocity stride: %li\n", stride);
  if (stat == TNG_CRITICAL) {
    mprinterr("Error: Major error encountered checking TNG velocities.\n");
    return TRAJIN_ERR;
  } else if (stat == TNG_SUCCESS) {
    hasVel = true;
  }

  // Get box status
  Matrix_3x3 boxShape(0.0);
  float* boxptr = 0;
  stat = tng_util_box_shape_read_range(traj_, 0, 0, &boxptr, &stride);
  mprintf("DEBUG: Box Stride: %li\n", stride);
  if (stat == TNG_CRITICAL) {
    mprinterr("Error: Major error encountered checking TNG box.\n");
    return TRAJIN_ERR;
  } else if (stat == TNG_SUCCESS) {
    convertArray(boxShape.Dptr(), boxptr, 9);
    mprintf("\tBox shape:");
    for (unsigned int i = 0; i < 9; i++)
    {
      mprintf(" %f", boxShape[i]);
    }
    mprintf("\n");
  }
  // NOTE: Use free here since thats what underlying TNG library does
  if (boxptr != 0) free( boxptr );

  // Set up coordinate info
  SetCoordInfo( CoordinateInfo( Box(boxShape), hasVel, false, false ) );

  // Set up blocks
  blockIds_.clear();
/*  if (CoordInfo().HasBox())
    blockIds_.push_back( TNG_TRAJ_BOX_SHAPE );
  blockIds_.push_back( TNG_TRAJ_POSITIONS );
  if (CoordInfo().HasVel())
    blockIds_.push_back( TNG_TRAJ_VELOCITIES );*/
  blockIds_.push_back( TNG_TRAJ_BOX_SHAPE );
  blockIds_.push_back( TNG_TRAJ_POSITIONS );
  blockIds_.push_back( TNG_TRAJ_VELOCITIES );
  blockIds_.push_back( TNG_TRAJ_FORCES );
  blockIds_.push_back( TNG_GMX_LAMBDA );

  closeTraj();
  return (int)nframes;
}

static inline const char* DtypeStr(char typeIn) {
  switch (typeIn) {
    case TNG_INT_DATA : return "integer";
    case TNG_FLOAT_DATA : return "float";
    case TNG_DOUBLE_DATA : return "double";
    default : return "unknown";
  }
  return 0;
}

/** Read specified trajectory frame. */
int Traj_TNG::readFrame(int set, Frame& frameIn) {
  int64_t numberOfAtoms = -1;
  tng_num_particles_get(traj_, &numberOfAtoms);
  // Determine next frame with data
  int64_t next_frame;                   // Will get set to the next frame (MD) with data
  int64_t n_data_blocks_in_next_frame;  // Will get set to # blocks in next frame
  int64_t *block_ids_in_next_frame = 0; // Will hold blocks in next frame
  tng_function_status stat = tng_util_trajectory_next_frame_present_data_blocks_find(
    traj_,
    current_frame_,
    blockIds_.size(),
    &blockIds_[0],
    &next_frame,
    &n_data_blocks_in_next_frame,
    &block_ids_in_next_frame);
  if (stat == TNG_CRITICAL) {
    mprinterr("Error: could not get data blocks in next frame (set %i)\n", set+1);
    return 1;
  }
  if (stat == TNG_FAILURE) {
    mprintf("DEBUG: No more blocks.\n");
    return 1;
  }
  mprintf("DEBUG: Set %i next_frame %li nblocksnext %i\n", set+1, next_frame, n_data_blocks_in_next_frame);

  if (n_data_blocks_in_next_frame < 1) {
    mprinterr("Error: No data blocks in next frame (set %i, TNG frame %li)\n", set+1, next_frame);
    return 1;
  }

  // Process data blocks
  void* values = 0;
  double frameTime;
  char datatype;
  for (int64_t idx = 0; idx < n_data_blocks_in_next_frame; idx++)
  {
    int64_t blockId = block_ids_in_next_frame[idx];
    int blockDependency;
    tng_data_block_dependency_get(traj_, blockId, &blockDependency);
    if (blockDependency & TNG_PARTICLE_DEPENDENT) {
      stat = tng_util_particle_data_next_frame_read( traj_,
                                                     blockId,
                                                     &values,
                                                     &datatype,
                                                     &next_frame,
                                                     &frameTime );
    } else {
      stat = tng_util_non_particle_data_next_frame_read( traj_,
                                                         blockId,
                                                         &values,
                                                         &datatype,
                                                         &next_frame,
                                                         &frameTime );
    }
    if (stat == TNG_CRITICAL) {
      mprinterr("Error: Could not read TNG block %li\n", blockId);
    } else if (stat == TNG_FAILURE) {
      mprintf("Warning: Skipping TNG block %li\n", blockId);
      continue;
    }
    mprintf("DEBUG: set %i frameTime %g block %li data %s\n", set+1, frameTime, blockId, DtypeStr(datatype));
    // TODO: ok to only expect float data?
    if (datatype != TNG_FLOAT_DATA) {
      mprinterr("Error: TNG block %li data type is %s, expected float!\n", blockId, DtypeStr(datatype));
      if (values != 0) free(values);
      return 1;
    }
    if ( blockId == TNG_TRAJ_BOX_SHAPE ) { // TODO switch?
      Matrix_3x3 boxShape(0.0);
      convertArray(boxShape.Dptr(), (float*)values, 9);
      frameIn.SetBox( Box(boxShape) );
      mprintf("DEBUG: box set %i %g %g %g\n", set,
              frameIn.BoxCrd().BoxX(),
              frameIn.BoxCrd().BoxY(),
              frameIn.BoxCrd().BoxZ());
    } else if ( blockId == TNG_TRAJ_POSITIONS ) {
      convertArray( frameIn.xAddress(), (float*)values, tngatoms_*3 );
      const double* tmpXYZ = frameIn.XYZ(0);
      mprintf("DEBUG: positions set %i %g %g %g\n", set, tmpXYZ[0], tmpXYZ[1], tmpXYZ[2]);
    }

  } // END loop over blocks in next frame
  if (values != 0) free(values);
  if (block_ids_in_next_frame != 0) free(block_ids_in_next_frame);

  // Update current frame
  current_frame_ = next_frame;
/*    
  int64_t stride;
  // Read coordinates
  tng_function_status stat = tng_util_pos_read_range(traj_, set, set, &ftmp_, &stride);
  
  if ( stat != TNG_SUCCESS ) {
    mprinterr("Error: Could not read set %i for TNG file.\n", set+1);
    return 1;
  }
  mprintf("DEBUG: positions set %i stride %li\n", set, stride);
  convertArray(frameIn.xAddress(), ftmp_, 3*tngatoms_);

  // Read time
  double tngtime;
  if (tng_util_time_of_frame_get(traj_, set, &tngtime) == TNG_SUCCESS) {
    mprintf("DEBUG: TNG time: %g\n", tngtime);
  }
*/
/*  if (CoordInfo().HasBox()) {
    float* boxptr = 0;
    if (tng_util_box_shape_read_range(traj_, set, set, &boxptr, &stride) != TNG_SUCCESS) {
      mprinterr("Error: Could not read set %i box for TNG file.\n", set+1);
      return 1;
    }
    mprintf("DEBUG: box set %i stride %li\n", set, stride);
    Matrix_3x3 boxShape(0.0);
    convertArray(boxShape.Dptr(), boxptr, 9);
    frameIn.SetBox( Box(boxShape) );
    mprintf("DEBUG: box set %i %g %g %g\n", set,
            frameIn.BoxCrd().BoxX(),
            frameIn.BoxCrd().BoxY(),
            frameIn.BoxCrd().BoxZ());
    free( boxptr );
  }*/
  return 0;
}

/** Read velocities from specified frame. */
int Traj_TNG::readVelocity(int set, Frame& frameIn) {

  return 0;
}

/** Read forces from specified frame. */
int Traj_TNG::readForce(int set, Frame& frameIn) {

  return 0;
}

// -----------------------------------------------------------------------------
/** Write help. */
void Traj_TNG::WriteHelp() {

}

/** Process write arguments. */
int Traj_TNG::processWriteArgs(ArgList& argIn, DataSetList const& DSLin) {

  return 0;
}

/** Set up trajectory for write. */
int Traj_TNG::setupTrajout(FileName const& fname, Topology* trajParm,
                                   CoordinateInfo const& cInfoIn, 
                                   int NframesToWrite, bool append)
{

  return 1;
}

/** Write specified trajectory frame. */
int Traj_TNG::writeFrame(int set, Frame const& frameOut) {

  return 0;
}

// =============================================================================
#ifdef MPI
/** Open trajectory for reading in parallel. */
int Traj_TNG::parallelOpenTrajin(Parallel::Comm const& commIn) {
  return 1;
}

/** Open trajectory for writing in parallel. */
int Traj_TNG::parallelOpenTrajout(Parallel::Comm const& commIn) {
  return 1;
}

/** Set up trajectory for write in parallel. */
int Traj_TNG::parallelSetupTrajout(FileName const& fname, Topology* trajParm,
                                           CoordinateInfo const& cInfoIn,
                                           int NframesToWrite, bool append,
                                           Parallel::Comm const& commIn)
{

  return 1;
}

/** Read frame in parallel. */
int Traj_TNG::parallelReadFrame(int set, Frame& frameIn) {

  return 1;
}

/** Write frame in parallel. */
int Traj_TNG::parallelWriteFrame(int set, Frame const& frameOut) {

  return 1;
}

/** Close trajectory in parallel. */
void Traj_TNG::parallelCloseTraj() {

}
#endif
#endif /* NO_TNGFILE */
