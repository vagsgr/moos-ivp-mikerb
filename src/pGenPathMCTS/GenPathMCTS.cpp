/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: GenPathMCTS.cpp                                  */
/*    DATE: 2026                                            */
/************************************************************/

#include <iterator>
#include <cmath>
#include <cstdlib>     // system()
#include <fstream>     // ofstream/ifstream gia ta tmp arxeia tou planner
#include <sstream>
#include "MBUtils.h"
#include "ACTable.h"
#include "GenPathMCTS.h"
#include "XYSegList.h"
#include "XYFormatUtilsPoint.h"   // string2Point()

using namespace std;

//---------------------------------------------------------
// Constructor
GenPathMCTS::GenPathMCTS()
{
  // --- defaults (mporoun na allaksoun apo to .moos) ---
  m_waypt_update_var = "TOUR_UPDATE";       // tairiazei me to updates=.. tou BHV_Waypoint
  m_planner_script   = "mcts_planner.py";   // sto idio dir me to launch.sh
  m_vname            = "";                   // tha tethei apo config (vname=$(VNAME))
  m_visit_radius     = 3;                    // "episkepsi" otan <3m (apo to lab)
  m_budget           = 720;                  // 600s underway * 1.2 m/s = 720m collecting-path
  m_replan_interval  = 5;                    // min 5s metaksy postings (na min "tradzetai" o helm)

  m_nav_x          = 0;
  m_nav_y          = 0;
  m_nav_ok         = false;
  m_got_first      = false;
  m_got_last       = false;
  m_path_posted    = false;
  m_replan_pending = false;
  m_last_plan_time = -1;

  m_my_visits      = 0;
  m_last_tour_size = 0;
  m_plan_count     = 0;
}

//---------------------------------------------------------
// Destructor
GenPathMCTS::~GenPathMCTS()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()
bool GenPathMCTS::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();

    if(key == "VISIT_POINT")
      handleVisitPoint(msg.GetString());
    else if(key == "POINT_CLAIMED")
      handlePointClaimed(msg.GetString());
    else if(key == "PLAN_CLAIM")
      handlePlanClaim(msg.GetString());
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
// [ΣΗΜ] Edw pleon ftanoun OLA ta simeia (to pPointAssign trexei se broadcast mode).
//   "firstpoint" = arxi neou set (reset), "lastpoint" = irthan ola.
void GenPathMCTS::handleVisitPoint(const string& sval)
{
  if(sval == "firstpoint") {
    m_ids.clear();
    m_xs.clear();
    m_ys.clear();
    m_visited.clear();
    // [ΣΗΜ] DEN kanoume clear to m_claimed: oi diekdikiseis isxyoun synexeia
    //   (se enan epomeno gyro re-tour den theloume na ksanapame se idi-parmena).
    m_got_first      = true;
    m_got_last       = false;
    m_path_posted    = false;
    m_replan_pending = false;
    return;
  }

  if(sval == "lastpoint") {
    m_got_last       = true;   // h generatePath() (stin Iterate) tha to dei
    m_replan_pending = true;
    return;
  }

  // Kanoniko simeio: parse x,y,id kai apothikeusi.
  if(!strContains(sval, "x=") || !strContains(sval, "y="))
    return;   // agnoise mi-egkyra

  XYPoint point = string2Point(sval);
  string  id    = tokStringParse(sval, "id", ',', '=');
  if(id == "")
    id = uintToString(m_ids.size() + 1);

  m_ids.push_back(id);
  m_xs.push_back(point.get_vx());
  m_ys.push_back(point.get_vy());
  m_visited.push_back(false);
}

