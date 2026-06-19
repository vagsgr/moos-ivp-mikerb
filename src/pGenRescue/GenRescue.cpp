/************************************************************/
/*    NAME: Mike Benjamin                                   */
/*    ORGN: MIT                                             */
/*    FILE: GenRescue.cpp                                   */
/*    DATE: April 18th, 2022                                */
/*    MOD : Lab 09 - dynamic rescue planning (Asg 4) +      */
/*          MCTS orienteering planner integration           */
/*          (Evangelos Siatis)                              */
/************************************************************/

#include <iterator>
#include <cstdlib>     // system(), atof()
#include <cmath>       // hypot()
#include <fstream>     // ofstream/ifstream gia ta tmp arxeia tou planner
#include <sstream>
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
  m_update_var      = "SURVEY_UPDATE";
  m_planner         = "greedy";        // asfales default (aytarkes, xwris python)
  m_planner_script  = "mcts_planner.py";
  m_budget          = 100000;          // Part1: huge -> o planner epistrefei OLOUS
  m_replan_interval = 5;               // min 5s metaksy postings (mh tradzetai o helm)
  m_adversarial     = false;           // Part 2: true -> PLAN_CLAIM + arbitration
  m_time_limit      = 0;               // 0 = anenergo -> xrhsh sta8erou m_budget
  m_max_speed       = 1.2;             // gia metatropi (xronos pou menei)->(apostasi)

  // [Lab09] state/metrites
  m_plan_pending       = false;
  m_last_plan_time     = -1;
  m_leg_start_time     = -1;
  m_deployed           = false;
  m_refuel_needed      = false;
  m_total_alerts       = 0;
  m_total_new_swimmers = 0;
  m_total_found_msgs   = 0;
  m_plans_posted       = 0;
  m_mcts_runs          = 0;
  m_mcts_fallbacks     = 0;
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
    else if(key == "PLAN_CLAIM")
      handled = handleMailPlanClaim(sval);
    else if(key == "DEPLOY") {
      // [Lab09] budget-tracking: to roloi tou leg ksekinaei sto DEPLOY (false->true)
      bool now = (tolower(sval) == "true");
      if(now && !m_deployed)
        m_leg_start_time = MOOSTime();
      m_deployed = now;
    }
    else if(key == "REFUEL_NEEDED") {
      // [Lab09] (Part 2 forward-compat) Otan o anefodiasmos TELEIWNEI (true->false),
      //   ksekinaei neo leg -> mhdenise to roloi kai zhthse fresh plan.
      bool now = (tolower(sval) == "true");
      if(m_refuel_needed && !now) {
        m_leg_start_time = MOOSTime();
        m_plan_pending   = true;
      }
      m_refuel_needed = now;
    }
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

  // [Lab09] Ksana-sxediazoume MONO otan: exoume nea plhroforia (m_plan_pending),
  //   egkyro NAV fix, KAI exei perasei to throttle interval (o MCTS mplokarei ~1s
  //   -> xwris throttle tha "tradzame" ton helm me synexh SURVEY_UPDATE).
  bool need_plan   = m_plan_pending && m_nav_x_set && m_nav_y_set;
  bool throttle_ok = (m_last_plan_time < 0) ||
                     ((MOOSTime() - m_last_plan_time) >= m_replan_interval);
  if(need_plan && throttle_ok) {
    postShortestPath();
    m_plan_pending   = false;
    m_last_plan_time = MOOSTime();
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
      m_vname = tolower(value);
    else if((param == "update_var") && (value != ""))
      m_update_var = value;       // [Lab09] override (default SURVEY_UPDATE)
    else if(param == "planner")
      m_planner = tolower(value);  // [Lab09] greedy | mcts
    else if((param == "planner_script") && (value != ""))
      m_planner_script = value;
    else if(param == "budget")
      setDoubleOnString(m_budget, value);
    else if(param == "replan_interval")
      setDoubleOnString(m_replan_interval, value);
    else if(param == "adversarial")
      setBooleanOnString(m_adversarial, value);
    else if(param == "time_limit")
      setDoubleOnString(m_time_limit, value);   // [Lab09] >0 -> dynamiko budget
    else if(param == "max_speed")
      setDoubleOnString(m_max_speed, value);
  }

  // [Lab09] An den dothike vname, dokimase to community name (= onoma oximatos).
  if(m_vname == "")
    m_vname = tolower(m_host_community);

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
  Register("PLAN_CLAIM", 0);     // [Lab09] adversarial: schedia antipalwn
  Register("DEPLOY", 0);         // [Lab09] budget-tracking: enarksi leg
  Register("REFUEL_NEEDED", 0);  // [Lab09] budget-tracking: reset leg meta refuel
  Register("NAV_X", 0);          // [Lab09] xreiazetai gia to greedy/mcts start point
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
//   [Lab09] enas swimmer swstike (apo emas h apo antipalo). Ton bgazoume apo
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
// Procedure: handleMailPlanClaim()
//   [Lab09] (adversarial / Part 2) Allo oxima anakoinwse to "schedio" tou:
//   "vname=<v>,px=..,py=..,ids=a:b:c". To apothikeuoume gia distance-arbitration.
//   Ta DIKA mas (idio vname) ta agnooume.

