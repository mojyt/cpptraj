#include "PotentialTerm.h"

const char* PotentialTerm::typeStr_[] = {
  "Bond",
  "Angle",
  "SimpleNonbnod",
  "OpenMM",
  0
};

const char* PotentialTerm::TypeStr(Type typeIn) {
  return typeStr_[typeIn];
}
