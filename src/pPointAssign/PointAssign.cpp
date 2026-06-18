/************************************************************/
/*    NAME: Evangelos Siatis                                */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: PointAssign.cpp                                 */
/*    DATE: 2026                                            */
/************************************************************/

#include <iterator>
#include "MBUtils.h"
#include "ACTable.h"
#include "PointAssign.h"
#include "XYPoint.h"              // gia na ftiaxnoume/serialize-aroume simeia
#include "XYFormatUtilsPoint.h"   // gia to string2Point()

using namespace std;

//---------------------------------------------------------
// Constructor: arxikopoiisi olwn twn metavlitwn katastasis
PointAssign::PointAssign()
{
  m_assign_by_region = false;   // default: enallaks (alternating)
  m_region_xmin      = -25;     // ta oria X tis perioxis (apo to lab)
  m_region_xmax      = 200;
  m_broadcast_all    = false;   // [Lab07_upgr] default OFF -> palio lab_07 idio

  m_points_total     = 0;
  m_invalid_points   = 0;
  m_got_first        = false;
  m_got_last         = false;
  m_uts_unpaused     = false;
  m_start_time       = -1;      // tha tethei stin prwti Iterate
}

//---------------------------------------------------------
// Destructor
PointAssign::~PointAssign()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()
// [ΣΗΜ] Edw ftanoun ta VISIT_POINT apo to uTimerScript.
bool PointAssign::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p=NewMail.begin(); p!=NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key    = msg.GetKey();
    string sval   = msg.GetString();

    if(key == "VISIT_POINT")
      handleVisitPoint(sval);
    else if(key == "POINT_CLAIMED")
      handlePointClaimed(sval);
    else if(key == "PLAN_CLAIM")
      // [Lab07_upgr] passthrough relay: schedio enos oximatos -> ola ta oximata
      //   (mesw ShoreBroker qbridge=PLAN_CLAIM). Diatiroume to payload (vname/pos/ids).
      Notify("PLAN_CLAIM_ALL", sval);
    else if(key == "NODE_REPORT") {
      // [ΣΗΜ HANDSHAKING] Kathe oxima stelnei NODE_REPORT otan einai syndedemeno.
      //   Otan to laboume, simainei oti to oxima einai UP kai ta qbridge routes
      //   (VISIT_POINT_<V> -> oxima) exoun stithei. Tote MONO ksepagwnoume.
      string vname = tolower(tokStringParse(sval, "NAME", ',', '='));
      for(unsigned int i=0; i<m_vnames.size(); i++)
        if(m_vnames[i] == vname)
          m_ready.insert(vname);
    }
    else if(key != "APPCAST_REQ") // handled by AppCastingMOOSApp
      reportRunWarning("Unhandled Mail: " + key);
  }

  return(true);
}

//---------------------------------------------------------
// Procedure: handleVisitPoint()
// [ΣΗΜ] H kardia tou app. Pairnei ena eiserxomeno string kai apofasizei:
//   - "firstpoint"/"lastpoint" -> proothei to marker se OLA ta oximata
//   - kanoniko simeio "x=..,y=..,id=.." -> to anathetei se ENA oxima
void PointAssign::handleVisitPoint(const string& sval)
{
  // --- Periptwsi 1: simaia enarksis. Proothise se kathe oxima. ---
  if(sval == "firstpoint") {
    m_got_first = true;
    for(unsigned int i=0; i<m_vnames.size(); i++)
      Notify("VISIT_POINT_" + toupper(m_vnames[i]), "firstpoint");
    return;
  }

  // --- Periptwsi 2: simaia liksis. Proothise se kathe oxima. ---
  if(sval == "lastpoint") {
    m_got_last = true;
    for(unsigned int i=0; i<m_vnames.size(); i++)
      Notify("VISIT_POINT_" + toupper(m_vnames[i]), "lastpoint");
    return;
  }

  // --- Periptwsi 3: kanoniko simeio. Prwta to parse-aroume. ---
  // Elegxos egkyrotitas: prepei na exei x= kai y=
  if(!strContains(sval, "x=") || !strContains(sval, "y=")) {
    m_invalid_points++;
    reportRunWarning("Invalid VISIT_POINT: " + sval);
    return;
  }

  XYPoint point = string2Point(sval);   // string -> antikeimeno
  double x = point.get_vx();
  double y = point.get_vy();
  string id = tokStringParse(sval, "id", ',', '=');  // vgazoume to id
  if(id == "")
    id = uintToString(m_points_total + 1);

  // Den uparxoun oximata? Den mporoume na anathesoume.
  if(m_vnames.size() == 0) {
    m_invalid_points++;
    return;
  }

  // [Lab07_upgr] BROADCAST MODE: stelne to IDIO simeio se OLA ta oximata. Kathe
  //   oxima exei to pliris set kai diekdikei dynamika (MCTS + claiming). To
  //   xrwma sto viewer = oudetero (to telos xrwmatizetai apo to oxima pou to pairnei).
  if(m_broadcast_all) {
    for(unsigned int i=0; i<m_vnames.size(); i++)
      Notify("VISIT_POINT_" + toupper(m_vnames[i]), sval);
    postViewPoint(x, y, "pt_" + id, "white");
    m_points_total++;
    return;
  }

  // --- (Palia symperifora) Apofasi: se POIO oxima paei (index sto m_vnames) ---
  unsigned int idx;
  if(m_assign_by_region)
    idx = chooseVehicle(x);                 // me vasi tin thesi X (east/west)
  else
    idx = m_points_total % m_vnames.size(); // enallaks (round-robin)

  // --- Proothisi: postare to IDIO ACRIVWS string sto sosto oxima ---
  // px VISIT_POINT_HENRY = "x=8, y=9, id=1"
  Notify("VISIT_POINT_" + toupper(m_vnames[idx]), sval);

  // --- Optiko: zwgrafise to simeio sto pMarineViewer me xrwma tou oximatos ---
  postViewPoint(x, y, "pt_" + id, colorForIndex(idx));

  // --- Enimerwsi metrities ---
  m_vcount[idx]++;
  m_points_total++;
}

