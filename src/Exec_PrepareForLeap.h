#ifndef INC_EXEC_PREPAREFORLEAP_H
#define INC_EXEC_PREPAREFORLEAP_H
#include "Exec.h"
#include <set>
#include <map>
#include <vector>
class AtomMask;
class CharMask;
class CpptrajFile;
/// Do common tasks to prepare a structure to be loaded into tleap 
class Exec_PrepareForLeap : public Exec {
  public:
    Exec_PrepareForLeap();
    void Help() const;
    DispatchObject* Alloc() const { return (DispatchObject*)new Exec_PrepareForLeap(); }
    RetType Execute(CpptrajState&, ArgList&);
  private:
    /// Hold indices for sugar
    class Sugar;

    typedef std::vector<int> Iarray;

    inline void ChangeResName(Residue&, NameType const&) const;
    inline void ChangeAtomName(Atom&, NameType const&) const;

    /// Set a reduced PDB res to glycam map when dat file not found.
    void SetGlycamPdbResMap();
    /// Load PDB res to glycam map from dat file
    int LoadGlycamPdbResMap(std::string const&);
    /// Set PDB residue names recognized by Amber FFs
    void SetPdbResNames();
    /// Load PDB residue names recognized by Amber FFs from dat file
    int LoadPdbResNames(std::string const&);

    void LeapBond(int,int,Topology const&, CpptrajFile*) const;
    int CalcStereocenterTorsion(double&, int, Topology const&, Frame const&) const;
    int CalcAnomericRefTorsion(double&, int, int, Topology const&, Frame const&, std::vector<bool> const&) const;
    int CalcAnomericTorsion(double&, int, int, int, Topology const&, Frame const&, std::vector<bool> const&) const;
    int FindRemainingChainCarbons(std::vector<int>&, int, Topology const&, int,
                                  std::vector<bool> const&) const;
    int FindSugarC1Linkages(Sugar const&, Topology&, Frame const&) const;
    /// Determine orientation around anomeric carbon
    int CalcAnomericTorsion(double&, int, int, int, Iarray const&,
                            Topology const&, Frame const&) const;
    /// Determine orientation around anomeric reference carbon
    int CalcAnomericRefTorsion(double&, int, int, int, Iarray const&,
                               Topology const&, Frame const&) const;

    static int totalPriority(Topology const&, int, int, int, std::vector<bool>&);

    double CalcChiralAtomTorsion(int, Topology const&, Frame const&, int&) const;

    Sugar IdSugarRing(int, Topology const&, Frame const&, int&);
    int ChangePdbAtomNamesToGlycam(char, Residue const&, Topology&) const;
    int IdentifySugar(Sugar const&, Topology&, Frame const&, CharMask const&, CpptrajFile*, std::set<BondType>&);
    int PrepareSugars(AtomMask&, Topology&, Frame const&, CpptrajFile*, bool);
    int FindTerByBonds(Topology&, CharMask const&) const;
    int SearchForDisulfides(double, std::string const&, std::string const&, bool,
                            Topology&, Frame const&, CpptrajFile*);
    /// \return true if residue name is recognized
    bool IsRecognizedPdbRes(NameType const&) const;
    /// \return Array of residue nums with unrecognized names
    Iarray GetUnrecognizedPdbResidues(Topology const&) const;
    /// \return Array indices of isolated unrecognized residues
    Iarray GetIsolatedUnrecognizedResidues(Topology const&, Iarray const&) const;
    /// Remove specified atoms
    int ModifyCoords(Topology&, Frame&, bool, char, std::string const&,
                     std::string const&, Iarray const&) const;
    int RemoveHydrogens(Topology&, Frame&) const;
    int DetermineHisProt(std::vector<NameType>&, std::vector<int>&, Topology const&,
                         NameType const&, NameType const&,
                         NameType const&, NameType const&, NameType const&, NameType const&) const;


    typedef std::pair<NameType, char> PairType;
    typedef std::map<NameType, char> MapType;
    MapType pdb_to_glycam_; ///< Map PDB residue names to Glycam 1 char names

    typedef std::set<NameType> SetType;
    SetType pdb_res_names_; ///< PDB residue names recognized by Amber FFs

    enum ResStatType { UNKNOWN = 0, VALIDATED, UNRECOGNIZED_SUGAR_LINKAGE, SUGAR_MISSING_C1X,
                       SUGAR_MISSING_RING_O };
    typedef std::vector<ResStatType> ResStatArray;
    ResStatArray resStat_;  ///< Contain status of each residue

    typedef std::pair<NameType, NameType> NamePairType;
    typedef std::map<NameType, NameType> NameMapType;
    /// Hold maps of pdb atom names to glycam atom names
    std::vector<NameMapType> pdb_glycam_name_maps_;

    typedef std::pair<char, int> ResIdxPairType;
    typedef std::map<char, int> ResIdxMapType;
    /// Map glycam residue chars to pdb-glycam atom name maps
    ResIdxMapType glycam_res_idx_map_;

    /// Map pdb residue names to glycam linkage residue names
    NameMapType pdb_glycam_linkageRes_map_;

    std::string leapunitname_;
    bool errorsAreFatal_;   ///< If false, try to skip errors.
    int debug_; ///< Debug level
    std::string solventResName_; ///< Solvent residue name
};
// ----- Sugar class ----------------------------------------------------------
class Exec_PrepareForLeap::Sugar {
  public:
    Sugar(int);
    Sugar(int,int,int,int,int,int,std::vector<int> const&,std::vector<bool> const&);

    int ResNum()          const { return rnum_; }
    int RingOxygenAtom()  const { return ring_oxygen_atom_; }
    int AnomericAtom()    const { return anomeric_atom_; }
    int AnomericRefAtom() const { return ano_ref_atom_; }
    int RingEndAtom()     const { return ring_end_atom_; }
    int HighestStereocenter() const { return highest_stereocenter_; }

    typedef std::vector<int>::const_iterator const_iterator;
    const_iterator ringbegin() const { return ring_atoms_.begin(); }
    const_iterator ringend()   const { return ring_atoms_.end(); }

    bool AtomIsChiral(int idx) const { return atomIsChiral_[idx]; }

    bool NotSet() const { return (ring_oxygen_atom_ == -1); }
    /// \return Number of ring atoms
    unsigned int NumRingAtoms() const;
    void PrintInfo(Topology const&) const;
  private:
    int rnum_;             ///< Residue index
    int ring_oxygen_atom_; ///< Index of the ring oxygen atom
    int anomeric_atom_;    ///< Index of the anomeric C atom (ring start)
    int ano_ref_atom_;     ///< Index of the anomeric reference C atom
    int ring_end_atom_;    ///< Index of the ring end atom
    int highest_stereocenter_; ///< Index of the highest stereocenter in the carbon chain
    std::vector<int> ring_atoms_; ///< Index of all non-oxygen ring atoms
    std::vector<bool> atomIsChiral_; ///< For each sugar residue atom, whether it is chiral or not.
};
#endif
