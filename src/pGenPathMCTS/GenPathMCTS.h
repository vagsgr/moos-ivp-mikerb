/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPathMCTS.h                                    */
/*    DATE: 2026                                            */
/*    DESC: Lab07_upgr - dexetai OLA ta VISIT_POINT, kalei  */
/*          ton MCTS planner (python) gia veltisti kalypsi  */
/*          mesa sto budget, kai diekdikei simeia (claiming)*/
/************************************************************/

#ifndef GenPathMCTS_HEADER
#define GenPathMCTS_HEADER

#include <string>
#include <vector>
#include <set>
#include <map>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

// [Lab07_upgr] To "schedio" (tour-intention) enos ALLOU oximatos: i thesi tou
//   tin wra pou sxediase + ta id pou skopeyei na episkeftei. To xrisimopoioume
//   gia distance-based arbitration: an to allo oxima einai PIO KONTA se ena
//   simeio, tou to afinoume (apofyge overlap, dikaios "diagwnismos").
struct PlanClaim {
  double px;
  double py;
  std::set<std::string> ids;
};

class GenPathMCTS : public AppCastingMOOSApp
{
 public:
   GenPathMCTS();
   ~GenPathMCTS();

 protected: // Standard MOOSApp functions to overload
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();

 protected: // Standard AppCastingMOOSApp function to overload
   bool buildReport();

 protected:
   void registerVariables();

 private: // Helper functions
   void handleVisitPoint(const std::string& sval);  // ena eiserxomeno VISIT_POINT
   void handlePointClaimed(const std::string& sval);// kapoios episkeftike simeio (permanent)
   void handlePlanClaim(const std::string& sval);   // allo oxima anakoinwse tin prothesi tou
   void generatePath();                             // kalei MCTS planner + postarei
   void updateVisited();                            // metra+diekdikei osa episkeftikame
   bool excludedByCloserOther(const std::string& id, double x, double y); // distance-arbitration

 private: // Configuration variables
   std::string m_waypt_update_var;   // se poia metavliti postaroume (px TOUR_UPDATE)
   std::string m_planner_script;     // path sto python script (mcts_planner.py)
   std::string m_vname;              // to onoma TOU oximatos (gia tag stis diekdikiseis)
   double      m_visit_radius;       // poso konta = "episkepsi" (default 3m)
   double      m_budget;             // mikos collecting-path se metra (default 720)
   double      m_replan_interval;    // elaxistos xronos (s) metaksy dyo TOUR_UPDATE

 private: // State variables
   std::vector<std::string> m_ids;   // ta id twn simeion (parallila me xs/ys)
   std::vector<double>      m_xs;
   std::vector<double>      m_ys;
   std::vector<bool>        m_visited;   // poia episkeftikame EMEIS
   std::set<std::string>    m_claimed;   // id pou EXOUN EPISKEFTEI (permanent, apo opoiondipote)
   std::map<std::string, PlanClaim> m_plan_claims; // vname -> trexon "schedio" tou allou

   double m_nav_x;                   // trexousa thesi oximatos
   double m_nav_y;
   bool   m_nav_ok;                  // exoume lavei NAV_X/NAV_Y;
   bool   m_got_first;               // eida to "firstpoint";
   bool   m_got_last;                // eida to "lastpoint" (= irthan ola);
   bool   m_path_posted;             // exoume postarei toulaxiston mia fora;
   bool   m_replan_pending;          // allakse kati (nea diekdikisi) -> ksanasxediase
   bool   m_refuel_needed;           // trexousa timi REFUEL_NEEDED (gia anixneusi listis)
   double m_last_plan_time;          // pote postaram teleftaia (gia throttle)

   unsigned int m_my_visits;         // posa simeia exoume diekdikisei EMEIS
   unsigned int m_last_tour_size;    // megethos teleftaias diadromis pou postaram
   unsigned int m_plan_count;        // poses fores trekse o planner
};

#endif
