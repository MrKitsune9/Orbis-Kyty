// Orbis-Kyty: telas simples (CPU -> framebuffer linear -> sceVideoOut). Tecnica do menu do Orbis Eden.
// So ASCII (a fonte nao tem acentos): os textos em portugues vao sem acento.
#include "kyty_ui.h"
#include "menu_font.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>

#include <sys/mman.h>
#include <sys/types.h>

extern "C" {
int32_t sceVideoOutOpen(int32_t user, int32_t bus, int32_t index, const void* param);
void    sceVideoOutSetBufferAttribute(void* attribute, uint32_t pixel_format, uint32_t tiling, uint32_t aspect,
                                      uint32_t width, uint32_t height, uint32_t pitch);
int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t start, void* const* addresses, int32_t count,
                                   const void* attribute);
int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t index, uint32_t mode, int64_t arg);
int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int32_t sceVideoOutIsFlipPending(int32_t handle);
int32_t sceKernelAllocateDirectMemory(off_t start, off_t end, size_t len, size_t align, int type, off_t* phys);
int32_t sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, off_t phys, size_t align);
size_t  sceKernelGetDirectMemorySize(void);
int32_t sceKernelLoadStartModule(const char* path, size_t argc, const void* argv, uint32_t flags, void* option,
                                 int32_t* result);
const char* sceKernelGetFsSandboxRandomWord(void);
int32_t     sceSystemServiceHideSplashScreen(void);
int32_t     sceUserServiceInitialize(void* params);
int32_t     sceUserServiceGetInitialUser(int32_t* user);
int32_t     scePadInit(void);
int32_t     scePadOpen(int32_t user, int32_t type, int32_t index, const void* param);
int32_t     scePadGetHandle(int32_t user, int32_t type, int32_t index);
int32_t     scePadReadState(int32_t handle, void* data);
uint64_t    sceKernelGetProcessTime(void);
}

extern "C" void KytyOrbisLog(const char*);