bool GenRescue::handleMailPlanClaim(string sval)
{
  string vname = tolower(tokStringParse(sval, "vname", ',', '='));
  if((vname == "") || (vname == m_vname))
    return(true);   // diko mas i axreasto

  PlanClaim pc;
  pc.px = atof(tokStringParse(sval, "px", ',', '=').c_str());
  pc.py = atof(tokStringParse(sval, "py", ',', '=').c_str());

  string ids = tokStringParse(sval, "ids", ',', '=');
  vector<string> idv = parseString(ids, ':');
  for(unsigned int i=0; i<idv.size(); i++) {
    string id = stripBlankEnds(idv[i]);
    if(id != "")
      pc.ids.insert(id);
  }

  m_plan_claims[vname] = pc;
  m_plan_pending       = true;   // allakse i katanomi -> ksanasxediase
  return(true);
}

//---------------------------------------------------------
// Procedure: excludedByCloserOther()
//   [Lab09] (adversarial) Paraxwroume ton swimmer se antipalo an EKEINOS einai
//   PIO KONTA (kai ton exei sto schedio tou). Isopalia -> spaei me to onoma.
//   Voronoi-like diamoirasmos xwris overlap. (Energo MONO an m_adversarial.)

bool GenRescue::excludedByCloserOther(const string& id, double x, double y)
{
  if(!m_adversarial)
    return(false);

  double my_d = hypot(x - m_nav_x, y - m_nav_y);
  map<string, PlanClaim>::iterator it;
  for(it=m_plan_claims.begin(); it!=m_plan_claims.end(); it++) {
    const string&    ovname = it->first;
    const PlanClaim& pc     = it->second;
    if(pc.ids.count(id) == 0)
      continue;   // o antipalos den ton thelei -> den mas afora
    double other_d = hypot(x - pc.px, y - pc.py);
    if(other_d < my_d)
      return(true);                            // pio konta -> diko tou
    if((other_d == my_d) && (ovname < m_vname))
      return(true);                            // isopalia -> proteraiotita onomatos
  }
  return(false);
}

//---------------------------------------------------------
// Procedure: currentBudget()
//   [Lab09] BUDGET-TRACKING. An time_limit<=0 -> sta8ero budget (Part 1: sose olous).
//   Alliws dynamiko: budget = (xronos pou menei sto leg) * max_speed. Etsi o
//   orienteering planner sxediazei me to PRAGMATIKO perithwrio pou apomenei (oso
//   trexei to roloi, mikrainei -> dialegei ligoterous & pio apodotikous swimmers).

double GenRescue::currentBudget()
{
  if(m_time_limit <= 0)
    return(m_budget);

  double elapsed   = (m_leg_start_time < 0) ? 0 : (MOOSTime() - m_leg_start_time);
  double remaining = m_time_limit - elapsed;
  if(remaining < 0)
    remaining = 0;

  double budget = remaining * m_max_speed;
  if(budget < 1)
    budget = 1;     // floor: na borei panta na dokimasei toulaxiston ton pio kontino
  return(budget);
}

