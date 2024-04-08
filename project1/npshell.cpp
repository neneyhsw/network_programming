#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "command_handler.h"

#define MAX_INPUT_VALUE 15000
#define MAX_CMD_LEN 256
using namespace std;

//
// Global variable
//
int same_pipe_ptr = 0;
pid_t pid;
int status = 0;

int SetEnv(vector<string> &cmd) {
  // printenv
  if (cmd[0] == "printenv") {
    if (cmd.size() == 2) {
      if (char *p = getenv(cmd[1].c_str())) {
        cout << p << endl;
      }
    }
    return EXIT_SUCCESS;
  } else if (cmd[0] == "setenv") {
    if (cmd.size() == 3) {
      setenv(cmd[1].c_str(), cmd[2].c_str(), 1);
    }
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

bool IsNumber(const string &str) {
  for (char const &c : str) {
    if (std::isdigit(c) == 0) {
      return false;
    }
  }
  return true;
}

void kill_child(int sig) {
  kill(pid, SIGKILL);
}

int main(int argc, char **argv) {
  string input;
  vector<string> initial_env{"setenv", "PATH", "bin:."};
  vector<int> npCount_v;
  vector<int> npPipeID_in, npPipeID_out;
  int EnvStatus = 0;
  EnvStatus = SetEnv(initial_env);

  while (true) {
    cout << "% ";
    getline(cin, input);
    vector<string> split_cmd = GetCmd(input);

    if (split_cmd.size() == 0) {
      continue;
    }
    if (split_cmd[0] == "exit") {
      break;
    }

    // if return EXIT_SUCCESS
    if (split_cmd[0] == "printenv" || split_cmd[0] == "setenv") {
      EnvStatus = SetEnv(split_cmd);
      if (!EnvStatus) {
        for (int i = 0; i < npCount_v.size(); i++) {
          npCount_v[i]--;
        }
      }
      continue;
    }

    // if no pipe
    if (input.find("|") == string::npos && input.find("!") == string::npos) {
      signal(SIGALRM,(void (*)(int))kill_child);
      
      pid = fork();
      if (pid < 0) {
        fprintf(stderr, "fork error.\n");
        exit(EXIT_FAILURE);
      } else if (pid == 0) {  // child

        // file redirection
        int argcount = 0;
        int fd = 0;
        vector<string>::iterator iter = std::find(split_cmd.begin(), split_cmd.end(), ">");
        if (iter != split_cmd.end()) {
          // find out argcount, avoid there are more than one parameters after ">" cmd
          for(int i = 0; i < split_cmd.size(); i++) {
            if (split_cmd[i] == ">")
              break;
            argcount++;
          }
          // direct the next position of the ">" mark (output file name)
          iter++;

          string temp_output = *iter;
          fd = open(temp_output.c_str(), O_CREAT|O_TRUNC|O_RDWR, 0644);
          dup2(fd, STDOUT_FILENO);
          close(fd);
        } else
          argcount = split_cmd.size();

        // check number pipe
        for (int i = 0; i < npCount_v.size(); i++) {
          if (npCount_v[i] == 0) {
            if (dup2(npPipeID_in[i], STDIN_FILENO) < 0) {
              fprintf(stderr, "np dup create error.\n");
              exit(EXIT_FAILURE);
            }

            // important !!!!!!
            close(npPipeID_in[i]);
            close(npPipeID_in[i]+1);
            break;
          }
        }

        char **args = (char **) malloc((argcount + 1) * sizeof(char *));
        for (int i = 0; i < argcount; i++) {
          args[i] = (char *) split_cmd[i].c_str();
        }
        args[argcount] = NULL;
        // char *cmd_argv[] = {strdup(split_cmd[0].c_str()), strdup(split_cmd[1].c_str()), NULL};  // parameters
        if (execvp(args[0], args) == -1) {
          fprintf(stderr, "Unknown command: [%s].\n", args[0]);
          exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
      }

      for (int i = 0; i < npCount_v.size(); i++) {
        if (npCount_v[i] == 0) {
          close(npPipeID_in[i]);
          close(npPipeID_in[i]+1);
          break;
        }
      }

      while (true) {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) != 0) {
          break;
        }
      }
    } else {  // pipe area
      int pipe_count = 0;
      int numberPipeCount = 0;
      char buffer[MAX_INPUT_VALUE] = {0};
      bool is_numberPipe = false;
      bool is_exclamation = false;  // "!" mark

      // check if the cmd is Number Pipe
      // if the penultimate cmd is "|" and the last cmd is a number, it is a Number Pipe
      if ((split_cmd[split_cmd.size()-2] == "|" || split_cmd[split_cmd.size()-2] == "!") && IsNumber(split_cmd[split_cmd.size()-1])) {
        numberPipeCount = stoi(split_cmd[split_cmd.size()-1]);
        is_numberPipe = true;
        if (split_cmd[split_cmd.size()-2] == "!") {
          is_exclamation = true;
        }
      }
      for (int i = 0; i < split_cmd.size(); i++) {
        if (split_cmd[i].find("|") != string::npos || split_cmd[i].find("!") != string::npos) {
          pipe_count++;
        }
      }

      vector<vector<string>> v_cmd(pipe_count+1);
      int cmd_count = 0;
      for (int i = 0; i < split_cmd.size(); i++) {
        if (split_cmd[i].find(">") != string::npos) {
          break;
        } else if (split_cmd[i].find("|") == string::npos && split_cmd[i].find("!") == string::npos) {
          v_cmd[cmd_count].push_back(split_cmd[i]);
        } else {
          cmd_count++;
        }
      }

      // one dimentional array
      // https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
      int pipefd[2*pipe_count];
      int fork_count = 0;
      if (!is_numberPipe)
        fork_count = pipe_count + 1;
      else
        fork_count = pipe_count;

      for (int i = 0; i < fork_count; i++) {
        if (is_numberPipe) {
          // it adds two memory space once loop, (0, 1), (2, 3)......
          if (pipe(pipefd + i*2) < 0) {
            fprintf(stderr, "pipe create error.\n");
            exit(EXIT_FAILURE);
          }
        }
        else {
          if (i < fork_count-1) {
            if (pipe(pipefd + i*2) < 0) {
              fprintf(stderr, "pipe create error.\n");
              exit(EXIT_FAILURE);
            }
          }
        }

        //signal(SIGCHLD, sig_handler);
        signal(SIGALRM,(void (*)(int))kill_child);

        pid = fork();
        if (pid < 0) {  // fork failed
          fprintf(stderr, "fork create error.\n");
          exit(EXIT_FAILURE);
        }
        else if (pid == 0) {  // child
          // STDIN_FILENO = 0, receive the value from previous pipe
          // STDOUT_FILENO = 1, send the value of current pipe
          if (i != 0) {  // if not first command
            if (dup2(pipefd[(i-1)*2], STDIN_FILENO) < 0) {  // stdin 0, 2, 4......
              fprintf(stderr, "in dup error.\n");
              exit(EXIT_FAILURE);
            }
          }
          if (i != fork_count-1) {  // if not last command
            if (dup2(pipefd[i*2+1], STDOUT_FILENO) < 0) {  // stdout 1, 3, 5......
              fprintf(stderr, "out dup error.\n");
              exit(EXIT_FAILURE);
            }
            if (is_exclamation) {
              if (dup2(pipefd[i*2+1], STDERR_FILENO) < 0) {
                fprintf(stderr, "out dup error.\n");
                exit(EXIT_FAILURE);
              }
            }
          }

          // the first fork need to check if there is number pipe that need to receive stdin
          if (i == 0) {
            for (int j = 0; j < npCount_v.size(); j++) {
              if (npCount_v[j] == 0) {
                if (dup2(npPipeID_in[j], STDIN_FILENO) < 0) {
                  fprintf(stderr, "np out dup error.\n");
                  exit(EXIT_FAILURE);
                }

                // important !!!!!!
                close(npPipeID_in[j]);
                close(npPipeID_in[j]+1);
                break;
              }
            }
          }

          same_pipe_ptr = 0;
          // store number pipe
          if (i == fork_count-1 && is_numberPipe) {
            // search npCount_v value
            for (int j = 0; j < npCount_v.size(); j++) {
              if (same_pipe_ptr != 0) {
                break;
              }
              if (npCount_v[j] == numberPipeCount) {
                same_pipe_ptr = 1;
                if (dup2(npPipeID_in[j]+1, STDOUT_FILENO) < 0) {
                  fprintf(stderr, "out dup error.\n");
                  exit(EXIT_FAILURE);
                }
                if (is_exclamation) {
                  if (dup2(npPipeID_in[j]+1, STDERR_FILENO) < 0) {
                    fprintf(stderr, "out dup error.\n");
                    exit(EXIT_FAILURE);
                  }
                }
              }
            }
            if (same_pipe_ptr == 0) {
              if (dup2(pipefd[i*2+1], STDOUT_FILENO) < 0) {
                fprintf(stderr, "out dup error.\n");
                exit(EXIT_FAILURE);
              }
              if (is_exclamation) {
                if (dup2(pipefd[i*2+1], STDERR_FILENO) < 0) {
                  fprintf(stderr, "out dup error.\n");
                  exit(EXIT_FAILURE);
                }
              }
            }
          }

          for (int j = 0; j < 2*pipe_count; j++)
            close(pipefd[j]);

          // file redirection
          if (i == fork_count-1) {
            int argcount = 0;
            int fd = 0;
            vector<string>::iterator iter = std::find(split_cmd.begin(), split_cmd.end(), ">");
            if (iter != split_cmd.end()) {
              // find out argcount, avoid there are more than one parameters after ">" cmd
              for(int i = 0; i < split_cmd.size(); i++) {
                if (split_cmd[i] == ">")
                  break;
                argcount++;
              }
              iter++;  // direct the next position of the ">" mark (output file name)
              string temp_output = *iter;
              fd = open(temp_output.c_str(), O_CREAT|O_TRUNC|O_RDWR, 0644);
              dup2(fd, STDOUT_FILENO);
              close(fd);
            }
            else
              argcount = split_cmd.size();
          }

          // exec
          char **args = (char **) malloc((v_cmd[i].size()+1) * sizeof(char *));
          for (int j = 0; j < v_cmd[i].size(); j++) {
            args[j] = (char *) v_cmd[i][j].c_str();
          }
          args[v_cmd[i].size()] = NULL;

          if (execvp(args[0], args) == -1) {
            fprintf(stderr, "Unknown command: [%s].\n", args[0]);
            exit(EXIT_FAILURE);
          }
          exit(EXIT_FAILURE);
        }
      }

      for (int i = 0; i < npCount_v.size(); i++) {
        if (npCount_v[i] == 0) {
          close(npPipeID_in[i]);
          close(npPipeID_in[i]+1);
          break;
        }
      }

      // store number pipe FD
      if (is_numberPipe && same_pipe_ptr == 0) {
        npCount_v.push_back(numberPipeCount);
        npPipeID_in.push_back(pipefd[(fork_count-1)*2]);  // store STDIN
      }

      // parent finally close all pipe
      for (int i = 0; i < 2*pipe_count; i++) {
        if (is_numberPipe && i >= 2*pipe_count-2) {
          continue;
        }
        close(pipefd[i]);
      }

      // wait last child process, if status != 0 is normal exit
      // https://www.twblogs.net/a/5b83317c2b717766a1eb53be
      while (true) {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) != 0)  // normal exit
          break;
      }
    }

    for (int i = 0; i < npCount_v.size(); i++) {
      if (npCount_v[i] == 0) {
        close(npPipeID_in[i]);
        close(npPipeID_in[i]+1);
      }
      npCount_v[i]--;
    }
  }

  return EXIT_SUCCESS;
}
