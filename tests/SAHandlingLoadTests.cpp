// Real SA registry installation + unit conversion, no game assets or GL context.
#include "common.h"
#include "HandlingMgr.h"
#include "Timer.h"
#include "SAHandlingData.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
float CTimer::ms_fTimeStep=1.0f;
void re3_assert(const char *,const char *,unsigned int,const char *) { std::abort(); }
void re3_log(const char *,const char *,unsigned int,const char *,const char *,...) {}
static const char *landstal="LANDSTAL 1800 5000 0.5 0 0 -0.3 70 0.65 0.85 0.49 5 290 15.9 55 4 P 12 0.6 1 33 0.9 0.2 0 0.3 -0.22 0.5 0.3 0.2 0.3 50000 2000 300002 1 1 0";
static void Check(bool ok,const char *why) { if(!ok){ std::fprintf(stderr,"SAHandlingLoadTests: %s\n",why); std::abort(); } }
static void Near(float a,float b,const char *why) { Check(std::fabs(a-b)<0.00001f,why); }
static bool Install(cHandlingDataMgr &m,const std::string &s,std::string *error=nullptr)
{ return m.LoadSAHandlingData(reinterpret_cast<const uint8*>(s.data()),s.size(),error); }
static bool Parse(const std::string &s,sahandling::Table &table)
{ return sahandling::Parse(reinterpret_cast<const uint8*>(s.data()),s.size(),table); }
int main(int argc,char **argv)
{
    const std::string first=std::string(landstal)+"\n";
    cHandlingDataMgr manager;
    Check(Install(manager,first),"valid SA record installed");
    Check(manager.HasSAHandlingData(),"SA registry enabled");
    int id=manager.GetHandlingId("LANDSTAL");
    Check(manager.IsSAHandling(id),"existing identity resolves to SA record");
    tHandlingData *h=manager.GetHandlingData((tVehicleType)id);
    Check(h->bSAData,"provenance preserved");
    Near(h->fMass,1800,"mass"); Near(h->fTurnMass,5000,"turn mass not recomputed from VC dimensions");
    Near(h->fDragMult,0.5f,"SA drag"); Near(h->GetAirResistance(),0.00025f,"SA default air resistance");
    Near(h->Transmission.fEngineInertia,55,"engine inertia loaded, not default");
    Near(h->Transmission.fEngineAcceleration,15.9f*0.4f/2500.0f/4.0f,"acceleration converted exactly once");
    Near(h->fBrakeDeceleration,12.0f/2500.0f,"braking conversion");
    Near(h->fInvMass,1.0f/1800,"inverse mass");
    Near(h->fBuoyancy,1800.0f*0.8f/70.0f,"SA buoyancy");
    Near(h->fCollisionDamageMultiplier,0.3f/1800.0f*2000.0f,"collision damage units");
    Check(h->nModelFlags==0x2000 && h->nHandlingFlags==0x300002,"independent flag words");
    Check(h->Transmission.Flags==h->nHandlingFlags,"transmission receives only SA handling flags");
    Check(manager.HasFrontWheelDrive((tVehicleType)id) && manager.HasRearWheelDrive((tVehicleType)id),"4WD");
    std::string error;
    Check(!Install(manager,first,&error) && !error.empty(),"live reload cannot invalidate stored pointers");
    Check(manager.GetHandlingData((tVehicleType)id)==h,"pointer remains valid after refused reload");
    sahandling::Table table;
    Check(Parse("\xEF\xBB\xBF\xEF\xBB\xBF"+first,table),"double BOM in supplied BR table");
    Check(Parse(first.substr(0,first.size()-1),table),"EOF without newline");
    Check(Parse(std::string(landstal)+" 0.1 120 -0.4\n",table) && table.cars[0].extensions.size()==3,"BR extension fields retained");
    Check(!Parse(std::string(landstal)+" nan\n",table),"nonfinite extension rejected");
    Check(!Parse(first+first,table),"duplicate base identities rejected");
    Check(!Parse(std::string(2048,'x')+"\n",table),"oversized line rejected");
    Check(!Parse(std::string("X\0Y",3),table),"embedded NUL rejected");
    // Every mandatory field truncation must fail without replacing output.
    std::string truncated=landstal;
    while(truncated.find_last_of(' ')!=std::string::npos){
        truncated.resize(truncated.find_last_of(' '));
        Check(!Parse(truncated,table),"missing mandatory field rejected");
    }
    const char *bad[]={"nan","inf","-1","1e99"};
    for(const char *mass:bad){
        std::string row=landstal; row.replace(row.find("1800"),4,mass);
        Check(!Parse(row,table),"invalid mass rejected");
    }
    // Independent auxiliary arrays must use full-width IDs, not uint8 or
    // arithmetic offsets into VC's fixed bike/boat/flying ranges.
    cHandlingDataMgr extended;
    std::string many;
    for(int i=0;i<300;++i){
        std::string row=landstal;
        row.replace(0,row.find(' '),"EXTRA_"+std::to_string(i));
        many+=row+"\n";
    }
    many+="! EXTRA_299 0.35 0.15 0.34 0.1 45 38 0.93 0.7 0.5 0.1 35 -40 -0.009 0.7 0.6\n";
    many+="% EXTRA_299 0.5 0.6 0.5 6 0.6 -0.8 4 0.7 0.998 0.999 0.85 0.98 0.96 4\n";
    many+="$ EXTRA_299 0.5 0.4 -0.00006 0.002 0.1 0.002 -0.006 0.0002 0.002 0.02 0.15 1 1 0.2 1 0.998 0.998 0.995 20 50 20\n";
    Check(Install(extended,many),"extended registry with auxiliary data");
    const int extendedId=extended.GetHandlingId("EXTRA_299");
    Check(extended.GetHandlingId("DOES_NOT_EXIST")==-1,"unknown SA name must not alias first dynamic ID");
    Check(extendedId>255,"test exercises an identifier above uint8");
    Check(extended.HasSABikeHandling(extendedId) && extended.HasSABoatHandling(extendedId) && extended.HasSAFlyingHandling(extendedId),"all auxiliary records keyed by full ID");
    Near(extended.GetBikePointer(extendedId)->fMaxLean,std::sin(45.0f*3.14159265358979323846f/180.0f),"SA bike conversion");
    Near(extended.GetBoatPointer(extendedId)->fLook_L_R_BehindCamHeight,4,"boat final field");
    Near(extended.GetFlyingPointer(extendedId)->fGearUpR,1,"SA flying gear-up field");
    Near(extended.GetFlyingPointer(extendedId)->fWindMult,0.2f,"SA flying wind field");
    Near(extended.GetFlyingPointer(extendedId)->vecSpeedRes.y,50,"SA flying field alignment");
    if(argc>1){
        std::ifstream file(argv[1],std::ios::binary);
        std::vector<uint8> bytes((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
        cHandlingDataMgr real;
        Check(real.LoadSAHandlingData(bytes.data(),bytes.size(),&error),error.c_str());
        sahandling::Table parsed;
        Check(sahandling::Parse(bytes.data(),bytes.size(),parsed),"real table parse");
        for(const auto &record:parsed.cars){
            int recordId=real.GetHandlingId(record.name.c_str());
            Check(real.IsSAHandling(recordId),"all real record names resolve");
            auto *data=real.GetHandlingData((tVehicleType)recordId);
            Near(data->fTurnMass,record.turnMass,"real turn mass preserved");
            Check(std::isfinite(data->Transmission.fMaxVelocity),"finite real terminal velocity");
        }
        std::printf("Real BR handling: %zu vehicles, %zu bikes, %zu boats, %zu flying records\n",parsed.cars.size(),parsed.bikes.size(),parsed.boats.size(),parsed.flying.size());
    }
    std::puts("SAHandlingLoadTests: ALL PASSED");
}
