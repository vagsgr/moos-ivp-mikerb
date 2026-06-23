/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT 2.680 (minicourse Athens)                   */
/*    FILE: BHV_ZigLeg.h                                     */
/*    DATE:                                                  */
/************************************************************/

#ifndef ZigLeg_HEADER
#define ZigLeg_HEADER

#include <string>
#include "IvPBehavior.h"

class BHV_ZigLeg : public IvPBehavior {
public:
  BHV_ZigLeg(IvPDomain);
  ~BHV_ZigLeg() {};

  bool         setParam(std::string, std::string);
  void         onSetParamComplete();
  void         onCompleteState();
  void         onIdleState();
  void         onHelmStart();
  void         postConfigStatus();
  void         onRunToIdleState();
  void         onIdleToRunState();
  IvPFunction* onRunState();

protected: // Configuration parameters
  double  m_zig_angle;        // [Lab13] gonia ektropis (moires, default 45)
  double  m_zig_duration;     // [Lab13] poso krataei to zig (s, default 10)
  double  m_peakwidth;        // [Lab13] sxima tou ZAIC_PEAK
  double  m_basewidth;
  double  m_summitdelta;

protected: // State variables
  double  m_last_wpt_index;   // [Lab13] teleutaio WPT_INDEX pou eidame
  double  m_mark_time;        // [Lab13] xronos pou allakse to index
  bool    m_zig_pending;      // [Lab13] perimenoume ta 5s gia na ksekinisei to zig?
  bool    m_zig_active;       // [Lab13] eimaste mesa se zig auti ti stigmi?
  double  m_zig_start_time;   // [Lab13] pote ksekinise to trexon zig
  double  m_zig_heading;      // [Lab13] KLEIDOMENO heading sti stigmi enarksis zig
};

#define IVP_EXPORT_FUNCTION

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain)
  {return new BHV_ZigLeg(domain);}
}
#endif
