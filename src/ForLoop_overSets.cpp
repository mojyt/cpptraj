#include "ForLoop_overSets.h"
#include "CpptrajStdio.h"
#include "ArgList.h"
#include "DataSetList.h"
#include "CpptrajState.h"

void ForLoop_overSets::helpText() {
  mprintf("\t<var> oversets <list>\n"
          "  Loop over data sets selected by values in comma-separated list.\n"
          "  Names may contain wildcard characters ('*' or '?').\n");
}

int ForLoop_overSets::SetupFor(CpptrajState& State, ArgList& argIn) {
  // <var> oversets <string0>[,<string1>...]
  //MH.varType_ = ftype;
  // Comma-separated list of data set names.
  std::string listArg = argIn.GetStringKey("oversets");
  if (listArg.empty()) {
    mprinterr("Error: 'for oversets': missing ' oversets <comma-separated list of names>'.\n");
    return 1;
  }
  ArgList list(listArg, ",");
  if (list.Nargs() < 1) {
    mprinterr("Error: Could not parse '%s' for 'for oversets'\n", listArg.c_str());
    return 1;
  }
  // Variable name.
  if (SetupLoopVar( State.DSL(), argIn.GetStringNext() )) return 1;
  // Go through list of names 
  for (int il = 0; il != list.Nargs(); il++) {
    DataSetList dsl = State.DSL().SelectSets( list[il] );
    if (dsl.empty()) {
      mprintf("Warning: '%s' selects no sets.\n");
    } else {
      for (DataSetList::const_iterator it = dsl.begin(); it != dsl.end(); ++it)
        List_.push_back( (*it)->Meta().PrintName() );
    }
  }
  // Description
  std::string description( "(" + VarName() + " oversets " + listArg + ")" );
  SetDescription( description );
  return 0;
}

int ForLoop_overSets::BeginFor(DataSetList const& CurrentVars) {
  sdx_ = List_.begin();
  return (int)List_.size();
}

bool ForLoop_overSets::EndFor(DataSetList& DSL) {
  if (sdx_ == List_.end()) return true;
  // Get variable value
  DSL.UpdateStringVar( VarName(), *(sdx_) );
  // Increment
  ++(sdx_);
  return false;
}
