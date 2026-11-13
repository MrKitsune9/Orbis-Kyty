// Chamado por todo stub de unported_stubs.S: anota o nome no boot.log (uma vez por funcao) e devolve 0.
#include <cstdio>
#include <cstring>
extern "C" void KytyOrbisLog(const char*);
extern "C" long KytyOrbisStubTrap(const char* name) {
	static const char* seen[512];
	static int         n = 0;
	for (int i = 0; i < n; i++) if (seen[i] == name) return 0;
	if (n < 512) seen[n++] = name;
	char buf[400];
	snprintf(buf, sizeof(buf), "[stub] nao portado: %s\n", name);
	KytyOrbisLog(buf);
	return 0;
}
