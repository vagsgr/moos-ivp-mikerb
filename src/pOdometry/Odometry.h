/************************************************************/
/*    NAME: Evangelos Siatis                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: Odometry.h                                          */
/*    DATE: December 29th, 1963                             */
/************************************************************/

#ifndef Odometry_HEADER
#define Odometry_HEADER

#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

class Odometry : public AppCastingMOOSApp
{
 public:
   Odometry();
   ~Odometry();

 protected: // Standard MOOSApp functions to overload  
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();

 protected: // Standard AppCastingMOOSApp function to overload 
   bool buildReport();

 protected:
   void registerVariables();

 private: // Configuration variables
   double m_depth_thresh;    // [ASS.8] kato apo afto to bathos den metrame apostasi

 private: // State variables
   bool   m_first_reading;
   double m_current_x;
   double m_current_y;
   double m_current_depth;   // [ASS.8] trexon bathos apo NAV_DEPTH
   double m_previous_x;
   double m_previous_y;
   double m_total_distance;
   double m_dist_at_depth;   // [ASS.8] apostasi mono otan bathos > depth_thresh
};

#endif 
