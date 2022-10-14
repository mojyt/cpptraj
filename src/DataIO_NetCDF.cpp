#include "DataIO_NetCDF.h"
#include "CpptrajStdio.h"
#ifdef BINTRAJ
# include <map>
# include <string>
# include <netcdf.h>
# include "DataSet_1D.h"
# include "NC_Routines.h"
# include "StringRoutines.h"
# include "Version.h"
// DataSets
# include "DataSet_Mesh.h"
# include "DataSet_MatrixDbl.h"
#endif

/// CONSTRUCTOR
DataIO_NetCDF::DataIO_NetCDF() :
  DataIO(true, true, true), // Valid for 1D, 2D, 3D
  ncid_(-1),
  dimIdx_(0),
  varIDptr_(0)
{

}

// DataIO_NetCDF::ID_DataFormat()
bool DataIO_NetCDF::ID_DataFormat(CpptrajFile& infile)
{
  if (infile.OpenFile()) return false;
  // Read first 3 bytes
  unsigned char magic[3];
  magic[0] = 0;
  magic[1] = 0;
  magic[2] = 0;
  infile.Read(magic, 3);
  infile.CloseFile();
  if (magic[0] == 'C' && magic[1] == 'D' && magic[2] == 'F') {
#   ifdef BINTRAJ
    return true;
#   else
    mprintf("Warning: '%s' is a NetCDF file but CPPTRAJ was compiled without NetCDF support.\n",
            infile.Filename().full());
#   endif
  }
  return false;
}

// -----------------------------------------------------------------------------
/** Hold info for a netcdf variable. */
class DataIO_NetCDF::NcVar {
  public:
    NcVar(int vidIn, nc_type vtypeIn, const char* vnameIn, int ndims, const int* dimids) :
      vid_(vidIn), vtype_(vtypeIn), vname_(vnameIn), hasBeenRead_(false), dimIds_(ndims)
    {
      for (int i = 0; i < ndims; i++) dimIds_[i] = dimids[i];
    }

    int VID()                  const { return vid_; }
    nc_type Vtype()            const { return vtype_; }
    std::string const& Vname() const { return vname_; }
    const char* vname()        const { return vname_.c_str(); }
    bool HasBeenRead()         const { return hasBeenRead_; }
    std::vector<int> const& DimIds() const { return dimIds_; }

    void MarkRead() { hasBeenRead_ = true; }
  private:
    int vid_;                 ///< Netcdf variable id
    nc_type vtype_;           ///< Netcdf variable type
    std::string vname_;       ///< Variable name
    bool hasBeenRead_;        ///< True if the variable has been read in.
    std::vector<int> dimIds_; ///< Netcdf dimension ids for this var.
};

// -----------------------------------------------------------------------------

// DataIO_NetCDF::ReadHelp()
void DataIO_NetCDF::ReadHelp()
{

}

// DataIO_NetCDF::processReadArgs()
int DataIO_NetCDF::processReadArgs(ArgList& argIn)
{

  return 0;
}

/// Get integer attribute from variable
/** \return 1 if an error occurred, 0 if found, and -1 if not present. */
static inline int GetVarIntAtt(int& ival, const char* desc, int ncid, int varid)
{
  ival = -1;
  int ncerr = nc_get_att_int(ncid, varid, desc, &ival);
  if (ncerr != NC_NOERR) {
    if (ncerr != NC_ENOTATT) {
      NC::CheckErr(ncerr);
      mprinterr("Error: Could not get '%s' attribute.\n", desc);
      return 1;
    }
    return -1;
  }
  return 0;
}

/// Get double attribute from variable
static inline int GetVarDblAtt(double& dval, const char* desc, int ncid, int varid)
{
  dval = 0;
  int ncerr = nc_get_att_double(ncid, varid, desc, &dval);
  if (ncerr != NC_NOERR) {
    if (ncerr != NC_ENOTATT) {
      NC::CheckErr(ncerr);
      mprinterr("Error: Could not get '%s' attribute.\n", desc);
      return 1;
    }
    return -1;
  }
  return 0;
}

/// Get DataSet metadata from a variable
static inline MetaData GetVarMetaData(int& errStat, int ncid, int varid)
{
  MetaData meta;
  errStat = 0;
  // Filename
  std::string att = NC::GetAttrText(ncid, varid, "filename");
  if (!att.empty())
    meta.SetFileName( att );
  // Name
  att = NC::GetAttrText(ncid, varid, "name");
  if (att.empty()) {
    mprinterr("Error: 'name' attribute missing for VID %i\n", varid);
    errStat = 1;
    return meta;
  }
  meta.SetName( att );
  // Aspect
  att = NC::GetAttrText(ncid, varid, "aspect");
  if (!att.empty())
    meta.SetAspect( att );
  // Legend
  att = NC::GetAttrText(ncid, varid, "legend");
  if (!att.empty())
    meta.SetLegend( att );
  // Index
  int ival;
  int ret = GetVarIntAtt(ival, "index", ncid, varid);
  if (ret == 1) {
    errStat = 1;
    return meta;
  } else if (ret == 0) {
    meta.SetIdx( ival );
  }
  // Ensemble number
  ret = GetVarIntAtt(ival, "ensemblenum", ncid, varid);
  if (ret == 1) {
    errStat = 1;
    return meta;
  } else if (ret == 0) {
    meta.SetEnsembleNum( ival );
  } 
  // TODO  TimeSeries?
  // Scalar mode
  MetaData::scalarMode smode = MetaData::UNKNOWN_MODE;
  att = NC::GetAttrText(ncid, varid, "scalarmode");
  if (!att.empty())
    smode = MetaData::ModeFromKeyword( att );
  meta.SetScalarMode( smode );
  // Scalar type
  MetaData::scalarType stype = MetaData::UNDEFINED;
  att = NC::GetAttrText(ncid, varid, "scalartype");
  if (!att.empty())
    stype = MetaData::TypeFromKeyword( att, smode );
  meta.SetScalarType( stype );

  return meta;
}

