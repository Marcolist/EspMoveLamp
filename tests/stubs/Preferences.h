#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
namespace MockNVS {
inline std::map<std::string,std::vector<unsigned char>> values;
inline bool failWrite=false;
}
class Preferences {
    std::string prefix;
public:
    bool begin(const char* name,bool) {prefix=std::string(name)+"/";return true;}
    void end() {}
    size_t getBytesLength(const char* key) {return MockNVS::values[prefix+key].size();}
    size_t getBytes(const char* key,void* out,size_t length) {
        const auto& data=MockNVS::values[prefix+key];
        if(data.size()!=length)return 0;
        memcpy(out,data.data(),length);return length;
    }
    size_t putBytes(const char* key,const void* data,size_t length) {
        if(MockNVS::failWrite){MockNVS::failWrite=false;return 0;}
        const auto* bytes=static_cast<const unsigned char*>(data);
        MockNVS::values[prefix+key]=std::vector<unsigned char>(bytes,bytes+length);return length;
    }
};
