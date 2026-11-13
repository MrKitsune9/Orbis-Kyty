// Pequenas pecas que o Linux tem e o Orbis nao: cpuinfo (so o nome da CPU) e process_vm_readv.
#include "cpuinfo.h"
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>

extern "C" {

static cpuinfo_package g_package = {};

bool cpuinfo_initialize(void) {
	static const char kName[] = "AMD Jaguar (PS4)";
	std::memcpy(g_package.name, kName, sizeof(kName));
	g_package.processor_count = 8;
	g_package.core_count      = 8;
	return true;
}

const struct cpuinfo_package* cpuinfo_get_package(uint32_t) { return &g_package; }

// O Orbis nao tem process_vm_readv. Usado so para testar se um endereco do jogo e legivel; aqui
// assumimos que e (o handler de falhas do Kyty cobre o resto).
ssize_t process_vm_readv(pid_t, const struct iovec* local, unsigned long liovcnt,
                         const struct iovec* remote, unsigned long riovcnt, unsigned long) {
	if (liovcnt < 1 || riovcnt < 1) return -1;
	size_t n = local[0].iov_len < remote[0].iov_len ? local[0].iov_len : remote[0].iov_len;
	return (ssize_t)n;
}

} // extern "C"

// ---- __cxa_thread_atexit: o libc++abi deste SDK nao tem. Destrutores de thread_local rodam quando a
// thread termina (chave pthread); na thread principal nao rodam (o processo termina de qualquer jeito).
#include <pthread.h>
#include <cstdlib>

namespace {
struct AtexitNode {
	void (*dtor)(void*);
	void*       obj;
	AtexitNode* next;
};
pthread_key_t  g_atexit_key;
pthread_once_t g_atexit_once = PTHREAD_ONCE_INIT;

void RunAtexit(void* head) {
	auto* n = static_cast<AtexitNode*>(head);
	while (n != nullptr) {
		AtexitNode* next = n->next;
		n->dtor(n->obj);
		std::free(n);
		n = next;
	}
}
void MakeKey() { pthread_key_create(&g_atexit_key, RunAtexit); }
} // namespace

extern "C" int __cxa_thread_atexit(void (*dtor)(void*), void* obj, void*) {
	pthread_once(&g_atexit_once, MakeKey);
	auto* node = static_cast<AtexitNode*>(std::malloc(sizeof(AtexitNode)));
	if (node == nullptr) return -1;
	node->dtor = dtor;
	node->obj  = obj;
	node->next = static_cast<AtexitNode*>(pthread_getspecific(g_atexit_key));
	pthread_setspecific(g_atexit_key, node);
	return 0;
}
