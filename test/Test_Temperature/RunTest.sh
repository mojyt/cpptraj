#!/bin/bash

. ../MasterTest.sh

CleanFiles temp.in T.dat T2.dat T3.dat T4.dat T.XP.agr

TESTNAME='Temperature tests'
Requires netcdf maxthreads 10

INPUT="-i temp.in"
cat > temp.in <<EOF
parm Ala10.99SB.mbondi2.parm7
trajin run0.nc
temperature out T.dat ntc 1 T_NoShake
EOF
RunCpptraj "Temperature test."
DoTest T.dat.save T.dat

cat > temp.in <<EOF
parm Ala10.99SB.mbondi2.parm7
trajin run.ntc2.nc
temperature out T2.dat ntc 2 T_ShakeH
EOF
RunCpptraj "Temperature with SHAKE on hydrogens."
DoTest T2.dat.save T2.dat

cat > temp.in <<EOF
parm Ala10.99SB.mbondi2.parm7
trajin run.ntc3.nc
# Write data with no header so they can be compared to each other.
temperature out T3.dat noheader ntc 3 T_ShakeAll update
temperature frame out T4.dat noheader
EOF
RunCpptraj "Temperature with SHAKE on all atoms."
DoTest T3.dat.save T3.dat
DoTest T3.dat.save T4.dat

UNITNAME='Temperature calc with extra points'
cat > temp.in <<EOF
parm ala3.19sb.opc.parm7
trajin pme.onstep.vel mdvel pme.onstep.vel
temperature Total ntc 2 out T.XP.agr
temperature Water ntc 2 out T.XP.agr :WAT
temperature NoXP  ntc 2 out T.XP.agr :WAT@O,H1,H2
EOF
RunCpptraj "$UNITNAME"
DoTest T.XP.agr.save T.XP.agr

EndTest
