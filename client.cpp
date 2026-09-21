#include <iostream>

void check(std::string expected, std::string actual) {
  if(actual != expected) {
    std::cerr << "expected " << expected << " but got " << actual << " - exiting\n";
    exit(-1);
  }
}

int main() {
  std::string received;
  getline(std::cin, received);
  check("uci", received);
  std::cout << "id name squengine\nid author squingly\nuciok\n";
  // set up board and such
  getline(std::cin, received);
  check("isready", received);
  std::cout << "readyok\n";
  // and now we loop!
}
