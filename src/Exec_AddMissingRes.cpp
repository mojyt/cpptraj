#include "Exec_AddMissingRes.h"
#include "BufferedLine.h"
#include "CharMask.h"
#include "CpptrajStdio.h"
#include "DataSet_Coords_CRD.h"
#include "ParmFile.h"
#include "StringRoutines.h"
#include "Trajin_Single.h"
#include "Trajout_Single.h"
#include <cstdlib> // atoi, abs
#include <cstring> //strncmp
#include <cmath> //pow, sqrt

/** Get gap info from PDB */
int Exec_AddMissingRes::FindGaps(Garray& Gaps, CpptrajFile& outfile, std::string const& pdbname)
const
{
  BufferedLine infile;
  if (infile.OpenFileRead( pdbname )) {
    mprinterr("Error: Could not open '%s' for reading.\n", pdbname.c_str());
    return 1;
  }
  const char* linePtr = infile.Line();
  int inMissing = 0;
  int nmissing = 0;
  bool firstGap = true;
  bool atTheEnd = false;
  std::string lastname, lastchain;
  int lastres = 0;
  std::string currentname, currentchain;
  int currentres;
  while (linePtr != 0) {
    if (strncmp(linePtr, "REMARK", 6) == 0)
    {
      ArgList line(linePtr);
      if (line.Nargs() > 2) {
        if (inMissing == 0) {
          // MISSING section not yet encountered.
          if (line[0] == "REMARK" && line[2] == "MISSING") {
            inMissing = 1;
          }
        } else if (inMissing == 1) {
          // In MISSING, looking for start of missing residues
          if (line[0] == "REMARK" && line[2] == "M") {
            inMissing = 2;
            nmissing = 0;
            firstGap = true;
            atTheEnd = false;
          }
        } else if (inMissing == 2) {
          // Reading MISSING residues
          if (line[1] != "465") {
            atTheEnd = true;
          } else
            nmissing++;
          if (firstGap) {
            // The very first gap TODO check Gaps.empty
            Gaps.push_back( Gap(line[2], atoi(line[4].c_str()), line[3]) );
            lastname = Gaps.back().LastName();
            lastres = Gaps.back().StartRes();
            lastchain = Gaps.back().Chain();
            firstGap = false;
          } else {
            currentname = line[2];
            currentres = atoi(line[4].c_str());
            currentchain = line[3];
            if (atTheEnd || currentres - lastres > 1 || currentchain != lastchain) {
              // New sequence starting or end. Finish current.
              Gaps.back().SetStopRes(lastres);
              /*
              mprintf("  Gap %c %4s %6i to %4s %6i %6u\n",
                      Gaps.back().Chain(),
                      Gaps.back().FirstName().c_str(), Gaps.back().StartRes(),
                      Gaps.back().LastName().c_str(), Gaps.back().StopRes(),
                      Gaps.back().Nres());*/
              if (atTheEnd) {
                break;
              }
              Gaps.push_back( Gap(currentres, currentchain) );
            }
            // Continue the current sequence
            Gaps.back().AddGapRes(currentname);
            lastname = currentname;
            lastres = currentres;
            lastchain = currentchain;
          }
        } // END inMissing == 2
      } // END nargs > 2
    } // END REMARK
    linePtr = infile.Line();
  } // END while linePtr != 0

  // Printout
  for (Garray::const_iterator it = Gaps.begin(); it != Gaps.end(); ++it) {
    outfile.Printf("  Gap %c %4s %6i to %4s %6i %6u\n",
                   it->Chain(),
                   it->FirstName().c_str(), it->StartRes(),
                   it->LastName().c_str(), it->StopRes(),
                   it->Nres());
    // Print residues
    unsigned int col = 1;
    for (Gap::name_iterator name = it->nameBegin(); name != it->nameEnd(); ++name) {
      outfile.Printf("%c", Residue::ConvertResName(*name));
      col++;
      if (col > 80) {
        outfile.Printf("\n");
        col = 1;
      }
    }
    if (col > 1)
      outfile.Printf("\n");
  }
  outfile.Printf("%i missing residues.\n", nmissing);
  if (Gaps.empty()) {
    mprintf("Warning: No gaps found.\n");
  }
  return 0;
} 

