// Numerical tests of the REAL SA transmission implementation; no RW/GPU assets.
#include "common.h"
#include "Timer.h"
#include "Transmission.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
float CTimer::ms_fTimeStep = 1.0f;
static void Check(bool ok, const char *why)
{
    if(!ok){ std::fprintf(stderr,"SATransmissionTests: %s\n",why); std::abort(); }
}
static void Near(float a,float b,const char *why) { Check(std::fabs(a-b)<0.00001f,why); }
static cTransmission Transmission(int gears = 5)
{
    cTransmission t = {};
    t.nNumberOfGears = gears;
    t.nDriveType = 'R'; t.nEngineType = 'P';
    t.fEngineAcceleration = 0.02f; t.fEngineInertia = 10.0f;
    t.fMaxVelocity = 1.0f; t.fMaxReverseVelocity = -0.2f;
    t.InitGearRatios();
    return t;
}
static float Drive(cTransmission &t, uint8 &gear, float speed, float gas = 1.0f,
                   eTransmissionCheat cheat = TRANSMISSION_CHEAT_NONE,
                   float *ratio = nullptr, float *smooth = nullptr, bool grounded = true)
{
    float count = 123.0f;
    float result = t.CalculateDriveAcceleration(gas,gear,count,speed,ratio,smooth,grounded,cheat);
    Check(count == 123.0f,"SA leaves gear-change counter unchanged");
    return result;
}
int main()
{
    cTransmission t = Transmission();
    // SA half-gear offset: VC would produce 0.2,0.4,...,1.0 instead.
    const float maxima[] = {-0.2f,0.28f,0.46f,0.64f,0.82f,1.0f};
    for(int i=0;i<6;++i) Near(t.Gears[i].fMaxVelocity,maxima[i],"SA gear spacing");
    Near(t.Gears[1].fShiftUpVelocity,0.28f*0.6667f,"first upshift threshold");
    Near(t.Gears[2].fShiftDownVelocity,0.28f*0.42f,"second downshift threshold");
    Near(t.Gears[1].fShiftDownVelocity,-0.01f,"forward/reverse threshold");
    Near(t.Gears[0].fShiftUpVelocity,-0.01f,"reverse/forward threshold");
    Near(t.Gears[5].fShiftUpVelocity,1.0f,"top-gear cap");
    uint8 gear = 5;
    t.CalculateGearForSimpleCar(100.0f,gear); Check(gear==5,"simple traffic gear capped");
    gear = 0; t.CalculateGearForSimpleCar(-100.0f,gear); Check(gear==0,"reverse gear cannot underflow");
    gear = 255; t.CalculateGearForSimpleCar(1.0f,gear); Check(gear==5,"invalid restored gear bounded");
    gear = 1; Near(Drive(t,gear,0),0.032f,"SA first-gear drive acceleration");
    gear = 1; Near(Drive(t,gear,0.1f),0.032f,"SA torque is not VC velocity interpolation");
    t.Flags = TRANSMISSION_1G_BOOST;
    gear = 1; Near(Drive(t,gear,0),0.048f,"SA first-gear boost");
    t.Flags = TRANSMISSION_2G_BOOST;
    gear = 1; Near(Drive(t,gear,0),0.04f,"SA second-gear boost curve");
    t.Flags = 0;
    gear = 1; Near(Drive(t,gear,0,1,TRANSMISSION_CHEAT_PERFECT),0.0384f,"perfect handling multiplier");
    gear = 1; Near(Drive(t,gear,0,1,TRANSMISSION_CHEAT_NITROS),0.064f,"nitros multiplier");
    gear = 0; Near(Drive(t,gear,-0.05f,-1),-0.036f,"SA reverse torque");
    gear = 0; Near(Drive(t,gear,0,1,TRANSMISSION_CHEAT_NITROS),0.032f,"direction change drops cheat");
    Check(gear==1,"reverse changes to forward on positive throttle");
    gear = 1; Near(Drive(t,gear,-0.05f,-1),-0.036f,"forward changes to reverse on negative throttle");
    Check(gear==0,"reverse selected");
    gear = 5; Drive(t,gear,0); Check(gear==1,"bounded multi-step downshift");
    gear = 1; Drive(t,gear,0.9f); Check(gear==5,"bounded multi-step upshift");
    gear = 1; Near(Drive(t,gear,1.1f),0,"forward hard limit");
    gear = 0; Near(Drive(t,gear,-0.3f,-1),0,"reverse hard limit");
    gear = 1; Near(Drive(t,gear,0,0),0,"zero throttle");
    gear = 1; Near(Drive(t,gear,0,1,TRANSMISSION_CHEAT_NONE,nullptr,nullptr,false),0.032f,"optional inertia pointers");
    float ratio = 0, smooth = 1;
    gear = 1;
    const float accelerated = Drive(t,gear,0,1,TRANSMISSION_CHEAT_NONE,&ratio,&smooth,true);
    Near(smooth,0.865f,"SA inertia smoothing"); Near(accelerated,0.032f*0.865f,"inertia applied to drive force");
    Check(ratio>0,"inertia captures normalized RPM");
    float otherRatio=0, otherSmooth=1;
    gear = 1; Drive(t,gear,0,1,TRANSMISSION_CHEAT_NONE,&otherRatio,&otherSmooth,false);
    Near(otherRatio,0.01f,"free-rev ratio increment"); Near(otherSmooth,0.1f,"airborne smoothing state");
    Near(smooth,0.865f,"separate vehicle inertia remains independent");
    CTimer::SetTimeStep(0.5f);
    gear = 1; Near(Drive(t,gear,0),0.016f,"timestep scaling");
    CTimer::SetTimeStep(1.0f);
    t.fEngineInertia = 0;
    gear = 1; Drive(t,gear,0,0,TRANSMISSION_CHEAT_NONE,&ratio,&smooth,false);
    Check(std::isfinite(ratio),"zero inertia and throttle are finite");
    gear = 1; Drive(t,gear,0,1,TRANSMISSION_CHEAT_NONE,&ratio,&smooth,false);
    Near(ratio,1,"zero inertia free-rev saturates");
    t = Transmission(1);
    gear = 1; Near(Drive(t,gear,0),0.008f,"one forward gear");
    gear = 0; Near(Drive(t,gear,-0.05f,-1),-0.008f,"single-speed reverse");
    for(int count : {0,6,127}){
        t = Transmission(count);
        gear = 255; Near(Drive(t,gear,0),0,"malformed gear count rejected");
    }
    t=Transmission(); gear=1;
    Near(Drive(t,gear,std::numeric_limits<float>::quiet_NaN()),0,"NaN speed rejected");
    Near(Drive(t,gear,0,std::numeric_limits<float>::infinity()),0,"infinite throttle rejected");
    // Sweep all supported gears and both throttle directions for termination,
    // finite output and in-range gear selection.
    for(int count=1;count<=5;++count){
        t=Transmission(count);
        for(int initial=0;initial<=count;++initial)
            for(int i=-20;i<=100;++i)
                for(float gas : {-1.0f,0.0f,1.0f}){
                    gear=initial;
                    Check(std::isfinite(Drive(t,gear,i*0.01f,gas)),"finite sweep force");
                    Check(gear<=count,"sweep gear remains bounded");
                }
    }
    std::puts("SATransmissionTests: ALL PASSED");
}