//---------------------------------------------------------
// Procedure: collectCandidates()
//   [Lab09] Mazepse tous ENERGOUS swimmers pros sxediasmo. (Oi swsmenoi exoun hdh
//   afairethei apo to m_swimmers.) Sto adversarial, petaxe osous o antipalos einai
//   pio konta. Epistrefei parallila: ids[] kai XYSegList me ta shmeia.

void GenRescue::collectCandidates(vector<string>& ids, XYSegList& pts)
{
  ids.clear();
  map<string, XYPoint>::iterator q;
  for(q=m_swimmers.begin(); q!=m_swimmers.end(); q++) {
    const string& id = q->first;
    double x = q->second.x();
    double y = q->second.y();
    if(excludedByCloserOther(id, x, y))
      continue;
    ids.push_back(id);
    pts.add_vertex(x, y);
  }
}

//---------------------------------------------------------
// Procedure: postShortestPath()
//   [Lab09] Dispatcher: dialeksoume planner (mcts h greedy). O mcts peftei se
//   greedy fallback an apotyxei (leipei python/script) -> to oxima den menei pote
//   akinhto.

void GenRescue::postShortestPath()
{
  if(m_swimmers.size() == 0) {
    postNullPath();
    return;
  }

  if(m_planner == "mcts") {
    if(planMCTS())
      return;                 // petyxe
    m_mcts_fallbacks++;       // apotyxia -> sygkratisi me greedy
  }
  planGreedy();
}

//---------------------------------------------------------
// Procedure: planGreedy()
//   [Lab09] Greedy nearest-neighbor (to baseline). Xtizei diadromi panw stous
//   energous swimmers ksekinwntas apo th 8esh NAV.

void GenRescue::planGreedy()
{
  vector<string> ids;
  XYSegList swim_pts;
  collectCandidates(ids, swim_pts);

  if(swim_pts.size() == 0) {
    postNullPath();
    return;
  }

  XYSegList path = greedyPath(swim_pts, m_nav_x, m_nav_y);
  // Greedy anadiataksei ta shmeia, ara den kratame antistoixia me ta ids·
  // gia to PLAN_CLAIM arkei to SET twn ids (xwris seira).
  postPath(path, ids);
}

//---------------------------------------------------------
// Procedure: planMCTS()
//   [Lab09] Kalei ton python MCTS orienteering planner (epanaxrhsh apo Lab07_upgr):
//   1) grapse infile: start(NAV) / budget / point id x y (gia osous swimmers)
//   2) trekse "python3 mcts_planner.py in out" (mplokarei ~1s)
//   3) diavase ti SEIRA episkepsis -> postare
//   Epistrefei false an kati apetyxe (o dispatcher pefteti se greedy).

bool GenRescue::planMCTS()
{
  vector<string> cand_ids;
  XYSegList cand_pts;
  collectCandidates(cand_ids, cand_pts);
  if(cand_pts.size() == 0) {
    postNullPath();
    return(true);
  }

  string base    = "/tmp/rescue_" + (m_vname=="" ? string("veh") : m_vname);
  string infile  = base + "_in.txt";
  string outfile = base + "_out.txt";

  // --- 1) grapse to input ---
  {
    ofstream ofs(infile.c_str());
    if(!ofs.good()) {
      reportRunWarning("Cannot write planner input: " + infile);
      return(false);
    }
    ofs << "start "  << m_nav_x << " " << m_nav_y << "\n";
    ofs << "budget " << currentBudget() << "\n";
    for(unsigned int i=0; i<cand_ids.size(); i++)
      ofs << "point " << cand_ids[i] << " "
          << cand_pts.get_vx(i) << " " << cand_pts.get_vy(i) << "\n";
  }

  // --- 2) trekse ton planner (mplokarei) ---
  string cmd = "python3 " + m_planner_script + " " + infile + " " + outfile;
  int rc = system(cmd.c_str());
  m_mcts_runs++;
  if(rc != 0) {
    reportRunWarning("MCTS planner returned non-zero (" + intToString(rc) +
                     ") -> greedy fallback");
    return(false);
  }

  // --- 3) diavase ti seira ---
  ifstream ifs(outfile.c_str());
  if(!ifs.good()) {
    reportRunWarning("Cannot read planner output: " + outfile);
    return(false);
  }
  XYSegList path;
  vector<string> tour_ids;
  string line;
  while(getline(ifs, line)) {
    if(line.empty() || (line[0] == '#'))
      continue;                       // scholio "# count=.. length=.."
    istringstream iss(line);
    string id; double x, y;
    if(iss >> id >> x >> y) {
      path.add_vertex(x, y);
      tour_ids.push_back(id);
    }
  }
  if(path.size() == 0) {
    reportRunWarning("MCTS planner produced empty path -> greedy fallback");
    return(false);
  }

  postPath(path, tour_ids);
  return(true);
}

