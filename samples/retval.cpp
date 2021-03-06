#include "../aotjs_runtime.h"

#include <iostream>

using namespace AotJS;

int main() {
  // Variables hoisted...
  Scope scope;

  {
    Scope scope2;

    Local work;
    Local play;
    Local life;

    work = new Function(
      "work",
      0, // argument count
      // no scope capture
      [] (Function& func, Local this_, ArgList args) -> Local {
        ScopeRetVal scope;
        return scope.escape(new String("work"));
      }
    );

    play = new Function(
      "play",
      0, // argument count
      [] (Function& func, Local this_, ArgList args) -> Local {
        ScopeRetVal scope;
        return scope.escape(new String("play"));
      }
    );

    // todo: operator overloading on Val
    life = work->call(Null(), {}) + play->call(Null(), {});

    // should say "workplay"
    std::cout << "should say 'workplay': " << life->dump() << "\n";

    std::cout << "before gc\n";
    std::cout << engine().dump();
    std::cout << "\n\n";

  }

  engine().gc();

  std::cout << "after gc\n";
  std::cout << engine().dump();
  std::cout << "\n";

  return 0;
}
