#include <cmath> // ceil
#include "DataSet_3D.h"
#include "CpptrajStdio.h"

/** CONSTRUCTOR */
DataSet_3D::DataSet_3D() {}

/** DESTRUCTOR */
DataSet_3D::~DataSet_3D() {}

/** COPY CONSTRUCTOR */
DataSet_3D::DataSet_3D(DataSet_3D const& rhs) :
  DataSet(rhs),
  gridBin_(rhs.gridBin_)
{}

/** ASSIGNMENT */
DataSet_3D& DataSet_3D::operator=(DataSet_3D const& rhs)
{
  if (this == &rhs) return *this;
  DataSet::operator=(rhs);
  gridBin_ = rhs.gridBin_;
  return *this;
}

// DataSet_3D::Allocate_N_O_Box()
int DataSet_3D::Allocate_N_O_Box(size_t nx, size_t ny, size_t nz, 
                                 Vec3 const& oxyz, Box const& boxIn)
{
  if (nx == 0 || ny == 0 || nz == 0) {
    mprinterr("Error: One or more grid sizes are 0: %zu %zu %zu\n", nx, ny, nz);
    return 1;
  }
  // Set origin and unit cell params.
  gridBin_.Setup_O_Box(nx, ny, nz, oxyz, boxIn);
  return Allocate3D(nx, ny, nz);
}

// DataSet_3D::Allocate_N_O_D()
int DataSet_3D::Allocate_N_O_D(size_t nx, size_t ny, size_t nz,
                               Vec3 const& oxyz, Vec3 const& dxyz)
{
  if (nx == 0 || ny == 0 || nz == 0) {
    mprinterr("Error: One or more grid sizes are 0: %zu %zu %zu\n", nx, ny, nz);
    return 1;
  }
  // Set origin and spacing, calculate maximum (for binning).
  gridBin_.Setup_O_D(nx, ny, nz, oxyz, dxyz);
  return Allocate3D(nx, ny, nz);
}

// Calc_Origin()
/** For even-spaced grids, origin is center - (N/2)*spacing.
  * For odd-spaced grids, origin is center - ((N-1/2)*spacing)+half_spacing
  */
static double Calc_Origin(int N, double D) {
  int odd = N % 2;
  int half = (N - odd) / 2;
  if (odd)
    return -(((double)half * D) + (D * 0.5));
  else
    return -((double)half * D);
}

/** \return Origin coords calculated from given center coords, spacings, and # of bins. */
Vec3 DataSet_3D::calcOriginFromCenter(Vec3 const& cxyz, Vec3 const& dxyz,
                                      size_t nx, size_t ny, size_t nz)
{
  return Vec3( cxyz[0] + Calc_Origin(nx, dxyz[0]),
               cxyz[1] + Calc_Origin(ny, dxyz[1]),
               cxyz[2] + Calc_Origin(nz, dxyz[2]) );
}

// DataSet_3D::Allocate_N_C_D()
int DataSet_3D::Allocate_N_C_D(size_t nx, size_t ny, size_t nz,
                               Vec3 const& cxyz, Vec3 const& dxyz)
{
  // Calculate origin from center coordinates.
  return Allocate_N_O_D(nx, ny, nz,
                        calcOriginFromCenter(cxyz, dxyz, nx, ny, nz),
                        dxyz);
/*
  Vec3 oxyz( cxyz[0] + Calc_Origin(nx, dxyz[0]),
             cxyz[1] + Calc_Origin(ny, dxyz[1]),
             cxyz[2] + Calc_Origin(nz, dxyz[2]) );
  return Allocate_N_O_D(nx,ny,nz,oxyz,dxyz);*/
}

// DataSet_3D::Allocate_X_C_D()
int DataSet_3D::Allocate_X_C_D(Vec3 const& sizes, Vec3 const& center, Vec3 const& dxyz)
{
  // Calculate bin counts
  size_t nx = (size_t)ceil(sizes[0] / dxyz[0]);
  size_t ny = (size_t)ceil(sizes[1] / dxyz[1]);
  size_t nz = (size_t)ceil(sizes[2] / dxyz[2]);
  return Allocate_N_C_D( nx, ny, nz, center, dxyz );
}

// DataSet_3D::GridInfo()
void DataSet_3D::GridInfo() const {
  Vec3 const& oxyz = gridBin_.GridOrigin();
  mprintf("\t\t-=Grid Dims=- %8s %8s %8s\n", "X", "Y", "Z");
  mprintf("\t\t        Bins: %8zu %8zu %8zu\n", NX(), NY(), NZ());
  mprintf("\t\t      Origin: %8g %8g %8g\n", oxyz[0], oxyz[1], oxyz[2]);
  //if (gridBin_.IsOrthoGrid()) {
    mprintf("\t\t     Spacing: %8g %8g %8g\n", gridBin_.DX(), gridBin_.DY(), gridBin_.DZ());
    mprintf("\t\t      Center: %8g %8g %8g\n",
            oxyz[0] + (NX()/2)*gridBin_.DX(),
            oxyz[1] + (NY()/2)*gridBin_.DY(),
            oxyz[2] + (NZ()/2)*gridBin_.DZ());
    //mprintf("\tGrid max    : %8.3f %8.3f %8.3f\n", gridBin_.MX(), gridBin_.MY(), gridBin_.MZ());
  //} else {
    Box const& box = gridBin_.GridBox();
    mprintf("\t\tBox: %s ABC={%g %g %g} abg={%g %g %g}\n", box.CellShapeName(),
            box.Param(Box::X), box.Param(Box::Y), box.Param(Box::Z),
            box.Param(Box::ALPHA), box.Param(Box::BETA), box.Param(Box::GAMMA));
  //}
}