/// \return Dimension from variable attributes
static inline std::vector<Dimension> GetVarDimensions(int& errStat, int ncid, int varid)
{
  errStat = 0;
  std::vector<Dimension> Dims;
  // Get # of dimensions
  int ndim = 0;
  int ret = GetVarIntAtt(ndim, "ndim", ncid, varid);
  if (ret != 0) {
    mprinterr("Error: Missing 'ndim' attribute.\n");
    return Dims;
  }
  mprintf("DEBUG: ndim= %i\n", ndim);
  Dims.resize( ndim );
  for (int i = 0; i < ndim; i++) {
    std::string label, min, step;
    if (ndim > 1) {
      label = "label" + integerToString(i);
      min = "min" + integerToString(i);
      step = "step" + integerToString(i);
    } else {
      label = "label";
      min = "min";
      step = "step";
    }
    Dimension& dim = Dims[i];
    // label is optional
    std::string att = NC::GetAttrText(ncid, varid, label.c_str());
    if (!att.empty()) dim.SetLabel( att );
    double dval;
    int ret = GetVarDblAtt(dval, min.c_str(), ncid, varid);
    if (ret != 0) {
      mprinterr("Error: '%s' attribute is missing for varid %i.\n", min.c_str(), varid);
      errStat = 1;
      return Dims;
    }
    dim.ChangeMin( dval );
    ret = GetVarDblAtt(dval, step.c_str(), ncid, varid);
    if (ret != 0) {
      mprinterr("Error: '%s' attribute is missing for varid %i.\n", step.c_str(), varid);
      errStat = 1;
      return Dims;
    }
    dim.ChangeStep( dval );
  }
  return Dims;
}

/** Mark variable with given vid as read. */
void DataIO_NetCDF::MarkVarIdRead(VarArray& Vars, int vid)
{
  for (VarArray::iterator it = Vars.begin(); it != Vars.end(); ++it)
  {
    if (it->VID() == vid) {
      it->MarkRead();
      break;
    }
  }
}

/** Read variable with CPPTRAJ conventions. */
int DataIO_NetCDF::read_cpptraj_vars(DataSetList& dsl, std::string const& dsname, VarArray& Vars,
                                     std::vector<unsigned int> const& dimLengths)
{
  for (VarArray::const_iterator var = Vars.begin(); var != Vars.end(); ++var)
  {
    if (var->HasBeenRead()) continue;

    // Get the description
    std::string desc = NC::GetAttrText(ncid_, var->VID(), "description");
    // Get the type from the description
    DataSet::DataType dtype = DataSet::TypeFromDescription( desc );
    mprintf("\t%s Description: %s (%i)\n", var->vname(), desc.c_str(), (int)dtype);
    // Get metaData
    int errStat = 0;
    MetaData meta = GetVarMetaData( errStat, ncid_, var->VID() );
    if (errStat != 0) {
      mprinterr("Error: Could not set up meta data for variable '%s'\n", var->vname());
      return 1;
    }
    // Get DataSet dimensions
    errStat = 0;
    std::vector<Dimension> Dims = GetVarDimensions(errStat, ncid_, var->VID());
    if (errStat != 0) return 1;
    for (std::vector<Dimension>::const_iterator dim = Dims.begin(); dim != Dims.end(); ++dim)
      mprintf("DEBUG:\t Var %s dim %s min %f step %f\n", var->vname(), dim->label(), dim->Min(), dim->Step());
    // Add the set
    DataSet* ds = dsl.AddSet( dtype, meta );
    if (ds == 0) {
      mprinterr("Error: Could not allocate set '%s'\n", meta.PrintName().c_str());
      return 1;
    }
    // Set DataSet dimensions
    unsigned int idx = 0;
    for (std::vector<Dimension>::const_iterator dim = Dims.begin(); dim != Dims.end(); ++dim)
      ds->SetDim(idx++, *dim);
    // Check netcdf variable dimensions
    if (var->DimIds().size() == 1) {
      // One flat dimension
      size_t start[1];
      size_t count[1];
      start[0] = 0;
      count[0] = dimLengths[ var->DimIds()[0] ];
      mprintf("DEBUG: %s dim length %zu\n", var->vname(), count[0]);
      if (dtype == DataSet::XYMESH) {
        // Get the Y var id
        int yvarid;
        int ret = GetVarIntAtt(yvarid, "yvarid", ncid_, var->VID());
        if (ret != 0) {
          mprinterr("Error: No 'yvarid' attribute for XY set '%s'.\n", var->vname());
          return 1;
        }
        DataSet_Mesh& set = static_cast<DataSet_Mesh&>( *ds );
        set.Resize(count[0]);
        DataSet_Mesh::Darray& Xvals = set.SetMeshX();
        DataSet_Mesh::Darray& Yvals = set.SetMeshY();
        // Get X
        if (NC::CheckErr(nc_get_vara(ncid_, var->VID(), start, count, (void*)(&Xvals[0])))) {
          mprinterr("Error: Could not get X values for XY set.\n");
          return 1;
        }
        Vars[var->VID()].MarkRead();
        // Get Y
        if (NC::CheckErr(nc_get_vara(ncid_, yvarid, start, count, (void*)(&Yvals[0])))) {
          mprinterr("Error: Could not get Y values for XY set.\n");
          return 1;
        }
        Vars[yvarid].MarkRead();
      } else if (ds->Group() == DataSet::SCALAR_1D) {
        DataSet_1D& set = static_cast<DataSet_1D&>( *ds );
        set.Resize( count[0] );
        if (NC::CheckErr(nc_get_vara(ncid_, var->VID(), start, count, (void*)(set.Yptr())))) {
          mprinterr("Error: Could not get values for set.\n");
          return 1;
        }
      } else if (ds->Group() == DataSet::MATRIX_2D) {
        // Get nrows/ncols
        int ncols, nrows;
        int ret = GetVarIntAtt(ncols, "ncols", ncid_, var->VID());
        if (ret != 0) {
          mprinterr("Error: Could not get 'ncols'.\n");
          return 1;
        }
        ret = GetVarIntAtt(nrows, "nrows", ncid_, var->VID());
        if (ret != 0) {
          mprinterr("Error: Could not get 'nrows'.\n");
          return 1;
        }
        // Get matrix kind
        std::string mkind = NC::GetAttrText(ncid_, var->VID(), "matrixkind");
        // Allocate
        DataSet_2D& mat = static_cast<DataSet_2D&>( *ds );
        int allocErr = 0;
        if (mkind == "full")
          allocErr = mat.Allocate2D(ncols, nrows);
        else if (mkind == "half")
          allocErr = mat.AllocateHalf(ncols);
        else if (mkind == "tri")
          allocErr = mat.AllocateTriangle(ncols);
        else {
          mprinterr("Error: Urecognized matrix kind: %s\n", mkind.c_str());
          return 1;
        }
        if (allocErr != 0) {
          mprinterr("Error: Could not allocate matrix.\n");
          return 1;
        }
        // Read values
        if (NC::CheckErr(nc_get_vara(ncid_, var->VID(), start, count, (void*)(mat.MatrixPtr())))) {
          mprinterr("Error: Could not get values for matrix.\n");
          return 1;
        }
        Vars[var->VID()].MarkRead();
        // Check for vect
        int vectVarId = -1;
        ret = GetVarIntAtt(vectVarId, "vectid", ncid_, var->VID());
        if (ret == 1) return 1;
        if (ret == 0) {
          mprintf("DEBUG: Matrix has diagonal vector data.\n");
          if (mat.Type() != DataSet::MATRIX_DBL) {
            mprinterr("Error: Variable has vect data but set is not double matrix.\n");
            return 1;
          }
          unsigned int vectLength = dimLengths[ vectVarId ];
          DataSet_MatrixDbl& dmat = static_cast<DataSet_MatrixDbl&>( mat );
          dmat.AllocateVector( vectLength );
          count[0] = vectLength;
          if (NC::CheckErr(nc_get_vara(ncid_, vectVarId, start, count, (void*)(&dmat.V1()[0])))) {
            mprinterr("Error: Could not get vect for matrix.\n");
            return 1;
          }
          Vars[vectVarId].MarkRead();
        }
      }
    } else {
      mprinterr("Error: Cannot read type '%s' yet.\n", desc.c_str());
      return 1;
    }
  }
  return 0;
}
   


