// Orbis-Kyty: ponto de entrada do PS4. Le /data/kyty/launch.txt (linha 1: pasta app0 do jogo PS5,
// linha 2: caminho do ELF) e chama o emulador. Tudo vai para /data/kyty/boot.log.
#include "emulator.h"
#include "common/threads.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static FILE* g_log = nullptr;

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

int main(int, char**) {
	mkdir("/data/kyty", 0777);
	rename("/data/kyty/boot.log", "/data/kyty/boot.old.log");
	g_log = fopen("/data/kyty/boot.log", "w");
	KytyOrbisLog("Orbis-Kyty: iniciando\n");

	FILE* f = fopen("/data/kyty/launch.txt", "r");
	if (!f) {
		KytyOrbisLog("ERRO: crie /data/kyty/launch.txt (linha 1: pasta app0, linha 2: eboot.bin do jogo)\n");
		return 1;
	}
	std::string app0 = ReadLine(f), elf = ReadLine(f);
	fclose(f);
	if (app0.empty() || elf.empty()) {
		KytyOrbisLog("ERRO: launch.txt precisa de 2 linhas\n");
		return 1;
	}

	Emulator::RunOptions opt;
	opt.app0_dir = app0;
	opt.elf      = elf;
	KytyOrbisLog("Chamando Emulator::Run\n");
	Common::InitializeThreads();
	Emulator::Run(opt);
	KytyOrbisLog("Emulator::Run retornou\n");
	return 0;
}
