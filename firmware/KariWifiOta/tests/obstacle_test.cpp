#include "../ObstaclePolicy.h"
#include <cassert>
#include <cstdio>
int main() {
  static_assert(OBSTACLE_STOP_DISTANCE_CM == 25, "default very-close threshold");
  static_assert(ULTRASONIC_SAMPLE_INTERVAL_MS >= 60, "HC-SR04 needs spacing between pings");
  static_assert(ULTRASONIC_ECHO_TIMEOUT_US >= 2000 && ULTRASONIC_ECHO_TIMEOUT_US <= 30000, "timeout covers near obstacles without blocking the loop");
  static_assert(OBSTACLE_TURN_SPEED > 0.2f && OBSTACLE_TURN_SPEED <= 0.5f, "turn speed stays inside the firmware hard max");
  static_assert(ULTRASONIC_TRIG_PIN == 4 && ULTRASONIC_ECHO_PIN == 18, "agreed N16R8 ultrasonic wiring");
  static_assert(ULTRASONIC_TRIG_PIN != ULTRASONIC_ECHO_PIN, "ultrasonic pins collide");

  assert(ultrasonicCmFromEchoUs(0) == ULTRASONIC_NO_ECHO_CM);
  assert(ultrasonicCmFromEchoUs(20u * 58u) == 20);
  assert(ultrasonicCmFromEchoUs(25u * 58u) == 25);
  assert(ultrasonicCmFromEchoUs(25u * 58u - 1u) == 24);

  ObstacleAvoidance idle;
  idle.noteDistance(10);
  assert(!idle.service(0, false));
  assert(!idle.active());
  idle.noteDistance(25);
  assert(!idle.service(0, true));
  idle.noteDistance(ultrasonicCmFromEchoUs(0));
  assert(!idle.service(0, true));

  ObstacleAvoidance wall;
  wall.noteDistance(24);
  float left = 0.4f, right = 0.4f;
  assert(wall.service(0, true));
  assert(wall.phase == ObstaclePhase::Stop);
  wall.overrideSpeeds(left, right);
  assert(left == 0.0f && right == 0.0f);
  assert(wall.service(OBSTACLE_STOP_MS - 1, true));
  wall.overrideSpeeds(left, right);
  assert(left == 0.0f && right == 0.0f);
  assert(wall.service(OBSTACLE_STOP_MS, true));
  assert(wall.phase == ObstaclePhase::Turn);
  wall.overrideSpeeds(left, right);
  assert(left == OBSTACLE_TURN_SPEED && right == -OBSTACLE_TURN_SPEED);

  wall.noteDistance(40);
  assert(!wall.service(OBSTACLE_STOP_MS + 10, true));
  assert(wall.phase == ObstaclePhase::Idle);

  ObstacleAvoidance moved;
  moved.noteDistance(10);
  assert(moved.service(0, true));
  moved.noteDistance(30);
  assert(!moved.service(OBSTACLE_STOP_MS / 2, true));
  assert(moved.phase == ObstaclePhase::Idle);

  ObstacleAvoidance held;
  held.noteDistance(12);
  assert(held.service(1000, true));
  const uint32_t turning = 1000 + OBSTACLE_STOP_MS;
  assert(held.service(turning, true));
  assert(held.phase == ObstaclePhase::Turn);
  const uint32_t again = turning + OBSTACLE_TURN_MS;
  assert(held.service(again, true));
  assert(held.phase == ObstaclePhase::Stop);
  held.overrideSpeeds(left, right);
  assert(left == 0.0f && right == 0.0f);

  ObstacleAvoidance released;
  released.noteDistance(12);
  assert(released.service(50, true));
  const uint32_t turnAt = 50 + OBSTACLE_STOP_MS;
  assert(released.service(turnAt, false));
  assert(released.phase == ObstaclePhase::Turn);
  assert(!released.service(turnAt + OBSTACLE_TURN_MS, false));
  assert(!released.active());

  ObstacleAvoidance aborted;
  aborted.noteDistance(8);
  assert(aborted.service(10, true));
  aborted.cancel();
  assert(!aborted.active());
  assert(!aborted.service(10, false));

  constexpr uint32_t start = 0xfffffff0u;
  ObstacleAvoidance wrapped;
  wrapped.noteDistance(5);
  assert(wrapped.service(start, true));
  assert(wrapped.service(start + OBSTACLE_STOP_MS, true));
  assert(wrapped.phase == ObstaclePhase::Turn);
  assert(wrapped.service(start + OBSTACLE_STOP_MS + OBSTACLE_TURN_MS - 1, true));
  assert(wrapped.phase == ObstaclePhase::Turn);

  std::puts("PASS: obstacle stop then right turn, clear/release, retrigger and clock wrap");
}
