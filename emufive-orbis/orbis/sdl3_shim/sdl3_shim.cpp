// SPDX-License-Identifier: GPL-3.0-or-later
// See SDL3/SDL.h. Audio: one sceAudioOut port per stream, fed by a thread that is paced by the
// hardware (sceAudioOutOutput blocks until the port has room), same scheme as the Orbis Eden sink.

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <ctime>
#include <iconv.h>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern "C" {
int sceAudioOutInit(void);
int sceAudioOutOpen(int user_id, int type, int index, unsigned int len, unsigned int freq,
                    unsigned int param);
int sceAudioOutOutput(int handle, const void* ptr);
int sceAudioOutClose(int handle);
}

namespace {
constexpr int      UserIdSystem  = 0xFF; // SCE_USER_SERVICE_USER_ID_SYSTEM
constexpr int      PortTypeMain  = 0;    // SCE_AUDIO_OUT_PORT_TYPE_MAIN
constexpr unsigned ParamS16Ster  = 1;    // SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO
constexpr unsigned GrainFrames   = 256;
constexpr unsigned OutputRate    = 48000; // the only rate the main port takes

bool InitAudioOut() {
	static const int result = sceAudioOutInit();
	return result == 0 || static_cast<unsigned>(result) == 0x8026000Eu; // 0x8026000E: already done
}

int16_t ToS16(float v) {
	v = std::clamp(v, -1.0f, 1.0f);
	return static_cast<int16_t>(std::lrintf(v * 32767.0f));
}
} // namespace

struct SDL_AudioStream {
	SDL_AudioSpec        spec {};
	int                  handle = -1;
	std::atomic<bool>    paused {true};
	std::atomic<bool>    stop {false};
	std::mutex           lock;
	std::vector<uint8_t> queue; // bytes in the stream's own format
	size_t               head = 0;
	std::thread          feeder;

	size_t BytesPerFrame() const {
		return static_cast<size_t>(spec.channels) * (spec.format == SDL_AUDIO_F32 ? 4u : 2u);
	}
	size_t Queued() {
		std::lock_guard<std::mutex> g(lock);
		return queue.size() - head;
	}

	// One stereo S16 frame at the output rate from the frame at (fractional) source index.
	void Render(int16_t* out, unsigned frames) {
		const size_t bpf = BytesPerFrame();
		const double step = static_cast<double>(spec.freq) / OutputRate; // nearest-neighbour resample
		std::lock_guard<std::mutex> g(lock);
		double pos = 0.0;
		for (unsigned i = 0; i < frames; ++i) {
			const size_t src_frame = static_cast<size_t>(pos);
			const size_t off = head + src_frame * bpf;
			if (off + bpf > queue.size()) {
				out[i * 2] = out[i * 2 + 1] = 0; // underrun: silence
				pos += step;
				continue;
			}
			float ch[8] = {};
			const int n = std::min(spec.channels, 8);
			for (int c = 0; c < n; ++c) {
				if (spec.format == SDL_AUDIO_F32) {
					float f;
					std::memcpy(&f, &queue[off + c * 4], 4);
					ch[c] = f;
				} else {
					int16_t s;
					std::memcpy(&s, &queue[off + c * 2], 2);
					ch[c] = static_cast<float>(s) / 32768.0f;
				}
			}
			float l = ch[0], r = n > 1 ? ch[1] : ch[0];
			if (n >= 6) { // 5.1/7.1 in SDL order (FL FR FC LFE BL BR [SL SR]): fold down
				l += 0.707f * ch[2] + 0.5f * ch[4];
				r += 0.707f * ch[2] + 0.5f * ch[5];
				if (n >= 8) {
					l += 0.5f * ch[6];
					r += 0.5f * ch[7];
				}
				l *= 0.6f;
				r *= 0.6f;
			}
			out[i * 2]     = ToS16(l);
			out[i * 2 + 1] = ToS16(r);
			pos += step;
		}
		const size_t consumed = std::min(queue.size() - head, static_cast<size_t>(pos) * bpf);
		head += consumed;
		if (head > (1u << 20) && head * 2 > queue.size()) { // compact now and then
			queue.erase(queue.begin(), queue.begin() + static_cast<std::ptrdiff_t>(head));
			head = 0;
		}
	}

	void Feed() {
		std::vector<int16_t> grain(GrainFrames * 2);
		while (!stop.load()) {
			if (paused.load()) {
				std::fill(grain.begin(), grain.end(), 0);
			} else {
				Render(grain.data(), GrainFrames);
			}
			sceAudioOutOutput(handle, grain.data());
		}
		sceAudioOutOutput(handle, nullptr); // wait for the last grain
	}
};

