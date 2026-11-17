#pragma once
// File semantics adapted from gta-reversed tHandlingData::InitFromData and
// cHandlingDataMgr (56a67498c822c7bf6fe07f663ddef7a6be3504f1).
// GTA Community / gta-reversed contributors. No native addresses/ABI copied.
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace sahandling {
struct Car {
    std::string name;
    float mass, turnMass, drag, centre[3];
    int submerged;
    float traction, tractionLoss, tractionBias;
    int gears;
    float velocity, acceleration, inertia;
    char drive, engine;
    float brake, brakeBias;
    int abs;
    float steering, spring, damping, highSpeedDamping, upper, lower, suspensionBias, antiDive;
    float seatOffset, collisionDamage;
    int money;
    uint32_t modelFlags, handlingFlags;
    int frontLights, rearLights, animGroup;
    // BR appends 1 or 3 finite values to some records. Preserve, don't guess semantics.
    std::vector<float> extensions;
};
struct Special { std::string name; std::vector<float> values; };
struct Table {
    std::vector<Car> cars;
    std::vector<Special> bikes, boats, flying;
    size_t ignoredCraneRecords = 0, ignoredAnimationGroups = 0;
};
inline bool Name(const std::string &s)
{
    if(s.empty() || s.size()>31) return false;
    for(char c : s)
        if(!((c>='A' && c<='Z') || (c>='a' && c<='z') ||
             (c>='0' && c<='9') || c=='_')) return false;
    return true;
}
inline bool Float(const std::string &s, float &value)
{
    if(s.empty()) return false;
    char *end; errno=0;
    float parsed=std::strtof(s.c_str(), &end);
    if(end==s.c_str() || *end || errno==ERANGE || !std::isfinite(parsed)) return false;
    value=parsed; return true;
}
inline bool Integer(const std::string &s, uint32_t &value, int base=10)
{
    if(s.empty() || s[0]=='-' || s[0]=='+') return false;
    char *end; errno=0;
    unsigned long long parsed=std::strtoull(s.c_str(),&end,base);
    if(end==s.c_str() || *end || errno || parsed>UINT32_MAX) return false;
    value=static_cast<uint32_t>(parsed); return true;
}
inline bool SmallInteger(const std::string &s, int &value, int maximum)
{
    uint32_t parsed;
    if(!Integer(s,parsed) || parsed>static_cast<uint32_t>(maximum)) return false;
    value=static_cast<int>(parsed); return true;
}
inline bool ParseCar(const std::vector<std::string> &f, Car &c)
{
    if((f.size()!=36 && f.size()!=37 && f.size()!=39) || !Name(f[0])) return false;
    Car parsed={}; parsed.name=f[0];
    float *floats[]={&parsed.mass,&parsed.turnMass,&parsed.drag,&parsed.centre[0],
        &parsed.centre[1],&parsed.centre[2],nullptr,&parsed.traction,&parsed.tractionLoss,
        &parsed.tractionBias,nullptr,&parsed.velocity,&parsed.acceleration,&parsed.inertia,
        nullptr,nullptr,&parsed.brake,&parsed.brakeBias,nullptr,&parsed.steering,
        &parsed.spring,&parsed.damping,&parsed.highSpeedDamping,&parsed.upper,&parsed.lower,
        &parsed.suspensionBias,&parsed.antiDive,&parsed.seatOffset,&parsed.collisionDamage};
    for(size_t i=0;i<29;++i) if(floats[i] && !Float(f[i+1],*floats[i])) return false;
    if(!SmallInteger(f[7],parsed.submerged,255) || !SmallInteger(f[11],parsed.gears,5) ||
       !SmallInteger(f[19],parsed.abs,1) || !SmallInteger(f[30],parsed.money,INT32_MAX) ||
       !Integer(f[31],parsed.modelFlags,16) || !Integer(f[32],parsed.handlingFlags,16) ||
       !SmallInteger(f[33],parsed.frontLights,3) || !SmallInteger(f[34],parsed.rearLights,3) ||
       !SmallInteger(f[35],parsed.animGroup,255)) return false;
    if(f[15].size()!=1 || std::strchr("FR4",f[15][0])==nullptr ||
       f[16].size()!=1 || std::strchr("PDE",f[16][0])==nullptr) return false;
    parsed.drive=f[15][0]; parsed.engine=f[16][0];
    if(parsed.mass<=0 || parsed.turnMass<=0 || parsed.drag<0 || parsed.gears<1 ||
       parsed.submerged<1 || parsed.velocity<=0 || parsed.velocity>10000 ||
       parsed.acceleration<0 || parsed.inertia<0 || parsed.brake<0 ||
       parsed.brakeBias<0 || parsed.brakeBias>1 || parsed.tractionLoss<0 ||
       parsed.tractionBias<0 || parsed.tractionBias>1 || parsed.spring<0 || parsed.damping<0 ||
       parsed.highSpeedDamping<0 ||
       (!(parsed.modelFlags & 0x08000000u) && parsed.upper<parsed.lower) ||
       parsed.suspensionBias<0) return false;
    for(size_t i=36;i<f.size();++i){
        float v; if(!Float(f[i],v)) return false;
        parsed.extensions.push_back(v);
    }
    c=parsed; return true;
}
inline bool Parse(const uint8_t *data, size_t size, Table &out, std::string *error=nullptr)
{
    if(error) error->clear();
    if(!data || !size || size>1024*1024 || std::memchr(data,0,size)){
        if(error) *error="empty, oversized or NUL-containing handling table";
        return false;
    }
    Table parsed;
    std::map<std::string,bool> names;
    size_t pos=0,lineNumber=0;
    // The supplied BR table starts with TWO UTF-8 BOMs.
    while(pos+3<=size && data[pos]==0xEF && data[pos+1]==0xBB && data[pos+2]==0xBF) pos+=3;
    while(pos<size){
        size_t end=pos;
        while(end<size && data[end]!='\n') ++end;
        ++lineNumber;
        if(end-pos>2047){ if(error) *error="handling line too long"; return false; }
        std::string line(reinterpret_cast<const char*>(data+pos),end-pos);
        pos=end+(end<size);
        if(line.find(";the end") == line.find_first_not_of(" \t\r") && line.find(";the end")!=std::string::npos) break;
        size_t comment=line.find_first_of(";#");
        if(comment!=std::string::npos) line.resize(comment);
        std::istringstream stream(line);
        std::vector<std::string> fields; std::string field;
        while(stream>>field){ if(fields.size()>=64) break; fields.push_back(field); }
        if(fields.empty()) continue;
        if(fields[0]=="^"){ ++parsed.ignoredAnimationGroups; continue; }
        if(fields[0]=="&"){ ++parsed.ignoredCraneRecords; continue; }
        // BR crane base entries are not road-vehicle handling and have zero gears.
        if(fields.size()>11 && (fields[0]=="PORT_CRANE" || fields[0]=="CRANE_OF") && fields[11]=="0"){
            ++parsed.ignoredCraneRecords; continue;
        }
        bool ok=true;
        if(fields[0]=="!" || fields[0]=="%" || fields[0]=="$"){
            const char type=fields[0][0];
            const size_t count=type=='!'?15:type=='%'?14:21;
            if(fields.size()!=count+2 || !Name(fields[1])) ok=false;
            else{
                const std::string key=fields[0]+fields[1];
                if(names.count(key)) ok=false;
                else{
                    Special record; record.name=fields[1];
                    for(size_t i=2;i<fields.size();++i){
                        float v;
                        // Canonical SA RCRAIDER contains a stray 's' after wind multiplier.
                        std::string token=fields[i];
                        if(type=='$' && i==15 && record.name=="RCRAIDER" && !token.empty() && token.back()=='s') token.pop_back();
                        if(!Float(token,v)){ok=false;break;}
                        record.values.push_back(v);
                    }
                    if(ok){
                        names[key]=true;
                        (type=='!'?parsed.bikes:type=='%'?parsed.boats:parsed.flying).push_back(record);
                    }
                }
            }
        }else{
            Car car;
            ok=parsed.cars.size()<1024 && ParseCar(fields,car) && !names.count(car.name);
            if(ok){ names[car.name]=true; parsed.cars.push_back(car); }
        }
        if(!ok){ if(error) *error="invalid SA handling at line "+std::to_string(lineNumber)+" ("+fields[0]+")"; return false; }
    }
    if(parsed.cars.empty()){ if(error) *error="no SA vehicle handling records"; return false; }
    out=parsed; return true; // all-or-nothing; a malformed file never partially replaces live data
}
} // namespace sahandling
