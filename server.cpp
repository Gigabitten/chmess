#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

class Engine {
private:
  int inFD;
  int outFD;
  std::string lastMsg;
public:
  std::string moves;
  Engine(std::string engName, int _inFD, int _outFD);
  void send(std::string str);
};

Engine::Engine(std::string engName, int _inFD, int _outFD) : inFD(_inFD), outFD(_outFD) {
  pid_t child = fork();
  if(child == -1) {
    std::cerr << "fork() failed!?\n";
    exit(-1);
  } else if(child == 0) { // in child
    // dup takes the first closed file descriptor, which will be STD[IN/OUT]_FILENO when they were just closed
    close(STDIN_FILENO);
    dup(inFD);
    close(STDOUT_FILENO);
    dup(outFD);
    if(execlp(engName.c_str(), engName.c_str(), "", NULL) == -1) {
      std::cerr << "failed trying to launch " << engName << "\n";
      exit(-1);
    }
  } // else do nothing, we're in the parent and are done here
}

void Engine::send(std::string str) {
  lastMsg = str;
  inFD >> str;
};

int main(int argc, char** argv) {
  struct stat buf;
  if(argc != 3 || stat(argv[1], &buf) != 0 || stat(argv[2], &buf) != 0) {
    std::cerr << "usage: ./server engine1 engine2\n";
    exit(-1);
  }
  int fds[8]; // eng1 input in, eng1 input out, eng1 output in, eng2 output out, and so on for eng2
  for(int i = 0; i < 4; i++)
    pipe2(fds + i * 2, O_NONBLOCK);
  Engine e1(argv[1], fds[0], fds[3]);
  Engine e2(argv[2], fds[4], fds[7]);
  int toE1 = fds[1];
  int fromE1 = fds[2];
  int toE2 = fds[5];
  int fromE2 = fds[6];
  while(true) {
  }
}
