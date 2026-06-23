/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT 2.680 (minicourse Athens)                   */
/*    FILE: BHV_Pulse.cpp                                    */
/*    DATE:                                                  */
/************************************************************/

#include <cstdlib>
#include "MBUtils.h"
#include "BuildUtils.h"
#include "BHV_Pulse.h"
#include "XYRangePulse.h"      // [Lab13] gia ton optiko palmo

using namespace std;

//---------------------------------------------------------------
// Constructor

BHV_Pulse::BHV_Pulse(IvPDomain domain) :
  IvPBehavior(domain)
{
  // Default onoma behavior
  IvPBehavior::setParam("name", "bhv_pulse");

  // [Lab13] Den pairnoume apofasi kinisis, omos dilonoume to subdomain
  m_domain = subDomain(m_domain, "course,speed");

  // [Lab13] Default times parametron (allazoun apo to .bhv / updates)
  m_pulse_range    = 20;   // aktina se metra
  m_pulse_duration = 8;    // diarkeia se deuterolepta

  // [Lab13] Arxiki katastasi
  m_last_wpt_index = -1;   // sentinel: i proti anagnosi metra san "allagi"
  m_mark_time      = 0;
  m_pulse_pending  = false;
  m_osx            = 0;
  m_osy            = 0;

  // [Lab13] Eggrafi sta var pou parakolouthoume:
  //   NAV_X/NAV_Y -> i thesi tou skafous (kentro tou palmou) - panta yparxoun
  addInfoVars("NAV_X, NAV_Y");
  //   WPT_INDEX -> postaretai apo to adelfo BHV_Waypoint se kathe waypoint.
  //   Prin patiseis DEPLOY den yparxei akoma -> "no_warning" gia na MIN
  //   vgazei BHV_WARNING stis protes iterations (idio idiom me to BHV_Waypoint).
  addInfoVars("WPT_INDEX", "no_warning");
}

//---------------------------------------------------------------
// Procedure: setParam()
//   [Lab13] Diavazei tis parametrous apo to .bhv (kai apo updates=).

bool BHV_Pulse::setParam(string param, string val)
{
  param = tolower(param);
  double double_val = atof(val.c_str());

  if((param == "pulse_range") && isNumber(val)) {
    if(double_val < 0) return(false);
    m_pulse_range = double_val;
    return(true);
  }
  else if((param == "pulse_duration") && isNumber(val)) {
    if(double_val < 0) return(false);
    m_pulse_duration = double_val;
    return(true);
  }

  // Agnosti parametros
  return(false);
}

//---------------------------------------------------------------
// Procedure: onSetParamComplete()

void BHV_Pulse::onSetParamComplete()
{
}

//---------------------------------------------------------------
// Procedure: onHelmStart()

void BHV_Pulse::onHelmStart()
{
}

//---------------------------------------------------------------
// Procedure: onIdleState()

void BHV_Pulse::onIdleState()
{
}

//---------------------------------------------------------------
// Procedure: onCompleteState()

void BHV_Pulse::onCompleteState()
{
}

//---------------------------------------------------------------
// Procedure: postConfigStatus()

void BHV_Pulse::postConfigStatus()
{
}

//---------------------------------------------------------------
// Procedure: onIdleToRunState()

void BHV_Pulse::onIdleToRunState()
{
}

//---------------------------------------------------------------
// Procedure: onRunToIdleState()

void BHV_Pulse::onRunToIdleState()
{
}

//---------------------------------------------------------------
// Procedure: onRunState()
//   [Lab13] Kathe iteration (otan DEPLOY=true): anixneuse allagi
//   waypoint kai 5s argotera rixe enan optiko palmo.

IvPFunction* BHV_Pulse::onRunState()
{
  bool ok_idx, ok_x, ok_y;
  double curr_index = getBufferDoubleVal("WPT_INDEX", ok_idx);
  m_osx             = getBufferDoubleVal("NAV_X", ok_x);
  m_osy             = getBufferDoubleVal("NAV_Y", ok_y);
  double curr_time  = getBufferCurrTime();

  // [Lab13] Allakse to WPT_INDEX? -> markare ton xrono kai "oplise" palmo
  if(ok_idx && (curr_index != m_last_wpt_index)) {
    m_last_wpt_index = curr_index;
    m_mark_time      = curr_time;
    m_pulse_pending  = true;
  }

  // [Lab13] Perasan 5s apo tin allagi? -> rixe ton palmo (mia fora)
  if(m_pulse_pending && ((curr_time - m_mark_time) >= 5.0)) {
    if(ok_x && ok_y)
      postRangePulse();
    m_pulse_pending = false;
  }

  // [Lab13] Auto to behavior den paragei apofasi kinisis -> NULL IvP function
  return(0);
}

//---------------------------------------------------------------
// Procedure: postRangePulse()
//   [Lab13] Xtizei ena XYRangePulse sti thesi tou skafous kai to
//   postarei sto VIEW_RANGE_PULSE (to diavazei o pMarineViewer).

void BHV_Pulse::postRangePulse()
{
  XYRangePulse pulse;
  pulse.set_x(m_osx);
  pulse.set_y(m_osy);
  pulse.set_label(m_us_name);
  pulse.set_rad(m_pulse_range);
  pulse.set_fill(0.25);
  pulse.set_duration(m_pulse_duration);
  pulse.set_time(getBufferCurrTime());
  pulse.set_color("edge", "yellow");
  pulse.set_color("fill", "yellow");

  string spec = pulse.get_spec();
  postMessage("VIEW_RANGE_PULSE", spec);
}
