/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.h                                   */
/*    DATE: 2026                                            */
/*    DESC: Lab 07 - moirazei ta VISIT_POINT se 2+ oximata  */
/************************************************************/

#ifndef PointAssign_HEADER
#define PointAssign_HEADER

#include <string>
#include <vector>
#include <set>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class PointAssign : public AppCastingMOOSApp
{
 public:
   PointAssign();
   ~PointAssign();

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
   // Diaxeirizetai ena eiserxomeno VISIT_POINT (marker i simeio).
   void handleVisitPoint(const std::string& sval);
   // Dialegei se POIO oxima (index sto m_vnames) paei to simeio me thesi x.
   unsigned int chooseVehicle(double x);
   // Stelnei ena simeio sto pMarineViewer (VIEW_POINT) gia optiko elegxo.
   void postViewPoint(double x, double y, std::string label, std::string color);
   // Epistrefei xrwma analoga me ton arithmo (index) tou oximatos.
   std::string colorForIndex(unsigned int index);

 private: // Configuration variables (diavazontai apo to .moos)
   std::vector<std::string> m_vnames;       // lista oximatwn (px henry, gilda)
   bool   m_assign_by_region;               // true=east/west, false=enallaks
   double m_region_xmin;                    // aristero orio perioxis (X)
   double m_region_xmax;                    // deksio orio perioxis (X)

 private: // State variables (katastasi gia debug/AppCasting)
   unsigned int m_points_total;             // posa PRAGMATIKA simeia lavame
   unsigned int m_invalid_points;           // simeia pou den parse-aran
   std::vector<unsigned int> m_vcount;      // posa se kathe oxima (idio index me m_vnames)
   bool m_got_first;                        // eida to "firstpoint";
   bool m_got_last;                         // eida to "lastpoint";
   bool m_uts_unpaused;                     // ksepagwsa idi to uTimerScript;
   std::set<std::string> m_ready;           // poia oximata exoun steilei NODE_REPORT
   double m_start_time;                     // pote ksekinise (gia timeout fallback)
};

#endif
