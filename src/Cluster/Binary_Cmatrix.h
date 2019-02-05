#ifndef INC_CLUSTER_BINARY_CMATRIX_H
#define INC_CLUSTER_BINARY_CMATRIX_H
#include <cstddef> // size_t
#include "../CpptrajFile.h" 
#include "../FileName.h"
#include "Cframes.h"

namespace Cpptraj {
namespace Cluster {

/// Used to read pairwise distance cache files in cpptraj binary format.
class Binary_Cmatrix {
  public:
    Binary_Cmatrix();

    /// \return true if file is binary cpptraj cluster matrix file.
    static bool ID_Cmatrix(FileName const&);

    /// \return Sieve value.
    int Sieve()          const { return sieve_; }
    /// \return Actual number of rows in matrix.
    size_t ActualNrows() const { return actual_nrows_; }

    /// Open cluster matrix file for reading. Set sieve and actual # rows. 
    int OpenCmatrixRead(FileName const&);
    /// Get cluster matrix element (col, row)
    double GetCmatrixElement(unsigned int, unsigned int) const;
    /// Get cluster matrix element (raw index)
    double GetCmatrixElement(unsigned int) const;
    /// Read cmatrix into given pointer
    int GetCmatrix(float*) const;
  private:
    static const unsigned char Magic_[];
    /// For reading/writing 8 byte unsigned integers
    typedef unsigned long long int uint_8;
    /// For reading/writing 8 byte signed integers
    typedef long long int sint_8;
    
    CpptrajFile file_;
    int sieve_;
    size_t actual_nrows_;
};

}
}
#endif