//---------------------------------------------------------
// Procedure: postPath()
//   [Lab09] Koino posting gia tous dyo planners: SURVEY_UPDATE (gia ton helm) +
//   VIEW_SEGLIST (gia to GUI) + (adversarial) PLAN_CLAIM (thesi + ids -> arbitration).

void GenRescue::postPath(const XYSegList& path, const vector<string>& ids)
{
  m_path = path;
  m_path.set_label("rescue_" + m_vname);

  Notify("VIEW_SEGLIST", m_path.get_spec());

  string update_str = "points = " + m_path.get_spec_pts();
  Notify(m_update_var, update_str);
  reportEvent(m_update_var + "=" + update_str);

  if(m_adversarial) {
    string ids_str;
    for(unsigned int i=0; i<ids.size(); i++)
      ids_str += (i ? ":" : "") + ids[i];
    string plan = "vname=" + m_vname +
                  ",px=" + doubleToStringX(m_nav_x,2) +
                  ",py=" + doubleToStringX(m_nav_y,2) +
                  ",ids=" + ids_str;
    Notify("PLAN_CLAIM", plan);
  }

  m_plans_posted++;
}

//---------------------------------------------------------
// Procedure: postNullPath()
//   [Lab09] Den emeinan swimmers: dwse ws monadiko waypoint th trexousa 8esh,
//   wste to waypt_survey na "oloklhrwsei" (pyrodotei to endflag RETURN=true).

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
  m_msgs << "Planner:      " << m_planner;
  if(m_planner == "mcts")
    m_msgs << " (" << m_planner_script << ")";
  m_msgs << endl;
  if(m_time_limit > 0) {
    double elapsed = (m_leg_start_time < 0) ? 0 : (MOOSTime() - m_leg_start_time);
    double remain  = m_time_limit - elapsed; if(remain < 0) remain = 0;
    m_msgs << "Budget:       DYNAMIC  time_limit=" << doubleToString(m_time_limit,0)
           << "s  speed=" << doubleToString(m_max_speed,1)
           << "  remain=" << doubleToString(remain,0) << "s"
           << "  -> " << doubleToString(currentBudget(),0) << "m" << endl;
  }
  else
    m_msgs << "Budget:       FIXED " << doubleToString(m_budget,0) << "m" << endl;
  m_msgs << "Adversarial:  " << boolToString(m_adversarial)      << endl;
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
  if(m_adversarial)
    m_msgs << "Rival plans known:       " << m_plan_claims.size() << endl;
  m_msgs << "---------------------------------------"            << endl;
  m_msgs << "Paths posted:            " << m_plans_posted        << endl;
  m_msgs << "MCTS runs / fallbacks:   " << m_mcts_runs << " / "
         << m_mcts_fallbacks                                     << endl;
  m_msgs << "Plan pending:            " << boolToString(m_plan_pending) << endl;
  m_msgs << "Current path length:     " << m_path.size() << " pts" << endl;

  return(true);
}
