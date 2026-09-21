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
  const std::string POSITION = "position";
  const std::string STARTPOS = "startpos";
  const std::string GO = "go";
  const std::string BESTMOVE = "bestmove";

  int fromFD;
  int toFD;
  std::string lastMsg;
  std::string verifyOutput(std::string expected, std::string got, std::string next);
  std::string fuzzyVerify(std::string expectedPrefix, std::string got, std::string next);
  std::string getNextMessage(std::string engineOutput);
public:
  std::string moves;
  Engine(std::string engName, int _fromFD, int _toFD, int inFD, int outFD);
  void sendTo(std::string str);
  std::vector<std::string> recvFrom();
  void process(std::string cmd);
};

std::string Engine::verifyOutput(std::string expected, std::string got, std::string next) {
  if (expected == got) {
    return next;
  }
  else {
    std::cerr << "Error! We expected " << expected << ", got " << got << " instead!\n";
    return "err";
  }
}

std::string Engine::fuzzyVerify(std::string expectedPrefix, std::string got, std::string next) {
  int prefixLength = expectedPrefix.length();
  // probably unecessary check
  if (expectedPrefix.empty() || prefixLength <= 0) {
    std::cerr << "Error! Empty prefix commands not accepted\n";
    return "err";
  }
  std::string gotPrefix = got.substr(0, prefixLength - 1);
  if (expectedPrefix == gotPrefix) {
    return next;
  }
  else {
    std::cerr << "Error! We expected a string starting with " << expectedPrefix << " got " << got << " instead!\n";
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
  if (engineOutput.length() >= 4 && (engineOutput.substr(0, 4) == "info" || engineOutput.substr(0, 1) == "id")) {
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
      return verifyOutput(READYOK, engineOutput, UCINEWGAME);
    }
    else if (lastMsg == UCINEWGAME) {
      return verifyOutput(READYOK, engineOutput, POSITION);
    }
    else if (lastMsg.substr(0, 1) == GO) {
      return fuzzyVerify(BESTMOVE, engineOutput, GO);
    }
    else return "err";
  }
}

/**
 * when we get something from the engine, do the next thing
 * NOTES:
  needs to handle <position startpos> vs <position e2e4 d7d5>
  if sending ucinew game
  also send isready and wait for readyok
  also, i don't think the engine outputs anything after the position command
  probably send another readyok? 
 */
void Engine::process(std::string engineOutput) {
  // first, if the engine has returned best move, update moves
  int bestMoveLength = BESTMOVE.length();
  if (engineOutput.length() >= bestMoveLength && engineOutput.substr(0, bestMoveLength - 1) == BESTMOVE) {
    // command will be of the form <bestmove e2e4>.
    // make sure command is of appropriate length: should be 5 longer than bestmove
    if (engineOutput.length() != bestMoveLength + 5) {
      std::cerr << "Malformed bestmove response [" << engineOutput << "] will be skipped\n";
    }
    else {
      std::string move = engineOutput.substr(bestMoveLength, engineOutput.length() - 1);
      if (this->moves == "") this->moves = move;
      else this->moves += (" " + move);
    }
  }
  std::string next = getNextMessage(engineOutput);
  if (next == "err") {
    return;
  }
  if (next == "") {
    std::cerr << "getNextMessage() returned empty String???\n";
    return;
  }
  if (next == ISREADY) {
    // nothing fancy to do here
    sendTo(ISREADY);
  }
  else if (next == UCINEWGAME) {
    // need to send a UCINEWGAME command
    // and also wait for the engine to be ready again
    sendTo(UCINEWGAME);
    sendTo(ISREADY);
  }
  else if (next == POSITION) {
    std::string msg = POSITION + " " + STARTPOS;
    if (this->moves != "") {
      msg += " ";
      msg += moves;
    }
    sendTo(msg);
  }
  else if (next == GO) {
    // hmmmmmmmmmm
    // TODO
    // moret hought is needed here
    // for now just send go ig?
    sendTo(GO);
  }
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