/** Read 1D variable(s) that share a dimension with CPPTRAJ conventions. */
int DataIO_NetCDF::read_1d_var(DataSetList& dsl, std::string const& dsname,
                               unsigned int dimLength, VarArray const& Vars)
const
{
  if (Vars.empty()) {
    mprinterr("Internal Error: read_1d_var() called with no variables.\n");
    return 1;
  }
  // Loop over the variables
  for (VarArray::const_iterator it = Vars.begin(); it != Vars.end(); ++it)
  {
    // Get the description
    std::string desc = NC::GetAttrText(ncid_, it->VID(), "description");
    // Get the type from the description
    DataSet::DataType dtype = DataSet::TypeFromDescription( desc );
    mprintf("\t%s Description: %s (%i)\n", it->vname(), desc.c_str(), (int)dtype);
    // Get metaData
    int errStat = 0;
    MetaData meta = GetVarMetaData( errStat, ncid_, it->VID() );
    if (errStat != 0) {
      mprinterr("Error: Could not set up meta data for variable '%s'\n", it->vname());
      return 1;
    }
    // Get dimension
    std::vector<Dimension> Dims = GetVarDimensions( errStat, ncid_, it->VID() );
    Dimension const& dim = Dims[0];
    if (errStat != 0) {
      mprinterr("Error: Could not set up dimension for variable '%s'\n", it->vname());
      return 1;
    }
    mprintf("DEBUG: Var %s dim %s min %f step %f\n", it->vname(), dim.label(), dim.Min(), dim.Step());
//    // For backwards compat., if no label set assume Frame
//    if (dim.Label().empty())
//      dim.SetLabel("Frame");
    // Add the set
    size_t start[1];
    size_t count[1];
    start[0] = 0;
    count[0]  = dimLength;
    if (dtype == DataSet::XYMESH) {
      // Expect 2 vars, X and Y, in that order. Only X will contain attributes
      if (Vars.size() != 2) {
        mprinterr("Error: Expected only 2 variables for XY set '%s'\n", it->vname());
        return 1;
      }
      // Check this is X, next is Y
      int yvarid;
      int ret = GetVarIntAtt(yvarid, "yvarid", ncid_, it->VID());
      if (ret != 0) {
        mprinterr("Error: No 'yvarid' attribute for XY set '%s'.\n", it->vname());
        return 1;
      }
      if (yvarid != (it+1)->VID()) {
        mprinterr("Error: Expected Y varid to be %i, got %i\n", yvarid, (it+1)->VID());
        return 1;
      }
      DataSet* ds = dsl.AddSet( dtype, meta );
      if (ds == 0) {
        mprinterr("Error: Could not create set '%s'\n", meta.PrintName().c_str());
        return 1;
      }
      ds->SetDim(0, dim);
      mprintf("DEBUG: '%s'\n", ds->legend());
      DataSet_Mesh& set = static_cast<DataSet_Mesh&>( *ds );
      set.Resize(dimLength);
      DataSet_Mesh::Darray& Xvals = set.SetMeshX();
      DataSet_Mesh::Darray& Yvals = set.SetMeshY();
      // Get X
      if (NC::CheckErr(nc_get_vara(ncid_, it->VID(), start, count, (void*)(&Xvals[0])))) {
        mprinterr("Error: Could not get X values for XY set.\n");
        return 1;
      }
      // Get Y
      it++;
      if (NC::CheckErr(nc_get_vara(ncid_, it->VID(), start, count, (void*)(&Yvals[0])))) {
        mprinterr("Error: Could not get Y values for XY set.\n");
        return 1;
      }
      break;
    } else {
      DataSet* ds = dsl.AddSet( dtype, meta );
      if (ds == 0) {
        mprinterr("Error: Could not create set '%s'\n", meta.PrintName().c_str());
        return 1;
      }
      ds->SetDim(0, dim);
      mprintf("DEBUG: '%s'\n", ds->legend());
      DataSet_1D& set = static_cast<DataSet_1D&>( *ds );
      set.Resize( dimLength );
      if (NC::CheckErr(nc_get_vara(ncid_, it->VID(), start, count, (void*)(set.Yptr())))) {
        mprinterr("Error: Could not get values for set.\n");
        return 1;
      }
    }
  }

  return 0;
}

