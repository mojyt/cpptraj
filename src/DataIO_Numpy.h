#ifndef INC_DATAIO_NUMPY_H
#define INC_DATAIO_NUMPY_H
#include "DataIO.h"
/// <Enter description of DataIO_Numpy here>
class DataIO_Numpy : public DataIO {
  public:
    DataIO_Numpy();
    static void ReadHelp();
    static void WriteHelp();
    static BaseIOtype* Alloc() { return (BaseIOtype*)new DataIO_Numpy(); }
    int processReadArgs(ArgList&);
    int ReadData(FileName const&, DataSetList&, std::string const&);
    int processWriteArgs(ArgList&);
    int WriteData(FileName const&, DataSetList const&);
    bool ID_DataFormat(CpptrajFile&);
};
#endif