/** Try to minimize using steepest descent. */
int Exec_AddMissingRes::Minimize(Topology const& topIn, Frame& frameIn, CharMask const& maskIn)
const
{
  double min_tol = 1.0E-5;

  // Output trajectory
  int iteration = 0;
  Trajout_Single trajOut;
  if (trajOut.InitTrajWrite("min.nc", ArgList(), DataSetList(), TrajectoryFile::AMBERNETCDF))
    return 1;
  if (trajOut.SetupTrajWrite((Topology*)&topIn, CoordinateInfo(), 0))
    return 1;
  if (trajOut.WriteSingle(iteration, frameIn)) return 1;

  // Selected bonds
  BondArray activeBonds;
  for (BondArray::const_iterator bnd = topIn.Bonds().begin(); bnd != topIn.Bonds().end(); ++bnd)
  {
    if (maskIn.AtomInCharMask( bnd->A1() ) ||
        maskIn.AtomInCharMask( bnd->A2() ))
    {
      //mprintf("DEBUG: Bond %i to %i\n", bnd->A1()+1, bnd->A2()+1);
      activeBonds.push_back( *bnd );
    }
  }
  // Selected atoms
  Iarray selectedAtoms;
  selectedAtoms.reserve( maskIn.Nselected() );
  for (int i = 0; i < topIn.Natom(); i++)
    if (maskIn.AtomInCharMask( i ))
      selectedAtoms.push_back( i );
  // Fake LJ coefficient, taken from CT-CT (CA-CA) interaction from an Amber ff
  //double LJA = 1043080.230000;
  //double LJB = 675.612247;
  // This makes them more like hydrogen atoms; HP-HP
  //double LJA = 201.823541;
  //double LJB = 3.560129;
  LJparmType CAtype(3.8, 10.0);
  NonbondType AB = CAtype.Combine_LB( CAtype );
  double LJA = AB.A();
  double LJB = AB.B();
  // Determine the point at which the LJ energy is 0;
  // distances less than this will be set to this point to allow
  // atoms to pass through each other.
  double ljsigma = 0.5 * pow(LJA / LJB, (1.0/6.0));
  mprintf("\tLJ energy becomes positive below %g ang.\n", ljsigma);
//  // Subtract off a bit so the energy will be a little positive
//  ljsigma -= 0.1;
//  mprintf("\tDistances below %g will be set to %g for LJ calc.\n", ljsigma, ljsigma);
//  double nbcut2 = ljsigma * ljsigma;
//  //double nbcut2 = 25.0; // 5 ang
 
  // Forces
  std::vector<Vec3> Farray(topIn.Natom(), Vec3(0.0));
  // Coordinates
  //std::vector<Vec3> Xarray;
  //Xarray.reserve( topIn.Natom() );
  //for (int at = 0; at < topIn.Natom(); at++)
  //  Xarray.push_back( Vec3(frameIn.XYZ(at)) );
  // Degrees of freedom
  double deg_of_freedom = 3 * maskIn.Nselected();
  double fnq = sqrt(deg_of_freedom);
  // Main loop for steepest descent
  //const double Rk = 1.0;
  const double dxstm = 1.0E-5;
  const double crits = 1.0E-6;
  double rms = 1.0;
  double dxst = 1.0;
  double last_e = 0.0;
  mprintf("          \t%8s %12s %12s\n", " ", "ENE", "RMS");
  while (rms > min_tol && iteration < nMinSteps_) {
    double e_total = 0.0;
    // ----- Determine bond energy and forces ----
    double E_bond = 0.0;
    for (BondArray::const_iterator bnd = activeBonds.begin(); bnd != activeBonds.end(); ++bnd)
    {
      BondParmType BP = topIn.BondParm()[ bnd->Idx() ];
      //Vec3 const& XYZ0 = Xarray[ bnd->A1() ];
      //Vec3 const& XYZ1 = Xarray[ bnd->A2() ];
      const double* XYZ0 = frameIn.XYZ( bnd->A1() );
      const double* XYZ1 = frameIn.XYZ( bnd->A2() );
      double rx = XYZ0[0] - XYZ1[0];
      double ry = XYZ0[1] - XYZ1[1];
      double rz = XYZ0[2] - XYZ1[2];
      double r2 = rx*rx + ry*ry + rz*rz;
      if (r2 > 0.0) {
        double r2inv = 1.0/r2;
        double r = sqrt(r2);
        //mprintf("DBG: %i A1=%i A2=%i R=%g\n", iteration, bnd->A1()+1, bnd->A2()+1, r);
        double rinv = r * r2inv;

        double db = r - BP.Req();
        double df = BP.Rk() * db;
        double e = df * db;
        E_bond += e;
        e_total += e;

        df *= 2.0 * rinv;

        double dfx = df * rx;
        double dfy = df * ry;
        double dfz = df * rz;

        if (maskIn.AtomInCharMask(bnd->A1())) {
          Farray[bnd->A1()][0] -= dfx;
          Farray[bnd->A1()][1] -= dfy;
          Farray[bnd->A1()][2] -= dfz;
        }

        if (maskIn.AtomInCharMask(bnd->A2())) {
          Farray[bnd->A2()][0] += dfx;
          Farray[bnd->A2()][1] += dfy;
          Farray[bnd->A2()][2] += dfz;
        }
      }
    }
    // ----- Determine VDW energy and forces -----
    // Want every atom to every selected atom.
    // We want atoms to feel each other, but not if they are too close.
    // This will allow them to pass through each other.
    double E_vdw = 0.0;
    double E_elec = 0.0;
    for (int idx = 0; idx != topIn.Natom(); idx++)
    {
      for (Iarray::const_iterator jdx = selectedAtoms.begin(); jdx != selectedAtoms.end(); ++jdx)
      {
        if (idx != *jdx)
        {
          // Ignore if idx and jdx are bonded.
          if (!topIn[idx].IsBondedTo(*jdx))
          {
            const double* XYZ0 = frameIn.XYZ( idx );
            const double* XYZ1 = frameIn.XYZ( *jdx );
            double rx = XYZ0[0] - XYZ1[0];
            double ry = XYZ0[1] - XYZ1[1];
            double rz = XYZ0[2] - XYZ1[2];
            double rij2 = rx*rx + ry*ry + rz*rz;
            if (rij2 > 0) {
              //double rij;
              //if (rij2 < nbcut2) {
              //  rij2 = nbcut2;
              //  // Make rij really big to scale down the coulomb part.
              //  rij = 99999;
              //} else
              //  rij = sqrt( rij2 );
              double rij = sqrt( rij2 );
              //double dfx = 0;
              //double dfy = 0;
              //double dfz = 0;
              // VDW
              double r2    = 1.0 / rij2;
              double r6    = r2 * r2 * r2;
              double r12   = r6 * r6;
              double f12   = LJA * r12;  // A/r^12
              double f6    = LJB * r6;   // B/r^6
              double e_vdw = f12 - f6;   // (A/r^12)-(B/r^6)
              E_vdw += e_vdw;
              e_total += e_vdw;
              // VDW force
              double fvdw = ((12*f12) - (6*f6)) * r2; // (12A/r^13)-(6B/r^7)
              double dfx = rx * fvdw;
              double dfy = ry * fvdw;
              double dfz = rz * fvdw;
              // COULOMB
              double qiqj = 1; // Give each atom charge of 1
              double e_elec = 1.0 * (qiqj / rij); // 1.0 is electrostatic constant, not really needed
              E_elec += e_elec;
              e_total += e_elec;
              // COULOMB force
              double felec = e_elec / rij; // kes * (qiqj / r) * (1/r)
              dfx += rx * felec;
              dfy += ry * felec;
              dfz += rz * felec;
              // Apply forces
              if (maskIn.AtomInCharMask(idx)) {
                Farray[idx][0] += dfx;
                Farray[idx][1] += dfy;
                Farray[idx][2] += dfz;
              }
              if (maskIn.AtomInCharMask(*jdx)) {
                Farray[*jdx][0] -= dfx;
                Farray[*jdx][1] -= dfy;
                Farray[*jdx][2] -= dfz;
              }
            } // END rij > 0
          } // END idx not bonded to jdx
        } // END idx != jdx
      } // END inner loop over jdx
    } // END outer loop over idx

    // Calculate the magnitude of the force vector.
    double sum = 0.0;
    for (std::vector<Vec3>::const_iterator FV = Farray.begin(); FV != Farray.end(); ++FV)
      sum += FV->Magnitude2();
    rms = sqrt( sum ) / fnq;
    // Adjust search step size
    if (dxst < crits) dxst = dxstm;
    dxst = dxst / 2.0;
    if (e_total < last_e) dxst = dxst * 2.4;
    double dxsth = dxst / sqrt( sum );
    last_e = e_total;
    // Update positions and reset force array.
    double* Xptr = frameIn.xAddress();
    for (int idx = 0; idx != topIn.Natom(); idx++, Xptr += 3)
    //std::vector<Vec3>::iterator FV = Farray.begin();
    //for (std::vector<Vec3>::iterator XV = Xarray.begin();
    //                                 XV != Xarray.end(); ++XV, ++FV)
    {
      //mprintf("xyz0= %g %g %g  Fxyz= %g %g %g\n", Xptr[0], Xptr[1], Xptr[2], Farray[idx][0], Farray[idx][1], Farray[idx][2]);
      Xptr[0] += Farray[idx][0] * dxsth;
      Xptr[1] += Farray[idx][1] * dxsth;
      Xptr[2] += Farray[idx][2] * dxsth;
      Farray[idx] = 0.0;
      //mprintf("xyz1= %g %g %g\n", Xptr[0], Xptr[1], Xptr[2]);
      //*XV += (*FV * dxsth);
      //*FV = 0.0;
    }
    // Write out current E.
    mprintf("Iteration:\t%8i %12.4E %12.4E EB=%12.4E EV=%12.4E EC=%12.4E\n",
            iteration, e_total, rms, E_bond, E_vdw, E_elec);
    iteration++;
    // Write out current coords
    if (trajOut.WriteSingle(iteration, frameIn)) return 1;
  } // END minimization loop
  // Final RMS error from equilibirum values
  double sumdiff2 = 0.0;
  for (BondArray::const_iterator bnd = activeBonds.begin(); bnd != activeBonds.end(); ++bnd)
  {
    BondParmType BP = topIn.BondParm()[ bnd->Idx() ];
    //Vec3 const& XYZ0 = Xarray[ bnd->A1() ];
    //Vec3 const& XYZ1 = Xarray[ bnd->A2() ];
    Vec3 XYZ0( frameIn.XYZ( bnd->A1() ) );
    Vec3 XYZ1( frameIn.XYZ( bnd->A2() ) );
    Vec3 V1_2 = XYZ0 - XYZ1;
    double r1_2 = sqrt( V1_2.Magnitude2() );
    double diff = r1_2 - BP.Req();
    sumdiff2 += (diff * diff);
    if (debug_ > 0)
        mprintf("\t\t%u to %u: D= %g  Eq= %g  Delta= %g\n",
                bnd->A1()+1, bnd->A2()+1, r1_2, BP.Req(), fabs(diff));
  }
/* 
  unsigned int idx = 0; // Index into FrameDistances
  double sumdiff2 = 0.0;
  for (unsigned int f1 = 0; f1 != nframes; f1++)
  {
    for (unsigned int f2 = f1 + 1; f2 != nframes; f2++)
    {
      Vec3 V1_2 = Xarray[f1] - Xarray[f2];
      double r1_2 = sqrt( V1_2.Magnitude2() );
      double Req = FrameDistances().GetElement(idx);
      double diff = r1_2 - Req;
      sumdiff2 += (diff * diff);
      if (debug_ > 0)
        mprintf("\t\t%u to %u: D= %g  Eq= %g  Delta= %g\n",
                f1+1, f2+1, r1_2, Req, fabs(diff));
      ++idx;
    }
  }
*/
  double rms_err = sqrt( sumdiff2 / (double)activeBonds.size() );
  mprintf("\tRMS error of final bond lengths: %g\n", rms_err);
/*
  // Write out final graph with cluster numbers.
  Iarray Nums;
  Nums.reserve( nframes );
  if (cnumvtime != 0) {
    ClusterSieve::SievedFrames const& sievedFrames = FrameDistances().FramesToCluster();
    DataSet_1D const& CVT = static_cast<DataSet_1D const&>( *cnumvtime );
    for (unsigned int n = 0; n != nframes; n++)
      Nums.push_back( (int)CVT.Dval(sievedFrames[n]) );
  } else
    for (int n = 1; n <= (int)nframes; n++)
      Nums.push_back( n );
*/
  return 0;
}

