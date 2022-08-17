#include "GistEntropyUtils.h"
#include "CpptrajFile.h" // DEBUG
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>



/** Compute translational and 6D distances to elements in V_XYZ_vec and V_Q_vec, store the smallest in NNd and NNs;
  *
  * For each solvent molecule defined by three elements of V_XYZ_vec (its
  * position) and four elements of V_Q_vec (its quaternion), computes the 3D
  * distance to X, Y, Z, as well as the 6D distance to X, Y, Z, W4, X4, Y4, Z4
  * (also position + quaternion). If the squared 3D distance is < NNd, replaces
  * the current value. If the squared 6D distance is < NNs, replaces the
  * current value.
  *
  * \param center water X, Y, Z
  * \param W4 water quaternion W
  * \param X4 water quaternion X
  * \param Y4 water quaternion Y
  * \param Z4 water quaternion Z
  * \param V_XYZ_vec positions of solvent molecules
  * \param V_Q_vec quaternions of solvent molecules
  * \param omit Index of molecule that should be omitted. Set to negative to include all.
  * \param NNd (IN- and OUTPUT) stores the nearest translational distance found so far.
  * \param NNs (IN- and OUTPUT) stores the nearest 6D distance found so far.
  */
void GistEntropyUtils::searchVectorsForNearestNeighbors6D(
    Vec3 center,
    float W4, float X4, float Y4, float Z4,
    const Farray& V_XYZ_vec, const Farray& V_Q_vec,
    int omit, double& NNd, double& NNs,
    CpptrajFile* debugOut)
{
  int nw_tot = V_XYZ_vec.size() / 3;
  # ifdef DEBUG_GIST
  if (int(V_Q_vec.size()) != 4 * nw_tot) {
      throw std::logic_error("Inconsistent size of coordinate and quaternion arrays in TransEntropy");
  }
  #endif
  // This seems a bit hacky, but increases calculation speed by a few percent.
  // Works because, if nw_tot is zero, the arrays never get dereferenced.
  const float* V_XYZ = &V_XYZ_vec[0];
  const float* V_Q = &V_Q_vec[0];
  for (int n1 = 0; n1 != nw_tot; ++n1)
  {
    int i1 = n1 * 3; // index into V_XYZ for n1
    double dx = (double)(center[0] - V_XYZ[i1  ]);
    double dy = (double)(center[1] - V_XYZ[i1+1]);
    double dz = (double)(center[2] - V_XYZ[i1+2]);
    double dd = dx*dx+dy*dy+dz*dz;
    if (debugOut != 0) debugOut->Printf("\t\t\tto wat %8i dd= %12.4f\n", n1, dd);

    if (dd < NNs && n1 != omit)
    {
      // NNd is always < NNs, therefore we only check dd < NNd if dd < NNs
      if (dd < NNd) { NNd = dd; }
      int q1 = n1 * 4; // index into V_Q for n1
      double rR = quaternion_angle(W4, X4, Y4, Z4, V_Q[q1], V_Q[q1+1], V_Q[q1+2], V_Q[q1+3]);
      double ds = rR*rR + dd;
      if (ds < NNs) { NNs = ds; }
    }
  }
}

/**
 * Search a 3D grid for the nearest neighbor of a molecule in 3D (translation) and 6D (translation+rotation)
 *
 * \param center coordinates of the reference molecule
 * \param W4 molecule orientation (quaternion)
 * \param X4 molecule orientation (quaternion)
 * \param Y4 molecule orientation (quaternion)
 * \param Z4 molecule orientation (quaternion)
 * \param V_XYZ vector (length N_voxels) of vectors (length N_other*3) of other molecules' positions
 * \param V_Q vector (length N_voxels) of vectors (length N_other*4) of other molecules' quaternions
 * \param grid_Nx number of grid voxels in x dimension.
 * \param grid_Ny number of grid voxels in y dimension.
 * \param grid_Nz number of grid voxels in z dimension.
 * \param grid_origin origin (xyz pos of the center of the first voxel) of the grid.
 * \param grid_spacing spacing of the grid, same in all directions.
 * \param n_layers maximum number of layers of voxels to search for nearest neighbors
 * \param omit_in_central_vox index of molecule in the central voxel to omit.
 * \return std::pair<double, double> SQUARED distances in 3D and 6D.
 */
