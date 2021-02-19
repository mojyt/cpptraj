#include "CpptrajStdio.h"
#include "Random.h"
#include "RNG.h"
#include "RNG_Marsaglia.h"
#include "RNG_Stdlib.h"

Random_Number::RngType Random_Number::default_ = MARSAGLIAS;

void Random_Number::SetDefaultRng(RngType r) {
  default_ = r;
}

/** CONSTRUCTOR */
Random_Number::Random_Number() :
  rng_(0)
{}

/** DESTRUCTOR */
Random_Number::~Random_Number() {
  if (rng_ != 0) delete rng_;
}

/** Allocate RNG. */
void Random_Number::allocateRng() {
  if (rng_ != 0) delete rng_;
  rng_ = 0;
  switch (default_) {
    case MARSAGLIAS :
      mprintf("\tRNG: Marsaglia\n");
      rng_ = new Cpptraj::RNG_Marsaglia();
      break;
    case STDLIB     :
      mprintf("\tRNG: C stdlib\n");
      rng_ = new Cpptraj::RNG_Stdlib();
      break;
  } 
}

/** Initialize RNG. */
void Random_Number::rn_set(int seedIn) {
  allocateRng();
  rng_->Set_Seed( seedIn );
}

/** Initialize with default seed. */
void Random_Number::rn_set() {
  rng_->Set_Seed();
}

/** Generate random number. */
double Random_Number::rn_gen() {
  return rng_->Generate();
}

/** Generate pseudo-random Gaussian sequence. */
double Random_Number::rn_gauss(double d0, double d1) {
  return rng_->GenerateGauss(d0, d1);
}

/** \return true if RNG has been set up. */
bool Random_Number::IsSet() const {
  return rng_->IsSet();
}

/** \return Value of RNG seed. */
int Random_Number::Seed() const {
  return rng_->Seed();
}
