#include "MuDSL.hpp"
#include "mull/stdlib/Stdlib.hpp"

int main(int argc, char **argv) {
  try {
    mull::Runtime runtime({argc > 1 ? argv[1] : "mull/stdlib"});
    mull::stdlib::install(runtime);
    runtime.run(R"(
      val Printf <- G:LoadLib("printf") or G:DieWith("Import Error: $E?");
      val Terminal <- G:LoadLib("terminal") or G:DieWith("Import Error: $E?");
      Printf:SetMainSink(G:GetStdout());
      Printf:Println("Hello, MuLL!");
    )");
  } catch (const mull::Error &e) { std::cerr << e.what() << '\n'; return 1; }
}