std::pair<double, double> GistEntropyUtils::searchGridNearestNeighbors6D(
    Vec3 center,
    int vox_x, int vox_y, int vox_z,
    float W4, float X4, float Y4, float Z4,
    const std::vector<Farray>& V_XYZ, const std::vector<Farray>& V_Q,
    int grid_Nx, int grid_Ny, int grid_Nz,
//    Vec3 grid_origin,
    double grid_spacing,
    int n_layers, int omit_in_central_vox,
    CpptrajFile* debugOut)
{
  // will keep the smallest squared distance. first: trans, second: six
  std::pair<double, double>nearest(GIST_HUGE, GIST_HUGE);
//  int vox_x = int(floor((center[0] - grid_origin[0]) / grid_spacing + 0.5));
//  int vox_y = int(floor((center[1] - grid_origin[1]) / grid_spacing + 0.5));
//  int vox_z = int(floor((center[2] - grid_origin[2]) / grid_spacing + 0.5));
  if (debugOut != 0) debugOut->Printf("\tNN6D: voxel ijk = %8i %8i %8i\n", vox_x, vox_y, vox_z);

  // In the innermost voxel, we want to omit the molecule that corresponds to the current center.
  int omit = omit_in_central_vox;
  for (int layer=0; layer<=n_layers; ++layer) {
    int xmin = std::max(vox_x - layer, 0);
    int xmax = std::min(vox_x + layer, grid_Nx-1);
    int ymin = std::max(vox_y - layer, 0);
    int ymax = std::min(vox_y + layer, grid_Ny-1);
    int zmin = std::max(vox_z - layer, 0);
    int zmax = std::min(vox_z + layer, grid_Nz-1);
    for (int x = xmin; x <= xmax; ++x) {
      // track whether the voxel is on the border (outermost layer) in each dimension
      bool x_is_border = (x == vox_x-layer || x == vox_x+layer);
      for (int y = ymin; y <= ymax; ++y) {
        bool y_is_border = (y == vox_y-layer || y == vox_y+layer);
        for (int z = zmin; z <= zmax; ++z) {
          bool z_is_border = (z == vox_z-layer || z == vox_z+layer);
          // omit voxels that are contained in previous layers, i.e., that are not on the border.
          if (x_is_border || y_is_border || z_is_border) {
            int other_vox = voxel_num(x, y, z, grid_Nx, grid_Ny, grid_Nz);
            if (debugOut != 0) debugOut->Printf("\t\tto other voxel %i\n", other_vox);
            searchVectorsForNearestNeighbors6D(
              center, W4, X4, Y4, Z4, V_XYZ[other_vox], V_Q[other_vox], omit, nearest.first, nearest.second, debugOut);
          }
        }
      }
    }
    // In the first layer there is only one voxel. After that is finished, we set omit to -1 to include all molecules.
    omit = -1;
    // If the 6D distance is small enough, we can stop the search early.
    // No need to check the translational distance, since it is always smaller than the 6D distance.
    double safe_dist = grid_spacing * layer;
    safe_dist *= safe_dist;
    if (nearest.second < safe_dist) {
      break;
    }
  }
  return nearest;
}

/**
 * Correction factors of the 6-D volume for NN entropy calculation.
 * 
 * Reference: J. Chem. Theory Comput. 2016, 12, 1, 1–8
 * The values are V_approx(d) / V_exact(d), where V_exact(d) is evaluated by numerically solving
 * Eq. 11 in Python using scipy.integrate.quad, and V_approx is calculated via Eq. 12.
 * The x values are 0, 0.01, 0.02, 0.03, ...
 */