//---------------------------------------------------------
// Procedure: handlePointClaimed()
// [ΣΗΜ] Kapoios (emeis i to allo oxima) diekdikise ena simeio. To string einai
//   "vname=<v>,id=<n>" (i sketo to id). Vgazoume to id, to vazoume sto m_claimed
//   wste o planner na MIN to ksanavalei. An den einai diko mas (den to eixame idi
//   episkeftei) -> kanoume re-plan gia na to afairesoume apo ti diadromi mas.
void GenPathMCTS::handlePointClaimed(const string& sval)
{
  string id = tokStringParse(sval, "id", ',', '=');
  if(id == "")
    id = sval;   // fallback: olo to string einai to id
  id = stripBlankEnds(id);
  if(id == "")
    return;

  bool was_new = (m_claimed.count(id) == 0);
  m_claimed.insert(id);

  // An to simeio einai sti d-iki mas lista kai DEN to exoume episkeftei akoma,
  // tote allaksan ta dedomena -> thelei re-plan.
  if(was_new) {
    for(unsigned int i=0; i<m_ids.size(); i++) {
      if(m_ids[i] == id && !m_visited[i]) {
        m_replan_pending = true;
        break;
      }
    }
  }
}

//---------------------------------------------------------
// Procedure: handlePlanClaim()
// [Lab07_upgr] Allo oxima anakoinwse to "schedio" tou: "vname=<v>,px=..,py=..,
//   ids=a:b:c..". To apothikeuoume (antikathista to proigoumeno tou). Sto epomeno
//   plan tha to lavoume ypopsi (distance-based arbitration). Ta DIKA mas ta agnooume.
void GenPathMCTS::handlePlanClaim(const string& sval)
{
  string vname = tolower(tokStringParse(sval, "vname", ',', '='));
  if(vname == "" || vname == m_vname)
    return;   // diko mas i axreasto

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

  m_plan_claims[vname]  = pc;
  m_replan_pending      = true;   // allakse i katanomi -> ksanasxediase
}

//---------------------------------------------------------
// Procedure: excludedByCloserOther()
// [Lab07_upgr] To simeio (id sti thesi x,y) prepei na to PARAXWRISOUME se allo
//   oxima an EKEINO einai PIO KONTA (kai to exei sto schedio tou). Isopalia
//   apostasis -> spaei me to onoma (mikrotero lexika = proteraiotita). Etsi
//   prokyptei enas Voronoi-like diamoirasmos xwris overlap.
bool GenPathMCTS::excludedByCloserOther(const string& id, double x, double y)
{
  double my_d = hypot(x - m_nav_x, y - m_nav_y);
  map<string, PlanClaim>::iterator it;
  for(it=m_plan_claims.begin(); it!=m_plan_claims.end(); it++) {
    const string&    ovname = it->first;
    const PlanClaim& pc     = it->second;
    if(pc.ids.count(id) == 0)
      continue;   // to allo oxima den to thelei -> den mas afora
    double other_d = hypot(x - pc.px, y - pc.py);
    if(other_d < my_d)
      return true;                                  // einai pio konta -> dikо tou
    if(other_d == my_d && ovname < m_vname)
      return true;                                  // isopalia -> proteraiotita onomatos
  }
  return false;
}

