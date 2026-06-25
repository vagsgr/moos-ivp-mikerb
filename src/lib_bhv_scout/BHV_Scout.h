/*****************************************************************/
/*    NAME: M.Benjamin,                                          */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: BHV_Scout.h                                          */
/*    DATE: April 30th 2022                                      */
/*                                                               */
/* This program is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU General Public License   */
/* as published by the Free Software Foundation; either version  */
/* 2 of the License, or (at your option) any later version.      */
/*                                                               */
/* This program is distributed in the hope that it will be       */
/* useful, but WITHOUT ANY WARRANTY; without even the implied    */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR       */
/* PURPOSE. See the GNU General Public License for more details. */
/*                                                               */
/* You should have received a copy of the GNU General Public     */
/* License along with this program; if not, write to the Free    */
/* Software Foundation, Inc., 59 Temple Place - Suite 330,       */
/* Boston, MA 02111-1307, USA.                                   */
/*****************************************************************/
 
#ifndef BHV_SCOUT_HEADER
#define BHV_SCOUT_HEADER

#include <string>
#include <map>                 // [Lab14] avoid-set: id -> thesi swimmer
#include <set>                 // [Lab14] rescued ids
#include "IvPBehavior.h"
#include "XYPoint.h"
#include "XYPolygon.h"

class BHV_Scout : public IvPBehavior {
public:
  BHV_Scout(IvPDomain);
  ~BHV_Scout() {};
  
  bool         setParam(std::string, std::string);
  void         onIdleState();
  IvPFunction* onRunState();
  void         onEveryState(std::string);
  
protected:
  IvPFunction* buildFunction();
  void         updateScoutPoint();
  void         postViewPoint(bool viewable=true);

  // [Lab14] idea 4: mazepse registered swimmers + vathmologise ypopsifia simeia
  void         ingestSwimmerMail();
  double       scorePoint(double px, double py) const;

protected: // State variables
  double   m_osx;
  double   m_osy;
  double   m_curr_time;

  double   m_ptx;
  double   m_pty;
  bool     m_pt_set;

  XYPolygon m_rescue_region;

  // [Lab14] avoid-set: gnwstoi registered swimmers (id->thesi) kai osoi diaswthikan
  std::map<std::string, XYPoint> m_known;
  std::set<std::string>          m_done;

protected: // Config variables
  double m_capture_radius;
  double m_desired_speed;

  std::string m_tmate;

  // [Lab14] knobs gia thn epilogi simeiou (sample-N-pick-best + pedio apwthisis)
  unsigned int m_num_candidates;   // posa tyxaia ypopsifia ana epilogi
  double       m_sigma;            // platos pediou apwthisis (m)
  double       m_travel_w;         // a: poini apostasis taxidiou
  double       m_w_reg;            // baros registered swimmer (Phase2: W_PLAN gia plan ids)
};

#define IVP_EXPORT_FUNCTION
extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain) 
  {return new BHV_Scout(domain);}
}
#endif