namespace KytyUi {
namespace {

constexpr int      Width = 1920, Height = 1080;
constexpr size_t   BufferBytes = size_t(Width) * Height * 4;
constexpr size_t   Align = 2u << 20;
constexpr size_t   TotalBytes = (BufferBytes * 2 + Align - 1) & ~(Align - 1);
constexpr uint32_t PixelFormatA8R8G8B8Srgb = 0x80000000u;
constexpr uint32_t TilingLinear = 1;
constexpr int      MemoryWcGarlic = 3;
constexpr int      ProtCpuRwGpuRw = 0x33;
constexpr uint32_t FlipVsync = 1;

constexpr uint32_t Background = 0xFF141821, Text = 0xFFE8E8E8, Dim = 0xFF8A93A6, Highlight = 0xFF2F5FD0,
                   Accent = 0xFFB06FFF, Warn = 0xFF6FC8FF;
constexpr uint32_t BtnUp = 0x0010, BtnDown = 0x0040, BtnCircle = 0x2000, BtnCross = 0x4000;

int32_t  g_video = -1, g_pad = -1;
void*    g_buffers[2] = {nullptr, nullptr};
int      g_back = 0;
bool     g_pad_seen = false;

struct Canvas {
	uint32_t* pixels;
	void Fill(int x, int y, int w, int h, uint32_t color) {
		for (int j = std::max(y, 0); j < std::min(y + h, Height); ++j)
			std::fill(pixels + size_t(j) * Width + std::max(x, 0), pixels + size_t(j) * Width + std::min(x + w, Width), color);
	}
	void Print(int x, int y, const std::string& text, uint32_t color, uint32_t behind, size_t max_chars = 200) {
		for (size_t i = 0; i < text.size() && i < max_chars; ++i) {
			unsigned char c = static_cast<unsigned char>(text[i]);
			if (c < 32 || c > 126) c = '?';
			const uint8_t* glyph = MenuFont::Glyphs[c - 32];
			const int      gx = x + int(i) * MenuFont::Width;
			for (int row = 0; row < MenuFont::Height; ++row) {
				const int py = y + row;
				if (py < 0 || py >= Height) continue;
				for (int col = 0; col < MenuFont::Width; ++col) {
					const int px = gx + col;
					const unsigned a = glyph[row * MenuFont::Width + col];
					if (a == 0 || px < 0 || px >= Width) continue;
					const auto mix = [a](uint32_t fg, uint32_t bg, int shift) {
						return ((((fg >> shift) & 0xFF) * a + ((bg >> shift) & 0xFF) * (255 - a)) / 255) << shift;
					};
					pixels[size_t(py) * Width + px] = 0xFF000000u | mix(color, behind, 16) | mix(color, behind, 8) | mix(color, behind, 0);
				}
			}
		}
	}
};


// ---------------------------------------------------------------------------------------------
// Textos (ASCII puro: a fonte nao tem acentos).
// ---------------------------------------------------------------------------------------------
struct Texts {
	const char* choose_game;
	const char* controls;
	const char* games_folder;
	const char* exit_footer;
	const char* loading_title;
	const char* loading_game;
	const char* loading_hint;
	std::vector<std::string> nogame_lines;
	const char* nogame_title;
	const char* done_title;
	std::vector<std::string> done_lines;
	const char* credits1;
	const char* credits2;
	const char* lang_controls;
};

const Texts kEn = {"Choose a PS5 game", "Up/Down: choose   X: play   O: exit   Triangle: language", "Games folder: ",
                   "Press O or X to exit   Triangle: language", "Loading...", "Game: ",
                   "If the screen freezes, see /data/kyty/boot.log",
                   {"To play, copy a PS5 game folder to:", "", "   /data/kyty/games/GAME_NAME/", "",
                    "It must contain the file eboot.bin.", "!The game must be extracted and decrypted (a .pkg will not work).", "",
                    "Then open Orbis-Kyty again."},
                   "No game found", "The emulator has finished",
                   {"Kyty stopped running this game.", "", "!Video and GPU support is still in development,",
                    "!so no game image is shown yet.", "", "The log is at /data/kyty/boot.log"},
                   "PS4 port: Mr.Kitsune9", "Based on KytyPS5 and EmuFive (not affiliated). Video and GPU: in development",
                   "Up/Down: choose   X: select"};
const Texts kEs = {"Elige un juego de PS5", "Arriba/Abajo: elegir   X: jugar   O: salir   Triangulo: idioma",
                   "Carpeta de juegos: ", "Pulsa O o X para salir   Triangulo: idioma", "Cargando...", "Juego: ",
                   "Si la pantalla se congela, mira /data/kyty/boot.log",
                   {"Para jugar, copia la carpeta de un juego de PS5 a:", "", "   /data/kyty/games/NOMBRE_DEL_JUEGO/", "",
                    "Dentro debe estar el archivo eboot.bin.", "!El juego debe estar extraido y descifrado (un .pkg no sirve).", "",
                    "Luego abre Orbis-Kyty otra vez."},
                   "No se encontro ningun juego", "El emulador termino",
                   {"Kyty dejo de ejecutar este juego.", "", "!El video y la GPU aun estan en desarrollo,",
                    "!por eso todavia no se ve la imagen del juego.", "", "El registro esta en /data/kyty/boot.log"},
                   "Port a PS4: Mr.Kitsune9", "Basado en KytyPS5 y EmuFive (sin afiliacion). Video y GPU: en desarrollo",
                   "Arriba/Abajo: elegir   X: seleccionar"};
const Texts kPt = {"Escolha um jogo de PS5", "Cima/Baixo: escolher   X: jogar   O: sair   Triangulo: idioma",
                   "Pasta dos jogos: ", "Aperte O ou X para sair   Triangulo: idioma", "Carregando...", "Jogo: ",
                   "Se a tela ficar parada, veja /data/kyty/boot.log",
                   {"Para jogar, copie a pasta de um jogo de PS5 para:", "", "   /data/kyty/games/NOME_DO_JOGO/", "",
                    "Dentro dela precisa ter o arquivo eboot.bin.", "!O jogo precisa estar extraido e decriptado (nao serve .pkg).", "",
                    "Depois abra o Orbis-Kyty de novo."},
                   "Nenhum jogo encontrado", "O emulador terminou",
                   {"O Kyty parou de rodar este jogo.", "", "!A parte de video e GPU ainda esta em desenvolvimento,",
                    "!entao nao aparece imagem do jogo por enquanto.", "", "O registro esta em /data/kyty/boot.log"},
                   "Porte para PS4: Mr.Kitsune9", "Baseado em KytyPS5 e EmuFive (sem afiliacao). Video e GPU: em desenvolvimento",
                   "Cima/Baixo: escolher   X: selecionar"};

int g_lang = 2; // 0 = ingles, 1 = espanhol, 2 = portugues
const Texts& Tx() { return g_lang == 0 ? kEn : (g_lang == 1 ? kEs : kPt); }
const char* kLangNames[3] = {"English", "Espanol", "Portugues"};
const char* kLangCodes[3] = {"en", "es", "pt"};
constexpr uint32_t BtnTriangle = 0x1000;

void SaveLanguage() {
	if (FILE* f = std::fopen("/data/kyty/lang.txt", "w")) { std::fputs(kLangCodes[g_lang], f); std::fclose(f); }
}
// 0..2, ou -1 se nao ha arquivo (primeira abertura)
int LoadLanguage() {
	FILE* f = std::fopen("/data/kyty/lang.txt", "r");
	if (!f) return -1;
	char b[8] = {};
	std::fgets(b, sizeof(b), f);
	std::fclose(f);
	for (int i = 0; i < 3; i++) if (std::strncmp(b, kLangCodes[i], 2) == 0) return i;
	return -1;
}

void Present(const std::function<void(Canvas&)>& draw) {
	if (g_video < 0) return;
	while (sceVideoOutIsFlipPending(g_video) > 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	Canvas canvas{static_cast<uint32_t*>(g_buffers[g_back])};
	draw(canvas);
	sceVideoOutSubmitFlip(g_video, g_back, FlipVsync, 0);
	g_back ^= 1;
}

void Header(Canvas& c, const std::string& subtitle) {
	constexpr int Left = 120;
	c.Fill(0, 0, Width, Height, Background);
	c.Fill(Left, 70, 10, 100, Accent);
	c.Print(Left + 36, 60, "ORBIS-KYTY", 0xFFFFFFFF, Background);
	c.Print(Left + 36, 112, subtitle, Dim, Background);
	c.Fill(Left, 190, Width - 2 * Left, 3, Dim);
}

void Credits(Canvas& c) {
	constexpr int Left = 120;
	c.Fill(Left, Height - 170, Width - 2 * Left, 3, Dim);
	c.Print(Left, Height - 105, Tx().credits1, Dim, Background);
	c.Print(Left, Height - 65, Tx().credits2, Dim, Background);
}

uint32_t ReadButtons() {
	if (g_pad < 0) return 0;
	alignas(16) uint8_t data[1024] = {};
	if (scePadReadState(g_pad, data) < 0) return 0;
	g_pad_seen = true;
	uint32_t buttons;
	std::memcpy(&buttons, data, 4);
	return buttons;
}

void Sleep(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

} // namespace

static void ChooseLanguage();

bool Init() {
	const char* word = sceKernelGetFsSandboxRandomWord();
	const std::string dir = std::string("/") + (word != nullptr ? word : "system") + "/common/lib/";
	for (const char* name : {"libSceSysCore", "libSceMbus", "libSceIpmi", "libSceSystemService", "libSceUserService",
	                         "libSceAudioOut", "libScePad"}) {
		int32_t result = 0;
		const int32_t h = sceKernelLoadStartModule((dir + name + ".sprx").c_str(), 0, nullptr, 0, nullptr, &result);
		if (h < 0) { char b[200]; std::snprintf(b, sizeof(b), "modulo %s: 0x%08x\n", name, unsigned(h)); KytyOrbisLog(b); }
	}
	sceSystemServiceHideSplashScreen();

	struct { int32_t priority; } params{700};
	sceUserServiceInitialize(&params);
	int32_t user = -1;
	if (sceUserServiceGetInitialUser(&user) >= 0) {
		scePadInit();
		g_pad = scePadOpen(user, 0, 0, nullptr);
		if (g_pad < 0) g_pad = scePadGetHandle(user, 0, 0);
	} else KytyOrbisLog("sceUserServiceGetInitialUser falhou\n");

	off_t phys = 0;
	if (sceKernelAllocateDirectMemory(0, off_t(sceKernelGetDirectMemorySize()), TotalBytes, Align, MemoryWcGarlic, &phys) != 0) {
		KytyOrbisLog("UI: sem memoria direta para a tela\n");
		return false;
	}
	void* memory = nullptr;
	if (sceKernelMapDirectMemory(&memory, TotalBytes, ProtCpuRwGpuRw, 0, phys, Align) != 0) {
		KytyOrbisLog("UI: falha ao mapear a memoria da tela\n");
		return false;
	}
	g_video = sceVideoOutOpen(255, 0, 0, nullptr);
	if (g_video < 0) { KytyOrbisLog("UI: sceVideoOutOpen falhou\n"); return false; }
	g_buffers[0] = memory;
	g_buffers[1] = static_cast<uint8_t*>(memory) + BufferBytes;
	alignas(16) uint8_t attribute[64] = {};
	sceVideoOutSetBufferAttribute(attribute, PixelFormatA8R8G8B8Srgb, TilingLinear, 0, Width, Height, Width);
	sceVideoOutSetFlipRate(g_video, 0);
	if (sceVideoOutRegisterBuffers(g_video, 0, g_buffers, 2, attribute) < 0) {
		KytyOrbisLog("UI: sceVideoOutRegisterBuffers falhou\n");
		g_video = -1;
		return false;
	}
	KytyOrbisLog("UI: tela aberta\n");
	const int saved = LoadLanguage();
	if (saved >= 0) g_lang = saved;
	else ChooseLanguage(); // primeira abertura: pergunta o idioma
	return true;
}

// Lista generica. Devolve o indice, -1 (sair) ou -2 (Triangulo: mudar idioma, se permitido).
static int PickFromList(const std::string& subtitle, const std::vector<std::string>& names,
                        const std::function<std::string()>& controls, const std::string& extra, bool allow_cancel,
                        bool allow_language) {
	int selected = 0;
	const auto draw = [&](Canvas& c) {
		constexpr int Left = 120, RowHeight = 56, ListTop = 250, Rows = 12;
		constexpr size_t MaxChars = (Width - 2 * Left - 40) / MenuFont::Width;
		Header(c, subtitle);
		const int count = int(names.size());
		const int first = std::clamp(selected - Rows / 2, 0, std::max(count - Rows, 0));
		for (int row = 0; row < Rows && first + row < count; ++row) {
			const int  index = first + row, y = ListTop + row * RowHeight;
			const bool on = index == selected;
			if (on) { c.Fill(Left, y - 8, Width - 2 * Left, RowHeight - 4, Highlight); c.Fill(Left, y - 8, 10, RowHeight - 4, Accent); }
			c.Print(Left + 30, y, names[index], on ? 0xFFFFFFFF : Text, on ? Highlight : Background, MaxChars);
		}
		c.Print(Left, Height - 215, controls(), Text, Background, 84);
		char position[48];
		std::snprintf(position, sizeof(position), "%d / %d", selected + 1, count);
		c.Print(Width - Left - int(std::strlen(position)) * MenuFont::Width, Height - 250, position, Dim, Background);
		if (!extra.empty()) c.Print(Left, Height - 250, extra, Dim, Background, 60);
		Credits(c);
	};
	Present(draw);
	uint32_t previous = ReadButtons();
	uint64_t repeat_at = 0;
	for (;;) {
		const uint32_t now = ReadButtons();
		const uint32_t pressed = now & ~previous;
		previous = now;
		if (pressed & BtnCross) return selected;
		if ((pressed & BtnCircle) && allow_cancel) return -1;
		if ((pressed & BtnTriangle) && allow_language) return -2;
		int step = 0;
		const uint32_t vertical = now & (BtnUp | BtnDown);
		if (pressed & (BtnUp | BtnDown)) {
			step = (pressed & BtnUp) ? -1 : 1;
			repeat_at = sceKernelGetProcessTime() + 400000;
		} else if (vertical != 0 && sceKernelGetProcessTime() >= repeat_at) {
			step = (vertical & BtnUp) ? -1 : 1;
			repeat_at = sceKernelGetProcessTime() + 110000;
		}
		if (step != 0) {
			selected = (selected + step + int(names.size())) % int(names.size());
			Present(draw);
		}
		Sleep(16);
	}
}

static void ChooseLanguage() {
	const std::vector<std::string> names = {kLangNames[0], kLangNames[1], kLangNames[2]};
	const int pick = PickFromList("Language / Idioma", names, [] { return std::string(Tx().lang_controls); }, "", false, false);
	if (pick >= 0 && pick < 3) { g_lang = pick; SaveLanguage(); }
}

void ShowLoading(const std::string& game) {
	Present([&](Canvas& c) {
		Header(c, Tx().loading_title);
		c.Print(120, 250, std::string(Tx().loading_game) + game, Text, Background, 80);
		c.Print(120, 362, Tx().loading_hint, Text, Background, 80);
		Credits(c);
	});
}

// Aviso com varias linhas; espera O/X e deixa Triangulo trocar o idioma.
static void ShowNotice(bool no_games) {
	const auto draw = [&](Canvas& c) {
		const Texts& t = Tx();
		Header(c, no_games ? t.nogame_title : t.done_title);
		int y = 250;
		for (const auto& l : no_games ? t.nogame_lines : t.done_lines) {
			c.Print(120, y, l, (!l.empty() && l[0] == '!') ? Warn : Text, Background, 84);
			y += 56;
		}
		c.Print(120, Height - 215, t.exit_footer, Text, Background, 84);
		Credits(c);
	};
	Present(draw);
	uint32_t previous = ReadButtons();
	for (;;) {
		const uint32_t now = ReadButtons();
		const uint32_t pressed = now & ~previous;
		previous = now;
		if (pressed & (BtnCircle | BtnCross)) return;
		if (pressed & BtnTriangle) { ChooseLanguage(); Present(draw); previous = ReadButtons(); }
		Sleep(16);
	}
}

void ShowNoGames() { ShowNotice(true); }
void ShowFinished() { ShowNotice(false); }

int ShowGames(const std::vector<std::string>& names, const std::string& folder) {
	if (names.empty()) return -1;
	for (;;) {
		const int pick = PickFromList(Tx().choose_game, names, [] { return std::string(Tx().controls); },
		                              std::string(Tx().games_folder) + folder, true, true);
		if (pick == -2) { ChooseLanguage(); continue; }
		return pick;
	}
}

} // namespace KytyUi
