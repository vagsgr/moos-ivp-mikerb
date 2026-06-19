/************************************************************/
/*    NAME: Mike Benjamin                                   */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.h                                     */
/*    DATE: April 18th, 2022                                */
/*    MOD : Lab 09 - dynamic rescue planning (Asg 4) +      */
/*          MCTS orienteering planner integration           */
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

// [Lab09] To "schedio" enos ALLOU (antipalou) oximatos: i thesi tou tin wra pou
//   sxediase + ta swimmer-id pou skopeyei na sosei. Xrhsimopoieitai gia distance-
//   based arbitration sto adversarial variant (Part 2): an o antipalos einai PIO
//   KONTA se enan swimmer, tou ton afhnoume -> Voronoi-like diamoirasmos, oxi overlap.
struct PlanClaim {
  double px;
  double py;
  std::set<std::string> ids;
};

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
  bool handleMailPlanClaim(std::string);          // [Lab09] adversarial (Part 2)

  // [Lab09] dispatcher + oi dyo planners (greedy = asfales fallback, mcts = exypno)
  void postShortestPath();                        // dispatcher -> planGreedy/planMCTS
  void planGreedy();                              // greedy nearest-neighbor (baseline)
  bool planMCTS();                                // kalei ton python MCTS planner
  void postNullPath();                            // kanena swimmer -> mene/gyrna

  // [Lab09] voithitika
  void collectCandidates(std::vector<std::string>& ids, XYSegList& pts);
  void postPath(const XYSegList& path, const std::vector<std::string>& ids);
  bool excludedByCloserOther(const std::string& id, double x, double y);
  double currentBudget();    // [Lab09] dynamiko (time-tracking) h sta8ero budget

 private: // Config variables
  std::string m_vname;
  std::string m_update_var;     // [Lab09] var pou akouei to waypt_survey (SURVEY_UPDATE)
  std::string m_planner;        // [Lab09] "greedy" (default) h "mcts"
  std::string m_planner_script; // [Lab09] path sto mcts_planner.py (gia planner=mcts)
  double      m_budget;         // [Lab09] sta8ero mikos collecting-path (m). Part1: huge
  double      m_replan_interval;// [Lab09] elaxistos xronos (s) metaksy dyo SURVEY_UPDATE
  bool        m_adversarial;    // [Lab09] Part 2: energopoiei PLAN_CLAIM + arbitration
  double      m_time_limit;     // [Lab09] orio collecting-time (s). 0 = anenergo (sta8ero budget)
  double      m_max_speed;      // [Lab09] gia metatropi xronou->apostasi (default 1.2 m/s)

 private: // State variables
  XYSegList  m_path;
  double     m_nav_x;
  double     m_nav_y;
  bool       m_nav_x_set;
  bool       m_nav_y_set;

  // [Lab09] energoi swimmers pou DEN exoun swthei akoma: id -> shmeio
  std::map<std::string, XYPoint> m_swimmers;
  // [Lab09] ids pou exoun hdh swthei (apo emas h apo antipalo) -> ta agnooume
  std::set<std::string>          m_rescued_ids;
  // [Lab09] vname -> trexon schedio antipalou (adversarial)
  std::map<std::string, PlanClaim> m_plan_claims;

  bool   m_plan_pending;        // [Lab09] xreiazetai neo path (nees plhrofories)
  double m_last_plan_time;      // [Lab09] pote postaram teleftaia (gia throttle)

  // [Lab09] budget-tracking: ena "leg" = sineximeno diasthma underway (apo DEPLOY
  //   h apo oloklirwsh refuel). To budget meiwnetai oso trexei to roloi tou leg.
  double m_leg_start_time;      // MOOSTime enarksis trexontos leg (-1 = prin to deploy)
  bool   m_deployed;            // trexousa timi DEPLOY
  bool   m_refuel_needed;       // trexousa timi REFUEL_NEEDED (gia anixneusi true->false)

  // metrites gia to AppCast report
  unsigned int m_total_alerts;        // posa SWIMMER_ALERT eidame synolika
  unsigned int m_total_new_swimmers;  // posoi MONADIKOI swimmers
  unsigned int m_total_found_msgs;    // posa FOUND_SWIMMER eidame
  unsigned int m_plans_posted;        // posa paths postaristikan
  unsigned int m_mcts_runs;           // poses fores trekse o MCTS planner
  unsigned int m_mcts_fallbacks;      // poses fores epese se greedy fallback
};

#endif
