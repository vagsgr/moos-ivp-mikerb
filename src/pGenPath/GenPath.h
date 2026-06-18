/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.h                                       */
/*    DATE: 2026                                            */
/*    DESC: Lab 07 - dexetai VISIT_POINT, ftiaxnei greedy   */
/*          diadromi, kai tin postarei sto waypoint behavior*/
/************************************************************/

#ifndef GenPath_HEADER
#define GenPath_HEADER

#include <string>
#include <vector>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"
#include "XYPoint.h"

class GenPath : public AppCastingMOOSApp
{
 public:
   GenPath();
   ~GenPath();

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
   void generatePath();                             // ftiaxnei+postarei diadromi (greedy)
   void updateVisited();                            // metra poia simeia episkeftikame

 private: // Configuration variables
   std::string m_waypt_update_var;   // se poia metavliti postaroume (px TOUR_UPDATE)
   double      m_visit_radius;       // poso konta = "episkepsi" (default 3m)

 private: // State variables
   std::vector<XYPoint> m_points;    // ta simeia pou mas anatethikan
   std::vector<bool>    m_visited;   // parallilo: poia episkeftikame
   double m_nav_x;                   // trexousa thesi oximatos
   double m_nav_y;
   bool   m_nav_ok;                  // exoume lavei NAV_X/NAV_Y;
   bool   m_got_first;               // eida to "firstpoint";
   bool   m_got_last;                // eida to "lastpoint" (= irthan ola);
   bool   m_path_posted;             // exoume idi postarei tin diadromi;
};

#endif