/** Write topology and frame to pdb. */
int Exec_AddMissingRes::WriteStructure(std::string const& fname, Topology* newTop, Frame const& newFrame,
                                       TrajectoryFile::TrajFormatType typeOut)
const
{
  Trajout_Single trajOut;
  if (trajOut.InitTrajWrite(fname, ArgList(), DataSetList(), typeOut))
    return 1;
  if (trajOut.SetupTrajWrite(newTop, CoordinateInfo(), 1))
    return 1;
  if (trajOut.WriteSingle(0, newFrame)) return 1;
  trajOut.EndTraj();
  return 0;
}


/// Placeholder for Residues
class Pres {
  public:
    Pres() : oresnum_(0), tresnum_(-1), chain_(' ') {}
    /// CONSTRUCTOR - Take Residue
    Pres(Residue const& res, int resnum) :
      name_(res.Name()), oresnum_(res.OriginalResNum()), tresnum_(resnum), chain_(res.ChainID())
      {}
    /// CONSTRUCTOR - Take name, number, chain
    Pres(std::string const& name, int rnum, char chain) :
      name_(name), oresnum_(rnum), tresnum_(-1), chain_(chain)
      {}
    /// First sort by chain, then by original residue number
    bool operator<(const Pres& rhs) const {
      if (chain_ == rhs.chain_)
        return (oresnum_ < rhs.oresnum_);
      else
        return (chain_ < rhs.chain_);
    }

    NameType const& Name() const { return name_; }
    int OriginalResNum()   const { return oresnum_; }
    int TopResNum()        const { return tresnum_; }
    char ChainID()         const { return chain_; }
  private:
    NameType name_;
    int oresnum_;   ///< Original (PDB) residue number.
    int tresnum_;   ///< Topology residue index; -1 if it was missing.
    char chain_;    ///< Original (PDB) chain ID.
};