// DataIO_NetCDF::ReadData()
int DataIO_NetCDF::ReadData(FileName const& fname, DataSetList& dsl, std::string const& dsname)
{
# ifdef BINTRAJ
  if (NC::CheckErr( nc_open( fname.full(), NC_NOWRITE, &ncid_ ) != NC_NOERR )) {
    mprinterr("Error: Could not open NetCDF data file '%s'\n", fname.full());
    return 1;
  }

  // Check if we have CPPTRAJ conventions
  bool hasCpptrajConventions = (NC::GetConventions(ncid_) == NC::NC_CPPTRAJDATA);
  if (hasCpptrajConventions)
    mprintf("\tNetCDF data file has CPPTRAJ conventions.\n");

  // Get the number of dimensions, variables, attributes, and ID of the
  // unlimited dimension (if any).
  int ndimsp, nvarsp, ngattsp,unlimdimidp;
  if (NC::CheckErr( nc_inq(ncid_, &ndimsp, &nvarsp, &ngattsp, &unlimdimidp) )) {
    mprinterr("Error: Could not get NetCDF data file information.\n");
    return 1;
  }
  mprintf("DEBUG: '%s' : ndimsp=%i  nvarsp=%i  ngattsp=%i  unlimdimidp=%i\n",
          fname.full(), ndimsp, nvarsp, ngattsp, unlimdimidp);
  char varName[NC_MAX_NAME+1];
  // Get the length of all dimensions
  std::vector<unsigned int> dimLengths;
  dimLengths.reserve(ndimsp);
  for (int idim = 0; idim < ndimsp; idim++) {
    size_t diml;
    if (NC::CheckErr(nc_inq_dim(ncid_, idim, varName, &diml))) {
      mprinterr("Error: Could not get length of NetCDF data dimension %i\n", idim);
      return 1;
    }
    dimLengths.push_back( diml );
    mprintf("DEBUG:\tDimension %i - '%s' (%u)\n", idim, varName, dimLengths[idim]);
  }

  VarArray AllVars;
  AllVars.reserve( nvarsp ); 

  // Loop over all variables in the NetCDF file.
  nc_type varType = 0;            // Variable type
  int nVarDims = -1;              // # variable dimensions
  int varDimIds[NC_MAX_VAR_DIMS]; // variable dim ids
  int nVarAttributes = -1;        // number of variable attributes
  for (int ivar = 0; ivar < nvarsp; ivar++) {
    if (NC::CheckErr(nc_inq_var(ncid_, ivar, varName, &varType, &nVarDims, varDimIds, &nVarAttributes))) {
      mprinterr("Error: Could not get NetCDF data variable name %i\n", ivar);
      return 1;
    }
    mprintf("DEBUG:\tVariable %i - '%s', %i dims, %i attributes\n", ivar, varName, nVarDims, nVarAttributes);
    AllVars.push_back( NcVar(ivar, varType, varName, nVarDims, varDimIds) );
/*
    if (nVarDims == 1) {
      //if (read_1d_var(dsl, dsname, ivar, varName, varType, nVarAttributes)) return 1;
      sets_1d[ varDimIds[0] ].push_back( NcVar(ivar, varType, varName) );
    } else {
      mprintf("Warning: Variable '%s' not yet handled by CPPTRAJ.\n", varName);
    }*/
  }
/*  mprintf("DEBUG: 1D sets for each dimension ID:\n");
  for (int idim = 0; idim < ndimsp; idim++) {
    mprintf("\t%i :", idim);
    for (Iarray::const_iterator it = sets_1d[idim].begin(); it != sets_1d[idim].end(); ++it)
      mprintf("  %i (%s)", it->VID(), it->vname());
    mprintf("\n");
  }*/
  for (VarArray::const_iterator it = AllVars.begin(); it != AllVars.end(); ++it)
    mprintf("  %i (%s)\n", it->VID(), it->vname());
  
  if (read_cpptraj_vars(dsl, dsname, AllVars, dimLengths)) return 1;
  //for (int idim = 0; idim < ndimsp; idim++) {
  //  if (read_1d_var( dsl, dsname, dimLengths[idim], sets_1d[idim] )) return 1;
  //}

  nc_close( ncid_ );
  return 0;
# else  
  return 1;
# endif
}

// -----------------------------------------------------------------------------
// DataIO_NetCDF::WriteHelp()
void DataIO_NetCDF::WriteHelp()
{

}

// DataIO_NetCDF::processWriteArgs()
int DataIO_NetCDF::processWriteArgs(ArgList& argIn)
{

  return 0;
}

/** Hold a pool of pointers to DataSets in the list. They will be marked off
  * as they are used.
  */
