#ifndef M5CRAWLER_CONTROL_PROTOCOL_H
#define M5CRAWLER_CONTROL_PROTOCOL_H

#include <Arduino.h>
#include <stdint.h>

// ESP-NOW channel shared by controller and crawler.
#define M5CRAWLER_ESPNOW_CHANNEL 1

// Change these two definitions to the actual STA MAC addresses of your devices.
// The devices display their own STA MAC address on the LCD at startup.
//
// Example:
//   #define M5CRAWLER_CONTROLLER_MAC {0x24, 0x58, 0x7C, 0xAA, 0xBB, 0xCC}
//   #define M5CRAWLER_CRAWLER_MAC    {0x24, 0x58, 0x7C, 0xDD, 0xEE, 0xFF}
//
// All-FF means "not configured yet"; the controller will use broadcast and
// the crawler will accept any sender. Set real MACs for peer-to-peer operation.
#ifndef M5CRAWLER_CONTROLLER_MAC
#define M5CRAWLER_CONTROLLER_MAC {0x4C, 0x75, 0x25, 0xAE, 0x29, 0xEC}
#endif

#ifndef M5CRAWLER_CRAWLER_MAC
#define M5CRAWLER_CRAWLER_MAC {0x48, 0x27, 0xE2, 0xE3, 0x12, 0x50}
#endif

// MAC address of the Stack-chan (CoreS3) mounted on the crawler.
// The crawler sends BodyCommand packets to this address.
// All-FF means broadcast (any Stack-chan on the same channel will follow).
#ifndef M5CRAWLER_STACKCHAN_MAC
#define M5CRAWLER_STACKCHAN_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#endif

#define M5CRAWLER_PROTOCOL_MAGIC 0x354D  // "M5" in little-endian memory.
#define M5CRAWLER_PROTOCOL_VERSION 1

#define M5CRAWLER_JOY_MIN (-100)
#define M5CRAWLER_JOY_MAX 100
#define M5CRAWLER_DEADZONE 5

#define M5CRAWLER_SPEED_MIN 1000
#define M5CRAWLER_SPEED_INITIAL 2000
#define M5CRAWLER_SPEED_STEP 1000
#define M5CRAWLER_SPEED_MAX 6000

#define M5CRAWLER_SEND_INTERVAL_MS 100
#define M5CRAWLER_DISPLAY_INTERVAL_MS 200
#define M5CRAWLER_RECV_TIMEOUT_MS 1000
#define M5CRAWLER_AVATAR_LOST_MS 500

struct __attribute__((packed)) ControlData {
  uint16_t magic;
  uint8_t version;
  uint8_t size;
  uint16_t sequence;
  int16_t x;
  int16_t y;
  int16_t maxSpd;
  uint8_t checksum;
};

static_assert(sizeof(ControlData) == 13, "Unexpected ControlData size");

inline bool isBroadcastMac(const uint8_t mac[6]) {
  for (int i = 0; i < 6; ++i) {
    if (mac[i] != 0xFF) return false;
  }
  return true;
}

inline bool macEquals(const uint8_t a[6], const uint8_t b[6]) {
  for (int i = 0; i < 6; ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

inline void formatMac(const uint8_t mac[6], char* out, size_t outSize) {
  if (outSize < 18) {
    if (outSize > 0) out[0] = '\0';
    return;
  }
  snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

inline uint8_t calculateControlChecksum(const ControlData& data) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
  uint8_t sum = 0;
  for (size_t i = 0; i < sizeof(ControlData) - 1; ++i) {
    sum = static_cast<uint8_t>(sum + bytes[i]);
  }
  return static_cast<uint8_t>(~sum);
}

inline void updateControlChecksum(ControlData& data) {
  data.checksum = calculateControlChecksum(data);
}

inline bool validateControlData(const ControlData& data) {
  if (data.magic != M5CRAWLER_PROTOCOL_MAGIC) return false;
  if (data.version != M5CRAWLER_PROTOCOL_VERSION) return false;
  if (data.size != sizeof(ControlData)) return false;
  if (data.checksum != calculateControlChecksum(data)) return false;
  if (data.x < M5CRAWLER_JOY_MIN || data.x > M5CRAWLER_JOY_MAX) return false;
  if (data.y < M5CRAWLER_JOY_MIN || data.y > M5CRAWLER_JOY_MAX) return false;
  if (data.maxSpd < M5CRAWLER_SPEED_MIN || data.maxSpd > M5CRAWLER_SPEED_MAX) return false;
  return true;
}

inline void initControlData(ControlData& data) {
  data.magic = M5CRAWLER_PROTOCOL_MAGIC;
  data.version = M5CRAWLER_PROTOCOL_VERSION;
  data.size = sizeof(ControlData);
  data.sequence = 0;
  data.x = 0;
  data.y = 0;
  data.maxSpd = M5CRAWLER_SPEED_INITIAL;
  updateControlChecksum(data);
}

// ---------------------------------------------------------------------------
// BodyCommand: crawler (StampS3) -> Stack-chan (CoreS3)
//
// The Stack-chan does not decide its own motion. The crawler computes the
// direction from its joystick command and sends the desired body orientation.
// Stack-chan simply applies the received command.
// ---------------------------------------------------------------------------
#define M5CRAWLER_BODY_MAGIC 0x4243  // "CB" in little-endian memory.
#define M5CRAWLER_BODY_VERSION 1

#define M5CRAWLER_BODY_YAW_MAX 90
#define M5CRAWLER_BODY_PITCH_DEFAULT 0
#define M5CRAWLER_BODY_SEND_INTERVAL_MS 100
#define M5CRAWLER_BODY_TIMEOUT_MS 1500

struct __attribute__((packed)) BodyCommand {
  uint16_t magic;
  uint8_t version;
  uint8_t size;
  uint16_t sequence;
  int16_t yaw;     // degrees, + = right
  int16_t pitch;   // degrees, + = up
  uint8_t moving;  // 0 = idle, 1 = crawler is moving
  uint8_t checksum;
};

static_assert(sizeof(BodyCommand) == 12, "Unexpected BodyCommand size");

inline uint8_t calculateBodyChecksum(const BodyCommand& cmd) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&cmd);
  uint8_t sum = 0;
  for (size_t i = 0; i < sizeof(BodyCommand) - 1; ++i) {
    sum = static_cast<uint8_t>(sum + bytes[i]);
  }
  return static_cast<uint8_t>(~sum);
}

inline void updateBodyChecksum(BodyCommand& cmd) {
  cmd.checksum = calculateBodyChecksum(cmd);
}

inline bool validateBodyCommand(const BodyCommand& cmd) {
  if (cmd.magic != M5CRAWLER_BODY_MAGIC) return false;
  if (cmd.version != M5CRAWLER_BODY_VERSION) return false;
  if (cmd.size != sizeof(BodyCommand)) return false;
  if (cmd.checksum != calculateBodyChecksum(cmd)) return false;
  if (cmd.yaw < -180 || cmd.yaw > 180) return false;
  if (cmd.pitch < -90 || cmd.pitch > 90) return false;
  return true;
}

inline void initBodyCommand(BodyCommand& cmd) {
  cmd.magic = M5CRAWLER_BODY_MAGIC;
  cmd.version = M5CRAWLER_BODY_VERSION;
  cmd.size = sizeof(BodyCommand);
  cmd.sequence = 0;
  cmd.yaw = 0;
  cmd.pitch = M5CRAWLER_BODY_PITCH_DEFAULT;
  cmd.moving = 0;
  updateBodyChecksum(cmd);
}

#endif  // M5CRAWLER_CONTROL_PROTOCOL_H