/** Try to generate linear coords beteween idx0 and idx1. */
void Exec_AddMissingRes::GenerateLinearGapCoords(int idx0, int idx1, Frame& frm)
{
  Vec3 vec0( frm.XYZ(idx0) );
  Vec3 vec1( frm.XYZ(idx1) );
  vec0.Print("vec0");
  vec1.Print("vec1");
  Vec3 V10 = vec1 - vec0;
  int nsteps = idx1 - idx0;
  if (nsteps < 1) {
    mprinterr("Internal Error: GenerateLinearGapCoords: Invalid steps from %i to %i (%i)\n",
              idx0, idx1, nsteps);
    return;
  }
  mprintf("DEBUG: Generating %i steps from %i to %i\n", nsteps, idx0+1, idx1+1);
  Vec3 delta = V10 / (double)nsteps;
  double* Xptr = frm.xAddress() + ((idx0+1)*3);
  for (int i = 1; i < nsteps; i++, Xptr += 3)
  {
    Vec3 xyz = vec0 + (delta * (double)i);
    xyz.Print("xyz");
    Xptr[0] = xyz[0];
    Xptr[1] = xyz[1];
    Xptr[2] = xyz[2];
  }
}

/** Generate coords following the vector from idx0 to idx1 attached at idx1
  * for residues from startidx up to and including endidx.
  */
void Exec_AddMissingRes::GenerateLinearTerminalCoords(int idx0, int idx1, int startidx, int endidx,
                                                Frame& frm)
{
  Vec3 vec0( frm.XYZ(idx0) );
  Vec3 vec1( frm.XYZ(idx1) );
  vec0.Print("vec0");
  vec1.Print("vec1");
  // The vector from 0 to 1 will be the "step" direction
  Vec3 V10 = vec1 - vec0;
  // Normalize the vector, then scale it down a bit to avoid very long fragments.
  V10.Normalize();
  V10 *= 0.5;
  // Loop over fragment to generate coords for
  double* Xptr = frm.xAddress() + (startidx * 3);
  mprintf("DEBUG: Generating terminal extending from %i-%i for indices %i to %i\n",
          idx0+1, idx1+1, startidx+1, endidx+1);
  for (int i = startidx; i <= endidx; i++, Xptr += 3)
  {
    int idist = abs( i - idx1 );
    double delta = (double)idist;
    Vec3 step = V10 * delta;
    Vec3 xyz = vec1 + step;
    xyz.Print("xyz");
    Xptr[0] = xyz[0];
    Xptr[1] = xyz[1];
    Xptr[2] = xyz[2];
  }
}

int Exec_AddMissingRes::AssignLinearCoords(Topology const& CAtop, CharMask const& CAmissing,
                                           Frame& CAframe)
const
{
  // Try to come up with better starting coords for missing CA atoms
  int gap_start = -1;
  int prev_res = -1;
  int gap_end = -1;
  int next_res = -1;
  char current_chain = ' ';
  int final_res = CAtop.Nres() - 1;
  for (int idx = 0; idx != CAtop.Nres(); idx++) {
    if (gap_start == -1) {
      // Not in a gap yet
      if (CAmissing.AtomInCharMask(idx)) {
        // This atom is missing. Start a gap.
        gap_start = idx;
        gap_end = -1;
        current_chain = CAtop.Res(idx).ChainID();
        //mprintf("CA Gap start: %i chain= %c\n", idx + 1, current_chain);
        // Is there a previous residue in the same chain?
        prev_res = idx - 1;
        if (prev_res < 0 || 
            (prev_res > -1 && CAtop.Res(prev_res).ChainID() != current_chain))
        {
          //mprintf("  No previous residue\n");
          prev_res = -1;
        }
      }
    } else {
      // In a gap.
      // If this is the final residue or the next residue is a different chain,
      // end the gap and no next residue.
      if (idx == final_res || CAtop.Res(idx+1).ChainID() != current_chain) {
        gap_end = idx;
        next_res = -1;
      } else if (!CAmissing.AtomInCharMask(idx)) {
        // This is the first non missing res after a gap.
        gap_end = idx - 1;
        next_res = idx;
      }
      // If the gap has ended, print and reset.
      if (gap_end != -1) {
        int num_res = gap_end - gap_start + 1;
        if (prev_res > -1) num_res++;
        if (next_res > -1) num_res++;
        mprintf("CA Gap end: %i to %i (%i to %i) chain %c #res= %i\n",
                gap_start+1, gap_end+1, prev_res+1, next_res+1, current_chain, num_res);
        if (prev_res > -1 && next_res > -1) {
          GenerateLinearGapCoords(prev_res, next_res, CAframe);
        } else if (prev_res == -1) {
          // N-terminal
          GenerateLinearTerminalCoords(gap_end+2, gap_end+1, gap_start, gap_end, CAframe);
        } else if (next_res == -1) {
          // C-terminal
          GenerateLinearTerminalCoords(gap_start-2, gap_start-1, gap_start, gap_end, CAframe);
        }
        gap_start = -1;
      }
    }
  }
  return 0;
}

/** Calculate pseudo force vector at given index. Only take into account
  * atoms within a certain cutoff, i.e. the local environment.
  */
int Exec_AddMissingRes::CalcFvecAtIdx(Vec3& vecOut, Vec3& XYZ0, int tgtidx, 
                                      Topology const& CAtop, Frame const& CAframe,
                                      CharMask const& isMissing)
{
  double cut2 = 100.0; // 10 ang
  vecOut = Vec3(0.0);
  XYZ0 = Vec3( CAframe.XYZ(tgtidx) );
  double e_total = 0.0;
  for (int idx = 0; idx != CAtop.Nres(); idx++)
  {
    if (idx != tgtidx && !isMissing.AtomInCharMask(idx))
    {
      const double* XYZ1 = CAframe.XYZ( idx );
      double rx = XYZ0[0] - XYZ1[0];
      double ry = XYZ0[1] - XYZ1[1];
      double rz = XYZ0[2] - XYZ1[2];
      double rij2 = rx*rx + ry*ry + rz*rz;
      if (rij2 > 0 && rij2 < cut2) {
        double rij = sqrt( rij2 );
        // COULOMB
        double qiqj = .01; // Give each atom charge of .1
        double e_elec = 1.0 * (qiqj / rij); // 1.0 is electrostatic constant, not really needed
        e_total += e_elec;
        // COULOMB force
        double felec = e_elec / rij; // kes * (qiqj / r) * (1/r)
        vecOut[0] += rx * felec;
        vecOut[1] += ry * felec;
        vecOut[2] += rz * felec;
      } //else {
        //mprinterr("Error: Atom clash between CA %i and %i\n", tgtidx+1, idx+1);
        //return 1;
      //}
    }
  }
  vecOut.Normalize();
  return 0;
}

