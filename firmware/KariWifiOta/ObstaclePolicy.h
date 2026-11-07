#pragma once
#include <stdint.h>
#include "HardwarePins.h"

// Very-close distance, centimeters. Tune this (and the maneuver timings below)
// without changing the remote-control motion model.
constexpr int OBSTACLE_STOP_DISTANCE_CM = 25;
constexpr uint32_t OBSTACLE_STOP_MS = 200;
constexpr uint32_t OBSTACLE_TURN_MS = 400;
// Logical chassis speed. Right is left > 0 and right < 0, matching calibration_test "right".
// Kept at or below the firmware hard max of 0.50; applyMotorSpeeds still clamps to runtime max.
constexpr float OBSTACLE_TURN_SPEED = 0.35f;
// HC-SR04 should not be pinged faster than about 60 ms. The echo timeout only
// needs to cover nearby obstacles so the control loop is not blocked for long.
constexpr uint32_t ULTRASONIC_SAMPLE_INTERVAL_MS = 80;
constexpr uint32_t ULTRASONIC_ECHO_TIMEOUT_US = 12000;
constexpr int ULTRASONIC_NO_ECHO_CM = 400;

// Integer centimeters. echoUs == 0 is a pulseIn timeout, treated as far so a
// missing or stuck sensor does not spin the chassis. 58 us ≈ 1 cm round trip.
constexpr int ultrasonicCmFromEchoUs(uint32_t echoUs) {
  return echoUs == 0 ? ULTRASONIC_NO_ECHO_CM : static_cast<int>(echoUs / 58u);
}

enum class ObstaclePhase : uint8_t { Idle = 0, Stop, Turn };

// Safety override for a remote-controlled chassis: while a drive command is
// active and the forward distance is below the threshold, replace outgoing
// wheel speeds with stop, then a short right turn. Clearing the obstacle or
// finishing that turn releases the override. A finished turn that is still
// blocked and still commanded starts another stop instead of driving forward.
struct ObstacleAvoidance {
  ObstaclePhase phase = ObstaclePhase::Idle;
  uint32_t phaseStartedAt = 0;
  int lastDistanceCm = ULTRASONIC_NO_ECHO_CM;
  bool haveDistance = false;

  void noteDistance(int cm) { lastDistanceCm = cm; haveDistance = true; }
  bool active() const { return phase != ObstaclePhase::Idle; }
  bool veryClose() const { return haveDistance && lastDistanceCm < OBSTACLE_STOP_DISTANCE_CM; }
  const char *phaseName() const {
    if (phase == ObstaclePhase::Stop) return "stop";
    if (phase == ObstaclePhase::Turn) return "turn_right";
    return "idle";
  }
  void cancel() { phase = ObstaclePhase::Idle; phaseStartedAt = 0; }

  // Returns true while outgoing motor commands must be replaced.
  bool service(uint32_t now, bool motionCommanded) {
    if (!haveDistance) return active();
    if (phase == ObstaclePhase::Idle) {
      if (veryClose() && motionCommanded) begin(ObstaclePhase::Stop, now);
      return active();
    }
    if (!veryClose()) { cancel(); return false; }
    const uint32_t elapsed = now - phaseStartedAt;
    if (phase == ObstaclePhase::Stop && elapsed >= OBSTACLE_STOP_MS) begin(ObstaclePhase::Turn, now);
    else if (phase == ObstaclePhase::Turn && elapsed >= OBSTACLE_TURN_MS) {
      cancel();
      if (veryClose() && motionCommanded) begin(ObstaclePhase::Stop, now);
    }
    return active();
  }

  void overrideSpeeds(float &left, float &right) const {
    if (phase == ObstaclePhase::Turn) { left = OBSTACLE_TURN_SPEED; right = -OBSTACLE_TURN_SPEED; }
    else { left = 0.0f; right = 0.0f; }
  }

 private:
  void begin(ObstaclePhase next, uint32_t now) { phase = next; phaseStartedAt = now; }
};