extern "C" {

static thread_local char g_error[128] = "";

bool SDL_InitSubSystem(SDL_InitFlags) { return true; }
void SDL_QuitSubSystem(SDL_InitFlags) {}
const char* SDL_GetError(void) { return g_error; }
void SDL_free(void* mem) { std::free(mem); }
uint64_t SDL_GetTicks(void) {
	timespec ts {};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<uint64_t>(ts.tv_sec) * 1000u + static_cast<uint64_t>(ts.tv_nsec) / 1000000u;
}
void SDL_Delay(uint32_t ms) { usleep(ms * 1000u); }
const char* SDL_GetBasePath(void) { return "/app0/"; }

SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID, const SDL_AudioSpec* spec,
                                           SDL_AudioStreamCallback, void*) {
	if (spec == nullptr || spec->channels < 1 || spec->freq < 8000 ||
	    (spec->format != SDL_AUDIO_S16 && spec->format != SDL_AUDIO_F32)) {
		std::snprintf(g_error, sizeof(g_error), "unsupported audio spec");
		return nullptr;
	}
	if (!InitAudioOut()) {
		std::snprintf(g_error, sizeof(g_error), "sceAudioOutInit failed");
		return nullptr;
	}
	auto* s = new SDL_AudioStream;
	s->spec = *spec;
	s->handle = sceAudioOutOpen(UserIdSystem, PortTypeMain, 0, GrainFrames, OutputRate, ParamS16Ster);
	if (s->handle < 0) {
		std::snprintf(g_error, sizeof(g_error), "sceAudioOutOpen failed: 0x%x", static_cast<unsigned>(s->handle));
		delete s;
		return nullptr;
	}
	s->feeder = std::thread([s] { s->Feed(); });
	return s;
}

void SDL_DestroyAudioStream(SDL_AudioStream* s) {
	if (s == nullptr) { return; }
	s->stop = true;
	if (s->feeder.joinable()) { s->feeder.join(); }
	if (s->handle >= 0) { sceAudioOutClose(s->handle); }
	delete s;
}
bool SDL_ResumeAudioStreamDevice(SDL_AudioStream* s) { if (!s) return false; s->paused = false; return true; }
bool SDL_PauseAudioStreamDevice(SDL_AudioStream* s) { if (!s) return false; s->paused = true; return true; }
bool SDL_PutAudioStreamData(SDL_AudioStream* s, const void* buf, int len) {
	if (!s || !buf || len < 0) { return false; }
	std::lock_guard<std::mutex> g(s->lock);
	const auto* p = static_cast<const uint8_t*>(buf);
	s->queue.insert(s->queue.end(), p, p + len);
	return true;
}
int SDL_GetAudioStreamData(SDL_AudioStream*, void*, int) { return 0; } // playback streams only
int SDL_GetAudioStreamQueued(SDL_AudioStream* s) { return s ? static_cast<int>(s->Queued()) : 0; }
int SDL_GetAudioStreamAvailable(SDL_AudioStream* s) { return s ? static_cast<int>(s->Queued()) : 0; }
bool SDL_ClearAudioStream(SDL_AudioStream* s) {
	if (!s) return false;
	std::lock_guard<std::mutex> g(s->lock);
	s->queue.clear();
	s->head = 0;
	return true;
}
SDL_AudioDeviceID SDL_GetAudioStreamDevice(SDL_AudioStream* s) { return s ? 1u : 0u; }
bool SDL_AudioDevicePaused(SDL_AudioDeviceID) { return false; }
SDL_AudioDeviceID* SDL_GetAudioRecordingDevices(int* count) { if (count) *count = 0; return nullptr; }
const char* SDL_GetAudioDeviceName(SDL_AudioDeviceID) { return "PS4 audio out"; }

SDL_Gamepad* SDL_GetGamepadFromID(SDL_JoystickID) { return nullptr; }
void SDL_CloseGamepad(SDL_Gamepad*) {}
SDL_GamepadType SDL_GetGamepadType(SDL_Gamepad*) { return SDL_GAMEPAD_TYPE_PS4; }
SDL_PropertiesID SDL_GetGamepadProperties(SDL_Gamepad*) { return 0; }
bool SDL_GetBooleanProperty(SDL_PropertiesID, const char*, bool d) { return d; }
bool SDL_RumbleGamepad(SDL_Gamepad*, uint16_t, uint16_t, uint32_t) { return false; }
bool SDL_SetGamepadLED(SDL_Gamepad*, uint8_t, uint8_t, uint8_t) { return false; }
bool SDL_SendGamepadEffect(SDL_Gamepad*, const void*, int) { return false; }
bool SDL_GamepadHasSensor(SDL_Gamepad*, SDL_SensorType) { return false; }
bool SDL_SetGamepadSensorEnabled(SDL_Gamepad*, SDL_SensorType, bool) { return false; }

SDL_iconv_t SDL_iconv_open(const char* tocode, const char* fromcode) {
	iconv_t cd = iconv_open(tocode, fromcode);
	return cd == reinterpret_cast<iconv_t>(-1) ? reinterpret_cast<SDL_iconv_t>(-1) : reinterpret_cast<SDL_iconv_t>(cd);
}
int SDL_iconv_close(SDL_iconv_t cd) {
	return cd && cd != reinterpret_cast<SDL_iconv_t>(-1) ? iconv_close(reinterpret_cast<iconv_t>(cd)) : 0;
}
size_t SDL_iconv(SDL_iconv_t cd, const char** inbuf, size_t* inleft, char** outbuf, size_t* outleft) {
	const size_t r = iconv(reinterpret_cast<iconv_t>(cd), const_cast<char**>(inbuf), inleft, outbuf, outleft);
	if (r != static_cast<size_t>(-1)) { return r; }
	switch (errno) {
		case E2BIG:  return SDL_ICONV_E2BIG;
		case EILSEQ: return SDL_ICONV_EILSEQ;
		case EINVAL: return SDL_ICONV_EINVAL;
		default:     return SDL_ICONV_ERROR;
	}
}

} // extern "C"
