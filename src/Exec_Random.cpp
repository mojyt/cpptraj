#include "Exec_Random.h"
#include "CpptrajStdio.h"
#include "Random.h"

// Exec_Random::Help()
void Exec_Random::Help() const
{
  mprintf("\t[setdefault %s]\n", CpptrajState::RngKeywords());
  mprintf("\t[createset <name> count <#> settype {int|float01} [seed <#>]]\n");
}

// Exec_Random::Execute()
Exec::RetType Exec_Random::Execute(CpptrajState& State, ArgList& argIn)
{
  std::string setarg = argIn.GetStringKey("setdefault");
  if (!setarg.empty()) {
    if (State.ChangeDefaultRng( setarg )) return CpptrajState::ERR;
  }

  std::string dsname = argIn.GetStringKey("createset");
  if (!dsname.empty()) {
    int iseed = argIn.getKeyInt("seed", -1);
    int count = argIn.getKeyInt("count", -1);
    if (count < 1) {
      mprinterr("Error: Must specify 'count' > 0 for 'createset'\n");
      return CpptrajState::ERR;
    }
    DataFile* outfile = State.DFL().AddDataFile( argIn.GetStringKey("out"), argIn );

    Random_Number rng;
    if (rng.rn_set( iseed )) return CpptrajState::ERR;
    std::string typestr = argIn.GetStringKey("settype");
    if (typestr == "int") {
      // Create integer set
      DataSet* ds = State.DSL().AddSet( DataSet::UNSIGNED_INTEGER, MetaData(dsname) );
      if (ds == 0) return CpptrajState::ERR;
      if (outfile != 0) outfile->AddDataSet( ds );
      for (int idx = 0; idx != count; idx++) {
        unsigned int rn = rng.rn_num();
        ds->Add(idx, &rn);
      }
    } else if (typestr == "float01") {
      // Create floating point set between 0 and 1
      DataSet* ds = State.DSL().AddSet( DataSet::DOUBLE, MetaData(dsname) );
      if (ds == 0) return CpptrajState::ERR;
      if (outfile != 0) outfile->AddDataSet( ds );
      for (int idx = 0; idx != count; idx++) {
        double rn = rng.rn_gen();
        ds->Add(idx, &rn);
      }
    } else {
      mprinterr("Error: Unrecognized 'settype' for 'createset': %s\n", typestr.c_str());
      return CpptrajState::ERR;
    }
  }

  return CpptrajState::OK;
}
