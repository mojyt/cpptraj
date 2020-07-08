#ifndef INC_POTENTIALFUNCTION_H
#define INC_POTENTIALFUNCTION_H
#include "PotentialTerm.h"
#include "EnergyArray.h"
#include "CharMask.h"
#include <string>
// Forward declares
class Topology;
class Frame;
class Box;
class ArgList;
/// Hold terms for additive potential.
class PotentialFunction {
  public:
    PotentialFunction() : current_(0), deg_of_freedom_(0) {}

    int AddTerm(PotentialTerm::Type, ArgList&);
    /// Set up all terms in the potential function.
    int SetupPotential(Topology const&, Box const&, std::string const&);

    int CalculateForce(Frame&);

    EnergyArray const& Energy() const { return earray_; }

    Topology* CurrentTop() const { return current_; }

    int DegreesOfFreedom() const { return deg_of_freedom_; }
  private:
    typedef std::vector<PotentialTerm*> Parray;

    Parray terms_;       ///< Array of potential function terms
    EnergyArray earray_; ///< Array of energy terms
    CharMask mask_;      ///< Selected atoms
    Topology* current_;  ///< Pointer to current topology
    int deg_of_freedom_; ///< Current degrees of freedom.
};
#endif
