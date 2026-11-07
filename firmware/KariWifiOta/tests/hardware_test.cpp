#include "../HardwarePolicy.h"
#include <cassert>
#include <vector>
int main() {
  HardwareProfile profile;
  assert(validHardwareProfile(profile));
  for(int bad : {0,3,19,20,22,25,26,32,33,37,38,43,44,45,46,48,49,255}) {
    auto next=profile;next.pins[0]=bad;assert(!validHardwareProfile(next));
  }
  auto duplicate=profile;duplicate.pins[0]=duplicate.pins[1];assert(!validHardwareProfile(duplicate));
  auto version=profile;version.version=0;assert(!validHardwareProfile(version));
  auto wrong=profile;wrong.flashMb=KARI_FLASH_MB==8?16:8;assert(!validHardwareProfile(wrong));
  auto custom=profile;uint8_t pins[]={1,2,8,9,10,21};for(int i=0;i<6;++i)custom.pins[i]=pins[i];assert(validHardwareProfile(custom));
  auto trig=profile;trig.pins[0]=ULTRASONIC_TRIG_PIN;assert(!validHardwareProfile(trig));
  auto echo=profile;echo.pins[0]=ULTRASONIC_ECHO_PIN;assert(!validHardwareProfile(echo));
  assert(ULTRASONIC_TRIG_PIN==4 && ULTRASONIC_ECHO_PIN==18);
  assert(reservedUltrasonicPin(ULTRASONIC_TRIG_PIN) && reservedUltrasonicPin(ULTRASONIC_ECHO_PIN));
  assert(!reservedUltrasonicPin(17));
  CompatibilityScanner scanner;
  const auto *marker=reinterpret_cast<const uint8_t *>(HARDWARE_DESCRIPTOR);
  for(size_t split=0;split<sizeof(HARDWARE_DESCRIPTOR);++split) {
    scanner.reset();scanner.feed(marker,split);scanner.feed(marker+split,sizeof(HARDWARE_DESCRIPTOR)-1-split);assert(scanner.found);
  }
  scanner.reset();const char legacy[]="ESP32-S3 firmware 2.0.2";scanner.feed(reinterpret_cast<const uint8_t *>(legacy),sizeof(legacy));assert(!scanner.found);
#if KARI_FLASH_MB == 8
  const char other[]="KARI_HW_V1|n16r8|1|app3M_fat9M_16MB|END";
#else
  const char other[]="KARI_HW_V1|n8r8|1|kari_8m|END";
#endif
  scanner.feed(reinterpret_cast<const uint8_t *>(other),sizeof(other));assert(!scanner.found);
  scanner.feed(marker,sizeof(HARDWARE_DESCRIPTOR)-1);assert(scanner.found);
}
