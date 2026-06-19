/************************************************************/
/*    NAME: Mike Benjamin                                   */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: April 18th, 2022                                */
/*    MOD : Lab 09 Assignment 4 - dynamic rescue planning   */
/*          (Evangelos Siatis)                              */
/************************************************************/

#include <iterator>
#include <cstdlib>
#include "GenRescue.h"
#include "MBUtils.h"
#include "ColorParse.h"
#include "XYPoint.h"
#include "XYSegList.h"
#include "GeomUtils.h"
#include "PathUtils.h"
#include "XYFormatUtilsPoly.h"
#include "XYFieldGenerator.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

GenRescue::GenRescue()
{
  // Initialize state variables
  m_nav_x = 0;
  m_nav_y = 0;
  m_nav_x_set = false;
  m_nav_y_set = false;

  // [Lab09] config defaults
  m_update_var = "SURVEY_UPDATE";

  // [Lab09] state/metrites
  m_plan_pending       = false;
  m_total_alerts       = 0;
  m_total_new_swimmers = 0;
  m_total_found_msgs   = 0;
  m_plans_posted       = 0;
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool GenRescue::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key  = msg.GetKey();
    string sval = msg.GetString();

    bool handled = true;
    if(key == "SWIMMER_ALERT")
      handled = handleMailNewSwimmer(sval);
    else if(key == "FOUND_SWIMMER")
      handled = handleMailFoundSwimmer(sval);
    else if(key == "NAV_X") {
      m_nav_x = msg.GetDouble();
      m_nav_x_set = true;
    }
    else if(key == "NAV_Y") {
      m_nav_y = msg.GetDouble();
      m_nav_y_set = true;
    }

    else if(key != "APPCAST_REQ") // handle by AppCastingMOOSApp
      handled = false;

    if(!handled)
      reportRunWarning("Unhandled Mail: " + key +"=" + sval);

  }
  return(true);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool GenRescue::OnConnectToServer()
{
  RegisterVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()

bool GenRescue::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // [Lab09] Ksana-sxediazoume MONO otan exoume nea plhroforia (m_plan_pending)
  // KAI exoume egkyro NAV fix (alliws to greedy 8a ksekinouse apo to (0,0)).
  if(m_plan_pending && m_nav_x_set && m_nav_y_set) {
    postShortestPath();
    m_plan_pending = false;
  }

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()

bool GenRescue::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.GetConfiguration(GetAppName(), sParams);

  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string sLine  = *p;
    string param  = tolower(biteStringX(sLine, '='));
    string value  = sLine;
    if(param == "vname")
      m_vname = value;
    else if((param == "update_var") && (value != ""))
      m_update_var = value;       // [Lab09] override (default SURVEY_UPDATE)
  }

  RegisterVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: RegisterVariables()

void GenRescue::RegisterVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("SWIMMER_ALERT", 0);
  Register("FOUND_SWIMMER", 0);
  Register("NAV_X", 0);          // [Lab09] xreiazetai gia to greedy start point
  Register("NAV_Y", 0);
}


//---------------------------------------------------------
// Procedure: handleMailNewSwimmer()
//   String: "x=23, y=54, id=04"  (apo to uFldRescueMgr, ka8e 15s)
//   [Lab09] kratame ka8e MONADIKO swimmer (dedupe by id) kai zhtame replan
//           mono otan o swimmer einai ontws kainourios.

bool GenRescue::handleMailNewSwimmer(string str)
{
  m_total_alerts++;

  string id;
  double x = 0, y = 0;
  bool x_set = false, y_set = false;

  vector<string> svector = parseString(str, ',');
  for(unsigned int i=0; i<svector.size(); i++) {
    string param = tolower(biteStringX(svector[i], '='));
    string value = svector[i];
    if(param == "id")
      id = value;
    else if(param == "x") { x = atof(value.c_str()); x_set = true; }
    else if(param == "y") { y = atof(value.c_str()); y_set = true; }
  }

  // Malformed alert
  if((id == "") || !x_set || !y_set)
    return(false);

  // [Lab09] Mporei na laboume alert akoma kai gia hdh-swsmeno swimmer -> agnooume
  if(m_rescued_ids.count(id))
    return(true);

  // [Lab09] Hdh gnwstos energos swimmer -> dedupe, kamia allagh sto path
  if(m_swimmers.count(id))
    return(true);

  // Kainourios swimmer -> apo8hkeush + zhthse replan
  XYPoint pt(x, y);
  pt.set_label(id);
  m_swimmers[id] = pt;
  m_total_new_swimmers++;
  m_plan_pending = true;
  reportEvent("New swimmer id=" + id + " (" + doubleToString(x,1) +
              "," + doubleToString(y,1) + ")");
  return(true);
}

