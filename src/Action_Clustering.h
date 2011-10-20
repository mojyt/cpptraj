#ifndef INC_ACTION_CLUSTERING_H
#define INC_ACTION_CLUSTERING_H
/// Class: Clustering
#include "Action.h"
#include "TriangleMatrix.h"
#include "ClusterList.h"
class Clustering: public Action {
    ClusterList::LINKAGETYPE Linkage;
    FrameList ReferenceFrames; // Hold frames from all trajin stmts
    AtomMask Mask0;            // Target atom mask
    double epsilon;            // Once the min distance is > epsilon, stop clustering
    int targetNclusters;       // Once there are targetNclusters, stop clustering
    DataSet *cnumvtime;        // Cluster vs time dataset
    char *summaryfile;         // Summary file name
    char *halffile;            // 1st/2nd half summary file name
    char *clusterfile;         // Cluster trajectory base filename.
    FileFormat clusterfmt;     // Cluster trajectory format.
    char *singlerepfile;       // Cluster all rep single trajectory filename.
    FileFormat singlerepfmt;   // Cluster all rep single trajectory format.
    char *repfile;             // Cluster rep to separate trajectory filename.
    FileFormat repfmt;         // Cluster rep to separate trajectory format.
    bool nofitrms;             // If true do not best-fit when calc RMSD.
    bool grace_color;          // If true print grace colors instead of cluster number

    int calcDistFromRmsd( TriangleMatrix *);
    int ClusterHierAgglo( TriangleMatrix *, ClusterList*);
    void CreateCnumvtime( ClusterList * );
    void WriteClusterTraj( ClusterList * );
    void WriteSingleRepTraj( ClusterList * );
    void WriteRepTraj( ClusterList * );
  public:
    Clustering();
    ~Clustering();

    int init();
    int action();
    void print();
};
#endif
