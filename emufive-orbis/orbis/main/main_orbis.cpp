// Orbis-Kyty: ponto de entrada do PS4. Le /data/kyty/launch.txt (linha 1: pasta app0 do jogo PS5,
// linha 2: caminho do ELF) e chama o emulador. Tudo vai para /data/kyty/boot.log.
#include "emulator.h"
#include "common/threads.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "common/orbis/mcontext.h"
#include "kyty_ui.h"

static FILE* g_log = nullptr;

// ---- Registro de falhas: roda ANTES de qualquer construtor global do Kyty (prioridade 101). ----
static void WriteRaw(const char* path, const char* text) {
	int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd >= 0) { write(fd, text, strlen(text)); fsync(fd); close(fd); }
}

extern "C" int main(int, char**);

static void CrashHandler(int sig, void* ctx) {
	auto*  m = OrbisContext(ctx);
	char   b[1024];
	snprintf(b, sizeof(b),
	         "CRASH sinal=%d rip=%llx rsp=%llx rbp=%llx falha_em=%llx rax=%llx rbx=%llx rcx=%llx rdx=%llx "
	         "rsi=%llx rdi=%llx main=%llx\n",
	         sig, (unsigned long long)m->mc_rip, (unsigned long long)m->mc_rsp, (unsigned long long)m->mc_rbp,
	         (unsigned long long)m->mc_addr, (unsigned long long)m->mc_rax, (unsigned long long)m->mc_rbx,
	         (unsigned long long)m->mc_rcx, (unsigned long long)m->mc_rdx, (unsigned long long)m->mc_rsi,
	         (unsigned long long)m->mc_rdi, (unsigned long long)(uintptr_t)&main);
	WriteRaw("/data/kyty/crash.log", b);
	// pilha de chamadas pelos frames (-fno-omit-frame-pointer), com limites para nao falhar de novo
	uint64_t* fp = reinterpret_cast<uint64_t*>(m->mc_rbp);
	uint64_t  lo = static_cast<uint64_t>(m->mc_rsp), hi = lo + (8u << 20);
	for (int i = 0; i < 24 && reinterpret_cast<uint64_t>(fp) >= lo && reinterpret_cast<uint64_t>(fp) < hi &&
	                (reinterpret_cast<uint64_t>(fp) & 7) == 0;
	     i++) {
		snprintf(b, sizeof(b), "  frame %d ret=%llx\n", i, (unsigned long long)fp[1]);
		WriteRaw("/data/kyty/crash.log", b);
		fp = reinterpret_cast<uint64_t*>(fp[0]);
	}
	_exit(1);
}

__attribute__((constructor(101))) static void EarlyInit() {
	mkdir("/data/kyty", 0777);
	WriteRaw("/data/kyty/early.log", "1: construtores iniciaram (o programa carregou)\n");
	for (int sig : {4, 5, 8, 10, 11}) sceKernelInstallExceptionHandler(sig, CrashHandler);
}

extern "C" void KytyOrbisLog(const char* s) {
	if (g_log) { fputs(s, g_log); fflush(g_log); fsync(fileno(g_log)); }
}

static std::string ReadLine(FILE* f) {
	char buf[1024] = {};
	if (!fgets(buf, sizeof(buf), f)) return {};
	std::string s(buf);
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
	return s;
}

static const char* kGamesDir = "/data/kyty/games";

// Pastas em /data/kyty/games que tem um eboot.bin dentro.
static std::vector<std::string> FindGames() {
	std::vector<std::string> games;
	if (DIR* d = opendir(kGamesDir)) {
		while (dirent* e = readdir(d)) {
			if (e->d_name[0] == '.') continue;
			std::string eboot = std::string(kGamesDir) + "/" + e->d_name + "/eboot.bin";
			struct stat st {};
			if (stat(eboot.c_str(), &st) == 0) games.emplace_back(e->d_name);
		}
		closedir(d);
	}
	std::sort(games.begin(), games.end());
	return games;
}

static void RunGame(const std::string& app0, const std::string& elf, bool ui) {
	if (ui) KytyUi::ShowLoading(app0);
	KytyOrbisLog(("Jogo: " + app0 + "\n").c_str());
	Emulator::RunOptions opt;
	opt.app0_dir = app0;
	opt.elf      = elf;
	KytyOrbisLog("Chamando Emulator::Run\n");
	Common::InitializeThreads();
	Emulator::Run(opt);
	KytyOrbisLog("Emulator::Run retornou\n");
	if (ui) KytyUi::ShowFinished();
}

int main(int, char**) {
	mkdir("/data/kyty", 0777);
	mkdir(kGamesDir, 0777);
	rename("/data/kyty/boot.log", "/data/kyty/boot.old.log");
	g_log = fopen("/data/kyty/boot.log", "w");
	WriteRaw("/data/kyty/early.log", "2: main alcancado\n");
	// o construtor de prioridade 101 nao rodou neste SDK (early.log nao teve a linha 1): instala aqui tambem
	for (int sig : {4, 5, 8, 10, 11}) sceKernelInstallExceptionHandler(sig, CrashHandler);
	KytyOrbisLog("Orbis-Kyty: iniciando\n");

	const bool ui = KytyUi::Init();

	// launch.txt (opcional) continua valendo: linha 1 = pasta do jogo, linha 2 = eboot.bin
	if (FILE* f = fopen("/data/kyty/launch.txt", "r")) {
		std::string app0 = ReadLine(f), elf = ReadLine(f);
		fclose(f);
		if (!app0.empty() && !elf.empty()) {
			KytyOrbisLog("launch.txt encontrado\n");
			RunGame(app0, elf, ui);
			return 0;
		}
	}

	std::vector<std::string> games = FindGames();
	if (games.empty()) {
		KytyOrbisLog("Nenhum jogo em /data/kyty/games\n");
		if (ui) KytyUi::ShowNoGames();
		return 0;
	}
	KytyOrbisLog(("Jogos encontrados: " + std::to_string(games.size()) + "\n").c_str());
	int choice = ui ? KytyUi::ShowGames(games, kGamesDir) : 0;
	if (choice < 0) return 0;
	const std::string app0 = std::string(kGamesDir) + "/" + games[choice];
	RunGame(app0, app0 + "/eboot.bin", ui);
	return 0;
}
