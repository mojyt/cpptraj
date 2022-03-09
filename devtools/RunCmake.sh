#!/bin/bash

# Can be used to test the cmake install.
# Assumes 'git submodule update --init --recursive' has been run in top dir.

if [ -z "$CPPTRAJHOME" ] ; then
  echo "CPPTRAJHOME must be set."
  exit 1
fi
HOME=$CPPTRAJHOME

installdir=$HOME/install
if [ ! -d "$installdir" ] ; then
  mkdir $installdir
fi

CUDA=0
OPENMP=0
MPI=0
while [ ! -z "$1" ] ; do
  case "$1" in
    'cuda' ) CUDA=1 ;;
    'openmp' ) OPENMP=1 ;;
    'mpi' ) MPI=1 ;;
    'all' ) CUDA=1 ; OPENMP=1 ; MPI=1 ;;
  esac
  shift
done

COMPILER=gnu
if [ $CUDA -eq 1 ] ; then
  export BUILD_FLAGS="-DCUDA=TRUE ${BUILD_FLAGS}"
fi
if [ $OPENMP -eq 1 ] ; then
  export BUILD_FLAGS="-DOPENMP=TRUE ${BUILD_FLAGS}"
fi
if [ $MPI -eq 1 ] ; then
  export BUILD_FLAGS="-DMPI=TRUE ${BUILD_FLAGS}"
  export BUILD_FLAGS="-DPnetCDF_LIBRARY=$PNETCDF_HOME/lib/libpnetcdf.a ${BUILD_FLAGS}"
  export BUILD_FLAGS="-DPnetCDF_INCLUDE_DIR=$PNETCDF_HOME/include ${BUILD_FLAGS}"
fi
#-DNetCDF_LIBRARIES_C=$HOME/lib/libnetcdf.so -DNetCDF_INCLUDES=$HOME/include
cmake .. -DCOLOR_CMAKE_MESSAGES=FALSE \
         $BUILD_FLAGS -DCOMPILER=${COMPILER^^} -DINSTALL_HEADERS=FALSE \
         -DCMAKE_INSTALL_PREFIX=$installdir -DCMAKE_LIBRARY_PATH=$HOME/lib \
         -DPRINT_PACKAGING_REPORT=TRUE 

