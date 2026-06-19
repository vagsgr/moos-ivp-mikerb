/************************************************************/
/*    NAME: Mike Benjamin                                   */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.h                                     */
/*    DATE: April 18th, 2022                                */
/*    MOD : Lab 09 Assignment 4 - dynamic rescue planning   */
/*          (Evangelos Siatis)                              */
/************************************************************/

#ifndef P_GEN_RESCUE_HEADER
#define P_GEN_RESCUE_HEADER

#include <vector>
#include <string>
#include <map>
#include <set>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "XYPoint.h"
#include "XYSegList.h"
#include "XYPolygon.h"

class GenRescue : public AppCastingMOOSApp
{
 public:
   GenRescue();
   ~GenRescue() {};

 protected:
  bool OnNewMail(MOOSMSG_LIST &NewMail);
  bool Iterate();
  bool OnConnectToServer();
  bool OnStartUp();
  bool buildReport();
  void RegisterVariables();

 protected:
  bool handleMailNewSwimmer(std::string);
  bool handleMailFoundSwimmer(std::string);
  bool handleMailRescueRegion(std::string);
  void postShortestPath();
  void postNullPath();

 private: // Config variables
  std::string m_vname;
  std::string m_update_var;   // [Lab09] var pou akouei to waypt_survey (SURVEY_UPDATE)

 private: // State variables
  XYSegList  m_path;
  double     m_nav_x;
  double     m_nav_y;
  bool       m_nav_x_set;
  bool       m_nav_y_set;

  // [Lab09] energoi swimmers pou DEN exoun swthei akoma: id -> shmeio
  std::map<std::string, XYPoint> m_swimmers;
  // [Lab09] ids pou exoun hdh swthei (apo emas h apo allon) -> ta agnooume
  std::set<std::string>          m_rescued_ids;

  bool   m_plan_pending;        // [Lab09] xreiazetai neo path (nees plhrofories)
  // metrites gia to AppCast report
  unsigned int m_total_alerts;        // posa SWIMMER_ALERT eidame synolika
  unsigned int m_total_new_swimmers;  // posoi MONADIKOI swimmers
  unsigned int m_total_found_msgs;    // posa FOUND_SWIMMER eidame
  unsigned int m_plans_posted;        // posa paths postaristikan
};

#endif
