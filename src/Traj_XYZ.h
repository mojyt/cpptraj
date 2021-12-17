#ifndef INC_TRAJ_XYZ_H
#define INC_TRAJ_XYZ_H
#include "TrajectoryIO.h"
#include "BufferedLine.h"
/// Read simple XYZ trajectories. 
class Traj_XYZ : public TrajectoryIO {
  public:
    Traj_XYZ();
    static BaseIOtype* Alloc() { return (BaseIOtype*)new Traj_XYZ(); }
    static void WriteHelp();
    static void ReadHelp();
  private:
    // ----- Inherited functions -----------------
    bool ID_TrajFormat(CpptrajFile&);
    int setupTrajin(FileName const&, Topology*);
    int setupTrajout(FileName const&, Topology*, CoordinateInfo const&,int, bool);
    int openTrajin();
    void closeTraj();
    int readFrame(int,Frame&);
    int writeFrame(int,Frame const&);
    void Info();
    int readVelocity(int, Frame&);
    int readForce(int, Frame&);
    int processWriteArgs(ArgList&, DataSetList const&);
    int processReadArgs(ArgList&);
    // -------------------------------------------
#   ifdef MPI
    // ----- Parallel functions ------------------
    int parallelOpenTrajin(Parallel::Comm const&);
    int parallelOpenTrajout(Parallel::Comm const&);
    int parallelSetupTrajout(FileName const&, Topology*, CoordinateInfo const&,
                             int, bool, Parallel::Comm const&);
    int parallelReadFrame(int, Frame&);
    int parallelWriteFrame(int, Frame const&);
    void parallelCloseTraj();
    // -------------------------------------------
#   endif
    /// Data line types
    enum Type { UNKNOWN=0,
                XYZ,       ///< <X> <Y> <Z>
                ATOM_XYZ,  ///< <#> <X> <Y> <Z>
                NAME_XYZ,  ///< <Name> <X> <Y> <Z>
                ELT_XYZ    ///< <Elt> <X> <Y> <Z>
              };
    /// Specify how/when headers should appear
    enum TitleType { NO_TITLE = 0,  ///< Never
                     SINGLE,        ///< First frame only
                     MULTIPLE,      ///< Every frame
                     UNKNOWN_TITLE
                   };
    /// Line format types
    enum LineFmtType { SINGLE_INTEGER,
                       THREE_DOUBLES,
                       STRING_AND_THREE_DOUBLES,
                       UNKNOWN_LINE_FORMAT };
    /// \return Format of given line
    static LineFmtType DetermineLineFormat(std::string const&);
    Type DetermineFormat(std::string&, std::string const&) const;
    inline void ReadTitle();
    int readXYZ(int, int, double*);

    static const char* FMT_XYZ_;
    static const char* FMT_ATOM_XYZ_;

    BufferedLine file_;
    std::string ofmt_;
    TitleType titleType_;
    Type ftype_;
    int set_;
    int width_;
    int prec_;
    const char* fmt_; ///< Format for reading
};
#endif
