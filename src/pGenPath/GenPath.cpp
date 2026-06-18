/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPath.cpp                                     */
/*    DATE: 2026                                            */
/************************************************************/

#include <iterator>
#include <cmath>
#include "MBUtils.h"
#include "ACTable.h"
#include "GenPath.h"
#include "XYSegList.h"
#include "XYFormatUtilsPoint.h"   // string2Point()

using namespace std;

//---------------------------------------------------------
// Constructor
GenPath::GenPath()
{
  m_waypt_update_var = "TOUR_UPDATE";   // default - to .moos mporei na to allaksei
  m_visit_radius     = 3;               // "episkepsi" otan eimaste <3m (apo to lab)

  m_nav_x        = 0;
  m_nav_y        = 0;
  m_nav_ok       = false;
  m_got_first    = false;
  m_got_last     = false;
  m_path_posted  = false;
}

//---------------------------------------------------------
// Destructor
GenPath::~GenPath()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()
bool GenPath::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();

    if(key == "VISIT_POINT")
      handleVisitPoint(msg.GetString());
    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_ok = true;
    }
    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_ok = true;
    }
    else if(key != "APPCAST_REQ")
      reportRunWarning("Unhandled Mail: " + key);
  }

  return(true);
}

//---------------------------------------------------------
// Procedure: handleVisitPoint()
// [ΣΗΜ] Mazevoume ta simeia. "firstpoint" = arxi neou set (kane reset),
//   "lastpoint" = irthan OLA (-> mporoume na ftiaksoume diadromi).
void GenPath::handleVisitPoint(const string& sval)
{
  if(sval == "firstpoint") {
    // Neo set: kathara ola gia na ksanaftiaksoume apo tin arxi.
    m_points.clear();
    m_visited.clear();
    m_got_first   = true;
    m_got_last    = false;
    m_path_posted = false;
    return;
  }

  if(sval == "lastpoint") {
    m_got_last = true;   // h generatePath() (stin Iterate) tha to dei
    return;
  }

  // Kanoniko simeio: parse kai apothikeusi.
  if(!strContains(sval, "x=") || !strContains(sval, "y="))
    return;   // agnoise mi-egkyra

  XYPoint point = string2Point(sval);
  m_points.push_back(point);
  m_visited.push_back(false);
}

//---------------------------------------------------------
// Procedure: generatePath()
// [ΣΗΜ] GREEDY nearest-neighbor: ksekina apo tin trexousa thesi tou oximatos,
//   kai kathe fora dialekse to PLISIESTERO mi-episkemmeno simeio. Den einai
//   veltisto (to veltisto TSP einai NP), alla einai apla kai arketo.
void GenPath::generatePath()
{
  unsigned int n = m_points.size();
  if(n == 0)
    return;

  vector<bool> used(n, false);
  XYSegList seglist;

  double cx = m_nav_x;   // "eikoniko" arxiko simeio = thesi oximatos
  double cy = m_nav_y;

  for(unsigned int step=0; step<n; step++) {
    int    best = -1;
    double best_dist = 0;
    // Vres to plisiestero mi-xrisimopoiimeno simeio
    for(unsigned int i=0; i<n; i++) {
      if(used[i])
        continue;
      double dx = m_points[i].get_vx() - cx;
      double dy = m_points[i].get_vy() - cy;
      double d  = hypot(dx, dy);
      if(best < 0 || d < best_dist) {
        best = (int)i;
        best_dist = d;
      }
    }
    if(best < 0)
      break;
    used[best] = true;
    double bx = m_points[best].get_vx();
    double by = m_points[best].get_vy();
    seglist.add_vertex(bx, by);
    cx = bx;   // proxwra: epomeno vima ksekina apo edw
    cy = by;
  }

  // [ΣΗΜ] Postare ti diadromi sto waypoint behavior (px TOUR_UPDATE).
  //   Format: "points = pts={x1,y1:x2,y2:...}"  (auto dinei to get_spec)
  string update_str = "points = " + seglist.get_spec();
  Notify(m_waypt_update_var, update_str);

  m_path_posted = true;
}

//---------------------------------------------------------
// Procedure: updateVisited()
// [ΣΗΜ] Gia to AppCasting: an to oxima einai konta (<visit_radius) se kapoio
//   mi-episkemmeno simeio, simeiwse to ws episkemmeno.
void GenPath::updateVisited()
{
  if(!m_nav_ok)
    return;
  for(unsigned int i=0; i<m_points.size(); i++) {
    if(m_visited[i])
      continue;
    double dx = m_points[i].get_vx() - m_nav_x;
    double dy = m_points[i].get_vy() - m_nav_y;
    if(hypot(dx, dy) <= m_visit_radius)
      m_visited[i] = true;
  }
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()
bool GenPath::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
bool GenPath::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // [ΣΗΜ] Molis exoume OLA ta simeia (lastpoint) KAI thesi oximatos, kai den
  //   exoume idi postarei -> ftiakse kai postare ti diadromi (mia fora).
  if(m_got_last && m_nav_ok && !m_path_posted)
    generatePath();

  updateVisited();

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
bool GenPath::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string orig  = *p;
    string line  = *p;
    string param = tolower(biteStringX(line, '='));
    string value = line;

    bool handled = false;
    if(param == "waypt_update_var") {
      m_waypt_update_var = value;
      handled = true;
    }
    else if(param == "visit_radius") {
      handled = setDoubleOnString(m_visit_radius, value);
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()
void GenPath::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);   // ta simeia apo to shoreside (mesa apo qbridge)
  Register("NAV_X", 0);         // trexousa thesi oximatos
  Register("NAV_Y", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()
bool GenPath::buildReport()
{
  unsigned int visited = 0;
  for(unsigned int i=0; i<m_visited.size(); i++)
    if(m_visited[i]) visited++;

  m_msgs << "Visit Radius:            " << doubleToStringX(m_visit_radius,1) << endl;
  m_msgs << "Update Var:              " << m_waypt_update_var << endl;
  m_msgs << "Total Points Received:   " << m_points.size() << endl;
  m_msgs << "First Point Received:    " << boolToString(m_got_first) << endl;
  m_msgs << "Last Point Received:     " << boolToString(m_got_last) << endl;
  m_msgs << "NAV_X/Y Received:        " << boolToString(m_nav_ok) << endl;
  m_msgs << "Path Posted:             " << boolToString(m_path_posted) << endl;
  m_msgs << endl;
  m_msgs << "Tour Status" << endl;
  m_msgs << "------------------------" << endl;
  m_msgs << "   Points Visited:       " << visited << endl;
  m_msgs << "   Points Unvisited:     " << (m_points.size() - visited) << endl;

  return(true);
}
