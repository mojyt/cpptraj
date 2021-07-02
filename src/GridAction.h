#ifndef INC_GRIDACTION_H
#define INC_GRIDACTION_H
#include "AtomMask.h"
#include "Vec3.h"
#include "Frame.h"
#include "DataSet_GridFlt.h"
#include "DataSetList.h"
class Topology;
class ArgList;
class CoordinateInfo;
/// Class for setting up a grid within an action.
class GridAction {
  public:
    /// Indicate whether to apply an offset to coords before gridding.
    enum OffsetType { NO_OFFSET = 0, BOX_CENTER, MASK_CENTER };
    /// Indicate where grid should be located
    enum MoveType { NO_MOVE = 0, TO_BOX_CTR, TO_MASK_CTR, RMS_FIT };
    /// CONSTRUCTOR
    GridAction();
    /// \return List of keywords recognized by GridInit.
    static const char* HelpText;
    /// \return Set-up grid (added to given DataSetList) after processing keywords.
    DataSet_GridFlt* GridInit(const char*, ArgList&, DataSetList&);
#   ifdef MPI
    /// Perform any parallel initialization
    int ParallelGridInit(Parallel::Comm const&, DataSet_GridFlt*);
#   endif
    /// Print information on given grid to STDOUT
    void GridInfo(DataSet_GridFlt const&);
    /// Perform any setup necessary for given Topology/CoordinateInfo
    int GridSetup(Topology const&, CoordinateInfo const&);
    /// Place atoms selected by given mask in given Frame on the given grid.
    inline void GridFrame(Frame const&, AtomMask const&, DataSet_GridFlt&) const;
    /// Move grid if necessary
    inline void MoveGrid(Frame const&, DataSet_GridFlt&);
    /// Anything needed to finalize the grid
    void FinishGrid(DataSet_GridFlt&) const;
    /// \return Type of offset to apply to coords before gridding.
    OffsetType GridOffsetType()  const { return gridOffsetType_;       }
    /// \return Mask to use for centering grid
    AtomMask const& CenterMask() const { return centerMask_; }
    /// \return Amount voxels should be incremented by
    float Increment()            const { return increment_;  }
  private:
    OffsetType gridOffsetType_;
    MoveType gridMoveType_;
    AtomMask centerMask_;
    float increment_;     ///< Set to -1 if negative, 1 if not.
    Frame tgt_;           ///< For MoveType RMS_FIT, previous frames selected coordinates
    Frame ref_;           ///< For MoveType RMS_FIT, current frames selected coordinates
    bool firstFrame_;     ///< For MoveType RMS_FIT, true if this is the first frame (no fit needed)
    bool x_align_;        ///< For MoveType RMS_FIT, if true ensure grid is X-aligned in FinishGrid();
};
// ----- INLINE FUNCTIONS ------------------------------------------------------
void GridAction::GridFrame(Frame const& currentFrame, AtomMask const& mask, 
                           DataSet_GridFlt& grid)
const
{
  if (gridOffsetType_ == BOX_CENTER) {
    Vec3 offset = currentFrame.BoxCrd().Center();
    for (AtomMask::const_iterator atom = mask.begin(); atom != mask.end(); ++atom)
      grid.Increment( Vec3(currentFrame.XYZ(*atom)) - offset, increment_ );
  } else if (gridOffsetType_ == MASK_CENTER) {
    Vec3 offset = currentFrame.VGeometricCenter( centerMask_ );
    for (AtomMask::const_iterator atom = mask.begin(); atom != mask.end(); ++atom)
      grid.Increment( Vec3(currentFrame.XYZ(*atom)) - offset, increment_ );
  } else {// mode_==NO_OFFSET
    for (AtomMask::const_iterator atom = mask.begin(); atom != mask.end(); ++atom)
      grid.Increment( currentFrame.XYZ(*atom), increment_ );
  }
}

/** Move grid if necessary. */
void GridAction::MoveGrid(Frame const& currentFrame, DataSet_GridFlt& grid)
{
  if (gridMoveType_ == TO_BOX_CTR)
    grid.SetGridCenter( currentFrame.BoxCrd().Center() );
  else if (gridMoveType_ == TO_MASK_CTR)
    grid.SetGridCenter( currentFrame.VGeometricCenter( centerMask_ ) );
  else if (gridMoveType_ == RMS_FIT) {
    grid.SetGridCenter( currentFrame.VGeometricCenter( centerMask_ ) );
    if (firstFrame_) {
      tgt_.SetFrame( currentFrame, centerMask_ );
      firstFrame_ = false;
    } else {
      ref_.SetFrame( currentFrame, centerMask_ );
      Matrix_3x3 Rot;
      Vec3 T1, T2;
      tgt_.RMSD( ref_, Rot, T1, T2, false );
      grid.Rotate_3D_Grid( Rot );
      tgt_.SetFrame( currentFrame, centerMask_ );
    }
  }
}
#endif
