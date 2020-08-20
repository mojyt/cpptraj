#include "Action_Box.h"
#include "CpptrajStdio.h"

Action_Box::Action_Box() : mode_(SET) {}

void Action_Box::Help() const {
  mprintf("\t{[x <xval>] [y <yval>] [z <zval>] {[alpha <a>] [beta <b>] [gamma <g>]\n"
          "\t [truncoct]} | nobox | auto [offset <offset>] [radii {vdw|gb|parse|none}]}\n"
          "  For each input frame, replace any box information with the information given.\n"
          "  If 'truncoct' is specified, alpha, beta, and gamma will be set to the\n"
          "  appropriate angle for a truncated octahedral box. If 'nobox' is specified,\n"
          "  all existing box information will be removed. If 'auto' is specified, an\n"
          "  orthogonal box will be set for existing atoms using the specified distance\n"
          "  offset value, ensuring specified radii (default vdw) are enclosed.\n");
}

// Action_Box::Init()
Action::RetType Action_Box::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
  // Get keywords
  if ( actionArgs.hasKey("nobox") )
    mode_ = REMOVE;
  else if (actionArgs.hasKey("auto")) {
    offset_ = actionArgs.getKeyDouble("offset", 0.0);
    if (offset_ < 0) {
      mprinterr("Error: Offset for auto must be >= 0.\n");
      return Action::ERR;
    }
    mode_ = AUTO;
    radiiMode_ = UNSPECIFIED;
    box_.SetAlpha(90.0);
    box_.SetBeta(90.0);
    box_.SetGamma(90.0);
    box_.SetX(1.0);
    box_.SetY(1.0);
    box_.SetZ(1.0);
    std::string rstr = actionArgs.GetStringKey("radii");
    if (!rstr.empty()) {
      if (rstr == "vdw")
        radiiMode_ = VDW;
      else if (rstr == "parse")
        radiiMode_ = PARSE;
      else if (rstr == "gb")
        radiiMode_ = GB;
      else if (rstr == "none")
        radiiMode_ = NONE;
      else {
        mprinterr("Error: Unrecognized radii type: %s\n", rstr.c_str());
        return Action::ERR;
      }
    }
  } else {
    mode_ = SET;
    box_.SetX( actionArgs.getKeyDouble("x", 0.0) );
    box_.SetY( actionArgs.getKeyDouble("y", 0.0) );
    box_.SetZ( actionArgs.getKeyDouble("z", 0.0) );
    box_.SetAlpha( actionArgs.getKeyDouble("alpha", 0.0) );
    box_.SetBeta(  actionArgs.getKeyDouble("beta",  0.0) );
    box_.SetGamma( actionArgs.getKeyDouble("gamma", 0.0) );
    if (actionArgs.hasKey("truncoct")) box_.SetTruncOct();
  }

  mprintf("    BOX:");
  if (mode_ == REMOVE)
    mprintf(" Removing box information.\n");
  else if (mode_ == AUTO) {
    mprintf(" Setting orthogonal box for atoms using offset of %g Ang\n", offset_);
    switch (radiiMode_) {
      case GB    : mprintf("\tUsing GB radii.\n"); break;
      case PARSE : mprintf("\tUsing PARSE radii.\n"); break;
      case VDW   : mprintf("\tUsing VDW radii.\n"); break;
      case NONE  : mprintf("\tNot using atomic radii.\n"); break;
      case UNSPECIFIED:
        mprintf("\tWill use VDW, GB, or PARSE radii if available (with that priority).\n");
        break;
    }
  } else {
    if (box_.BoxX() > 0) mprintf(" X=%.3f", box_.BoxX());
    if (box_.BoxY() > 0) mprintf(" Y=%.3f", box_.BoxY());
    if (box_.BoxZ() > 0) mprintf(" Z=%.3f", box_.BoxZ());
    if (box_.Alpha() > 0) mprintf(" A=%.3f", box_.Alpha());
    if (box_.Beta() > 0) mprintf(" B=%.3f", box_.Beta());
    if (box_.Gamma() > 0) mprintf(" G=%.3f", box_.Gamma());
    mprintf("\n");
  }
  return Action::OK;
}

