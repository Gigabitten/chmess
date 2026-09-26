#include <iostream>
#include <errno.h>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

void check(std::string expected, std::string actual) {
  if(actual != expected) {
    std::cout << "debug: squengine expected " << expected << " but got " << actual << " - exiting\n";
    std::cout << "OHNO\n";
    exit(-1);
  } else {
    std::cout << "debug: squengine got " << actual << "\n";
  }
}

void waitCheckRespond(std::string expected, std::string response) {
  std::string received;
  bool wait = false;
  do {
    std::getline(std::cin, received);
    wait = std::cin.fail() && errno == EAGAIN;
    // just to prevent outright spam
    if(wait)
      std::this_thread::sleep_for(10us);
  }
  while(wait);
  check(expected, received);
  std::cout << response;
}

int main() {
  waitCheckRespond("uci", "id name squengine\nid author squingly\nuciok\n");
  // set up board and such
  waitCheckRespond("isready", "readyok\n");
  // and now we loop!
}
