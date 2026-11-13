// SPDX-License-Identifier: GPL-3.0-or-later
// A small stand-in for the parts of SDL3 that KytyPS5's core libraries (libs/audio, controller,
// libCes, libFont) use, built on the PS4's own services (sceAudioOut, libc iconv). There is no SDL3
// for the PS4. Anything not listed here is not provided on purpose: the window, the launcher and
// the DualSense code are replaced or left out on the Orbis build, not emulated.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {

// ---- basics -----------------------------------------------------------------------------------
typedef uint32_t SDL_InitFlags;
inline constexpr SDL_InitFlags SDL_INIT_AUDIO   = 0x00000010u;
inline constexpr SDL_InitFlags SDL_INIT_GAMEPAD = 0x00002000u;

bool        SDL_InitSubSystem(SDL_InitFlags flags);
void        SDL_QuitSubSystem(SDL_InitFlags flags);
const char* SDL_GetError(void);
void        SDL_free(void* mem);
uint64_t    SDL_GetTicks(void);
void        SDL_Delay(uint32_t ms);
/// "/app0/": where the PS4 mounts the package contents.
const char* SDL_GetBasePath(void);

// ---- audio streams (sceAudioOut) ----------------------------------------------------------------
typedef uint16_t SDL_AudioFormat;
inline constexpr SDL_AudioFormat SDL_AUDIO_S16 = 0x8010; // signed 16-bit, little endian
inline constexpr SDL_AudioFormat SDL_AUDIO_F32 = 0x8120; // 32-bit float, little endian
typedef uint32_t SDL_AudioDeviceID;
inline constexpr SDL_AudioDeviceID SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK = 0xFFFFFFFFu;

struct SDL_AudioSpec {
	SDL_AudioFormat format;
	int             channels;
	int             freq;
};
struct SDL_AudioStream; // opaque
typedef void (*SDL_AudioStreamCallback)(void* userdata, SDL_AudioStream* stream, int additional_amount,
                                        int total_amount);

SDL_AudioStream*   SDL_OpenAudioDeviceStream(SDL_AudioDeviceID devid, const SDL_AudioSpec* spec,
                                             SDL_AudioStreamCallback callback, void* userdata);
void               SDL_DestroyAudioStream(SDL_AudioStream* stream);
bool               SDL_ResumeAudioStreamDevice(SDL_AudioStream* stream);
bool               SDL_PauseAudioStreamDevice(SDL_AudioStream* stream);
bool               SDL_PutAudioStreamData(SDL_AudioStream* stream, const void* buf, int len);
int                SDL_GetAudioStreamData(SDL_AudioStream* stream, void* buf, int len);
int                SDL_GetAudioStreamQueued(SDL_AudioStream* stream);
int                SDL_GetAudioStreamAvailable(SDL_AudioStream* stream);
bool               SDL_ClearAudioStream(SDL_AudioStream* stream);
SDL_AudioDeviceID  SDL_GetAudioStreamDevice(SDL_AudioStream* stream);
bool               SDL_AudioDevicePaused(SDL_AudioDeviceID dev);
/// The PS4 build has no audio capture: always an empty list.
SDL_AudioDeviceID* SDL_GetAudioRecordingDevices(int* count);
const char*        SDL_GetAudioDeviceName(SDL_AudioDeviceID devid);

// ---- gamepads (the controller is read by the Orbis input layer, not through SDL) ----------------
typedef uint32_t SDL_JoystickID;
typedef uint32_t SDL_PropertiesID;
struct SDL_Gamepad; // opaque; never created
enum SDL_GamepadType { SDL_GAMEPAD_TYPE_UNKNOWN = 0, SDL_GAMEPAD_TYPE_STANDARD, SDL_GAMEPAD_TYPE_PS4, SDL_GAMEPAD_TYPE_PS5 };
enum SDL_SensorType { SDL_SENSOR_INVALID = -1, SDL_SENSOR_UNKNOWN, SDL_SENSOR_ACCEL, SDL_SENSOR_GYRO };
inline constexpr float SDL_STANDARD_GRAVITY = 9.80665f;
#define SDL_PROP_GAMEPAD_CAP_MONO_LED_BOOLEAN "SDL.joystick.cap.mono_led"

SDL_Gamepad*     SDL_GetGamepadFromID(SDL_JoystickID id);
void             SDL_CloseGamepad(SDL_Gamepad* gamepad);
SDL_GamepadType  SDL_GetGamepadType(SDL_Gamepad* gamepad);
SDL_PropertiesID SDL_GetGamepadProperties(SDL_Gamepad* gamepad);
bool             SDL_GetBooleanProperty(SDL_PropertiesID props, const char* name, bool default_value);
bool             SDL_RumbleGamepad(SDL_Gamepad* gamepad, uint16_t low, uint16_t high, uint32_t ms);
bool             SDL_SetGamepadLED(SDL_Gamepad* gamepad, uint8_t r, uint8_t g, uint8_t b);
bool             SDL_SendGamepadEffect(SDL_Gamepad* gamepad, const void* data, int size);
bool             SDL_GamepadHasSensor(SDL_Gamepad* gamepad, SDL_SensorType type);
bool             SDL_SetGamepadSensorEnabled(SDL_Gamepad* gamepad, SDL_SensorType type, bool enabled);

// ---- iconv (libCes): the PS4 libc has iconv ------------------------------------------------------
typedef struct SDL_iconv_data_t* SDL_iconv_t;
#define SDL_ICONV_ERROR  (static_cast<size_t>(-1))
#define SDL_ICONV_E2BIG  (static_cast<size_t>(-2))
#define SDL_ICONV_EILSEQ (static_cast<size_t>(-3))
#define SDL_ICONV_EINVAL (static_cast<size_t>(-4))
SDL_iconv_t SDL_iconv_open(const char* tocode, const char* fromcode);
int         SDL_iconv_close(SDL_iconv_t cd);
size_t      SDL_iconv(SDL_iconv_t cd, const char** inbuf, size_t* inbytesleft, char** outbuf,
                      size_t* outbytesleft);

} // extern "C"