/** \return A vector containing residues from start up to and including end. */
static inline std::vector<int> ResiduesToSearch(int startRes, int endRes) {
  std::vector<int> residuesToSearch;
  if (startRes < endRes) {
    for (int i = startRes; i <= endRes; i++)
      residuesToSearch.push_back( i );
  } else {
    for (int i = startRes; i >= endRes; i--)
      residuesToSearch.push_back( i );
  }
  return residuesToSearch;
}

/** Calculate a guiding force connecting XYZ0 and XYZ1 */
static inline void CalcGuideForce(Vec3 const& XYZ0, Vec3 const& XYZ1, double maxDist,
                                  double Rk, Vec3& fvec0, Vec3& fvec1)
{
  // Vector from 0 to 1
  Vec3 v01 = XYZ1 - XYZ0;
  // Distance
  double r2 = v01.Magnitude2();
  double r01 = sqrt( r2 );
  // Only apply the guiding force above maxDist 
  if (r01 > maxDist) {
    // Normalize
    v01.Normalize();
    // Augment
    v01 *= Rk;
    v01.Print("guide");
    // Add
    fvec0 += v01;
    fvec1 -= v01;
  }
}
  
/** Search for coords from anchor0 to anchor1, start at start, end at end. */
int Exec_AddMissingRes::CoordSearchGap(int anchor0, int anchor1, Iarray const& residues,
                                     Topology const& CAtop, CharMask& isMissing, Frame& CAframe)
const
{
  // First calculate the force vector at anchor0
  mprintf("Anchor Residue 0: %i\n", anchor0+1);
  Vec3 XYZ0, Vec0;
  CalcFvecAtIdx(Vec0, XYZ0, anchor0, CAtop, CAframe, isMissing);
  XYZ0.Print("Anchor 0 coords");
  Vec0.Print("anchor 0 vec");
  // Calculate the force vector at anchor1
  mprintf("Anchor Residue 1: %i\n", anchor1+1);
  Vec3 XYZ1, Vec1;
  CalcFvecAtIdx(Vec1, XYZ1, anchor1, CAtop, CAframe, isMissing);
  XYZ1.Print("Anchor 1 coords");
  Vec1.Print("anchor 1 vec");
  // Guide vector
  double guidefac = 3.8;
  double guidek = 1.0;
  CalcGuideForce(XYZ0, XYZ1, guidefac, guidek, Vec0, Vec1);

  // Determine the halfway index
  int halfIdx = residues.size() / 2; 
  // N-terminal
  Iarray fromAnchor0 = ResiduesToSearch(residues.front(), residues[halfIdx-1]);
  // C-terminal
  Iarray fromAnchor1 = ResiduesToSearch(residues.back(), residues[halfIdx]);

  mprintf("DEBUG: Generating Gap residues:\n");
  mprintf("\tFrom %i:", anchor0+1);
  for (Iarray::const_iterator it = fromAnchor0.begin(); it != fromAnchor0.end(); ++it)
    mprintf(" %i", *it + 1);
  mprintf("\n");
  mprintf("\tFrom %i:", anchor1+1);
  for (Iarray::const_iterator it = fromAnchor1.begin(); it != fromAnchor1.end(); ++it)
    mprintf(" %i", *it + 1);
  mprintf("\n");

  // Loop over fragment to generate coords for
  double fac = 2.0;
  Iarray::const_iterator a0 = fromAnchor0.begin();
  Iarray::const_iterator a1 = fromAnchor1.begin();
  while (a0 != fromAnchor0.end() || a1 != fromAnchor1.end()) {
    if (a0 != fromAnchor0.end()) {
      double* Xptr = CAframe.xAddress() + (*a0 * 3);
      Vec3 xyz = XYZ0 + (Vec0 * fac);
      mprintf("  %i %12.4f %12.4f %12.4f\n", *a0+1, xyz[0], xyz[1], xyz[2]);
      Xptr[0] = xyz[0];
      Xptr[1] = xyz[1];
      Xptr[2] = xyz[2];
      // Mark as not missing
      isMissing.SelectAtom(*a0, false);
      // Update the anchor
      CalcFvecAtIdx(Vec0, XYZ0, *a0, CAtop, CAframe, isMissing);
      ++a0;
    }
    if (a1 != fromAnchor1.end()) {
      double* Xptr = CAframe.xAddress() + (*a1 * 3);
      Vec3 xyz = XYZ1 + (Vec1 * fac);
      mprintf("  %i %12.4f %12.4f %12.4f\n", *a1+1, xyz[0], xyz[1], xyz[2]);
      Xptr[0] = xyz[0];
      Xptr[1] = xyz[1];
      Xptr[2] = xyz[2];
      // Mark as not missing
      isMissing.SelectAtom(*a1, false);
      // Update the anchor
      CalcFvecAtIdx(Vec1, XYZ1, *a1, CAtop, CAframe, isMissing);
      ++a1;
    }
    CalcGuideForce(XYZ0, XYZ1, guidefac, guidek, Vec0, Vec1);
  }

  return 0;
}

/** Search for coords using anchorRes as an anchor, start at start, end at end. */
int Exec_AddMissingRes::CoordSearchTerminal(int anchorRes, int startRes, int endRes,
                                     Topology const& CAtop, CharMask& isMissing, Frame& CAframe)
