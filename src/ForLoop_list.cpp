#include "ForLoop_list.h"
#include "CpptrajStdio.h"
#include "ArgList.h"
#include "FileName.h"

int ForLoop_list::SetupFor(CpptrajState& State, std::string const& expr, ArgList& argIn) {
  int Niterations = -1;
  // <var> in <string0>[,<string1>...]
  //MH.varType_ = ftype;
  // Variable name
  SetVarName( argIn.GetStringNext() );
  // Comma-separated list of strings
  std::string listArg = argIn.GetStringNext();
  if (listArg.empty()) {
    mprinterr("Error: 'for in': missing comma-separated list of strings.\n");
    return 1;
  }
  ArgList list(listArg, ",");
  if (list.Nargs() < 1) {
    mprinterr("Error: Could not parse '%s' for 'for in'\n", listArg.c_str());
    return 1;
  }
  for (int il = 0; il != list.Nargs(); il++) {
    // Check if file name expansion should occur
    if (list[il].find_first_of("*?") != std::string::npos) {
      File::NameArray files = File::ExpandToFilenames( list[il] );
      for (File::NameArray::const_iterator fn = files.begin(); fn != files.end(); ++fn)
        List_.push_back( fn->Full() );
    } else
      List_.push_back( list[il] );
  }
  Niterations = (int)List_.size();
  // Description
  std::string description( VarName() + " in " + listArg );
  SetDescription( description );
  SetNiterations( Niterations );
  return 0;
}

