// Orbis-Kyty: telas simples desenhadas direto na saida de video do PS4 (sem Vulkan).
// Idiomas: ingles, espanhol e portugues (escolha salva em /data/kyty/lang.txt; Triangulo troca).
#pragma once
#include <string>
#include <vector>

namespace KytyUi {

// Carrega os modulos do sistema, abre controle e video, e le o idioma salvo (na 1a vez, pergunta).
// false = sem tela (o programa segue sem UI).
bool Init();

// Lista de jogos: Cima/Baixo escolhem, X joga, O sai, Triangulo muda o idioma. Devolve o indice ou -1.
int ShowGames(const std::vector<std::string>& names, const std::string& folder);
// Avisos (esperam O ou X; Triangulo muda o idioma).
void ShowNoGames();
void ShowFinished();
// Sem esperar tecla.
void ShowLoading(const std::string& game);

} // namespace KytyUi