class DataIO_NetCDF::SetPool {
  public:
    /// CONSTRUCTOR - place sets from DataSetList in this pool
    SetPool(DataSetList const& dsl) : nUsed_(0) {
      sets_.reserve( dsl.size() );
      isUsed_.assign( dsl.size(), false );
      for (DataSetList::const_iterator it = dsl.begin(); it != dsl.end(); ++it)
        sets_.push_back( *it );
    }
    /// \return set at idx
    DataSet const* Set(unsigned int idx) const { return sets_[idx]; }
    /// \return Number of sets
    unsigned int Nsets() const { return sets_.size(); }
    /// \return true if set at idx has been used
    bool IsUsed(unsigned int idx) const { return isUsed_[idx]; }
    /// \return true if all sets have been marked as used
    bool AllUsed() const { return (nUsed_ == isUsed_.size()); }
    /// Print unused sets to stdout
    void PrintUnused() const {
      for (unsigned int idx = 0; idx < sets_.size(); idx++)
        if (!isUsed_[idx]) mprintf("\tUnused: %s\n", sets_[idx]->legend());
    }
    /// Mark set at idx as used
    void MarkUsed(unsigned int idx) {
      isUsed_[idx] = true;
      nUsed_++;
    }
  private:
    std::vector<DataSet const*> sets_;
    std::vector<bool> isUsed_;
    unsigned int nUsed_;
};

/** Hold a pointer to DataSet and its original index. Used to refer back to
  * original DataSetList from sets in a SetPool.
  */
class DataIO_NetCDF::Set {
  public:
    Set(DataSet const* ds, int oidx) : ds_(ds), oidx_(oidx) {}

    DataSet const* DS() const { return ds_; }
    int OriginalIdx() const { return oidx_; }
  private:
    DataSet const* ds_;
    int oidx_;
};

#ifdef BINTRAJ
/// Tell netcdf file to end define mode
static inline int EndDefineMode(int ncid) {
  // End netcdf definitions
  if (NC::CheckErr(nc_enddef(ncid))) {
    mprinterr("NetCDF data error on ending definitions.");
    return 1;
  }
  return 0;
}

/// Tell netcdf file to enter define mode
static inline int EnterDefineMode(int ncid) {
  int err = nc_redef(ncid);
  if (err != NC_NOERR && err != NC_EINDEFINE) {
    NC::CheckErr( err );
    return 1;
  }
  return 0;
}

/// Add a string as text attribute to a variable
static inline int AddDataSetStringAtt(std::string const& str, const char* desc, int ncid, int varid)
{
  if (!str.empty()) {
    if (NC::CheckErr(nc_put_att_text(ncid, varid, desc, str.size(), str.c_str()) ))
    {  
      mprinterr("Error: Writing attribute %s.\n", desc);
      return 1;
    }
  }
  return 0;
}

/// Add an integer as integer attribute
static inline int AddDataSetIntAtt(int ival, const char* desc, int ncid, int varid)
{
  // NOTE: -1 means not set TODO define that in MetaData
  if (ival != -1) {
    if (NC::CheckErr(nc_put_att_int(ncid, varid, desc, NC_INT, 1, &ival)))
    {
      mprinterr("Error: Writing attribute %s.\n", desc);
      return 1;
    }
  }
  return 0;
}

/// Add a double as double attribute
static inline int AddDataSetDblAtt(double dval, const char* desc, int ncid, int varid)
{
  if (NC::CheckErr(nc_put_att_double(ncid, varid, desc, NC_DOUBLE, 1, &dval)))
  {
    mprinterr("Error: Writing attribute %s.\n", desc);
    return 1;
  }
  return 0;
}

/// Add DataSet metadata to a variable
static inline int AddDataSetMetaData(MetaData const& meta, int ncid, int varid)
{
  // Filename
  if (AddDataSetStringAtt(meta.Fname().Full(), "filename", ncid, varid)) return 1;
  // Name
  if (AddDataSetStringAtt(meta.Name(), "name", ncid, varid)) return 1;
  // Aspect
  if (AddDataSetStringAtt(meta.Aspect(), "aspect", ncid, varid)) return 1;
  // Legend
  if (AddDataSetStringAtt(meta.Legend(), "legend", ncid, varid)) return 1;
  // Index
  if (AddDataSetIntAtt(meta.Idx(), "index", ncid, varid)) return 1;
  // Ensemble number
  if (AddDataSetIntAtt(meta.EnsembleNum(), "ensemblenum", ncid, varid)) return 1;
  // TODO  TimeSeries?
  // Scalar type
  if (meta.ScalarType() != MetaData::UNDEFINED) {
    if (AddDataSetStringAtt(meta.TypeString(), "scalartype", ncid, varid)) return 1;
  }
  // Scalar mode
  if (meta.ScalarMode() != MetaData::UNKNOWN_MODE) {
    if (AddDataSetStringAtt(meta.ModeString(), "scalarmode", ncid, varid)) return 1;
  }

  return 0;
}

/// Add DataSet dimension to a variable with optional suffix 
static inline int AddDataSetDimension(std::string const& suffix, Dimension const& dim, int ncid, int varid)
{
  // Add dimension min and step
  std::string min, step, label;
  if (!suffix.empty()) {
    min = "min" + suffix;
    step = "step" + suffix;
    label = "label" + suffix;
  } else {
    min.assign("min");
    step.assign("step");
    label.assign("label");
  }
  if (AddDataSetDblAtt(dim.Min(),  min.c_str(),  ncid, varid)) return 1;
  if (AddDataSetDblAtt(dim.Step(), step.c_str(), ncid, varid)) return 1;
  if (AddDataSetStringAtt(dim.Label(), label.c_str(), ncid, varid)) return 1;
  return 0;
}

/// Add DataSet dimension to a variable
static inline int AddDataSetDimension(Dimension const& dim, int ncid, int varid)
{
  return AddDataSetDimension("", dim, ncid, varid);
}

