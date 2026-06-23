/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT 2.680 (minicourse Athens)                   */
/*    FILE: BHV_Pulse.h                                      */
/*    DATE:                                                  */
/************************************************************/

#ifndef Pulse_HEADER
#define Pulse_HEADER

#include <string>
#include "IvPBehavior.h"

class BHV_Pulse : public IvPBehavior {
public:
  BHV_Pulse(IvPDomain);
  ~BHV_Pulse() {};

  bool         setParam(std::string, std::string);
  void         onSetParamComplete();
  void         onCompleteState();
  void         onIdleState();
  void         onHelmStart();
  void         postConfigStatus();
  void         onRunToIdleState();
  void         onIdleToRunState();
  IvPFunction* onRunState();

protected: // Local Utility functions
  void         postRangePulse();          // [Lab13] xtizei+postarei VIEW_RANGE_PULSE

protected: // Configuration parameters
  double  m_pulse_range;      // [Lab13] aktina (m) tou palmou
  double  m_pulse_duration;   // [Lab13] diarkeia (s) tou optikou palmou

protected: // State variables
  double  m_last_wpt_index;   // [Lab13] teleutaio WPT_INDEX pou eidame
  double  m_mark_time;        // [Lab13] xronos pou allakse to index
  bool    m_pulse_pending;    // [Lab13] ekkremei palmos pou perimenei ta 5s?
  double  m_osx;              // [Lab13] NAV_X (ownship)
  double  m_osy;              // [Lab13] NAV_Y (ownship)
};

#define IVP_EXPORT_FUNCTION

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain)
  {return new BHV_Pulse(domain);}
}
#endif
