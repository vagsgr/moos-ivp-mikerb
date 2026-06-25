/*****************************************************************/
/*    NAME: M.Benjamin                                           */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: BHV_Scout.cpp                                        */
/*    DATE: April 30th 2022                                      */
/*****************************************************************/

#include <cstdlib>
#include <math.h>
#include "BHV_Scout.h"
#include "MBUtils.h"
#include "AngleUtils.h"
#include "BuildUtils.h"
#include "GeomUtils.h"
#include "ZAIC_PEAK.h"
#include "OF_Coupler.h"
#include "XYFormatUtilsPoly.h"

using namespace std;

//-----------------------------------------------------------
// Constructor()

BHV_Scout::BHV_Scout(IvPDomain gdomain) : 
  IvPBehavior(gdomain)
{
  IvPBehavior::setParam("name", "scout");
 
  // Default values for behavior state variables
  m_osx  = 0;
  m_osy  = 0;

  // All distances are in meters, all speed in meters per second
  // Default values for configuration parameters 
  m_desired_speed  = 1;
  m_capture_radius = 10;

  m_pt_set = false;

  // [Lab14] defaults gia th "exypni" epilogi simeiou (idea 4)
  m_num_candidates = 25;
  m_sigma          = 40;
  m_travel_w       = 0.5;
  m_w_reg          = 0.4;

  addInfoVars("NAV_X, NAV_Y");
  addInfoVars("RESCUE_REGION");
  addInfoVars("SCOUTED_SWIMMER");
  // [Lab14] avoid-set inputs: registered swimmers + poioi diaswthikan
  addInfoVars("SWIMMER_ALERT, FOUND_SWIMMER, RESCUED_SWIMMER");
}

//---------------------------------------------------------------
// Procedure: setParam() - handle behavior configuration parameters

bool BHV_Scout::setParam(string param, string val) 
{
  // Convert the parameter to lower case for more general matching
  param = tolower(param);
  
  bool handled = true;
  if(param == "capture_radius")
    handled = setPosDoubleOnString(m_capture_radius, val);
  else if(param == "desired_speed")
    handled = setPosDoubleOnString(m_desired_speed, val);
  else if(param == "tmate")
    handled = setNonWhiteVarOnString(m_tmate, val);
  else if(param == "num_candidates")
    handled = setUIntOnString(m_num_candidates, val);
  else if(param == "repulsion_sigma")
    handled = setPosDoubleOnString(m_sigma, val);
  else if(param == "travel_weight")
    handled = setNonNegDoubleOnString(m_travel_w, val);
  else if(param == "reg_weight")
    handled = setNonNegDoubleOnString(m_w_reg, val);
  else
    handled = false;

  srand(time(NULL));
  
  return(handled);
}

//-----------------------------------------------------------
// Procedure: onEveryState()

void BHV_Scout::onEveryState(string str)
{
  // [Lab14] enimerwse to avoid-set se KA8E katastasi (run/idle) -- oxi mono otan
  // trexei to behavior -- wste na exoume oloklhrwmenh eikona twn swimmers.
  ingestSwimmerMail();

  // --- baseline reporting: prow8hse to SCOUTED_SWIMMER ston rescue teammate ---
  if(!getBufferVarUpdated("SCOUTED_SWIMMER"))
    return;

  string report = getBufferStringVal("SCOUTED_SWIMMER");
  if(report == "")
    return;

  if(m_tmate == "") {
    postWMessage("Mandatory Teammate name is null");
    return;
  }
  postOffboardMessage(m_tmate, "SWIMMER_ALERT", report);
}

//-----------------------------------------------------------
// Procedure: ingestSwimmerMail()
//   [Lab14] idea 4: chtise to avoid-set apo ta mhnymata ths uFldRescueMgr.
//   SWIMMER_ALERT="x=..,y=..,id=.."  -> registered swimmer (id->thesi)
//   FOUND/RESCUED_SWIMMER="id=..,.."  -> svise ton (h periochi eleftherwnetai)

