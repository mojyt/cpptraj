#ifndef INC_STRUCTURE_ZMATRIX_H
#define INC_STRUCTURE_ZMATRIX_H
#include "InternalCoords.h"
#include "../Vec3.h"
class Frame;
class Topology;
namespace Cpptraj {
namespace Structure {
/// Hold internal coordinates for a system.
class Zmatrix {
    typedef std::vector<InternalCoords> ICarray;
    typedef std::vector<int> Iarray;
  public:
    /// CONSTRUCTOR
    Zmatrix();
    /// Set debug level
    void SetDebug(int d) { debug_ = d; }
    /// Add internal coordinate and topology index
    void AddIC(InternalCoords const&, int);
    /// Add internal coordinate and topology index as next available seed
    int AddICseed(InternalCoords const&, int);
    /// Set seed atoms from frame/top
    int SetSeeds(Frame const&, Topology const&, int, int, int);
    /// Convert Frame/Topology to internal coordinates array
    int SetFromFrame(Frame const&, Topology const&, int);

    /// \return True if any of the seeds are not set
    bool NoSeeds() const;
    /// Set Frame from internal coords
    int SetToFrame(Frame&) const;
    /// Print to stdout
    void print() const;

    typedef ICarray::const_iterator const_iterator;
    const_iterator begin() const { return IC_.begin(); }
    const_iterator end()   const { return IC_.end(); }
  private:
    int debug_;         ///< Print debug info
    ICarray IC_;        ///< Hold internal coordinates for all atoms
    Iarray topIndices_; ///< For each IC, corresponding atom index in topology
    int seed0_;         ///< Index into IC_ of first seed atom
    int seed1_;         ///< Index into IC_ of second seed atom
    int seed2_;         ///< Index into IC_ of third seed atom
    Vec3 seed0Pos_;     ///< Seed 0 xyz
    Vec3 seed1Pos_;     ///< Seed 1 xyz
    Vec3 seed2Pos_;     ///< Seed 2 xyz
    int seed0TopIdx_;   ///< Seed 0 topology index if seed0Pos_ is set
    int seed1TopIdx_;   ///< Seed 1 topology index if seed1Pos_ is set
    int seed2TopIdx_;   ///< Seed 2 topology index if seed2Pos_ is set
};
}
}
#endif