//---------------------------------------------------------
// Procedure: generatePath()
// [ΣΗΜ] H kardia tou upgrade. Anti gia greedy, kaloume ton MCTS planner se python:
//   1) grafoume ena tmp arxeio: start(NAV), budget, kai OSA simeia den exoun
//      episkeftei OUTE diekdikithei.
//   2) trexoume "python3 mcts_planner.py in out" (mplokarei, <1s).
//   3) diavazoume ti seira pou epestrepse kai postaroume TOUR_UPDATE.
void GenPathMCTS::generatePath()
{
  // --- mazepse ta ypopsifia simeia ---
  //   skip an: (a) to episkeftikame, (b) to episkeftike kapoios (permanent claim),
  //   (c) allo oxima einai pio konta kai to thelei (distance-based arbitration).
  vector<unsigned int> cand;
  for(unsigned int i=0; i<m_ids.size(); i++) {
    if(m_visited[i])
      continue;
    if(m_claimed.count(m_ids[i]) > 0)
      continue;
    if(excludedByCloserOther(m_ids[i], m_xs[i], m_ys[i]))
      continue;
    cand.push_back(i);
  }
  if(cand.empty())
    return;   // tipota na sxediasoume

  // --- onomata tmp arxeion (ana oxima, gia na min synkrouontai) ---
  string base    = "/tmp/genpath_" + (m_vname=="" ? string("veh") : m_vname);
  string infile  = base + "_in.txt";
  string outfile = base + "_out.txt";

  // --- 1) grapse to input gia ton planner ---
  {
    ofstream ofs(infile.c_str());
    if(!ofs.good()) {
      reportRunWarning("Cannot write planner input: " + infile);
      return;
    }
    ofs << "start "  << m_nav_x << " " << m_nav_y << "\n";
    ofs << "budget " << m_budget << "\n";
    for(unsigned int k=0; k<cand.size(); k++) {
      unsigned int i = cand[k];
      ofs << "point " << m_ids[i] << " " << m_xs[i] << " " << m_ys[i] << "\n";
    }
  }

  // --- 2) trekse ton planner (mplokarei) ---
  string cmd = "python3 " + m_planner_script + " " + infile + " " + outfile;
  int rc = system(cmd.c_str());
  m_plan_count++;
  if(rc != 0) {
    reportRunWarning("MCTS planner returned non-zero: " + intToString(rc));
    return;
  }

  // --- 3) diavase ti seira kai ftiakse XYSegList ---
  ifstream ifs(outfile.c_str());
  if(!ifs.good()) {
    reportRunWarning("Cannot read planner output: " + outfile);
    return;
  }
  XYSegList seglist;
  vector<string> tour_ids;     // [Lab07_upgr] ta id tis diadromis (gia to PLAN_CLAIM)
  unsigned int npts = 0;
  string line;
  while(getline(ifs, line)) {
    if(line.empty() || line[0] == '#')
      continue;             // scholio "# count=.. length=.."
    istringstream iss(line);
    string id; double x, y;
    if(iss >> id >> x >> y) {
      seglist.add_vertex(x, y);
      tour_ids.push_back(id);
      npts++;
    }
  }
  if(npts == 0)
    return;

  // --- postare ti diadromi sto waypoint behavior ---
  string update_str = "points = " + seglist.get_spec();
  Notify(m_waypt_update_var, update_str);

  // [Lab07_upgr] anakoinwse to "schedio" mas (thesi + id) -> ta alla oximata kanoun
  //   distance-arbitration kai apofeugoume overlap.
  string ids_str;
  for(unsigned int k=0; k<tour_ids.size(); k++)
    ids_str += (k ? ":" : "") + tour_ids[k];
  string plan = "vname=" + m_vname + ",px=" + doubleToStringX(m_nav_x,2) +
                ",py=" + doubleToStringX(m_nav_y,2) + ",ids=" + ids_str;
  Notify("PLAN_CLAIM", plan);

  m_path_posted    = true;
  m_replan_pending = false;
  m_last_plan_time = MOOSTime();
  m_last_tour_size = npts;
}

