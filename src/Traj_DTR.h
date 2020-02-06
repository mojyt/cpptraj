#ifndef INC_TRAJ_DTR_H
#define INC_TRAJ_DTR_H
#include "TrajectoryIO.h"
#include "vmdplugin/dtrplugin.hxx"
/// Used to read Desmond DTR trajectory 
class Traj_DTR : public TrajectoryIO {
  public:
    Traj_DTR();
    static BaseIOtype* Alloc() { return (BaseIOtype*)new Traj_DTR(); }
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

    desres::molfile::FrameSetReader* DTR_;
};
#endif