//---------------------------------------------------------
// Procedure: chooseVehicle()
// [ΣΗΜ] Xwrizei tin perioxi [xmin,xmax] se N kathetes lwrides (mia ana oxima)
//   kai epistrefei se poia lwrida peftei to x. Gia 2 oximata: dytika->0, anatolika->1.
unsigned int PointAssign::chooseVehicle(double x)
{
  unsigned int n = m_vnames.size();
  if(n <= 1) return(0);

  double width = (m_region_xmax - m_region_xmin) / (double)n;
  if(width <= 0) return(0);

  int idx = (int)((x - m_region_xmin) / width);
  if(idx < 0) idx = 0;
  if(idx >= (int)n) idx = n - 1;   // clamp sta oria
  return((unsigned int)idx);
}

//---------------------------------------------------------
// Procedure: handlePointClaimed()
// [Lab07_upgr] Ena oxima diekdikise simeio. To string: "vname=<v>,id=<n>".
//   Rolos mas (sto shoreside):
//   (1) DEDUPE me to id (mono i prwti diekdikisi metraei).
//   (2) SCOREBOARD: +1 sto oxima, postare SCORE_<VNAME>.
//   (3) RELAY: postare POINT_CLAIMED_ALL=<id> -> to uFldShoreBroker qbridge to
//       skorpaei se OLA ta oximata (-> POINT_CLAIMED), wste to allo oxima na to
//       afairesei apo ti diadromi tou. (Apofeugoume diples episkepseis.)
void PointAssign::handlePointClaimed(const string& sval)
{
  string vname = tolower(tokStringParse(sval, "vname", ',', '='));
  string id    = tokStringParse(sval, "id", ',', '=');
  if(id == "")
    return;

  // (1) dedupe
  if(m_claimed_ids.count(id) > 0)
    return;
  m_claimed_ids.insert(id);

  // (2) scoreboard
  for(unsigned int i=0; i<m_vnames.size(); i++) {
    if(m_vnames[i] == vname) {
      m_vscore[i]++;
      Notify("SCORE_" + toupper(m_vnames[i]), (double)m_vscore[i]);
      break;
    }
  }

  // (3) relay pros ola ta oximata (mesw ShoreBroker qbridge=POINT_CLAIMED)
  Notify("POINT_CLAIMED_ALL", "id=" + id);
}

//---------------------------------------------------------
// Procedure: postViewPoint()
// [ΣΗΜ] Stelnei ena VIEW_POINT wste to pMarineViewer na to zwgrafisei.
//   To label PREPEI na einai monadiko (alliws o viewer grafei to ena panw sto allo).
void PointAssign::postViewPoint(double x, double y, string label, string color)
{
  XYPoint point(x, y);
  point.set_label(label);
  point.set_color("vertex", color);
  point.set_param("vertex_size", "4");

  string spec = point.get_spec();   // string anaparastasi tou simeiou
  Notify("VIEW_POINT", spec);
}

//---------------------------------------------------------
// Procedure: colorForIndex()
// [ΣΗΜ] Diaforetiko xrwma ana oxima (gia na ksexwrizoun ston xarti).
string PointAssign::colorForIndex(unsigned int index)
{
  static const char* palette[] = {"yellow","dodger_blue","red","green","white","orange"};
  unsigned int n = sizeof(palette)/sizeof(palette[0]);
  return(palette[index % n]);
}