const
{
  // First calculate the force vector at the anchorRes
  mprintf("Anchor Residue %i\n", anchorRes+1);
  Vec3 XYZ0, anchorVec;
  CalcFvecAtIdx(anchorVec, XYZ0, anchorRes, CAtop, CAframe, isMissing);
  XYZ0.Print("Anchor coords");
  anchorVec.Print("DEBUG: anchorVec");
  // Determine the direction
  Iarray residuesToSearch = ResiduesToSearch(startRes, endRes);
  // Loop over fragment to generate coords for
  double fac = 2.0;
  mprintf("DEBUG: Generating linear fragment extending from %i for indices %i to %i (%zu)\n",
          anchorRes+1, startRes+1, endRes+1, residuesToSearch.size());
  for (Iarray::const_iterator it = residuesToSearch.begin();
                              it != residuesToSearch.end(); ++it)
  {
    double* Xptr = CAframe.xAddress() + (*it * 3);
    Vec3 xyz = XYZ0 + (anchorVec * fac);
    mprintf("  %i %12.4f %12.4f %12.4f\n", *it+1, xyz[0], xyz[1], xyz[2]);
    Xptr[0] = xyz[0];
    Xptr[1] = xyz[1];
    Xptr[2] = xyz[2];
    //CAframe.printAtomCoord( *it );
    // Mark as not missing
    isMissing.SelectAtom(*it, false);
    // Update the anchor
    CalcFvecAtIdx(anchorVec, XYZ0, *it, CAtop, CAframe, isMissing);
    
  }

  return 0;
}
    


/** Generate coords using an energy search. */
int Exec_AddMissingRes::AssignCoordsBySearch(Topology const& newTop, Frame const& newFrame,
                                             Topology const& CAtop, Frame& CAframe,
                                             Garray const& Gaps, CharMask const& CAmissing)
const
{
  CharMask isMissing = CAmissing;
  // For each gap, try to assign better coordinates to residues
  for (Garray::const_iterator gap = Gaps.begin(); gap != Gaps.end(); ++gap)
  {
    // Locate this Gap in the new topology
    std::string maskStr0("::" + std::string(1,gap->Chain()) + "&:;" + 
                         integerToString(gap->StartRes()) + "-" + integerToString(gap->StopRes()));
    AtomMask mask0;
    if (mask0.SetMaskString( maskStr0 )) {
      mprinterr("Internal Error: Could not set up mask string %s when assigning coords by search.\n",
                maskStr0.c_str());
      return 1;
    }
    if (newTop.SetupIntegerMask( mask0, newFrame )) return 1;
    Iarray rn = newTop.ResnumsSelectedBy(mask0);
    int gapStart = rn.front();
    int gapEnd = rn.back();
    mprintf("\tGap %c %i-%i : %i-%i\n", gap->Chain(), gap->StartRes(), gap->StopRes(),
            gapStart + 1, gapEnd + 1);
    // Is there a previous residue
    int prev_res = gapStart - 1;
    if (prev_res < 0 ||
        newTop.Res(prev_res).ChainID() != newTop.Res(gapStart).ChainID())
      prev_res = -1;
    // Is there a next residue
    int next_res = gapEnd + 1;
    if (next_res == newTop.Nres() ||
        newTop.Res(next_res).ChainID() != newTop.Res(gapEnd).ChainID())
      next_res = -1;
    mprintf("\t  Prev res %i  Next res %i\n", prev_res + 1, next_res + 1);
    if (prev_res == -1 && next_res == -1) {
      mprinterr("Error: Gap is unconnected.\n");
      return 1;
    }
    if (prev_res > -1 && next_res > -1) {
      //int halfidx = rn.size() / 2;
      // N-terminal
      //CoordSearch(next_res, gapEnd, rn[halfidx], CAtop, isMissing, CAframe);
      // C-terminal
      //CoordSearch(prev_res, gapStart, rn[halfidx-1], CAtop, isMissing, CAframe);
      CoordSearchGap(prev_res, next_res, rn, CAtop, isMissing, CAframe); 
    } else if (prev_res == -1) {
      // N-terminal
      CoordSearchTerminal(next_res, gapEnd, gapStart, CAtop, isMissing, CAframe);
    } else if (next_res == -1) {
      // C-terminal
      CoordSearchTerminal(prev_res, gapStart, gapEnd, CAtop, isMissing, CAframe);
    }
    //for (Iarray::const_iterator it = rn.begin(); it != rn.end(); ++it)
    //  mprintf(" %i", *it+1);
    //mprintf("\n");
  }

  return 0;
}


