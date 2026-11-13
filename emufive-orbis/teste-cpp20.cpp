#include <ranges>
#include <vector>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <cstdio>
int main(){
  std::vector<int> v{3,1,2};
  std::ranges::sort(v);
  std::jthread t([]{ std::puts("hi"); });
  auto p = std::filesystem::path("/data") / "x";
  auto sq = v | std::views::transform([](int x){return x*x;});
  return (int)p.string().size() + (int)std::ranges::distance(sq);
}