//---------------------------------------------------------
// Procedure: OnConnectToServer()
bool PointAssign::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate() - happens AppTick times per second
bool PointAssign::Iterate()
{
  AppCastingMOOSApp::Iterate();

  if(m_start_time < 0)
    m_start_time = MOOSTime();

  // [ΣΗΜ HANDSHAKING] Ksepagwnoume to uTimerScript MONO otan OLA ta oximata
  //   exoun steilei NODE_REPORT (= einai etoima na lavoun). Etsi:
  //   (a) ta qbridge routes pros ta oximata exoun stithei -> den xanontai ta simeia
  //   (b) to diko mas register gia VISIT_POINT exei "kathisei" -> den xanetai to firstpoint
  //   Fallback: an meta apo 30s den irthan ola, ksepagwse etsi ki alliws (me warning).
  if(!m_uts_unpaused) {
    bool all_ready  = (m_ready.size() >= m_vnames.size());
    bool timed_out  = ((MOOSTime() - m_start_time) > 30);
    if(all_ready || timed_out) {
      if(timed_out && !all_ready)
        reportRunWarning("Timeout: ksepagwma uTimerScript xwris ola ta oximata");
      Notify("UTS_PAUSE", "false");
      m_uts_unpaused = true;
    }
  }

  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp() - happens before connection is open
bool PointAssign::OnStartUp()
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
    if(param == "vname") {
      // [ΣΗΜ] Prosthetei ena oxima sti lista. KALEITAI MIA fora ana oxima.
      //   ta onomata DEN einai hardcoded -> ercontai apo to .moos.
      m_vnames.push_back(tolower(value));
      m_vcount.push_back(0);
      m_vscore.push_back(0);
      handled = true;
    }
    else if(param == "assign_by_region") {
      handled = setBooleanOnString(m_assign_by_region, value);
    }
    else if(param == "broadcast_all") {
      handled = setBooleanOnString(m_broadcast_all, value);
    }
    else if(param == "region_xmin") {
      handled = setDoubleOnString(m_region_xmin, value);
    }
    else if(param == "region_xmax") {
      handled = setDoubleOnString(m_region_xmax, value);
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  // [ΣΗΜ] Asfaleia: an den dothike kanena vname, valе henry+gilda gia na trexei.
  if(m_vnames.size() == 0) {
    reportConfigWarning("No vname given - using default henry, gilda");
    m_vnames.push_back("henry"); m_vcount.push_back(0); m_vscore.push_back(0);
    m_vnames.push_back("gilda"); m_vcount.push_back(0); m_vscore.push_back(0);
  }

  registerVariables();
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()
void PointAssign::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("VISIT_POINT", 0);   // [ΣΗΜ] grafomaste gia ta simeia tou uTimerScript
  Register("NODE_REPORT", 0);   // [ΣΗΜ] gia na kseroume pote ta oximata einai etoima
  Register("POINT_CLAIMED", 0); // [Lab07_upgr] episkepseis apo ta oximata (claim-relay+score)
  Register("PLAN_CLAIM", 0);    // [Lab07_upgr] schedia oximatwn (passthrough relay)
}

//------------------------------------------------------------
// Procedure: buildReport()
// [ΣΗΜ] Auto fainetai sto AppCasting panel -> debug.
bool PointAssign::buildReport()
{
  m_msgs << "Config:                                     " << endl;
  m_msgs << "  Broadcast all:    " << boolToString(m_broadcast_all)
         << "  (Lab07_upgr: ola ta simeia se ola ta oximata)" << endl;
  m_msgs << "  Assign by region: " << boolToString(m_assign_by_region) << endl;
  m_msgs << "  Region X:         [" << doubleToStringX(m_region_xmin,1)
         << ", " << doubleToStringX(m_region_xmax,1) << "]" << endl;
  m_msgs << "  Vehicles:         " << m_vnames.size() << endl;
  m_msgs << "  Vehicles ready:   " << m_ready.size() << " ("
         << boolToString(m_uts_unpaused) << " unpaused)" << endl;
  m_msgs << endl;
  m_msgs << "First point recvd:  " << boolToString(m_got_first) << endl;
  m_msgs << "Last point recvd:   " << boolToString(m_got_last)  << endl;
  m_msgs << "Total points:       " << m_points_total << endl;
  m_msgs << "Invalid points:     " << m_invalid_points << endl;
  m_msgs << endl;

  ACTable actab(3);
  actab << "Vehicle | Points Assigned | SCORE (claimed)";
  actab.addHeaderLines();
  for(unsigned int i=0; i<m_vnames.size(); i++)
    actab << m_vnames[i] << uintToString(m_vcount[i]) << uintToString(m_vscore[i]);
  m_msgs << actab.getFormattedString();
  m_msgs << endl;
  m_msgs << "[Lab07_upgr] Total claimed (unique): " << m_claimed_ids.size() << endl;

  return(true);
}
