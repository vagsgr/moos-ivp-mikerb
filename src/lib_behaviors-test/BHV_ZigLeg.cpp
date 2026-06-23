/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT 2.680 (minicourse Athens)                   */
/*    FILE: BHV_ZigLeg.cpp                                   */
/*    DATE:                                                  */
/************************************************************/

#include <cstdlib>
#include "MBUtils.h"
#include "AngleUtils.h"        // [Lab13] gia angle360()
#include "BuildUtils.h"
#include "BHV_ZigLeg.h"
#include "ZAIC_PEAK.h"         // [Lab13] gia tin antikeimeniki synartisi sto course

using namespace std;

//---------------------------------------------------------------
// Constructor

BHV_ZigLeg::BHV_ZigLeg(IvPDomain domain) :
  IvPBehavior(domain)
{
  IvPBehavior::setParam("name", "bhv_zigleg");

  // [Lab13] PAIRNOUME apofasi sto course (kai speed gia syvatotita domain)
  m_domain = subDomain(m_domain, "course,speed");

  // [Lab13] Default parametroi
  m_zig_angle    = 45;   // moires ektropis
  m_zig_duration = 10;   // deuterolepta diarkeias tou zig

  // [Lab13] Sxima ZAIC_PEAK (idies times me to BHV_ConstantHeading)
  m_peakwidth    = 10;
  m_basewidth    = 170;
  m_summitdelta  = 25;

  // [Lab13] Arxiki katastasi
  m_last_wpt_index = -1;
  m_mark_time      = 0;
  m_zig_pending    = false;
  m_zig_active     = false;
  m_zig_start_time = 0;
  m_zig_heading    = 0;

  // [Lab13] NAV_HEADING -> gia to kleidoma. NAV_X/Y oxi anagkaia edo.
  addInfoVars("NAV_HEADING");
  addInfoVars("WPT_INDEX", "no_warning");
}

//---------------------------------------------------------------
// Procedure: setParam()

bool BHV_ZigLeg::setParam(string param, string val)
{
  param = tolower(param);
  double double_val = atof(val.c_str());

  if((param == "zig_angle") && isNumber(val)) {
    m_zig_angle = double_val;          // epitrepetai kai arnitiki (alli pleura)
    return(true);
  }
  else if((param == "zig_duration") && isNumber(val)) {
    if(double_val < 0) return(false);
    m_zig_duration = double_val;
    return(true);
  }
  else if((param == "peakwidth") && isNumber(val)) {
    m_peakwidth = vclip_min(double_val, 0);
    return(true);
  }
  else if((param == "basewidth") && isNumber(val)) {
    m_basewidth = vclip_min(double_val, 0);
    return(true);
  }
  else if((param == "summitdelta") && isNumber(val)) {
    m_summitdelta = vclip(double_val, 0, 100);
    return(true);
  }

  return(false);
}

//---------------------------------------------------------------
// Standard hooks (den ta xreiazomaste edo)

void BHV_ZigLeg::onSetParamComplete() {}
void BHV_ZigLeg::onHelmStart() {}
void BHV_ZigLeg::onIdleState() {}
void BHV_ZigLeg::onCompleteState() {}
void BHV_ZigLeg::postConfigStatus() {}
void BHV_ZigLeg::onIdleToRunState() {}

//---------------------------------------------------------------
// Procedure: onRunToIdleState()
//   [Lab13] An vgoume apo run (px RETURN) sviste opoiodipote energo zig.

void BHV_ZigLeg::onRunToIdleState()
{
  m_zig_pending = false;
  m_zig_active  = false;
}

//---------------------------------------------------------------
// Procedure: onRunState()
//   [Lab13] State machine:
//     1) anixneuse allagi WPT_INDEX -> markare xrono, "oplise" zig (pending)
//     2) 5s meta -> KLEIDOSE to heading kai ksekinise to zig (active)
//     3) gia zig_duration -> parage ZAIC_PEAK sto (kleidomeno heading + gonia)
//     4) otan teleiosei -> NULL (to waypoint ksanapairnei ton elegxo)

IvPFunction* BHV_ZigLeg::onRunState()
{
  bool ok_idx, ok_hdg;
  double curr_index = getBufferDoubleVal("WPT_INDEX", ok_idx);
  double curr_hdg   = getBufferDoubleVal("NAV_HEADING", ok_hdg);
  double curr_time  = getBufferCurrTime();

  // (1) Allakse to WPT_INDEX? -> markare kai oplise to zig
  if(ok_idx && (curr_index != m_last_wpt_index)) {
    m_last_wpt_index = curr_index;
    m_mark_time      = curr_time;
    m_zig_pending    = true;
    m_zig_active     = false;   // tyxon proigoumeno zig akyronetai
  }

  // (2) Perasan 5s apo to waypoint? -> KLEIDOSE heading & ksekinise zig
  if(m_zig_pending && ((curr_time - m_mark_time) >= 5.0)) {
    if(ok_hdg) {
      m_zig_heading    = curr_hdg;       // <-- TO KLEIDOMA (alliws kanei kyklous)
      m_zig_start_time = curr_time;
      m_zig_active     = true;
    }
    m_zig_pending = false;
  }

  // (3) Eimaste mesa sto zig kai den exei liksei o xronos?
  if(m_zig_active && ((curr_time - m_zig_start_time) < m_zig_duration)) {
    double desired_course = angle360(m_zig_heading + m_zig_angle);

    ZAIC_PEAK zaic(m_domain, "course");
    zaic.setSummit(desired_course);
    zaic.setPeakWidth(m_peakwidth);
    zaic.setBaseWidth(m_basewidth);
    zaic.setSummitDelta(m_summitdelta);
    zaic.setValueWrap(true);             // course einai kykliko (359->0)

    IvPFunction *ipf = zaic.extractIvPFunction();
    if(ipf)
      ipf->setPWT(m_priority_wt);        // megalytero apo to waypoint -> nikaei
    else
      postWMessage("ZigLeg: apotyxia dimiourgias IvP function");

    return(ipf);
  }

  // (4) Teleiose to zig (i den eimaste se zig) -> kamia apofasi
  if(m_zig_active && ((curr_time - m_zig_start_time) >= m_zig_duration))
    m_zig_active = false;

  return(0);
}