/** Try to add in missing residues. */
int Exec_AddMissingRes::AddMissingResidues(DataSet_Coords_CRD* dataOut,
                                           Topology const& topIn,
                                           Frame const& frameIn,
                                           Garray const& Gaps)
{
  typedef std::set<Pres> Pset;
  Pset AllResidues;
  // First add all existing residues
  for (int rnum = 0; rnum < topIn.Nres(); ++rnum) {
    std::pair<Pset::iterator, bool> ret = AllResidues.insert( Pres(topIn.Res(rnum), rnum) );
    if (!ret.second) {
      mprinterr("Internal Error: Somehow residue %s was duplicated.\n",
                topIn.TruncResNameNum(rnum).c_str());
      return 1;
    }
  }

  // Loop over gaps; add missing residues
  for (Garray::const_iterator gap = Gaps.begin(); gap != Gaps.end(); ++gap)
  {
    mprintf("\tGap %c %i to %i\n", gap->Chain(), gap->StartRes(), gap->StopRes());
    int currentRes = gap->StartRes();
    for (Gap::name_iterator it = gap->nameBegin(); it != gap->nameEnd(); ++it, ++currentRes) {
      //mprintf("DEBUG: %s %i\n", it->c_str(), currentRes);
      std::pair<Pset::iterator, bool> ret = AllResidues.insert( Pres(*it, currentRes, gap->Chain()) );
      if (!ret.second) {
        mprinterr("Internal Error: Somehow residue %s %i in chain %c was duplicated.\n",
                  it->c_str(), currentRes, gap->Chain());
        return 1;
      }
    }
    /*
    // Start res connector mask
    std::string maskStr0("::" + std::string(1,gap->Chain()) + "&:;" + integerToString(gap->StartRes()-1));
    // Stop res connector mask
    std::string maskStr1("::" + std::string(1,gap->Chain()) + "&:;" + integerToString(gap->StopRes()+1));
    mprintf("\t  Mask0=[%s] Mask1=[%s]\n", maskStr0.c_str(), maskStr1.c_str());
    // Find start res connector
    AtomMask mask0( maskStr0 );
    if (topIn.SetupIntegerMask( mask0, coordsIn )) return 1;
    // Find stop res connector
    AtomMask mask1( maskStr1 );
    if (topIn.SetupIntegerMask( mask1, coordsIn )) return 1;
    mprintf("\t  Selected0=%i Selected1=%i\n", mask0.Nselected(), mask1.Nselected());
    */
  }

  // Print residues.
  // Count number of present atoms and missing residues.
  int nAtomsPresent = 0;
  int nResMissing = 0;
  int newIdx = 0;
  Iarray topResNumToNew;
  for (Pset::const_iterator it = AllResidues.begin(); it != AllResidues.end(); ++it, ++newIdx)
  {
    //if (debug_ > 1)
      mprintf("\t  %6s %8i %8i %8i %c\n", *(it->Name()), it->OriginalResNum(),
              it->TopResNum()+1, newIdx+1, it->ChainID());
    if (it->TopResNum() < 0)
      nResMissing++;
    else {
      nAtomsPresent += topIn.Res(it->TopResNum()).NumAtoms();
      topResNumToNew.push_back( newIdx );
    }
  }
  mprintf("\t%i atoms present, %i residues missing.\n", nAtomsPresent, nResMissing);
  mprintf("DEBUG: %6s %6s\n", "TopRes", "NewRes");
  for (unsigned int t = 0; t != topResNumToNew.size(); t++)
    mprintf("       %6i %6i\n", t+1, topResNumToNew[t]+1);

  // Create new Frame
  Frame newFrame(nAtomsPresent + nResMissing);
  newFrame.ClearAtoms();
  // Create a new topology with all residues. For missing residues, create a CA atom.
  Topology newTop;
  // Zero coord for new CA atoms
  Vec3 Zero(0.0);
  // Topology for CA atoms
  Topology CAtop;
  // Frame for CA atoms
  Frame CAframe;
  // Mask for missing CA atoms
  CharMask CAmissing;
  for (Pset::const_iterator it = AllResidues.begin(); it != AllResidues.end(); ++it)
  {
    int topResNum = it->TopResNum();
    if (topResNum < 0) {
      // This was a missing residue
      newTop.AddTopAtom( Atom("CA", "C "),
                         Residue(it->Name(), it->OriginalResNum(), ' ', it->ChainID()) );
      newFrame.AddVec3( Zero );
      // CA top
      CAtop.AddTopAtom( Atom("CA", "C "),
                        Residue(it->Name(), it->OriginalResNum(), ' ', it->ChainID()) );
      CAframe.AddVec3( Zero );
      CAmissing.AddAtom(true);
    } else {
      // This residue is present in the original PDB
      Residue const& topres = topIn.Res(it->TopResNum());
      Residue newres(topres.Name(), topres.OriginalResNum(), topres.Icode(), topres.ChainID());
      int caidx = -1;
      // Calculate the center of the residue as we go in case we need it
      Vec3 vcenter(0.0);
      for (int at = topres.FirstAtom(); at < topres.LastAtom(); at++) {
        if (topIn[at].Name() == "CA") caidx = at;
        newTop.AddTopAtom( Atom(topIn[at].Name(), topIn[at].ElementName()), newres );
        //frameIn.printAtomCoord(at);
        const double* txyz = frameIn.XYZ(at);
        newFrame.AddXYZ( txyz );
        vcenter[0] += txyz[0];
        vcenter[1] += txyz[1];
        vcenter[2] += txyz[2];
        //newFrame.printAtomCoord(newTop->Natom()-1);
      }
      // CA top
      if (caidx == -1) {
        mprintf("Warning: No CA atom found for residue %s\n", topIn.TruncResNameNum(it->TopResNum()).c_str());
        // Use the center of the residue
        vcenter /= (double)topres.NumAtoms();
        mprintf("Warning: Using center: %g %g %g\n", vcenter[0], vcenter[1], vcenter[2]);
        CAtop.AddTopAtom( Atom("CA", "C"), newres );
        CAframe.AddVec3( vcenter );
        CAmissing.AddAtom(false);
      } else {
        CAtop.AddTopAtom( Atom(topIn[caidx].Name(), topIn[caidx].ElementName()), newres );
        CAframe.AddXYZ( frameIn.XYZ(caidx) );
        CAmissing.AddAtom(false);
      }
    }
  } // END loop over all residues
  // Try to determine which frames are terminal so pdbter works since
  // we wont be able to determine molecules by bonds.
  for (int ridx = 0; ridx != newTop.Nres(); ridx++)
  {
    if (ridx + 1 == newTop.Nres() ||
        newTop.Res(ridx).ChainID() != newTop.Res(ridx+1).ChainID())
      newTop.SetRes(ridx).SetTerminal(true);
  }
  // Finish new top and write
  newTop.SetParmName("newpdb", "temp.pdb");
  newTop.CommonSetup( false ); // No molecule search
  newTop.Summary();
  if (WriteStructure("temp.pdb", &newTop, newFrame, TrajectoryFile::PDBFILE)) {
    mprinterr("Error: Write of temp.pdb failed.\n");
    return 1;
  }

  // Print info on gaps in new topology
  for (Garray::const_iterator gap = Gaps.begin(); gap != Gaps.end(); ++gap)
  {
    std::string maskStr0("::" + std::string(1,gap->Chain()) + "&:;" + 
                         integerToString(gap->StartRes()) + "-" + integerToString(gap->StopRes()));
    AtomMask mask0;
    if (mask0.SetMaskString( maskStr0 )) {
      mprinterr("Internal Error: Invalid mask string during Gap printout: '%s'\n", maskStr0.c_str());
      return 1;
    }
    if (newTop.SetupIntegerMask( mask0, newFrame )) return 1;
    Iarray rn = newTop.ResnumsSelectedBy(mask0);
    mprintf("\tGap %c %i-%i : %i-%i\n", gap->Chain(), gap->StartRes(), gap->StopRes(),
            rn.front() + 1, rn.back() + 1);
    //for (Iarray::const_iterator it = rn.begin(); it != rn.end(); ++it)
    //  mprintf(" %i", *it+1);
    //mprintf("\n");
  }

  // CA top
  // Add pseudo bonds between adjacent CA atoms in the same chain.
  BondParmType CAbond(300.0, 3.8);
  for (int cares = 1; cares < CAtop.Nres(); cares++) {
    // Since only CA atoms, residue # is atom #
    Residue const& res0 = CAtop.Res(cares - 1);
    Residue const& res1 = CAtop.Res(cares);
    if (res0.ChainID() == res1.ChainID()) {
      CAtop.AddBond(cares-1, cares, CAbond);
    }
  }
  CAtop.SetParmName("capdb", "temp.ca.mol2");
  CAtop.CommonSetup(true); // molecule search
  CAtop.Summary();
  // Write CA top
  if (WriteStructure("temp.ca.mol2", &CAtop, CAframe, TrajectoryFile::MOL2FILE)) {
    mprinterr("Error: Write of temp.ca.mol2 failed.\n");
    return 1;
  }

  // Try to assign new coords
  if (AssignCoordsBySearch(newTop, newFrame, CAtop, CAframe, Gaps, CAmissing)) {
    mprinterr("Error: Could not assign coords by search.\n");
    return 1;
  }

  // Minimize
  if (Minimize(CAtop, CAframe, CAmissing)) {
    mprinterr("Error: Minimization of CA atoms failed.\n");
    return 1;
  }

  // Transfer CA coords to newFrame
  for (int idx = 0; idx != CAtop.Nres(); idx++)
  {
    if (CAmissing.AtomInCharMask(idx)) {
      Residue const& cares = CAtop.Res(idx);
      // Select CA in newTop
      std::string maskStr0("::" + std::string(1,cares.ChainID()) + "&:;" +
                           integerToString(cares.OriginalResNum()) + "&@CA");
      AtomMask mask0;
      if (mask0.SetMaskString(maskStr0)) {
        mprinterr("Internal Error: Invalid mask string when trying to map CA back to new topology: %s\n",
                  maskStr0.c_str());
        return 1;
      }
      if (newTop.SetupIntegerMask(mask0)) return 1;
      if (mask0.Nselected() != 1) {
        mprinterr("Internal Error: When trying to find CA %i in new topology, expected 1 atom, got %i\n",
                  idx + 1, mask0.Nselected());
        return 1;
      }
      mprintf("DEBUG: CA idx %i [%s] newTop atom# %i\n", idx+1, maskStr0.c_str(), mask0[0]+1);
      double* Xptr = newFrame.xAddress() + (3*mask0[0]);
      const double* XYZ = CAframe.XYZ(idx);
      Xptr[0] = XYZ[0];
      Xptr[1] = XYZ[1];
      Xptr[2] = XYZ[2];
    }
  }

  // Set output coords
  dataOut->CoordsSetup( newTop, CoordinateInfo() );
  dataOut->AddFrame( newFrame );
                           
  return 0;
}