//---------------------------------------------------------
// Procedure: handleMailFoundSwimmer()
//   String: "id=01, finder=abe"
//   [Lab09] enas swimmer swstike (apo emas h apo allon). Ton bgazoume apo
//           ta energa kai ksana-sxediazoume xwris auton.

bool GenRescue::handleMailFoundSwimmer(string str)
{
  m_total_found_msgs++;

  string id, finder;
  vector<string> svector = parseString(str, ',');
  for(unsigned int i=0; i<svector.size(); i++) {
    string param = tolower(biteStringX(svector[i], '='));
    string value = svector[i];
    if(param == "id")
      id = value;
    else if(param == "finder")
      finder = value;
  }

  if(id == "")
    return(false);

  m_rescued_ids.insert(id);

  // An htan stous energous, bgale ton kai zhthse replan
  if(m_swimmers.count(id)) {
    m_swimmers.erase(id);
    m_plan_pending = true;
    reportEvent("Swimmer id=" + id + " rescued by " + finder + " -> replan");
  }
  return(true);
}

//---------------------------------------------------------
// Procedure: postShortestPath()
//   [Lab09] Xtizei greedy nearest-neighbor path panw stous ENERGOUS
//           swimmers, ksekinwntas apo th 8esh tou oxhmatos (NAV).

void GenRescue::postShortestPath()
{
  // Den exoun apomeinei swimmers -> tipota na sxediasoume
  if(m_swimmers.size() == 0) {
    postNullPath();
    return;
  }

  // Maze ola ta shmeia twn energwn swimmers
  XYSegList swim_pts;
  map<string, XYPoint>::iterator q;
  for(q=m_swimmers.begin(); q!=m_swimmers.end(); q++)
    swim_pts.add_vertex(q->second.x(), q->second.y());

  // Greedy diataksh apo th 8esh tou oxhmatos
  m_path = greedyPath(swim_pts, m_nav_x, m_nav_y);
  m_path.set_label("rescue_" + m_vname);

  Notify("VIEW_SEGLIST", m_path.get_spec());

  string update_str = "points = " + m_path.get_spec_pts();
  Notify(m_update_var, update_str);
  reportEvent(m_update_var + "=" + update_str);
  m_plans_posted++;
}

//---------------------------------------------------------
// Procedure: postNullPath()
//   [Lab09] Den emeinan swimmers: dwse ws monadiko waypoint th
//           trexousa 8esh, wste to waypt_survey na "oloklhrwsei"
//           (kai na pyrodothsei to endflag RETURN=true).

void GenRescue::postNullPath()
{
  if(!m_nav_x_set || !m_nav_y_set)
    return;

  XYSegList segl;
  segl.add_vertex(m_nav_x, m_nav_y);
  segl.set_label("rescue_" + m_vname);

  Notify("VIEW_SEGLIST", segl.get_spec());

  string update_str = "points = " + segl.get_spec_pts();
  Notify(m_update_var, update_str);
  reportEvent(m_update_var + "=" + update_str + " (no swimmers left)");
}


//---------------------------------------------------------
// Procedure: buildReport()

bool GenRescue::buildReport()
{
  m_msgs << "Vehicle Name: " << m_vname                          << endl;
  m_msgs << "Update Var:   " << m_update_var                     << endl;
  m_msgs << "NAV Fix:      ";
  if(m_nav_x_set && m_nav_y_set)
    m_msgs << "(" << doubleToString(m_nav_x,1) << ", "
           << doubleToString(m_nav_y,1) << ")"                   << endl;
  else
    m_msgs << "<none yet>"                                       << endl;
  m_msgs << "---------------------------------------"            << endl;
  m_msgs << "SWIMMER_ALERTs received: " << m_total_alerts        << endl;
  m_msgs << "Unique swimmers known:   " << m_total_new_swimmers  << endl;
  m_msgs << "FOUND_SWIMMER msgs:      " << m_total_found_msgs    << endl;
  m_msgs << "Swimmers rescued:        " << m_rescued_ids.size()  << endl;
  m_msgs << "Swimmers still active:   " << m_swimmers.size()     << endl;
  m_msgs << "Paths posted:            " << m_plans_posted        << endl;
  m_msgs << "Plan pending:            " << boolToString(m_plan_pending) << endl;
  m_msgs << "Current path length:     " << m_path.size() << " pts" << endl;

  return(true);
}
