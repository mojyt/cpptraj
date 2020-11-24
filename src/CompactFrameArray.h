#ifndef INC_COMPACTFRAMEARRAY_H
#define INC_COMPACTFRAMEARRAY_H
#include "CoordinateInfo.h"
/// Hold a compact version of the time-depedent information contained in an array of Frames
class CompactFrameArray {
  public:
    CompactFrameArray();

    /// \return True if set up for any components
    bool HasComponents() const { return !(components_.empty()); }
    /// \return Array size in bytes.
    unsigned int SizeInBytes() const;
    /// \return Size of a single frame in elements
    unsigned int FrameSize() const;
    /// \return True if components/offsets do not match
    bool operator!=(CompactFrameArray const&) const;

    typedef std::vector<float>::iterator iterator;
    typedef std::vector<float>::const_iterator const_iterator;
    /// \return Modifiable iterator to beginning of specified frame
    iterator frameBegin(unsigned int idx) { return compactFrames_.begin() + (idx * offsets_.back()); }
    /// \return Const interator to beginning of specified frame
    const_iterator frameBegin(unsigned int idx) const { return compactFrames_.begin() + (idx * offsets_.back()); }

    /// Allocate for specified number of frames
    void Resize(int);
    /// Set up frame array to hold coordinate info with specified # atoms, optionally # frames.
    int SetupFrameArray(CoordinateInfo const&, unsigned int, int);

    /// Seek to specified frame, allocate space if necessary.
    void SeekAndAllocate(unsigned int);
    /// Copy component from double array to specified frame
    int SetFromDblPtr(const double*, CoordinateInfo::Component);
    /// Copy component from integer array to specified frame
    int SetFromIntPtr(const int*, CoordinateInfo::Component);
    /// Copy component from double value to specified frame
    int SetFromDblVal(double, CoordinateInfo::Component);

    /// Copy component from specified frame to double array
    int GetToDblPtr(double*, unsigned int, CoordinateInfo::Component) const;
    /// Copy component from specified frame to integer array
    int GetToIntPtr(int*, unsigned int, CoordinateInfo::Component) const;
    /// \return component for specified frame (assumes size is 1)
    float GetVal(unsigned int, CoordinateInfo::Component) const;
  private:
    void addComponent(long int&, CoordinateInfo::Component, long int);

    typedef std::vector<float> Farray;
    Farray compactFrames_;                              ///< Array storing all info.
    int componentIdx_[CoordinateInfo::NCOMPONENTS];     ///< Index into arrays for each component
    std::vector<CoordinateInfo::Component> components_; ///< List info contained in this array.
    std::vector<long int> offsets_;                     ///< Offsets for each component present.
    unsigned int currentIdx_;                           ///< Current frame position.
    unsigned int maxIdx_;                               ///< Max frame position.
};
#endif