// Exec_AddMissingRes::Help()
void Exec_AddMissingRes::Help() const
{
  mprintf("\tpdbname <pdbname> name <setname> [out <filename>]\n"
          "\t[parmargs <parm args>] [trajargs <trajin args>]\n"
          "\t[pdbout <pdb>] [nminsteps <nmin>]\n");
}

// Exec_AddMissingRes::Execute()
Exec::RetType Exec_AddMissingRes::Execute(CpptrajState& State, ArgList& argIn)
{
  debug_ = State.Debug();
  std::string pdbname = argIn.GetStringKey("pdbname");
  if (pdbname.empty()) {
    mprinterr("Error: provide PDB name.\n");
    return CpptrajState::ERR;
  }
  mprintf("\tPDB name: %s\n", pdbname.c_str());
  CpptrajFile* outfile = State.DFL().AddCpptrajFile(argIn.GetStringKey("out"),
                                                    "AddMissingRes", DataFileList::TEXT, true);
  if (outfile==0) {
    mprinterr("Internal Error: Unable to allocate 'out' file.\n");
    return CpptrajState::ERR;
  }
  mprintf("\tOutput file: %s\n", outfile->Filename().full());
  nMinSteps_ = argIn.getKeyInt("nminsteps", 1000);
  mprintf("\t# minimization steps: %i\n", nMinSteps_);
  // Arg lists
  ArgList parmArgs;
  std::string parmArgStr = argIn.GetStringKey("parmargs");
  if (!parmArgStr.empty()) {
    parmArgs.SetList(parmArgStr, ",");
    mprintf("\tParm args: %s\n", parmArgStr.c_str());
  }
  ArgList trajArgs;
  std::string trajArgStr = argIn.GetStringKey("trajargs");
  if (!trajArgStr.empty()) {
    trajArgs.SetList(trajArgStr, ",");
    mprintf("\tTraj args: %s\n", trajArgStr.c_str());
  }
  std::string dsname = argIn.GetStringKey("name");
  if (dsname.empty())  {
    mprinterr("Error: Output set name must be specified with 'name'.\n");
    return CpptrajState::ERR;
  }
  DataSet_Coords_CRD* dataOut = (DataSet_Coords_CRD*)State.DSL().AddSet(DataSet::COORDS, dsname);
  if (dataOut == 0) {
    mprinterr("Error: Unable to allocate output coords data set.\n");
    return CpptrajState::ERR;
  }
  mprintf("\tOutput set: %s\n", dataOut->legend());

  // Find missing residues/gaps in the PDB
  Garray Gaps;
  if (FindGaps(Gaps, *outfile, pdbname)) {
    mprinterr("Error: Finding missing residues failed.\n");
    return CpptrajState::ERR;
  }
  mprintf("\tThere are %zu gaps in the PDB.\n", Gaps.size());

  // Read in topology
  ParmFile parmIn;
  Topology topIn;
  if (parmIn.ReadTopology(topIn, pdbname, parmArgs, debug_)) {
    mprinterr("Error: Read of topology from PDB failed.\n");
    return CpptrajState::ERR;
  }
  topIn.Summary();

  // Set up input trajectory
  Trajin_Single trajIn;
  if (trajIn.SetupTrajRead(pdbname, trajArgs, &topIn)) {
    mprinterr("Error: Setup of PDB for coordinates read failed.\n");
    return CpptrajState::ERR;
  }
  trajIn.PrintInfo(1);
  // Create input frame
  Frame frameIn;
  frameIn.SetupFrameV(topIn.Atoms(), trajIn.TrajCoordInfo());
  // Read input
  if (trajIn.BeginTraj()) {
    mprinterr("Error: Opening PDB for coordinates read failed.\n");
    return CpptrajState::ERR;
  }
  trajIn.GetNextFrame(frameIn);
  trajIn.EndTraj();

  // Try to add in missing residues
  if (AddMissingResidues(dataOut, topIn, frameIn, Gaps)) {
    mprinterr("Error: Attempt to add missing residues failed.\n");
    return CpptrajState::ERR;
  }

  return CpptrajState::OK;
}