/** Define dimension. Ensure name is unique by appending an index.
  * Dimension label will be '<label>.<index>'.
  * \return defined dimension ID.
  */
int DataIO_NetCDF::defineDim(std::string& dimLabel, std::string const& label,
                             unsigned int dimSize, std::string const& Legend)
{
  if (label.empty()) {
    mprinterr("Internal Error: defineDim(): label is empty.\n");
    return -1;
  }
  dimLabel.assign( label + "." + integerToString(dimIdx_++) );
  int dimId = -1;
  if (NC::CheckErr( nc_def_dim(ncid_, dimLabel.c_str(), dimSize, &dimId ))) {
    mprinterr("Error: Could not define dimension ID for '%s' (%s)\n", Legend.c_str(), dimLabel.c_str());
    return -1;
  }
  return dimId;
}

/** Write 1D X-Y Mesh data set. */
int DataIO_NetCDF::writeData_1D_xy(DataSet const* ds) {
  mprintf("DEBUG: XY set '%s'\n", ds->legend());
  // Define the dimension
  if (EnterDefineMode(ncid_)) return 1;
  std::string dimLabel;
  int dimId = defineDim( dimLabel, ds->Dim(0).Label(), ds->Size(), ds->Meta().Legend() );
  if (dimId == -1) return 1;
  int dimensionID[1];
  dimensionID[0] = dimId;
  // Define the X variable
  std::string xvarstr = dimLabel + "." + ds->Meta().PrintName() + ".X";
  int xid;
  if (NC::CheckErr(nc_def_var(ncid_, xvarstr.c_str(), NC_DOUBLE, 1, dimensionID, &xid))) {
    mprinterr("Error: Could not define X var for '%s'\n", ds->legend());
    return 1;
  }
  // Define the Y variable
  std::string yvarstr = dimLabel + "." + ds->Meta().PrintName() + ".Y";
  int yid;
  if (NC::CheckErr(nc_def_var(ncid_, yvarstr.c_str(), NC_DOUBLE, 1, dimensionID, &yid))) {
    mprinterr("Error: Could not define Y var for '%s'\n", ds->legend());
    return 1;
  }
  // Add DataSet metadata as attributes
  if (AddDataSetMetaData( ds->Meta(), ncid_, xid )) return 1;
  //if (AddDataSetMetaData( ds->Meta(), ncid_, yid )) return 1;
   // Store the description
  if (AddDataSetStringAtt( ds->description(), "description", ncid_, xid)) return 1;
  //if (AddDataSetStringAtt( ds->description(), "description", ncid_, yid)) return 1;
  // Add number of dimensions
  if (AddDataSetIntAtt( ds->Ndim(), "ndim", ncid_, xid )) return 1;
  // Store the dimension
  if (AddDataSetDimension(ds->Dim(0), ncid_, xid)) return 1;
  // Have each var refer to the other
  if (AddDataSetIntAtt( yid, "yvarid", ncid_, xid )) return 1;
  if (AddDataSetIntAtt( xid, "xvarid", ncid_, yid )) return 1;
  // End define mode
  if (EndDefineMode( ncid_ )) return 1;
  // Write the X and Y variables
  size_t start[1];
  size_t count[1];
  start[0] = 0;
  count[0] = ds->Size();

  DataSet_1D const& ds1d = static_cast<DataSet_1D const&>( *ds );
  std::vector<double> idxs; // TODO access from XYMESH directly
  idxs.reserve(ds1d.Size());
  for (unsigned int ii = 0; ii < ds1d.Size(); ii++)
    idxs.push_back( ds1d.Xcrd(ii) );
  if (NC::CheckErr(nc_put_vara(ncid_, xid, start, count, &idxs[0]))) {
    mprinterr("Error: Could not write X variable from '%s'\n", ds1d.legend());
    return 1;
  }
  if (NC::CheckErr(nc_put_vara(ncid_, yid, start, count, ds1d.DvalPtr()))) {
    mprinterr("Error: Could not write variable '%s'\n", ds1d.legend());
    return 1;
  }

  return  0;
}

/** Write 1D DataSets that share an index dimension. */
int DataIO_NetCDF::writeData_1D(DataSet const* ds, Dimension const& dim, SetArray const& sets) {
  if (ds->Type() == DataSet::PH)
    mprintf("Warning: Currently only State information saved for pH sets.\n");
  mprintf("DEBUG: Sets for dimension '%s' %f %f:", dim.label(), dim.Min(), dim.Step());
  for (SetArray::const_iterator it = sets.begin(); it != sets.end(); ++it)
    mprintf(" %s", it->DS()->legend());
  mprintf("\n");
  // Define the dimension. Ensure name is unique by appending an index.
  if (EnterDefineMode(ncid_)) return 1;
  std::string dimLabel;
  int dimId = defineDim( dimLabel, dim.Label(), ds->Size(), ds->Meta().Legend() );
  if (dimId == -1) return 1;
  //int dimensionID[NC_MAX_VAR_DIMS];
  int dimensionID[1];
  dimensionID[0] = dimId;
  
  // Define the variable(s). Names should be unique: <DimName>.<VarName>
  for (SetArray::const_iterator it = sets.begin(); it != sets.end(); ++it) {
    //isIndex = 0;
    // Choose type
    nc_type dtype;
    switch (it->DS()->Type()) {
      case DataSet::DOUBLE  :
      case DataSet::XYMESH  :          dtype = NC_DOUBLE ; break;
      case DataSet::PH      :
      case DataSet::INTEGER :          dtype = NC_INT ; break;
      case DataSet::FLOAT   :          dtype = NC_FLOAT ; break;
      case DataSet::UNSIGNED_INTEGER : dtype = NC_UINT ; break; // TODO netcdf4 only?
      default:
        mprinterr("Internal Error: Unhandled DataSet type for 1D NetCDF variable.\n");
        return 1;
    }
    std::string varName = dimLabel + "." + it->DS()->Meta().PrintName();
    if ( NC::CheckErr( nc_def_var(ncid_, varName.c_str(), dtype, 1, dimensionID, varIDptr_ + it->OriginalIdx() ) ) ) {
      mprinterr("Error: Could not define variable '%s'\n", varName.c_str());
      return 1;
    }
    // Add DataSet metadata as attributes
    if (AddDataSetMetaData( it->DS()->Meta(), ncid_, varIDs_[it->OriginalIdx()] )) return 1;
    // Add number of dimensions
    if (AddDataSetIntAtt( it->DS()->Ndim(), "ndim", ncid_, varIDs_[it->OriginalIdx()] )) return 1;
    // Add dimension min and step
    if (AddDataSetDimension( dim, ncid_, varIDs_[it->OriginalIdx()])) return 1;
    // Store the description
    if (AddDataSetStringAtt(it->DS()->description(), "description", ncid_, varIDs_[it->OriginalIdx()])) return 1;
  } // END define variable(s)
  if (EndDefineMode( ncid_ )) return 1;
  // Write the variables
  size_t start[1];
  size_t count[1];
  start[0] = 0;
  count[0] = ds->Size();
  for (SetArray::const_iterator it = sets.begin(); it != sets.end(); ++it) {
    DataSet_1D const& ds1d = static_cast<DataSet_1D const&>( *(it->DS()) );
    if (NC::CheckErr(nc_put_vara(ncid_, varIDs_[it->OriginalIdx()], start, count, ds1d.DvalPtr()))) {
      mprinterr("Error: Could not write variable '%s'\n", ds1d.legend());
      return 1;
    }
  }
  return 0;
}

