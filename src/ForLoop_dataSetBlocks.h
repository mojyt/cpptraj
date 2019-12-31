#ifndef INC_FORLOOP_DATASETBLOCKS_H
#define INC_FORLOOP_DATASETBLOCKS_H
#include "ForLoop.h"
/// For loop, data set blocks
class ForLoop_dataSetBlocks : public ForLoop {
  public:
    ForLoop_dataSetBlocks();

    int SetupFor(CpptrajState&, std::string const&, ArgList&);
    int BeginFor(DataSetList const&);
    bool EndFor(DataSetList const&);
    
  private:
    DataSet* sourceSet_; ///< Set to loop over
    int blocksize_;      ///< Size of blocks
    int blockoffset_;    ///< Block offset
};
#endif