//---------------------------------------------------------
// Procedure: updateVisited()
// [ΣΗΜ] Otan to oxima ftasei konta (<visit_radius) se mi-episkemmeno simeio:
//   (a) simeiwse to ws visited, (b) DIEKDIKISE to -> Notify POINT_CLAIMED me to
//   onoma mas + id (claim-on-visit). To shoreside tha to skorpisei sto allo oxima.
void GenPathMCTS::updateVisited()
{
  if(!m_nav_ok)
    return;
  for(unsigned int i=0; i<m_ids.size(); i++) {
    if(m_visited[i])
      continue;
    double dx = m_xs[i] - m_nav_x;
    double dy = m_ys[i] - m_nav_y;
    if(hypot(dx, dy) <= m_visit_radius) {
      m_visited[i] = true;
      m_claimed.insert(m_ids[i]);
      m_my_visits++;
      Notify("POINT_CLAIMED", "vname=" + m_vname + ",id=" + m_ids[i]);
    }
  }
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()
bool GenPathMCTS::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
bool GenPathMCTS::Iterate()
{
  AppCastingMOOSApp::Iterate();

  // [ΣΗΜ] Prwta tsekare episkepseis/diekdikiseis (mporei na thesoun replan_pending).
  updateVisited();

  // [ΣΗΜ] Sxediase otan: exoume OLA ta simeia + thesi oximatos, KAI eite den
  //   exoume postarei akoma eite kati allakse (replan_pending). Me throttle
  //   (replan_interval) gia na min vombardizoume ton helm me TOUR_UPDATE.
  bool need_plan = (!m_path_posted || m_replan_pending);
  bool throttle_ok = (m_last_plan_time < 0) ||
                     ((MOOSTime() - m_last_plan_time) >= m_replan_interval);
  if(m_got_last && m_nav_ok && need_plan && throttle_ok)
    generatePath();

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
bool GenPathMCTS::OnStartUp()
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
    else if(param == "planner_script") {
      m_planner_script = value;
      handled = true;
    }
    else if(param == "vname") {
      m_vname = tolower(value);
      handled = true;
    }
    else if(param == "visit_radius") {
      handled = setDoubleOnString(m_visit_radius, value);
    }
    else if(param == "budget") {
      handled = setDoubleOnString(m_budget, value);
    }
    else if(param == "replan_interval") {
      handled = setDoubleOnString(m_replan_interval, value);
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // [ΣΗΜ] An den dothike vname, dokimase to community name (= onoma oximatos edw).
  if(m_vname == "") {
    m_vname = tolower(m_host_community);
    if(m_vname == "")
      reportConfigWarning("No vname given - claims will be untagged");
  }

  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()
void GenPathMCTS::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);     // OLA ta simeia (broadcast apo to shoreside)
  Register("POINT_CLAIMED", 0);   // episkepseis (permanent, apo emas i to allo oxima)
  Register("PLAN_CLAIM", 0);      // schedia (proθeseis) twn allwn oximatwn
  Register("NAV_X", 0);           // trexousa thesi oximatos
  Register("NAV_Y", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()
bool GenPathMCTS::buildReport()
{
  unsigned int unclaimed = 0;
  for(unsigned int i=0; i<m_ids.size(); i++)
    if(!m_visited[i] && m_claimed.count(m_ids[i])==0 &&
       !excludedByCloserOther(m_ids[i], m_xs[i], m_ys[i]))
      unclaimed++;

  m_msgs << "Vehicle:                 " << m_vname << endl;
  m_msgs << "Planner:                 " << m_planner_script << endl;
  m_msgs << "Budget (m):              " << doubleToStringX(m_budget,1) << endl;
  m_msgs << "Visit Radius:            " << doubleToStringX(m_visit_radius,1) << endl;
  m_msgs << "Update Var:              " << m_waypt_update_var << endl;
  m_msgs << "NAV_X/Y Received:        " << boolToString(m_nav_ok) << endl;
  m_msgs << endl;
  m_msgs << "Points" << endl;
  m_msgs << "------------------------" << endl;
  m_msgs << "   Total Received:       " << m_ids.size() << endl;
  m_msgs << "   First/Last:           " << boolToString(m_got_first)
         << " / " << boolToString(m_got_last) << endl;
  m_msgs << "   Visited (all veh):    " << m_claimed.size() << endl;
  m_msgs << "   Other plans known:    " << m_plan_claims.size() << endl;
  m_msgs << "   Mine to plan (avail): " << unclaimed << endl;
  m_msgs << endl;
  m_msgs << "Planning" << endl;
  m_msgs << "------------------------" << endl;
  m_msgs << "   Path Posted:          " << boolToString(m_path_posted) << endl;
  m_msgs << "   Replan Pending:       " << boolToString(m_replan_pending) << endl;
  m_msgs << "   Planner Runs:         " << m_plan_count << endl;
  m_msgs << "   Last Tour Size:       " << m_last_tour_size << endl;
  m_msgs << "   MY Visits (claimed):  " << m_my_visits << endl;

  return(true);
}