/** Write a 2D set. */
int DataIO_NetCDF::writeData_2D(DataSet const* ds) {
  // Define the dimension of the underlying array. Ensure name is unique by appending an index.
  if (EnterDefineMode(ncid_)) return 1;
  DataSet_2D const& set = static_cast<DataSet_2D const&>( *ds );
  std::string label("Matrix");
  std::string dimLabel;
  int dimId = defineDim( dimLabel, label, set.Size(), set.Meta().Legend() );
  if (dimId == -1) return 1;
  int dimensionID[1];
  dimensionID[0] = dimId;
  // Choose type
  nc_type dtype;
  switch (set.Type()) {
    case DataSet::MATRIX_DBL : dtype = NC_DOUBLE ; break;
    case DataSet::MATRIX_FLT : dtype = NC_FLOAT ; break; 
    default:
      mprinterr("Internal Error: Unhandled DataSet type for 2D NetCDF variable.\n");
      return 1;
  }
  // Define the matrix variable
  std::string varName = dimLabel + "." + set.Meta().PrintName();
  int varid;
  if ( NC::CheckErr( nc_def_var(ncid_, varName.c_str(), dtype, 1, dimensionID, &varid ) ) ) {
    mprinterr("Error: Could not define matrix variable '%s'\n", varName.c_str());
    return 1;
  }
  // Add DataSet metadata as attributes
  if (AddDataSetMetaData( set.Meta(), ncid_, varid )) return 1;
  // Add number of dimensions
  if (AddDataSetIntAtt( set.Ndim(), "ndim", ncid_, varid )) return 1;
  // Add dimension min and step
  if (AddDataSetDimension( integerToString(0), set.Dim(0), ncid_, varid)) return 1;
  if (AddDataSetDimension( integerToString(1), set.Dim(1), ncid_, varid)) return 1;
  // Store rows and columns
  if (AddDataSetIntAtt( set.Ncols(), "ncols", ncid_, varid )) return 1;
  if (AddDataSetIntAtt( set.Nrows(), "nrows", ncid_, varid )) return 1;
  // Store the matrix kind
  std::string kind;
  switch (set.MatrixKind()) {
    case DataSet_2D::FULL : kind.assign("full"); break;
    case DataSet_2D::HALF : kind.assign("half"); break;
    case DataSet_2D::TRI  : kind.assign("tri"); break;
  }
  if (AddDataSetStringAtt(kind, "matrixkind", ncid_, varid)) return 1;
  // Store the description
  if (AddDataSetStringAtt(set.description(), "description", ncid_, varid)) return 1;
  // Define the diagonal vector/mass array if present
  int vectVarId = -1;
  int massVarId = -1;
  if (set.Type() == DataSet::MATRIX_DBL) {
    DataSet_MatrixDbl const& dmat = static_cast<DataSet_MatrixDbl const&>( set );
    if (dmat.Nsnapshots() > 0) {
      if (AddDataSetIntAtt( dmat.Nsnapshots(), "nsnapshots", ncid_, varid)) return 1;
    }
    if (!dmat.Vect().empty()) {
      int vectDimId = defineDim( dimLabel, "vect", dmat.Vect().size(), set.Meta().Legend() + " diagonal vector" );
      if (vectDimId == -1) return 1;
      dimensionID[0] = vectDimId;
      std::string vectName = varName + "." + "vect";
      if (NC::CheckErr(nc_def_var(ncid_, vectName.c_str(), NC_DOUBLE, 1, dimensionID, &vectVarId))) {
        mprinterr("Error: Could not define vect variable for matrix '%s'\n", set.legend());
        return 1;
      }
      // Note that there is a vect variable
      if (AddDataSetIntAtt(vectVarId, "vectid", ncid_, varid)) return 1;
    }
    if (!dmat.Mass().empty()) {
      int massDimId = defineDim( dimLabel, "mass", dmat.Mass().size(), set.Meta().Legend() + " mass" );
      if (massDimId == -1) return 1;
      dimensionID[0] = massDimId;
      std::string massName = varName + "." + "mass";
      if (NC::CheckErr(nc_def_var(ncid_, massName.c_str(), NC_DOUBLE, 1, dimensionID, &massVarId))) {
        mprinterr("Error: Could not define mass variable for matrix '%s'\n", set.legend());
        return 1;
      }
      // Note that there is a mass variable
      if (AddDataSetIntAtt(massVarId, "massid", ncid_, varid)) return 1;
    }
  }
  // END define variable
  if (EndDefineMode( ncid_ )) return 1;
  // Write the matrix 
  size_t start[1];
  size_t count[1];
  start[0] = 0;
  count[0] = set.Size();
  if (NC::CheckErr(nc_put_vara(ncid_, varid, start, count, set.MatrixPtr()))) {
    mprinterr("Error: Could not write matrix '%s'\n", set.legend());
    return 1;
  }
  if (vectVarId != -1) {
    DataSet_MatrixDbl const& dmat = static_cast<DataSet_MatrixDbl const&>( set );
    count[0] = dmat.Vect().size();
    if (NC::CheckErr(nc_put_vara(ncid_, vectVarId, start, count, &(dmat.Vect()[0])))) {
      mprinterr("Error: Could not write vect variable for matrix '%s'\n", set.legend());
      return 1;
    }
  }
  if (massVarId != -1) {
    DataSet_MatrixDbl const& dmat = static_cast<DataSet_MatrixDbl const&>( set );
    count[0] = dmat.Mass().size();
    if (NC::CheckErr(nc_put_vara(ncid_, massVarId, start, count, &(dmat.Mass()[0])))) {
      mprinterr("Error: Could not write mass variable for matrix '%s'\n", set.legend());
      return 1;
    }
  }

  return 0;
}
#endif /* BINTRAJ */