static const double SIX_CORR[] = {
    1.0,
    1.000003125455630304e+00,
    1.000012500072675614e+00,
    1.000028125369058385e+00,
    1.000050001166624902e+00,
    1.000078127848379239e+00,
    1.000112505906411098e+00,
    1.000153135942462024e+00,
    1.000200018667562141e+00,
    1.000253154902197128e+00,
    1.000312545576326873e+00,
    1.000378191729313526e+00,
    1.000450094510122900e+00,
    1.000528255177182357e+00,
    1.000612675098450088e+00,
    1.000703355751511259e+00,
    1.000800298723556470e+00,
    1.000903505711404851e+00,
    1.001012978521582220e+00,
    1.001128719070329298e+00,
    1.001250729383678539e+00,
    1.001379011597455904e+00,
    1.001513567957365458e+00,
    1.001654400819016688e+00,
    1.001801512647993553e+00,
    1.001954906019890235e+00,
    1.002114583620387078e+00,
    1.002280548245304326e+00,
    1.002452802800658294e+00,
    1.002631350302726432e+00,
    1.002816193878131701e+00,
    1.003007336763887647e+00,
    1.003204782307485665e+00,
    1.003408533966964500e+00,
    1.003618595310989514e+00,
    1.003834970018930406e+00,
    1.004057661880942476e+00,
    1.004286674798051449e+00,
    1.004522012782243179e+00,
    1.004763679956544031e+00,
    1.005011680555124576e+00,
    1.005266018923383520e+00,
    1.005526699518048517e+00,
    1.005793726907273200e+00,
    1.006067105770742875e+00,
    1.006346840899769557e+00,
    1.006632937197409650e+00,
    1.006925399678562094e+00,
    1.007224233470084052e+00,
    1.007529443810905923e+00,
    1.007841036052142814e+00,
    1.008159015657217550e+00,
    1.008483388201977471e+00,
    1.008814159374819663e+00,
    1.009151334976818859e+00,
    1.009494920921848449e+00,
    1.009844923236720149e+00,
    1.010201348061309012e+00,
    1.010564201648693761e+00,
    1.010933490365291787e+00,
    1.011309220691001931e+00,
    1.011691399219344811e+00,
    1.012080032657609596e+00,
    1.012475127827000776e+00,
    1.012876691662786710e+00,
    1.013284731214455281e+00,
    1.013699253645866660e+00,
    1.014120266235409851e+00,
    1.014547776376164334e+00,
    1.014981791576061942e+00,
    1.015422319458047618e+00,
    1.015869367760252384e+00,
    1.016322944336158995e+00,
    1.016783057154773795e+00,
    1.017249714300801910e+00,
    1.017722923974824445e+00,
    1.018202694493475446e+00,
    1.018689034289625095e+00,
    1.019181951912564443e+00,
    1.019681456028188382e+00,
    1.020187555419187930e+00,
    1.020700258985239417e+00,
    1.021219575743198993e+00,
    1.021745514827297807e+00,
    1.022278085489341182e+00,
    1.022817297098907785e+00,
    1.023363159143556578e+00,
    1.023915681229026653e+00,
    1.024474873079452619e+00,
    1.025040744537566439e+00,
    1.025613305564919475e+00,
    1.026192566242089210e+00,
    1.026778536768903516e+00,
    1.027371227464655590e+00,
    1.027970648768329998e+00,
    1.028576811238827382e+00,
    1.029189725555187618e+00,
    1.029809402516824512e+00,
    1.030435853043756067e+00,
    1.031069088176836290e+00,
    1.031709119077995895e+00,
    1.032355957030479221e+00,
    1.033009613439084706e+00,
    1.033670099830414468e+00,
    1.034337427853111224e+00,
    1.035011609278116085e+00,
    1.035692655998913692e+00,
    1.036380580031788678e+00,
    1.037075393516079025e+00,
    1.037777108714437402e+00,
    1.038485738013089188e+00,
    1.039201293922094704e+00,
    1.039923789075617444e+00,
    1.040653236232188084e+00,
    1.041389648274978486e+00,
    1.042133038212069041e+00,
    1.042883419176726667e+00,
    1.043640804427682367e+00,
    1.044405207349407227e+00,
    1.045176641452397082e+00,
    1.045955120373453617e+00,
    1.046740657875974145e+00,
    1.047533267850236705e+00,
    1.048332964313692273e+00,
    1.049139761411259419e+00,
    1.049953673415618294e+00,
    1.050774714727506831e+00,
    1.051602899876025399e+00,
    1.052438243518934335e+00,
    1.053280760442960595e+00,
    1.054130465564107721e+00,
    1.054987373927959160e+00,
    1.055851500709994673e+00,
    1.056722861215900533e+00,
    1.057601470881890160e+00,
    1.058487345275017644e+00,
    1.059380500093500599e+00,
    1.060280951167044350e+00,
    1.061188714457161897e+00,
    1.062103806057510091e+00,
    1.063026242194208715e+00,
    1.063956039226180428e+00,
    1.064893213645480285e+00,
    1.065837782077635687e+00,
    1.066789761281978999e+00,
    1.067749168151993722e+00,
    1.068716019715655996e+00,
    1.069690333135775662e+00,
    1.070672125710350198e+00,
    1.071661414872908002e+00,
    1.072658218192863444e+00,
    1.073662553375868356e+00,
    1.074674438264170417e+00,
    1.075693890836967981e+00,
    1.076720929210772226e+00,
    1.077755571639769094e+00,
    1.078797836516183883e+00,
    1.079847742370646291e+00,
    1.080905307872559007e+00,
    1.081970551830471861e+00,
    1.083043493192447748e+00,
    1.084124151046446549e+00,
    1.085212544620691277e+00,
    1.086308693284057769e+00,
    1.087412616546445943e+00,
    1.088524334059170373e+00,
    1.089643865615340435e+00,
    1.090771231150248433e+00,
    1.091906450741761958e+00,
    1.093049544610708246e+00,
    1.094200533121272745e+00,
    1.095359436781392803e+00,
    1.096526276243152687e+00,
    1.097701072303182812e+00,
    1.098883845903064538e+00,
    1.100074618129726733e+00,
    1.101273410215854121e+00,
    1.102480243540291616e+00,
    1.103695139628457111e+00,
    1.104918120152744931e+00,
    1.106149206932942830e+00,
    1.107388421936645218e+00,
    1.108635787279667273e+00,
    1.109891325226463499e+00,
    1.111155058190547607e+00,
    1.112427008734912182e+00,
    1.113707199572452344e+00,
    1.114995653566391631e+00,
    1.116292393730705435e+00,
    1.117597443230552656e+00,
    1.118910825382702035e+00,
    1.120232563655966018e+00,
    1.121562681671633310e+00,
    1.122901203203903187e+00,
    1.124248152180322702e+00,
    1.125603552682225006e+00,
    1.126967428945165439e+00,
    1.128339805359369175e+00,
    1.129720706470168645e+00,
    1.131110156978449632e+00,
    1.132508181741098463e+00,
    1.133914805771447432e+00,
    1.135330054239723330e+00,
    1.136753952473501528e+00,
    1.138186525958151840e+00,
    1.139627800337297048e+00,
    1.141077801413266313e+00,
    1.142536555147546151e+00,
    1.144004087661246061e+00,
    1.145480425235550825e+00,
    1.146965594312182146e+00,
    1.148459621493860494e+00,
    1.149962533544769849e+00,
    1.151474357391016223e+00,
    1.152995120121095729e+00,
    1.154524848986360874e+00,
    1.156063571401487966e+00,
    1.157611314944944736e+00,
    1.159168107359459743e+00,
    1.160733976552494884e+00,
    1.162308950596713464e+00,
    1.163893057730457370e+00,
    1.165486326358217140e+00,
    1.167088785051109801e+00,
    1.168700462547352270e+00,
    1.170321387752737863e+00,
    1.171951589741115685e+00,
    1.173591097754867807e+00,
    1.175239941205387995e+00,
    1.176898149673562211e+00,
    1.178565752910252007e+00,
    1.180242780836767258e+00,
    1.181929263545360431e+00,
    1.183625231299700875e+00,
    1.185330714535360874e+00,
    1.187045743860300151e+00,
    1.188770350055349256e+00,
    1.190504564074696292e+00,
    1.192248417046370745e+00,
    1.194001940272732210e+00,
    1.195765165230953331e+00,
    1.197538123573511193e+00,
    1.199320847128669376e+00,
    1.201113367900971118e+00,
    1.202915718071721152e+00,
    1.204727929999480196e+00,
    1.206550036220548350e+00,
    1.208382069449455143e+00,
    1.210224062579447812e+00,
    1.212076048682981799e+00,
    1.213938061012205916e+00,
    1.215810132999453064e+00,
    1.217692298257730732e+00,
    1.219584590581203054e+00,
    1.221487043945686635e+00,
    1.223399692509133496e+00,
    1.225322570612121575e+00,
    1.227255712778341445e+00,
    1.229199153715081261e+00,
    1.231152928313718808e+00,
    1.233117071650201790e+00,
    1.235091618985539430e+00,
    1.237076605766282755e+00,
    1.239072067625013540e+00,
    1.241078040380824588e+00,
    1.243094560039808005e+00,
    1.245121662795533268e+00,
    1.247159385029530609e+00,
    1.249207763311774189e+00,
    1.251266834401159267e+00,
    1.253336635245983155e+00,
    1.255417202984422165e+00,
    1.257508574945010782e+00,
    1.259610788647115509e+00,
    1.261723881801409153e+00,
    1.263847892310347998e+00,
    1.265982858268640099e+00,
    1.268128817963718236e+00,
    1.270285809876207761e+00,
    1.272453872680395337e+00,
    1.274633045244698559e+00,
    1.276823366632120926e+00,
    1.279024876100728347e+00,
    1.281237613104098116e+00,
    1.283461617291786316e+00,
    1.285696928509781456e+00,
    1.287943586800960549e+00,
    1.290201632405544752e+00,
    1.292471105761547223e+00,
    1.294752047505226322e+00,
    1.297044498471527918e+00,
    1.299348499694535253e+00,
    1.301664092407908146e+00,
    1.303991318045320646e+00,
    1.306330218240903340e+00,
    1.308680834829675010e+00,
    1.311043209847973845e+00,
    1.313417385533888648e+00,
    1.315803404327684056e+00,
    1.318201308872222866e+00,
    1.320611142013386807e+00,
    1.323032946800496878e+00,
    1.325466766486722126e+00,
    1.327912644529493091e+00,
    1.330370624590908379e+00,
    1.332840750538140551e+00,
    1.335323066443834028e+00,
    1.337817616586501446e+00,
    1.340324445450919555e+00,
    1.342843597728514471e+00,
    1.345375118317752472e+00,
    1.347919052324513700e+00,
    1.350475445062475854e+00,
    1.353044342053484339e+00,
    1.355625789027921302e+00,
    1.358222981368890636e+00,
    1.360848586899972457e+00,
    1.363510406995975588e+00,
    1.366213497872389659e+00,
    1.368961680944712933e+00,
    1.371757995159494481e+00,
    1.374604921293568482e+00,
    1.377504515434772925e+00,
    1.380458497198371770e+00,
    1.383468312187266624e+00,
    1.386535178422109826e+00,
    1.389660122123393293e+00,
    1.392844006057311290e+00,
    1.396087552484634786e+00,
    1.399391362063370225e+00,
    1.402755929640414623e+00,
    1.406181657598090329e+00,
    1.409668867243801360e+00,
    1.413217808609071424e+00,
    1.416828668938528102e+00,
    1.420501580087243809e+00,
    1.424236624999572598e+00,
    1.428033843408224257e+00,
    1.431893236866423891e+00,
    1.435814773205680295e+00,
    1.439798390496529912e+00,
    1.443844000574909803e+00,
    1.447951492191005718e+00,
    1.452120733823790477e+00,
    1.456351576203180009e+00,
    1.460643854570026878e+00,
    1.464997390708335789e+00,
    1.469411994769957319e+00,
    1.473887466916937727e+00,
    1.478423598792857119e+00,
    1.483020174889636111e+00,
    1.487676973676759884e+00,
    1.492393768752984018e+00,
    1.497170329785598941e+00,
    1.502006423395567003e+00,
    1.506901813948792190e+00,
    1.511856264268706562e+00,
    1.516869536277753783e+00,
    1.521941391574383351e+00,
    1.527071591951737473e+00,
    1.532259899849632401e+00,
    1.537506078839460777e+00,
    1.542809893863296589e+00,
    1.548171111702498104e+00,
    1.553589501208314294e+00,
    1.559064833580360521e+00,
    1.564596882601754224e+00,
    1.570185424847024391e+00,
    1.575830239865263938e+00,
    1.581531110340732127e+00,
    1.587287822232936740e+00,
    1.593100164898030302e+00,
    1.598967931193234104e+00,
    1.604890917565742203e+00,
    1.610868924127579982e+00,
    1.616901754717695816e+00,
    1.622989216952367419e+00,
    1.629131122265072618e+00,
    1.635327285936807096e+00,
    1.641577527117556778e+00,
    1.647881648042947900e+00,
    1.654239538026444301e+00,
    1.660650965488034370e+00,
    1.667115785919890403e+00,
    1.673633837889659581e+00,
    1.680204963821849917e+00,
    1.686829009977830029e+00,
    1.693505826431998873e+00,
    1.700235267044542642e+00,
    1.707017189431151438e+00,
    1.713851454930007234e+00,
    1.720737928566402175e+00,
    1.727676479015204603e+00,
    1.734666978561452133e+00,
    1.741709303054010061e+00,
    1.748803331890093071e+00,
    1.755948947917725711e+00,
    1.763146037445034953e+00,
    1.770394490167760626e+00,
    1.777694199128471375e+00,
    1.785045060669792516e+00,
    1.792446974386936454e+00,
    1.799899843080620565e+00,
    1.807403572708867978e+00,
    1.814958072339125206e+00,
    1.822563254100443064e+00,
    1.830219033135438433e+00,
    1.837925327552433430e+00,
    1.845682058377837276e+00,
    1.853489149508616096e+00,
    1.861346527665157069e+00,
    1.869254122344391478e+00,
    1.877211865773286581e+00,
    1.885219692862723173e+00,
    1.893277541161873856e+00,
    1.901385350812945241e+00,
    1.909543064506468379e+00,
    1.917750627437093458e+00,
    1.926007987259902521e+00,
    1.934315094047268202e+00,
    1.942671900246291106e+00,
    1.951078360636785858e+00,
    1.959534432289881334e+00,
    1.968040074527186656e+00,
    1.976595248880577360e+00,
    1.985199919052578421e+00,
    1.993854050877357897e+00,
    2.002557612282325650e+00,
    2.011310573250364442e+00,
    2.020112905782653900e+00,
    2.028964583862113980e+00,
    2.037865583417469040e+00,
    2.046815882287902344e+00,
    2.055815460188332278e+00,
    2.064864298675288090e+00,
    2.073962381113374054e+00,
    2.083109692642328259e+00,
    2.092306220144685813e+00,
    2.101551952213996710e+00,
    2.110846879123636555e+00,
    2.120190992796185281e+00,
    2.129584286773358759e+00,
    2.139026756186503064e+00,
    2.148518397727636309e+00,
    2.158059209621028707e+00,
    2.167649191595307556e+00,
    2.177288344856114666e+00,
    2.186976672059233184e+00,
    2.196714177284282421e+00,
    2.206500866008857642e+00,
    2.216336745083208992e+00,
    2.226221822705372944e+00,
    2.236156108396801123e+00,
    2.246139612978449218e+00,
    2.256172348547328621e+00,
    2.266254328453510603e+00,
    2.276385567277580790e+00,
    2.286566080808519086e+00,
    2.296795886022020561e+00,
    2.307075001059239572e+00,
    2.317403444606306984e+00,
    2.327781238872014491e+00,
    2.338208403571527150e+00,
    2.348684961902977708e+00,
    2.359210937530065610e+00,
    2.369786355162801339e+00,
    2.380411240538968798e+00,
    2.391085620405973611e+00,
    2.401809522503026262e+00,
    2.412582975543691166e+00,
    2.423406009198756816e+00,
    2.434278654079458093e+00,
    2.445200941721016541e+00,
    2.456172904566504389e+00,
    2.467194575951013213e+00,
    2.478265969535722313e+00,
    2.489387182044847435e+00,
    2.500558187746378280e+00,
    2.511779043941804179e+00,
    2.523049788199596577e+00,
    2.534370458891582079e+00,
    2.545741095179145486e+00,
    2.557161736999711277e+00,
    2.568632425053479107e+00,
    2.580153200790418211e+00,
    2.591724106397514493e+00,
    2.603345184786256539e+00,
    2.615016479580366759e+00,
    2.626738035103780788e+00,
    2.638509896368829377e+00,
    2.650332109064672981e+00,
    2.662204719545952170e+00,
    2.674127774821634773e+00,
    2.686101322544111714e+00,
    2.698125410998465146e+00,
    2.710200089091971076e+00,
    2.722325406343771625e+00,
    2.734501412874776882e+00,
    2.746728159397719082e+00,
    2.759005697207434160e+00,
    2.771334078171299176e+00,
    2.783713354719861588e+00,
    2.796143579837620408e+00,
    2.808624807054060923e+00,
    2.821157090434728065e+00,
    2.833740484572579543e+00,
    2.846375044579446012e+00,
    2.859060826077663542e+00,
    2.871797885191860189e+00,
    2.884586278540897553e+00,
    2.897426063229953552e+00,
    2.910317296842765522e+00,
    2.923260037434001202e+00,
    2.936254343521782939e+00,
    2.949300274080344231e+00,
    2.962397888532810608e+00,
    2.975547246744133734e+00,
    2.988748409014142737e+00,
    3.002001436070715013e+00,
    3.015306389063086012e+00,
    3.028663328881382011e+00,
    3.042072319519614432e+00,
    3.055533421330440369e+00,
    3.069046697757828124e+00,
    3.082612211961514692e+00,
    3.096230027484869307e+00,
    3.109900208249018583e+00,
    3.123622818547037827e+00,
    3.137397923038276915e+00,
    3.151225586742767426e+00,
    3.165105875035743033e+00,
    3.179038853642242035e+00,
    3.193024588631822702e+00,
    3.207063146413351884e+00,
    3.221154593729914417e+00,
    3.235298997653771824e+00,
    3.249496425581446690e+00,
    3.263746945228877649e+00,
    3.278050624626648979e+00,
    3.292407412103905973e+00,
    3.306817736340863068e+00,
    3.321281306250056797e+00,
    3.335798311086146661e+00,
    3.350368820384432844e+00,
    3.364992903967996885e+00,
    3.379670631943482384e+00,
    3.394402074696963645e+00,
    3.409187302889873816e+00,
    3.424026387454999654e+00,
    3.438919399592554882e+00,
    3.453866410766321948e+00,
    3.468867492699832411e+00,
    3.483922717372657019e+00,
    3.499032157016710887e+00,
    3.514195884112663482e+00,
    3.529413971386374360e+00,
    3.544686491805410178e+00,
    3.560013518575612768e+00,
    3.575395125137718733e+00,
    3.590831385164054090e+00,
    3.606322372555260447e+00,
    3.621868161437082900e+00,
    3.637468826157230328e+00,
    3.653124441282255663e+00,
    3.668835081594512548e+00,
    3.684600822089145744e+00,
    3.700421737971143710e+00,
    3.716297904652433370e+00,
    3.732229397749017519e+00,
    3.748216293078168171e+00,
    3.764258666655656338e+00,
    3.780356594693039529e+00,
    3.796510153594979009e+00,
    3.812719419956608569e+00,
    3.828984470560931275e+00,
    3.845305382376293046e+00,
    3.861682232553859340e+00,
    3.878115098425140239e+00,
    3.894604057499579053e+00,
    3.911149187462147570e+00,
    3.927750566170993274e+00,
    3.944408271655141185e+00,
    3.961122382067063796e+00,
    3.977892975906072159e+00,
    3.994720131564883481e+00,
    4.011603927778676137e+00,
    4.028544443397261254e+00,
    4.045541757428269491e+00,
    4.062595949034912834e+00,
    4.079707097534015503e+00,
    4.096875282393999562e+00,
    4.114100583232901620e+00,
    4.131383079816433934e+00,
    4.148722852056055288e+00,
    4.166119980007081836e+00,
    4.183574543866852125e+00,
    4.201086623972861922e+00,
    4.218656300791127300e+00,
    4.236283654963701295e+00,
    4.253968767208317736e+00,
    4.271711718415286185e+00,
    4.289512589596488290e+00,
    4.307371461893563236e+00,
    4.325288416576262840e+00,
    4.343263535040843948e+00,
    4.361296898808458167e+00,
    4.379388589523595776e+00,
    4.397538688952515429e+00,
    4.415747278981729806e+00,
    4.434014441616519697e+00,
    4.452340258979425869e+00,
    4.470724813308804002e+00,
    4.489168186957383178e+00,
    4.507670462390859889e+00,
    4.526231722186500050e+00,
    4.544852049031744556e+00,
    4.563531525722881454e+00,
    4.582270235163701244e+00,
    4.601068260364179707e+00,
    4.619925684439182945e+00,
    4.638842590607186622e+00,
    4.657819062189028081e+00,
    4.676855182606646011e+00,
    4.695951035381886740e+00,
    4.715106704135263449e+00,
    4.734322272584806868e+00,
    4.753597824544857353e+00,
    4.772933443924933350e+00,
    4.792329214728610509e+00,
    4.811785221052311989e+00,
    4.831301547084351000e+00,
    4.850878277103713998e+00,
    4.870515495479017964e+00,
    4.890213286667504988e+00,
    4.909971735213913391e+00,
    4.929790925749522046e+00,
    4.949670942991082789e+00,
    4.969611871739847864e+00,
    4.989613796880574270e+00,
    5.009676803380552990e+00,
    5.029800976288638203e+00,
    5.049986400734313818e+00,
    5.070233161926749332e+00,
    5.090541345153893893e+00,
    5.110911035781547263e+00,
    5.131342319252479633e+00,
    5.151835281085541673e+00,
    5.172390006874802992e+00,
    5.193006582288671957e+00,
    5.213685040199649734e+00,
    5.234425625030590723e+00,
    5.255228264059654819e+00,
    5.276093096113730674e+00,
    5.297020207220492161e+00,
    5.318009683477068705e+00,
    5.339061611049229938e+00,
    5.360176076170191095e+00,
    5.381353165142075845e+00,
    5.402592964330681369e+00,
    5.423895560169243346e+00,
    5.445261039155445459e+00,
    5.466689487851153828e+00,
    5.488180992881694920e+00,
    5.509735640935177869e+00,
    5.531353518761773280e+00,
    5.553034713173052417e+00,
    5.574779311041303309e+00,
    5.596587399298855736e+00,
    5.618459064937446179e+00,
    5.640394395007554351e+00,
    5.662393476617755717e+00,
    5.684456396934119304e+00,
    5.706583243179556675e+00,
    5.728774102633214405e+00,
    5.751029062629870126e+00,
    5.773348210559333005e+00,
    5.795731633865851329e+00,
    5.818179420047520090e+00,
    5.840691656655719655e+00,
    5.863268431294535787e+00,
    5.885909831620202759e+00,
    5.908615945340543796e+00,
    5.931386860214422185e+00,
    5.954222664051207481e+00,
    5.977123444710241706e+00,
    6.000089290100299344e+00,
    6.023120288179088178e+00,
    6.046216526952711057e+00,
    6.069378094475170293e+00,
    6.092605078847881828e+00,
    6.115897568219142322e+00,
    6.139255650783684182e+00,
    6.162679414782151532e+00,
    6.186168948500665010e+00,
    6.209724340270311949e+00,
    6.233345678466704953e+00,
    6.257033051509512944e+00,
    6.280786547862024172e+00,
    6.304606256030677258e+00,
    6.328492264564623326e+00,
    6.352444662055301450e+00,
    6.376463537135991011e+00,
    6.400548978481398699e+00,
    6.424701074807230405e+00,
    6.448919914869769343e+00,
    6.473205587465481692e+00,
    6.497558181430600044e+00,
    6.521977785640715730e+00,
    6.546464489010397791e+00,
    6.571018380492792410e+00,
    6.595639549079232999e+00,
    6.620328083798868946e+00,
    6.645084073718273920e+00,
    6.669907607941087058e+00,
    6.694798775607643471e+00,
    6.719757665894589671e+00,
    6.744784368014546061e+00,
    6.769878971215757879e+00,
    6.795041564781699073e+00,
    6.820272238030786305e+00,
    6.845571080315991708e+00,
    6.870938181024518698e+00,
    6.896373629577464470e+00,
    6.921877515429497585e+00,
    6.947449928068514247e+00,
    6.973090957015322111e+00,
    6.998800691823338305e+00,
    7.024579222078243035e+00,
    7.050426637397690932e+00,
    7.076343027430979760e+00,
    7.102328481858775966e+00,
    7.128383090392789612e+00,
    7.154506942775470613e+00,
    7.180700128779747615e+00,
    7.206962738208696706e+00,
    7.233294860895295386e+00,
    7.259696586702081511e+00,
    7.286168005520927693e+00,
    7.312709207272749090e+00,
    7.339320281907180998e+00,
    7.366001319402394998e+00,
    7.392752409764746346e+00,
    7.419573643028555487e+00,
    7.446464946347679081e+00,
    7.473426898536014384e+00,
    7.500459100985710670e+00,
    7.527561806748458650e+00,
    7.554735105994450528e+00,
    7.581979088920308030e+00,
    7.609293845748831053e+00,
    7.636679466728722332e+00,
    7.664136042134419569e+00,
    7.691663662265789014e+00,
    7.719262417447906977e+00,
    7.746932398030861755e+00,
    7.774673694389475642e+00,
    7.802486396923117518e+00,
    7.830370596055454158e+00,
    7.858326382234233520e+00,
    7.886353845931075135e+00,
    7.914453077641242729e+00,
    7.942624167883427511e+00,
    7.970867207199535898e+00,
    7.999182286154498556e+00,
    8.027569495336031480e+00,
    8.056028925354445036e+00,
    8.084560666842465437e+00,
    8.113164810454980724e+00,
    8.141841446868877341e+00,
    8.170590666782846512e+00,
    8.199412560917167525e+00,
    8.228307220013538981e+00,
    8.257274734834849639e+00,
    8.286315196165059405e+00,
    8.315428694808923993e+00,
    8.344615321591925650e+00,
    8.373875167359820182e+00,
    8.403208322978963807e+00,
    8.432614879335609714e+00,
    8.462094927336002215e+00,
    8.491648557906163575e+00,
    8.521275861991682632e+00,
    8.550976930557578015e+00,
    8.580751854588124061e+00,
    8.610600725086651863e+00,
    8.640523633075426702e+00,
    8.670520669595457974e+00,
    8.700591925705285945e+00,
    8.730737492486086992e+00,
    8.760957461031011917e+00,
    8.791251922455479928e+00,
    8.821620967891870890e+00,
    8.852064683772741915e+00,
    8.882583175418647059e+00,
    8.913176519862126668e+00,
    8.943844813023410723e+00,
    8.974588146122320254e+00,
    9.005406610395761291e+00,
    9.036300297097527690e+00,
    9.067269297498173231e+00,
    9.098313702884876619e+00,
    9.129433604561297599e+00,
    9.160629093847443727e+00,
    9.191900262079519379e+00,
    9.223247200609787200e+00,
    9.254670000806452634e+00,
    9.286168754053516494e+00,
    9.317743551750645281e+00,
    9.349394485313043290e+00,
    9.381121646171303396e+00,
    9.412925125771337775e+00,
    9.444805015574162965e+00,
    9.476761407055837694e+00,
    9.508794391707317217e+00,
    9.540904061034332528e+00,
    9.573090506557287327e+00,
    9.605353819811099925e+00,
    9.637694092345094887e+00,
    9.670111415722915993e+00,
    9.702605881522362807e+00,
    9.735177581335310748e+00,
    9.767826606767593844e+00,
    9.800553049438846642e+00,
    9.833357000982445584e+00,
    9.866238553045370452e+00,
    9.899197797288096012e+00,
    9.932234825384510302e+00,
    9.965349729021751202e+00,
    9.998542599900135386e+00,
    1.003181352973307128e+01,
    1.006516261024688674e+01,
    1.009858993004480787e+01,
    1.013209559028679152e+01,
    1.016567967332945521e+01,
    1.019934227408596428e+01,
    1.023308348434593285e+01,
    1.026690339591133672e+01,
    1.030080210059639612e+01,
    1.033477969022747800e+01,
    1.036883625664302677e+01,
    1.040297189169344705e+01,
    1.043718668724099530e+01,
    1.047148073515972655e+01,
    1.050585412733535406e+01,
    1.054030695566520137e+01,
    1.057483931205809391e+01,
    1.060945128843425955e+01,
    1.064414297672522380e+01,
    1.067891446887377427e+01,
    1.071376585683383098e+01,
    1.074869723257037535e+01,
    1.078370868805935423e+01,
    1.081880031528759467e+01,
    1.085397220625302950e+01,
    1.088922445296313946e+01,
    1.092455714743777051e+01,
    1.095997038170618865e+01,
    1.099546424780839082e+01,
    1.103103883779478878e+01,
    1.106669424372607935e+01,
    1.110243055767321962e+01,
    1.113824787171730435e+01,
    1.117414627794951087e+01,
    1.121012586847099257e+01,
    1.124618673539285041e+01,
    1.128232897083603348e+01,
    1.131855266693124129e+01,
    1.135485791581889536e+01,
    1.139124480964903263e+01,
    1.142771344058122907e+01,
    1.146426390078456770e+01,
    1.150089628243750361e+01,
    1.153761067772786575e+01,
    1.157440717885272896e+01,
    1.161128587801836431e+01,
    1.164824686744019289e+01,
    1.168529023934266675e+01,
    1.172241608595926010e+01,
    1.175962449953234135e+01,
    1.179691557231318377e+01,
    1.183428939656179324e+01,
    1.187174606454695791e+01,
    1.190928566854608484e+01,
    1.194690830084524080e+01,
    1.198461405373896227e+01,
    1.202240301953030865e+01,
    1.206027529053070246e+01,
    1.209823095905997192e+01,
    1.213627011744618756e+01,
    1.217439285802564974e+01,
    1.221259927314286209e+01,
    1.225088945515038930e+01,
    1.228926349640887850e+01,
    1.232772148928694556e+01,
    1.236626352616113422e+01,
    1.240488969941589659e+01,
    1.244360010144344386e+01,
    1.248239482464379435e+01,
    1.252127396142464377e+01,
    1.256023760420135638e+01,
    1.259928584539686902e+01,
    1.263841877744168052e+01,
    1.267763278947218986e+01,
    1.271693883638016231e+01,
    1.275632664308852959e+01,
    1.279579926298416659e+01,
    1.283535703599269517e+01,
    1.287500005458868735e+01,
    1.291472841125395910e+01,
    1.295454219847725419e+01,
    1.299444150875464565e+01,
    1.303442643458903305e+01,
    1.307449706849036275e+01,
    1.311465350297546273e+01,
    1.315489583056804790e+01,
    1.319522414379864372e+01,
    1.323563853520455424e+01,
    1.327613909732980879e+01,
    1.331672592272513711e+01,
    1.335739910394783969e+01,
    1.339815873356188902e+01,
    1.343900490413766491e+01,
    1.347993770825218185e+01,
    1.352095723848882258e+01,
    1.356206358743738782e+01,
    1.360325684769403765e+01,
    1.364453711186125773e+01,
    1.368590447254778120e+01,
    1.372735902236856376e+01,
    1.376890085394479435e+01,
    1.381053005990375482e+01,
    1.385224673287883057e+01,
    1.389405096550947682e+01,
    1.393594285044116532e+01,
    1.397792248032534701e+01,
    1.401998994781937213e+01,
    1.406214534558653817e+01,
    1.410438876629595129e+01,
    1.414672030262252633e+01,
    1.418914004724698330e+01,
    1.423164809285575849e+01,
    1.427424453214098676e+01,
    1.431692945778245729e+01,
    1.435970296253758072e+01,
    1.440256513906132163e+01,
    1.444551608008624477e+01,
    1.448855587833234715e+01,
    1.453168462652514314e+01,
    1.457490241739557391e+01,
    1.461820934367995406e+01,
    1.466160549811996461e+01,
    1.470509097346261917e+01,
    1.474866586246022493e+01,
    1.479233025787028843e+01,
    1.483608425245561335e+01,
    1.487992793898411215e+01,
    1.492386141022888957e+01,
    1.496788475896824444e+01,
    1.501199807798516161e+01,
    1.505620146006825699e+01,
    1.510049499801080408e+01,
    1.514487878461108572e+01,
    1.518935291267239585e+01,
    1.523391747500288318e+01,
    1.527857256441564182e+01,
    1.532331827372855670e+01,
    1.536815469576437643e+01,
    1.541308192335059424e+01,
    1.545810004931949244e+01,
    1.550320916650805536e+01,
    1.554840936775800131e+01,
    1.559370074591562449e+01,
    1.563908339383193535e+01,
    1.568455740436251133e+01,
    1.573012287036750578e+01,
    1.577577988471163373e+01,
    1.582152854026405286e+01,
    1.586736892989850567e+01,
    1.591330114649313998e+01,
    1.595932528293050545e+01,
    1.600544143209758374e+01,
    1.605164968688572458e+01,
    1.609795014019060844e+01,
    1.614434288491222347e+01,
    1.619082801395486726e+01,
    1.623740562022707934e+01,
    1.628407579664161986e+01,
    1.633083863611566500e+01,
    1.637769423156982640e+01,
    1.642464267592995242e+01,
    1.647168406212530201e+01,
    1.651881848308939027e+01,
    1.656604603175982504e+01,
    1.661336680107828201e+01,
    1.666078088399041235e+01,
    1.670828837344589957e+01,
    1.675588936239838134e+01,
    1.680358394380543530e+01,
    1.685137221062856838e+01,
    1.689925425583320617e+01,
    1.694723017238861118e+01};

/**
 * Linear interpolation with extrapolation for out-of-bounds values.
 * 
 * Given discrete function f(x) with *values* at positions starting from zero with given *spacing*,
 * return the linearly interpolated value at *x*.
 * If x is out of bounds, extrapolates linearly using the two first (or last) values.
 * 
 * @param  x       position to interpolate values at
 * @param  values  discrete function values
 * @param  spacing spacing of x positions
 */
double GistEntropyUtils::interpolate(double x, const double* values, size_t array_len, double spacing)
{
    double dbl_index = x / spacing;
    int index = std::max(0, std::min(static_cast<int>(array_len) - 2, static_cast<int>(dbl_index)));
    double dx = dbl_index - index;
    double interp = (1-dx) * values[index] + dx * values[index+1];
    return interp;
}

// Interpolate SIX_CORR at a given position
double GistEntropyUtils::sixVolumeCorrFactor(double NNs)
{
    return interpolate(NNs, SIX_CORR, 1001, SIX_CORR_SPACING);
}