// Action_Box::Setup()
Action::RetType Action_Box::Setup(ActionSetup& setup) {
  cInfo_ = setup.CoordInfo();
  if (mode_ == REMOVE) {
    mprintf("\tRemoving box info.\n");
    cInfo_.SetBox( Box() );
  } else {
    // SET, AUTO
    Box pbox( box_ );
    // Fill in missing box information from current box 
    pbox.SetMissingInfo( setup.CoordInfo().TrajBox() );
    mprintf("\tNew box type is %s\n", pbox.TypeName() );
    cInfo_.SetBox( pbox );
    // Get radii for AUTO
    if (mode_ == AUTO) {
      if (radiiMode_ == VDW && !setup.Top().Nonbond().HasNonbond()) {
        mprintf("Warning: No VDW radii in topology %s; skipping.\n", setup.Top().c_str());
        return Action::SKIP;
      }
      RadiiType modeToUse = radiiMode_;
      if (modeToUse == UNSPECIFIED) {
        // If VDW radii present, use those.
        if (setup.Top().Nonbond().HasNonbond())
          modeToUse = VDW;
        else if (setup.Top().Natom() > 0 && setup.Top()[0].GBRadius() > 0)
          modeToUse = GB;
        else
          modeToUse = PARSE;
      }
      switch (modeToUse) {
        case GB    : mprintf("\tUsing GB radii.\n"); break;
        case PARSE : mprintf("\tUsing PARSE radii.\n"); break;
        case VDW   : mprintf("\tUsing VDW radii.\n"); break;
        case UNSPECIFIED:
        case NONE:
          mprintf("\tNot using atomic radii.\n");
          break;
      }
      Radii_.clear();
      Radii_.reserve( setup.Top().Natom() );
      for (int atnum = 0; atnum != setup.Top().Natom(); ++atnum) {
        switch (modeToUse) {
          case GB   : Radii_.push_back( setup.Top()[atnum].GBRadius()    ); break;
          case PARSE: Radii_.push_back( setup.Top()[atnum].ParseRadius() ); break;
          case VDW  : Radii_.push_back( setup.Top().GetVDWradius(atnum)  ); break;
          case UNSPECIFIED:
          case NONE:
            Radii_.push_back( 0.0 );
            break;
        }
      }
    } 
  }
  setup.SetCoordInfo( &cInfo_ );
  return Action::MODIFY_TOPOLOGY;
}

// Action_Box::DoAction()
Action::RetType Action_Box::DoAction(int frameNum, ActionFrame& frm) {
  if (mode_ == REMOVE) {
    frm.ModifyFrm().SetBox( Box() );
  } else if (mode_ == AUTO) {
    Box fbox( box_ );
    int atom = 0;
    Vec3 min(frm.Frm().XYZ( atom ));
    Vec3 max(min);
    Vec3 Rmin( Radii_[atom] );
    Vec3 Rmax( Radii_[atom] );
    for (; atom != frm.Frm().Natom(); ++atom)
    {
      const double* xyz = frm.Frm().XYZ( atom );
      if (xyz[0] < min[0]) {
       min[0] = xyz[0];
       Rmin[0] = Radii_[atom];
      }
      if (xyz[0] > max[0]) {
        max[0] = xyz[0];
        Rmax[0] = Radii_[atom];
      }
      if (xyz[1] < min[1]) {
        min[1] = xyz[1];
        Rmin[1] = Radii_[atom];
      }
      if (xyz[1] > max[1]) {
        max[1] = xyz[1];
        Rmax[1] = Radii_[atom];
      }
      if (xyz[2] < min[2]) {
        min[2] = xyz[2];
        Rmin[2] = Radii_[atom];
      }
      if (xyz[2] > max[2]) {
        max[2] = xyz[2];
        Rmax[2] = Radii_[atom];
      }
    }
    //min.Print("min");
    //max.Print("max");
    //Rmin.Print("Rmin");
    //Rmax.Print("Rmax");
    min -= (Rmin + offset_);
    max += (Rmax + offset_);
    fbox.SetX(max[0] - min[0]);
    fbox.SetY(max[1] - min[1]);
    fbox.SetZ(max[2] - min[2]);
    frm.ModifyFrm().SetBox( fbox );
  } else {
    Box fbox( box_ );
    fbox.SetMissingInfo( frm.Frm().BoxCrd() );
    frm.ModifyFrm().SetBox( fbox );
  }
  return Action::MODIFY_COORDS;
}