// DataIO_NetCDF::WriteData()
int DataIO_NetCDF::WriteData(FileName const& fname, DataSetList const& dsl)
{
# ifdef BINTRAJ
  ncid_ = -1;
  dimIdx_ = 0;
  varIDs_.clear();
  // TODO check existing file
  if (NC::CheckErr( nc_create( fname.full(), NC_64BIT_OFFSET, &ncid_ ) ))
    return 1;

  // Place incoming DataSets into a pool. As they are handled they will
  // be removed from the pool.
  SetPool setPool( dsl );

  varIDs_.assign( dsl.size(), -1 );
  varIDptr_ = &varIDs_[0];

  // Check our incoming data sets. Try to find common dimensions.
  for (unsigned int idx = 0; idx < setPool.Nsets(); idx++)
  {
    if (setPool.IsUsed(idx)) continue;

    DataSet const* ds = setPool.Set( idx );
    if (ds->Group() == DataSet::MATRIX_2D) {
      if (writeData_2D(ds)) {
        mprinterr("Error: matrix set write failed.\n");
        return 1;
      }
      setPool.MarkUsed( idx );
    } else if (ds->Type() == DataSet::XYMESH) {
      // ----- XY Mesh ---------------------------
      if (writeData_1D_xy(ds)) {
        mprinterr("Error: xy mesh set write failed.\n");
        return 1;
      }
      setPool.MarkUsed( idx );
    } else if (ds->Group() == DataSet::SCALAR_1D) {
      // ----- 1D scalar -------------------------
      SetArray sets(1, Set(ds, idx));
      setPool.MarkUsed( idx );
      // Group this set with others that share the same dimension and length.
      Dimension const& dim = ds->Dim(0);
      for (unsigned int jdx = idx + 1; jdx < setPool.Nsets(); jdx++)
      {
        DataSet const* ds2 = setPool.Set( jdx );
        if (ds->Size() == ds2->Size() && dim == ds2->Dim(0)) {
          sets.push_back( Set(ds2, jdx) );
          setPool.MarkUsed( jdx );
        }
      }
      if (writeData_1D(ds, dim, sets)) {
        mprinterr("Error: 1D NetCDF data set write failed.\n");
        return 1;
      }
    } else {
      mprinterr("Error: '%s' is an unhandled set type for NetCDF.\n", ds->legend());
      return 1;
    }
  } // END loop over set pool
  // Warn if for some reason we didnt use all the sets.
  if (!setPool.AllUsed()) {
    mprintf("Warning: Not all sets were used.\n");
    setPool.PrintUnused();
  }
  mprintf("DEBUG: Variable IDs:\n");
  for (unsigned int idx = 0; idx != dsl.size(); idx++)
    mprintf("\t'%s' vid= %i\n", dsl[idx]->legend(), varIDs_[idx]);

  // Attributes
  if (EnterDefineMode(ncid_)) return 1;
  //if (NC::CheckErr(nc_put_att_text(ncid_,NC_GLOBAL,"title",title.size(),title.c_str())) ) {
  //  mprinterr("Error: Writing title.\n");
  //  return 1;
  //}
  if (NC::CheckErr(nc_put_att_text(ncid_,NC_GLOBAL,"program",7,"cpptraj")) ) {
    mprinterr("Error: Writing program.\n");
    return 1;
  }
  std::string programVersion(CPPTRAJ_INTERNAL_VERSION);
  if (NC::CheckErr(nc_put_att_text(ncid_,NC_GLOBAL,"programVersion",
                                   programVersion.size(), programVersion.c_str())))
  {
    mprinterr("Error: Writing program version.\n");
    return 1;
  }
  if (NC::PutConventions(ncid_, NC::NC_CPPTRAJDATA))
    return 1;
  if (NC::CheckErr(nc_put_att_text(ncid_,NC_GLOBAL,"ConventionVersion",3,"1.0")) ) {
    mprinterr("Error: Writing NetCDF data conventions version.\n");
    return 1;
  }

  // End netcdf definitions
  if (EndDefineMode(ncid_)) {
    mprinterr("NetCDF data error on ending definitions.");
    return 1;
  }
  NC::Debug(ncid_);

  nc_close(ncid_); 
  return 0;
# else
  return 1;
# endif
}
