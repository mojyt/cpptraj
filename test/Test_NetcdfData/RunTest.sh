#!/bin/bash

. ../MasterTest.sh

CleanFiles ncdata.in d1.nc d1.dat rmsf.dat

TESTNAME='NetCDF data file tests.'
Requires netcdf

INPUT='-i ncdata.in'

UNITNAME='Write basic 1D NetCDF data'
SFX='nc'
cat > ncdata.in <<EOF
parm ../tz2.parm7
trajin ../tz2.nc 1 10
distance d1 :1 :12 out d1.$SFX
angle a1 :1 :2 :3 out d1.$SFX
run
trajin ../tz2.nc 11 15
dihedral :1 :2 :3 :4 out d1.$SFX
run
clear trajin
trajin ../tz2.nc
align :2-12&!@H=
rmsf :2,4,9,11&!@H= byres out d1.$SFX 
EOF
RunCpptraj "$UNITNAME"
NcTest d1.nc.save d1.nc

UNITNAME='Read basic 1D NetCDF data'
cat > ncdata.in <<EOF
readdata d1.nc.save
list
quit
EOF
RunCpptraj "$UNITNAME"

EndTest