void BHV_Scout::ingestSwimmerMail()
{
  if(getBufferVarUpdated("SWIMMER_ALERT")) {
    string s = getBufferStringVal("SWIMMER_ALERT");
    string sid, sx, sy;
    if(tokParse(s, "id", ',', '=', sid) &&
       tokParse(s, "x",  ',', '=', sx)  &&
       tokParse(s, "y",  ',', '=', sy)) {
      if(!m_done.count(sid))
        m_known[sid] = XYPoint(atof(sx.c_str()), atof(sy.c_str()));
    }
  }

  if(getBufferVarUpdated("FOUND_SWIMMER")) {
    string sid;
    if(tokParse(getBufferStringVal("FOUND_SWIMMER"), "id", ',', '=', sid)) {
      m_done.insert(sid);
      m_known.erase(sid);
    }
  }

  if(getBufferVarUpdated("RESCUED_SWIMMER")) {
    string sid;
    if(tokParse(getBufferStringVal("RESCUED_SWIMMER"), "id", ',', '=', sid)) {
      m_done.insert(sid);
      m_known.erase(sid);
    }
  }
}

//-----------------------------------------------------------
// Procedure: scorePoint()
//   [Lab14] Oso PIO MAKRIA apo to rescue-coverage (registered swimmers) kai oso
//   PIO KONTA ston eauto mas, toso kalytero (megalytero) score. To athroisma
//   Gaussian = "pedio apwthisis": kovetai omala me thn apostasi (sigma).

double BHV_Scout::scorePoint(double px, double py) const
{
  double repulsion = 0;
  std::map<std::string, XYPoint>::const_iterator it;
  for(it = m_known.begin(); it != m_known.end(); ++it) {
    if(m_done.count(it->first))    // asfaleia: agnose rescued
      continue;
    double dx = px - it->second.x();
    double dy = py - it->second.y();
    double d2 = (dx*dx) + (dy*dy);
    double w  = m_w_reg;           // Phase2: W_PLAN an to id einai sto rescue plan
    repulsion += w * exp(-d2 / (m_sigma * m_sigma));
  }
  double travel = hypot(px - m_osx, py - m_osy);
  return( -repulsion - (m_travel_w * travel) );
}

//-----------------------------------------------------------
// Procedure: onIdleState()

void BHV_Scout::onIdleState() 
{
  m_curr_time = getBufferCurrTime();
}

//-----------------------------------------------------------
// Procedure: onRunState()

IvPFunction *BHV_Scout::onRunState() 
{
  // Part 1: Get vehicle position from InfoBuffer and post a 
  // warning if problem is encountered
  bool ok1, ok2;
  m_osx = getBufferDoubleVal("NAV_X", ok1);
  m_osy = getBufferDoubleVal("NAV_Y", ok2);
  if(!ok1 || !ok2) {
    postWMessage("No ownship X/Y info in info_buffer.");
    return(0);
  }
  
  // Part 2: Determine if the vehicle has reached the destination 
  // point and if so, declare completion.
  updateScoutPoint();
  double dist = hypot((m_ptx-m_osx), (m_pty-m_osy));
  //postEventMessage("Dist=" + doubleToStringX(dist,1));
  if(dist <= m_capture_radius) {
    m_pt_set = false;
    postViewPoint(false);
    return(0);
  }

  // Part 3: Post the waypoint as a string for consumption by 
  // a viewer application.
  postViewPoint(true);

  // Part 4: Build the IvP function 
  IvPFunction *ipf = buildFunction();
  if(ipf == 0) 
    postWMessage("Problem Creating the IvP Function");
  
  return(ipf);
}

//-----------------------------------------------------------
// Procedure: updateScoutPoint()

