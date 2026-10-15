#include <assert.h>
#include <iostream>
#include <set>
#include "protocol.h"
#include "library.h"
#include "motion.h"
#include "library_store.h"

void testMapping() {
    static_assert(Config::LED_COUNT == 120, "Hardware has 120 LEDs");
    for (bool serpentine : {false, true}) {
        std::set<uint16_t> seen;
        for (unsigned s = 0; s < 4; ++s)
            for (unsigned r = 0; r < 3; ++r)
                for (unsigned p = 0; p < 30; ++p)
                    seen.insert(Lamp::pixelIndex(s, r, p, 3, 30, serpentine));
        assert(seen.size() == 360 && *seen.begin() == 0 && *seen.rbegin() == 359);
    }
    std::set<uint16_t> seen;
    for (unsigned s = 0; s < 4; ++s)
        for (unsigned p = 0; p < 30; ++p)
            seen.insert(Lamp::pixelIndex(s, 0, p, 1, 30, false));
    assert(seen.size() == 120 && *seen.rbegin() == 119);
    // Physical order: top, front upper, front lower, bottom. Pixel 0 = left.
    const unsigned chainLeft[] = {0,59,60,119}, chainRight[] = {29,30,89,90};
    for (uint8_t outputs : {1,4}) {
        std::set<uint16_t> physical;
        for (unsigned s=0;s<4;++s) {
            auto left=Lamp::wiredPixelIndex(s,0,0,1,30,false,outputs);
            auto right=Lamp::wiredPixelIndex(s,0,29,1,30,false,outputs);
            assert(left == (outputs==1 ? chainLeft[s] : s*30));
            assert(right == (outputs==1 ? chainRight[s] : s*30+29));
            for (unsigned p=0;p<30;++p) {
                auto index=Lamp::wiredPixelIndex(s,0,p,1,30,false,outputs);
                assert(index/30==s); // keeps each strip on its designated output
                physical.insert(index);
            }
        }
        assert(physical.size()==120 && *physical.begin()==0 && *physical.rbegin()==119);
    }
}
void testMotionAndEffects() {
    for (auto& scene : Lamp::SCENES) {
        for (uint32_t time = 0; time < 90000; time += 137) {
            auto target = Lamp::targetAngle(scene, time, Config::SERVO_MIN_DEG, Config::SERVO_MAX_DEG);
            assert(target >= Config::SERVO_MIN_DEG && target <= Config::SERVO_MAX_DEG && isfinite(target));
            const float current = Config::SERVO_HOME_DEG, maxStep = Config::SERVO_SPEED_DPS * .02f;
            auto next = Lamp::approach(current, target, maxStep);
            assert(fabsf(next - current) <= maxStep + .00001f);
            assert(fabsf(next - target) <= fabsf(current - target) + .00001f);
            for (unsigned strip = 0; strip < 4; ++strip) {
                for (unsigned p = 0; p < 30; ++p) {
                    auto c = Lamp::color(scene.effect, time, strip, 0, p, 30);
                    if (scene.effect == Lamp::Effect::Off) assert(c.r + c.g + c.b == 0);
                    if (scene.effect == Lamp::Effect::Night && strip != Config::STRIP_BOTTOM) assert(c.r + c.g + c.b == 0);
                }
            }
        }
    }
    auto front = Lamp::color(Lamp::Effect::Reading, 0, Config::STRIP_FRONT_UPPER, 0, 0, 30);
    auto top = Lamp::color(Lamp::Effect::Reading, 0, 0, 0, 0, 30);
    assert(front.r > top.r && front.g > top.g && front.b > top.b);
    const float minPulse = Config::SERVO_MIN_US + Config::SERVO_MIN_DEG / Config::SERVO_RANGE_DEG *
                           (Config::SERVO_MAX_US - Config::SERVO_MIN_US);
    const float maxPulse = Config::SERVO_MIN_US + Config::SERVO_MAX_DEG / Config::SERVO_RANGE_DEG *
                           (Config::SERVO_MAX_US - Config::SERVO_MIN_US);
    assert(fabsf(minPulse - 1400) < .01f && fabsf(maxPulse - 1600) < .01f);
    assert(Lamp::due(15, UINT32_MAX - 10, 25));
    assert(!Lamp::due(15, UINT32_MAX - 10, 27));
}
void testPower() {
    for (uint32_t sum = 0; sum <= 120 * 3 * 255; sum += 101) {
        for (unsigned level = 0; level <= 255; ++level) {
            uint8_t applied = Lamp::limitedBrightness(sum, level, 4000);
            assert(applied <= level);
            assert(uint64_t(sum) * 20 * applied <= uint64_t(4000) * 255 * 255);
        }
    }
    assert(Lamp::limitedBrightness(0, 255, 0) == 255); // black frame draws no modelled RGB current
    assert(Lamp::limitedBrightness(120 * 3 * 255, 255, 4000) == 141);
}
JsonDocument validPacket() {
    JsonDocument d;
    d["v"] = Lamp::PROTOCOL_VERSION;
    d["type"] = "set";
    d["key"] = LAMP_PAIRING_KEY;
    d["boot"] = 123;
    d["session"] = 456;
    d["seq"] = 1;
    d["selection"] = 1;
    d["scene"] = 4;
    d["brightness"] = 255;
    d["motion"] = true;
    Lamp::encodeManual(d, false, Lamp::ManualSettings{});
    return d;
}
void testProtocol() {
    Lamp::Command cmd{};
    auto d = validPacket();
    assert(Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    assert(cmd.scene == 4 && cmd.brightness == 255 && cmd.motion && cmd.session == 456);
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 124, cmd)); // restarted lamp
    for (const char* missing : {"v", "type", "key", "boot", "session", "seq", "scene", "brightness", "motion", "manual", "target", "colors", "selection"}) {
        auto invalid = validPacket();
        invalid.remove(missing);
        assert(!Lamp::decodeCommand(invalid.as<JsonObjectConst>(), 123, cmd));
    }
    for (const char* bad : {"-1", "256", "1.5", "true", "\"60\"", "null", "{}", "[]"}) {
        auto invalid = validPacket();
        JsonDocument value;
        assert(!deserializeJson(value, bad));
        invalid["brightness"].set(value.as<JsonVariantConst>());
        assert(!Lamp::decodeCommand(invalid.as<JsonObjectConst>(), 123, cmd));
    }
    d = validPacket(); d["scene"] = Lamp::SCENE_COUNT;
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["motion"] = 1;
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["key"] = "wrong";
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["session"] = 0;
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["v"] = "1";
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    Lamp::CommandGate gate;
    using Order = Lamp::CommandOrder;
    assert(gate.inspect(100, UINT32_MAX - 1) == Order::New);
    assert(gate.inspect(100, UINT32_MAX - 1) == Order::Duplicate);
    assert(gate.inspect(100, UINT32_MAX - 2) == Order::Stale);
    assert(gate.inspect(100, 0) == Order::New); // sequence rollover
    assert(gate.inspect(100, UINT32_MAX) == Order::Stale);
    assert(gate.inspect(101, 1) == Order::New); // new controller boot
}
void testManual() {
    Lamp::Command cmd{};
    auto d = validPacket();
    Lamp::ManualSettings settings;
    settings.target = Config::SERVO_MAX_DEG;
    settings.colors[0] = 0xff0000;
    settings.colors[1] = 0x00ff00;
    settings.colors[2] = 0x0000ff;
    Lamp::encodeManual(d, true, settings);
    assert(Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    assert(cmd.manual && cmd.settings.target == Config::SERVO_MAX_DEG);
    assert(Lamp::manualColor(cmd.settings, 0).r == 255);
    assert(Lamp::manualColor(cmd.settings, 1).b == 255);
    assert(Lamp::manualColor(cmd.settings, 2).b == 255);
    assert(Lamp::manualColor(cmd.settings, 3).g == 255); // bottom is the last strip
    d["target"] = Config::SERVO_MIN_DEG;
    assert(Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    for (float invalid : {Config::SERVO_MIN_DEG - 1, Config::SERVO_MAX_DEG + 1, INFINITY, NAN}) {
        d["target"] = invalid;
        assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    }
    for (const char* invalid : {"true", "\"90\"", "null", "{}", "[]"}) {
        d = validPacket();
        JsonDocument value;
        assert(!deserializeJson(value, invalid));
        d["target"].set(value.as<JsonVariantConst>());
        assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    }
    for (const char* invalid : {"[]", "[0,0]", "[0,0,0,0]", "[0,0,-1]", "[0,0,16777216]", "[0,0,1.5]", "[0,0,true]", "[0,0,\"255\"]", "null", "{}"}) {
        d = validPacket();
        JsonDocument value;
        assert(!deserializeJson(value, invalid));
        d["colors"].set(value.as<JsonVariantConst>());
        assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    }
    d = validPacket(); d["manual"] = 1;
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["v"] = 1;
    assert(!Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    d = validPacket(); d["scene"] = 0;
    Lamp::encodeManual(d, true, settings);
    assert(Lamp::decodeCommand(d.as<JsonObjectConst>(), 123, cmd));
    assert(measureJson(d) < Config::PACKET_BYTES);
}
void testLibrary() {
    auto library=Lamp::defaultLibrary();
    assert(Lamp::validLibrary(library));
    assert(library.count==4 && library.scenes[0].settings.colors[2]==0xfff3e4);
    assert(library.scenes[1].settings.colors[0]==0xff0818 && library.scenes[1].settings.colors[2]==0);
    assert(library.scenes[2].motion && library.scenes[2].settings.target==Config::SERVO_MAX_DEG);
    assert(library.scenes[3].preset==5 && library.scenes[3].motion);
    JsonDocument doc; Lamp::encodeLibrary(doc,library);
    Lamp::SceneLibrary decoded;
    assert(Lamp::decodeLibrary(doc.as<JsonObjectConst>(),decoded));
    assert(decoded.count==4 && decoded.favorites[3]==4);
    const auto& s=decoded.scenes[0];
    assert(Lamp::sceneMatches(s,1,s.brightness,s.motion,true,s.settings));
    assert(!Lamp::sceneMatches(s,0,s.brightness,s.motion,true,s.settings));
    assert(!Lamp::sceneMatches(s,1,s.brightness-1,s.motion,true,s.settings));
    auto altered=s.settings; altered.colors[2]^=1;
    assert(!Lamp::sceneMatches(s,1,s.brightness,s.motion,true,altered));
    for (const char* field:{"id","name","icon","tint","preset","brightness","motion","manual","target","colors"}) {
        JsonDocument bad;Lamp::encodeLibrary(bad,library);bad["scenes"][0].remove(field);
        assert(!Lamp::decodeLibrary(bad.as<JsonObjectConst>(),decoded));
    }
    for (const char* invalid:{"-1","8","true","\"1\"","1.5"}) {
        JsonDocument bad,v;Lamp::encodeLibrary(bad,library);assert(!deserializeJson(v,invalid));
        bad["scenes"][0]["icon"].set(v.as<JsonVariantConst>());
        assert(!Lamp::decodeLibrary(bad.as<JsonObjectConst>(),decoded));
    }
    JsonDocument bad; Lamp::encodeLibrary(bad,library);bad["favorites"][1]=bad["favorites"][0];
    assert(!Lamp::decodeLibrary(bad.as<JsonObjectConst>(),decoded));
    bad.clear();Lamp::encodeLibrary(bad,library);bad["favorites"][0]=999;
    assert(!Lamp::decodeLibrary(bad.as<JsonObjectConst>(),decoded));
    bad.clear();Lamp::encodeLibrary(bad,library);bad["scenes"][1]["id"]=1;
    assert(!Lamp::decodeLibrary(bad.as<JsonObjectConst>(),decoded));
    bad.clear();Lamp::encodeLibrary(bad,library);bad["scenes"][0]["name"]="bad\nname";
    assert(!Lamp::decodeLibrary(bad.as<JsonObjectConst>(),decoded));
    bad.clear();Lamp::encodeLibrary(bad,library);bad["scenes"][0]["name"]=std::string(33,'x');
    assert(!Lamp::decodeLibrary(bad.as<JsonObjectConst>(),decoded));
    bad.clear();Lamp::encodeLibrary(bad,library);bad["scenes"][0]["target"]=Config::SERVO_MAX_DEG+1;
    assert(!Lamp::decodeLibrary(bad.as<JsonObjectConst>(),decoded));
    library.count=Lamp::MAX_SAVED_SCENES;
    for(uint8_t i=4;i<library.count;++i){library.scenes[i]=library.scenes[0];library.scenes[i].id=i+1;}
    assert(Lamp::validLibrary(library));
    JsonDocument full;Lamp::encodeLibrary(full,library);
    assert(measureJson(full)<24576);
    assert(Lamp::decodeLibrary(full.as<JsonObjectConst>(),decoded));
    assert(decoded.count==32 && decoded.scenes[31].id==32);
    full["scenes"].as<JsonArray>().add(full["scenes"][0]);
    assert(!Lamp::decodeLibrary(full.as<JsonObjectConst>(),decoded));
}
void testReferenceReturn() {
    Lamp::ReferenceReturn parking;
    assert(!parking.active()); // power-on must not enable the servo
    parking.start();
    parking.tick(1000,Config::SERVO_MAX_DEG);
    parking.tick(10000,Config::SERVO_MAX_DEG);
    assert(parking.active()); // elapsed time alone cannot finish the move
    parking.tick(11000,Config::SERVO_REFERENCE_DEG);
    parking.tick(11699,Config::SERVO_REFERENCE_DEG);
    assert(parking.active());
    parking.tick(11700,Config::SERVO_REFERENCE_DEG);
    assert(!parking.active());
    parking.start();parking.tick(UINT32_MAX-100,Config::SERVO_REFERENCE_DEG);
    parking.tick(599,Config::SERVO_REFERENCE_DEG);
    assert(!parking.active()); // 700ms including millis wrap
    parking.start();parking.cancel();assert(!parking.active());
}
void testStorage() {
    MockNVS::values.clear();
    Lamp::SceneLibrary loaded;
    assert(!LibraryStore::load(loaded));
    auto library=Lamp::defaultLibrary();
    assert(LibraryStore::save(library));
    assert(LibraryStore::load(loaded));
    assert(loaded.count==4 && strcmp(loaded.scenes[0].name,"Meeting")==0);
    library.favorites[0]=4;library.favorites[3]=1;library.revision++;
    library.scenes[0].settings.colors[2]=0xffffff;
    assert(LibraryStore::save(library));
    Lamp::SceneLibrary restarted;
    assert(LibraryStore::load(restarted));
    assert(restarted.favorites[0]==4 && restarted.scenes[0].settings.colors[2]==0xffffff);
    MockNVS::failWrite=true;library.revision++;
    assert(!LibraryStore::save(library));
    assert(LibraryStore::load(restarted));assert(restarted.revision==2);
    auto& blob=MockNVS::values["move-scenes/library"];
    blob.back()^=0xff;assert(!LibraryStore::load(restarted));
    blob.pop_back();assert(!LibraryStore::load(restarted));
}
int main(int argc, char** argv) {
    if(argc>1 && strcmp(argv[1],"--validate")==0) {
        JsonDocument doc;
        if(deserializeJson(doc,std::cin))return 1;
        Lamp::SceneLibrary candidate;
        if(!Lamp::decodeLibrary(doc.as<JsonObjectConst>(),candidate))return 1;
        candidate.revision++;
        JsonDocument result;Lamp::encodeLibrary(result,candidate);
        std::string json;serializeJson(result,json);std::cout<<json;return 0;
    }
    if(argc>1 && strcmp(argv[1],"--fixture")==0) {
        JsonDocument doc;Lamp::encodeLibrary(doc,Lamp::defaultLibrary());
        std::string json;serializeJson(doc,json);std::cout<<json;return 0;
    }
    testMapping(); testMotionAndEffects(); testPower(); testProtocol(); testManual(); testLibrary(); testReferenceReturn();
    testStorage();
    std::cout << "PASS: LEDs, motion, packets, scene library, favorites, persistence/failure recovery, reference return\n";
}
