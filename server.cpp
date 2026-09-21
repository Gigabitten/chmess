#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

class Engine {
private:
  const std::string INFO = "info";
  const std::string UCI = "uci";
  const std::string UCIOK = "uciok";
  const std::string ISREADY = "isready";
  const std::string READYOK = "readyok";
  const std::string UCINEWGAME = "ucinewgame";

  int fromFD;
  int toFD;
  std::string lastMsg;
  std::string verifyOutput(std::string expected, std::string got, std::string next);
public:
  std::string moves;
  Engine(std::string engName, int _fromFD, int _toFD, int inFD, int outFD);
  std::string getNextMessage(std::string engineOutput);
  void sendTo(std::string str);
  std::vector<std::string> recvFrom();
  void process(std::string cmd);
};

std::string Engine::verifyOutput(std::string expected, std::string got, std::string next) {
  if (expected == got) {
    return next;
  }
  else {
    std::cerr << "Error! Engine expected " << expected << ", got " << got << " instead!\n";
    return "err";
  }
}

Engine::Engine(std::string engName, int _fromFD, int _toFD, int inFD, int outFD) : fromFD(_fromFD), toFD(_toFD) {
  pid_t child = fork();
  if(child == -1) {
    std::cerr << "fork() failed!?\n";
    exit(-1);
  } else if(child == 0) { // in child
    lastMsg = "";
    moves = "";
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

void Engine::sendTo(std::string str) {
  lastMsg = str;
  write(toFD, str.c_str(), str.size() + 1);
};

std::string Engine::getNextMessage(std::string engineOutput) {
  if (engineOutput.empty()) {
    return "";
  }
  // i think we can ignore any info string??
  if (engineOutput.length() >= 4 && engineOutput.substr(0, 4) == "info") {
    return "";
  }
  if (lastMsg.empty()) {
    // default to sending uci
    return UCI;
  }
  else {
    if (lastMsg == UCI) {
      return verifyOutput(UCIOK, engineOutput, ISREADY);
    }
    else if (lastMsg == ISREADY) {
      return verifyOutput(READYOK, engineOutput, NULL);
    }
    else return "err";
  }
}

void Engine::process(std::string cmd) {
}

std::vector<std::string> Engine::recvFrom() {
  std::vector<std::string> ret;
  char buf[1024];
  read(fromFD, buf, 1023);
  if(buf[1023] != EOF)
    buf[1023] = '\0'; // just to be sure
  char* end = buf;
  for(char* start = end; *start != EOF && (start - buf < 1023); start = end) {
    for(end = start; *end != '\0' && *end != EOF; end++); // find eof or end
    ret.push_back(std::string(start, end));
  }
  return ret;
}

int main(int argc, char** argv) {
  struct stat buf;
  if(argc != 3 || stat(argv[1], &buf) != 0 || stat(argv[2], &buf) != 0) {
    std::cerr << "usage: ./server engine1 engine2\n";
    exit(-1);
  }
  int fds[8]; // eng1 input in, eng1 input out, eng1 output in, eng2 output out, and so on for eng2
  for(int i = 0; i < 4; i++)
    pipe2(fds + i * 2, O_NONBLOCK);
  Engine e1(argv[1], fds[2], fds[1], fds[0], fds[3]);
  Engine e2(argv[2], fds[6], fds[5], fds[4], fds[7]);
  e1.sendTo("UCI");
  e2.sendTo("UCI");
  while(true) {
    std::vector<std::string> cmds = e1.recvFrom();
    for(std::string& cmd : cmds)
      e1.process(cmd);
    cmds.clear();
    cmds = e2.recvFrom();
    for(std::string& cmd : cmds)
      e2.process(cmd);
  }
}
