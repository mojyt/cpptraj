#ifndef INC_STRUCTURE_ZMATRIX_H
#define INC_STRUCTURE_ZMATRIX_H
#include "InternalCoords.h"
#include "../Vec3.h"
class Frame;
class Topology;
class Molecule;
namespace Cpptraj {
namespace Structure {
/// Hold internal coordinates for a system.
class Zmatrix {
    typedef std::vector<InternalCoords> ICarray;
  public:
    /// CONSTRUCTOR
    Zmatrix();
    /// Set debug level
    void SetDebug(int d) { debug_ = d; }
    /// Add internal coordinate
    int AddIC(InternalCoords const&);
    /// Add internal coordinate as next available IC seed
    //int AddICseed(InternalCoords const&);
    /// Set seed atoms from frame/top
    int SetSeedPositions(Frame const&, Topology const&, int, int, int);
    /// Convert specifed molecule of Frame/Topology to internal coordinates array
    int SetFromFrame(Frame const&, Topology const&, int);
    /// Convert from Cartesian to Zmatrix by tracing a molecule
    int SetFromFrame_Trace(Frame const&, Topology const&, int);
    /// Convert molecule 0 of Frame/Topology to internal coordinates array
    int SetFromFrame(Frame const&, Topology const&);

    /// Set Frame from internal coords
    int SetToFrame(Frame&) const;
    /// Print to stdout
    void print() const;

    typedef ICarray::const_iterator const_iterator;
    const_iterator begin() const { return IC_.begin(); }
    const_iterator end()   const { return IC_.end(); }

    /// \return number of internal coords
    unsigned int N_IC() const { return IC_.size(); }
    /// \return memory usage in bytes
    unsigned int sizeInBytes() const { return (7*sizeof(int)) +
                                              (9*sizeof(double)) + // 3 Vec3
                                              (IC_.size() * InternalCoords::sizeInBytes()); }
    /// \reserve space for # of internal coords TODO zero out seeds?
    void reserve(unsigned int n) { IC_.reserve( n ); }
    /// \return Internal coord of specified index
    InternalCoords const& operator[](int i) const { return IC_[i]; }
  private:
    typedef std::vector<int> Iarray;
    typedef std::vector<bool> Barray;
    /// Simple version of auto set seeds based on connectivity only
    int autoSetSeeds_simple(Frame const&, Topology const&, Molecule const&);
    /// Automatically set seeds
//    int autoSetSeeds(Frame const&, Topology const&, unsigned int, int);
    /// Calculate and add an internal coordinate given indices and Cartesian coords.
    void addIc(int,int,int,int,const double*,const double*,const double*,const double*);
    /// Add internal coordiantes by tracing a molecule
    int traceMol(int, int, int, Frame const&, Topology const&, unsigned int, unsigned int&, Barray&);
    /// \return True if IC seeds are set
    //bool HasICSeeds() const;
    /// \return True if Cartesian seeds are set
    bool HasCartSeeds() const;

    int debug_;     ///< Print debug info
    ICarray IC_;    ///< Hold internal coordinates for all atoms
    int icseed0_;   ///< Index into IC_ of first seed
    int icseed1_;   ///< Index into IC_ of second seed
    int icseed2_;   ///< Index into IC_ of third seed
    Vec3 seed0Pos_; ///< Seed 0 xyz
    Vec3 seed1Pos_; ///< Seed 1 xyz
    Vec3 seed2Pos_; ///< Seed 2 xyz
    int seedAt0_;   ///< Seed 0 topology index if seed0Pos_ is set
    int seedAt1_;   ///< Seed 1 topology index if seed1Pos_ is set
    int seedAt2_;   ///< Seed 2 topology index if seed2Pos_ is set
};
}
}
#endif
