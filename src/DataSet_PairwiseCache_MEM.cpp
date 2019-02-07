#include "DataSet_PairwiseCache_MEM.h"
#include "CpptrajStdio.h"

int DataSet_PairwiseCache_MEM::Allocate(SizeArray const& sizeIn) {
  int err = 0;
  if (!sizeIn.empty()) {
    // Sanity check.
    if (sizeIn.size() > 1 && sizeIn[1] != sizeIn[0])
      mprintf("Warning: DataSet_PairwiseCache dimensions must be equal (%zu != %zu)\n"
              "Warning: Matrix will be %zu x %zu upper triangle\n",
              sizeIn[0], sizeIn[1], sizeIn[0], sizeIn[0]);
    err = Mat_.resize(0, sizeIn[0]); // Upper triangle
  } else
    Mat_.clear();
  return err;
}


int DataSet_PairwiseCache_MEM::SetupCache(unsigned int Ntotal, Cframes const& framesToCache,
                                          int sieve, std::string const& metricDescription)
{
  unsigned int sizeIn = framesToCache.size();
  if (sizeIn > 0) {
    if (Mat_.resize(0, sizeIn)) return 1; // Upper triangle
#   ifdef DEBUG_CLUSTER
    mprintf("DEBUG: PairwiseMatrix_MEM set up for %i rows, size= %zu bytes.\n",
            Mat_.Nrows(), Mat_.sizeInBytes());
#   endif
    // TODO probably need to save metricDescription and sieve here for potential file write.
  } else
    Mat_.clear();
  return SetupFrameToIdx(framesToCache, Ntotal);
}


/** Print cached distances to stdout. */
void DataSet_PairwiseCache_MEM::PrintCached() const {
  for (Cframes::const_iterator it1 = FrameToIdx().begin(); it1 != FrameToIdx().end(); ++it1)
  {
    if (*it1 != -1) {
      for (Cframes::const_iterator it2 = it1 + 1; it2 != FrameToIdx().end(); ++it2)
      {
        if (*it2 != -1)
          mprintf("\t%i %i %f\n", *it1+1, *it2+1, Mat_.element(*it1, *it2));
      }
    }
  }
}