void BHV_Scout::updateScoutPoint()
{
  if(m_pt_set)
    return;

  string region_str = getBufferStringVal("RESCUE_REGION");
  if(region_str == "")
    postWMessage("Unknown RESCUE_REGION");
  else
    postRetractWMessage("Unknown RESCUE_REGION");

  XYPolygon region = string2Poly(region_str);
  if(!region.is_convex()) {
    postWMessage("Badly formed RESCUE_REGION");
    return;
  }
  m_rescue_region = region;

  // [Lab14] posoi energoi (registered, oxi diaswthentes) swimmers gnwrizoume
  unsigned int active = 0;
  std::map<std::string, XYPoint>::iterator it;
  for(it = m_known.begin(); it != m_known.end(); ++it)
    if(!m_done.count(it->first))
      active++;

  double best_x = 0, best_y = 0;
  bool   have_best = false;

  if(active == 0) {
    // FALLBACK = symperifora baseline: ena tyxaio simeio mesa sto region.
    // (kanena gnwsto swimmer akoma -> tipota na apofygoume, mh kollaei pote)
    have_best = randPointInPoly(m_rescue_region, best_x, best_y);
  }
  else {
    // [Lab14] idea 4: deigmatise N tyxaia ypopsifia, dialekse to MEGISTO score
    // (= pio makria apo to rescue-coverage, oxi poly makria gia taxidi).
    double best_score = 0;
    for(unsigned int i = 0; i < m_num_candidates; i++) {
      double cx = 0, cy = 0;
      if(!randPointInPoly(m_rescue_region, cx, cy))
        continue;
      double sc = scorePoint(cx, cy);
      if(!have_best || (sc > best_score)) {
        best_score = sc;
        best_x = cx;
        best_y = cy;
        have_best = true;
      }
    }
  }

  if(!have_best) {
    postWMessage("Unable to generate scout point");
    return;
  }

  m_ptx = best_x;
  m_pty = best_y;
  m_pt_set = true;
  string msg = "New pt: " + doubleToStringX(m_ptx) + "," + doubleToStringX(m_pty)
             + " (known=" + uintToString((unsigned int)m_known.size())
             + ",active=" + uintToString(active) + ")";
  postEventMessage(msg);
}

//-----------------------------------------------------------
// Procedure: postViewPoint()

void BHV_Scout::postViewPoint(bool viewable) 
{

  XYPoint pt(m_ptx, m_pty);
  pt.set_vertex_size(5);
  pt.set_vertex_color("orange");
  pt.set_label(m_us_name + "'s next waypoint");
  
  string point_spec;
  if(viewable)
    point_spec = pt.get_spec("active=true");
  else
    point_spec = pt.get_spec("active=false");
  postMessage("VIEW_POINT", point_spec);
}


//-----------------------------------------------------------
// Procedure: buildFunction()

IvPFunction *BHV_Scout::buildFunction() 
{
  if(!m_pt_set)
    return(0);
  
  ZAIC_PEAK spd_zaic(m_domain, "speed");
  spd_zaic.setSummit(m_desired_speed);
  spd_zaic.setPeakWidth(0.5);
  spd_zaic.setBaseWidth(1.0);
  spd_zaic.setSummitDelta(0.8);  
  if(spd_zaic.stateOK() == false) {
    string warnings = "Speed ZAIC problems " + spd_zaic.getWarnings();
    postWMessage(warnings);
    return(0);
  }
  
  double rel_ang_to_wpt = relAng(m_osx, m_osy, m_ptx, m_pty);
  ZAIC_PEAK crs_zaic(m_domain, "course");
  crs_zaic.setSummit(rel_ang_to_wpt);
  crs_zaic.setPeakWidth(0);
  crs_zaic.setBaseWidth(180.0);
  crs_zaic.setSummitDelta(0);  
  crs_zaic.setValueWrap(true);
  if(crs_zaic.stateOK() == false) {
    string warnings = "Course ZAIC problems " + crs_zaic.getWarnings();
    postWMessage(warnings);
    return(0);
  }

  IvPFunction *spd_ipf = spd_zaic.extractIvPFunction();
  IvPFunction *crs_ipf = crs_zaic.extractIvPFunction();

  OF_Coupler coupler;
  IvPFunction *ivp_function = coupler.couple(crs_ipf, spd_ipf, 50, 50);

  return(ivp_function);
}
