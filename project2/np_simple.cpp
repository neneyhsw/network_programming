#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_INPUT_VALUE 15000
#define MAX_CMD_LEN 256

/* SOCK HEADER */
#include <arpa/inet.h> //close
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> //strlen
#include <sys/socket.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/types.h>
#include <unistd.h> //close
#define PORT 35353

using namespace std;

void removeVectorSpaces(vector<string> &v, string delimiter)
{
  vector<string>::iterator iter;
  for (iter = v.begin(); iter != v.end();)
  {
    if (*iter == delimiter)
      v.erase(iter);
    else
      iter++;
  }
}

// for string delimiter
vector<string> splitString(string s, string delimiter)
{
  size_t pos_start = 0, pos_end, delim_len = delimiter.length();
  string token;
  vector<string> res;

  while ((pos_end = s.find(delimiter, pos_start)) != string::npos)
  {
    token = s.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.push_back(token);
    res.push_back(delimiter); // yhsw <++
  }

  res.push_back(s.substr(pos_start));
  return res;
}

int setEnv(vector<string> &cmd)
{
  // printenv
  if (cmd[0] == "printenv")
  {
    if (cmd.size() == 2)
      if (char *p = getenv(cmd[1].c_str()))
        cout << p << endl;
    return EXIT_SUCCESS;
  }

  // setenv
  if (cmd[0] == "setenv")
  {
    if (cmd.size() == 3)
      setenv(cmd[1].c_str(), cmd[2].c_str(), 1);
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

bool isNumber(const string &str)
{
  for (char const &c : str)
  {
    if (std::isdigit(c) == 0)
      return false;
  }
  return true;
}

/* replace '\n' and '\r' */
bool replace(string &str, const string &from, const string &to)
{
  size_t start_pos = str.find(from);
  if (start_pos == string::npos)
    return false;
  str.replace(start_pos, from.length(), to);
  return true;
}

static int sig_handler(int sig)
{
  int retval;
  retval = wait(NULL);
  return retval;
}

int same_pipe_ptr = 0;
pid_t pid;
int status = 0;

void kill_child(int sig)
{
  kill(pid, SIGKILL);
}

int main(int argc, char **argv)
{

  /* np server GLOBAL VAR*/
  string input;
  vector<string> initial_env{"setenv", "PATH", "bin:."};
  vector<int> npCount_v;
  vector<int> npPipeID_in, npPipeID_out;
  setEnv(initial_env);

  /* SOCK ACCEPT */
  int port = atoi(argv[1]);  // 使用者帶入port參數

  int opt = true;
  int master_socket, addrlen, new_socket, client_socket[30], max_clients = 30, activity, i, valread, sd;
  int max_sd;
  struct sockaddr_in address;

  //char recv_buffer[MAX_INPUT_VALUE];

  // set of socket descriptors
  fd_set readfds;

  // a message
  //char *message = "ECHO Daemon v1.0 \r\n";
  char *message = "% ";

  // initialise all client_socket[] to 0 so not checked
  for (i = 0; i < max_clients; i++)
  {
    client_socket[i] = 0;
  }

  // create a master socket
  if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
  {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // set master socket to allow multiple connections,
  // this is just a good habit, it will work without this
  if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
  {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  // type of socket created
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  // bind the socket to localhost port 8888
  if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
  {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  printf("Listener on port %d \n", port);

  // try to specify maximum of 3 pending connections for the master socket
  if (listen(master_socket, 3) < 0)
  {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  // accept the incoming connection
  addrlen = sizeof(address);
  puts("Waiting for connections ...");

  while (true)
  {
    //clear the socket set
    FD_ZERO(&readfds);

    //add master socket to set
    FD_SET(master_socket, &readfds);
    max_sd = master_socket;

    //add child sockets to set
    for (i = 0; i < max_clients; i++)
    {
      //socket descriptor
      sd = client_socket[i];

      //if valid socket descriptor then add to read list
      if (sd > 0)
        FD_SET(sd, &readfds);

      //highest file descriptor number, need it for the select function
      if (sd > max_sd)
        max_sd = sd;
    }

    //wait for an activity on one of the sockets , timeout is NULL ,
    //so wait indefinitely
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

    if ((activity < 0) && (errno != EINTR))
    {
      printf("select error");
    }

    //If something happened on the master socket ,
    //then its an incoming connection
    if (FD_ISSET(master_socket, &readfds))
    {

      if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
      {
        perror("accept");
        exit(EXIT_FAILURE);
      }

      //inform user of socket number - used in send and receive commands
      printf("New connection, socket fd is %d , ip is: %s , port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

      pid_t new_pid;
      new_pid = fork();

      if (new_pid < 0)
      {
        fprintf(stderr, "fork error\n");
        return EXIT_FAILURE;
      }
      else if (new_pid == 0)
      {
        //close(master_socket);
        dup2(new_socket, STDOUT_FILENO);
        dup2(new_socket, STDERR_FILENO);

        while (true)
        {
          char recv_buffer[MAX_INPUT_VALUE] = {0};

          //send new connection greeting message
          if (send(new_socket, message, strlen(message), 0) != strlen(message))
          {
            perror("send");
          }

          //puts("Welcome message sent successfully");

          recv(new_socket, recv_buffer, sizeof(recv_buffer), 0);
          //cout << "get: " << recv_buffer << "\n";

          /* npshell AREA */
          //string input;
          //vector<string> initial_env{"setenv", "PATH", "bin:."};
          //vector<int> npCount_v;
          //vector<int> npPipeID_in, npPipeID_out;
          //setEnv(initial_env);

          //cout << "% ";
          //while (getline(cin, input))
          //{

          //char *tmp = NULL;
          //if ((tmp = strstr(recv_buffer, "\n")))
          //  *tmp = '\0';

          string recv_str(recv_buffer);
          replace(recv_str, "\n", "");
          replace(recv_str, "\r", "");

          //recv_str.erase(recv_str.find('\n'));
          //cout << "get: " << recv_str << "\n";

          if (recv_str == "")
          {
            continue;
          }

          vector<string> temp_split_cmd, space_split_cmd, split_cmd;
          temp_split_cmd = splitString(recv_str, "|");
          for (int i = 0; i < temp_split_cmd.size(); i++)
          {
            space_split_cmd = splitString(temp_split_cmd[i], " ");
            for (int j = 0; j < space_split_cmd.size(); j++)
              split_cmd.push_back(space_split_cmd[j]);
          }
          removeVectorSpaces(split_cmd, "");  // remove multiple spaces between two parameters
          removeVectorSpaces(split_cmd, " "); // remove space in cmd that generate by first split "|"

          // 檢查split_cmd最後一格是不是 "!"
          vector<string> temp_exclamation;
          if (split_cmd[split_cmd.size() - 1].find("!") != string::npos)
          {
            temp_exclamation = splitString(split_cmd[split_cmd.size() - 1], "!");
            removeVectorSpaces(temp_exclamation, ""); // 處理多餘的vector
            split_cmd.erase(split_cmd.end());         // 去除最後一個vector (包含"!")
            split_cmd.push_back(temp_exclamation[0]);
            split_cmd.push_back(temp_exclamation[1]);
          }

          // escape bash
          if (split_cmd[0] == "exit") {
          //if (input.find("exit") != string::npos)
            return EXIT_SUCCESS;
            break;
          }

          // if return EXIT_SUCCESS
          if (!setEnv(split_cmd))
          {
            for (int i = 0; i < npCount_v.size(); i++)
            {
              npCount_v[i]--;
            }
            //cout << "% ";
            continue;
          }

          // if no pipe
          if (recv_str.find("|") == string::npos && recv_str.find("!") == string::npos)
          {

            /* SOCK AREA */
            //int fd[2] = {0};
            //pipe(fd);

            //signal(SIGCHLD, sig_handler);
            signal(SIGALRM, (void (*)(int))kill_child);
            // fork cmd
            pid = fork();
            if (pid < 0)
            {
              fprintf(stderr, "fork error.\n");
              exit(EXIT_FAILURE);
            }
            else if (pid == 0)
            { // child

              /* SOCK AREA */
              //dup2(fd[1], STDOUT_FILENO);
              //close(fd[1]);

              // file redirection
              int argcount = 0;
              int fd = 0;
              vector<string>::iterator iter = std::find(split_cmd.begin(), split_cmd.end(), ">");
              if (iter != split_cmd.end())
              {
                // 找出argcount, 避免>之後的參數不只一個
                for (int i = 0; i < split_cmd.size(); i++)
                {
                  if (split_cmd[i] == ">")
                    break;
                  argcount++;
                }
                iter++; // 指向>符號的下個位置(為輸出檔案名稱)
                string temp_output = *iter;
                fd = open(temp_output.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
                dup2(fd, STDOUT_FILENO);
                close(fd);
              }
              else
                argcount = split_cmd.size();

              // check number pipe
              for (int i = 0; i < npCount_v.size(); i++)
              {
                if (npCount_v[i] == 0)
                {
                  if (dup2(npPipeID_in[i], STDIN_FILENO) < 0)
                  {
                    fprintf(stderr, "np dup create error.\n");
                    exit(EXIT_FAILURE);
                  }

                  //  重要!!!!!!
                  close(npPipeID_in[i]);
                  close(npPipeID_in[i] + 1);
                  break;
                }
              }

              char **args = (char **)malloc((argcount + 1) * sizeof(char *));
              for (int i = 0; i < argcount; i++)
                args[i] = (char *)split_cmd[i].c_str();
              args[argcount] = NULL;
              // char *cmd_argv[] = {strdup(split_cmd[0].c_str()), strdup(split_cmd[1].c_str()), NULL};  // parameters

              //dup2(new_socket, STDOUT_FILENO);
              //dup2(new_socket, STDERR_FILENO);
              if (execvp(args[0], args) == -1)
              {
                fprintf(stderr, "Unknown command: [%s].\n", args[0]);
                exit(EXIT_FAILURE);
              }
              exit(EXIT_FAILURE);
            }

            /* SOCK SEND */
            //else {
            //  char send_buffer[1000000];
            //  ssize_t size = read(fd[0], send_buffer, 1000000);
            //  if ( (size > 0) && (size < sizeof(send_buffer))) {
            //    send_buffer[size] = '\0';
            //    send(new_socket, send_buffer, strlen(send_buffer), 0);
            //  }
            //}

            for (int i = 0; i < npCount_v.size(); i++)
            {
              if (npCount_v[i] == 0)
              {
                close(npPipeID_in[i]);
                close(npPipeID_in[i] + 1);
                break;
              }
            }

            while (true)
            {
              waitpid(pid, &status, 0);
              if (WIFEXITED(status) != 0)
                break;
            }
          }
          else
          { // pipe area
            int pipe_count = 0;
            int numberPipeCount = 0;
            char buffer[MAX_INPUT_VALUE] = {0};
            bool is_numberPipe = false;
            bool is_exclamation = false; // 驚嘆號

            // whether is numberPipe
            // 如果倒數第二個vector為 | , 且最後一個vector為數字, 表示為numberPipe
            if ((split_cmd[split_cmd.size() - 2] == "|" || split_cmd[split_cmd.size() - 2] == "!") && isNumber(split_cmd[split_cmd.size() - 1]))
            {
              numberPipeCount = stoi(split_cmd[split_cmd.size() - 1]);
              is_numberPipe = true;
              if (split_cmd[split_cmd.size() - 2] == "!")
                is_exclamation = true;
            }
            for (int i = 0; i < split_cmd.size(); i++)
              if (split_cmd[i].find("|") != string::npos || split_cmd[i].find("!") != string::npos)
                pipe_count++;

            vector<vector<string>> v_cmd(pipe_count + 1);
            int cmd_count = 0;
            for (int i = 0; i < split_cmd.size(); i++)
            {
              if (split_cmd[i].find(">") != string::npos)
                break;
              if (split_cmd[i].find("|") == string::npos && split_cmd[i].find("!") == string::npos)
                v_cmd[cmd_count].push_back(split_cmd[i]);
              else
                cmd_count++;
            }

            // one dimentional array
            // https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
            int pipefd[2 * pipe_count];

            /* SOCK AREA */
            //int fd_s[2] = {0};
            //pipe(fd_s);

            int fork_count = 0;
            if (!is_numberPipe)
              fork_count = pipe_count + 1;
            else
              fork_count = pipe_count;

            for (int i = 0; i < fork_count; i++)
            {
              if (is_numberPipe)
              {
                if (pipe(pipefd + i * 2) < 0)
                { // 一次迴圈加兩個記憶體位置, (0, 1), (2, 3)......
                  fprintf(stderr, "pipe create error.\n");
                  exit(EXIT_FAILURE);
                }
              }
              else
              {
                if (i < fork_count - 1)
                {
                  if (pipe(pipefd + i * 2) < 0)
                  {
                    fprintf(stderr, "pipe create error.\n");
                    exit(EXIT_FAILURE);
                  }
                }
              }

              //signal(SIGCHLD, sig_handler);
              signal(SIGALRM, (void (*)(int))kill_child);

              pid = fork();
              if (pid < 0)
              { // fork failed
                fprintf(stderr, "fork create error.\n");
                exit(EXIT_FAILURE);
              }
              else if (pid == 0)
              { // child
                // STDIN_FILENO = 0, 接收前一個pipe的value
                // STDOUT_FILENO = 1, 傳出當前pipe的value
                if (i != 0)
                { // if not first command
                  if (dup2(pipefd[(i - 1) * 2], STDIN_FILENO) < 0)
                  { // stdin 0, 2, 4......
                    fprintf(stderr, "in dup error.\n");
                    exit(EXIT_FAILURE);
                  }
                }
                if (i != fork_count - 1)
                { // if not last command
                  if (dup2(pipefd[i * 2 + 1], STDOUT_FILENO) < 0)
                  { // stdout 1, 3, 5......
                    fprintf(stderr, "out dup error.\n");
                    exit(EXIT_FAILURE);
                  }
                  if (is_exclamation)
                  {
                    if (dup2(pipefd[i * 2 + 1], STDERR_FILENO) < 0)
                    {
                      fprintf(stderr, "out dup error.\n");
                      exit(EXIT_FAILURE);
                    }
                  }
                }

                /* SOCK AREA */
                //if (i == (fork_count - 1)) {
                //  dup2(fd_s[1], STDOUT_FILENO);
                //  close(fd_s[1]);
                //}
                //else {
                //  close(fd_s[1]);
                //  close(fd_s[0]);
                //}

                // 第一個fork先檢查有沒有number pipe需要stdin
                if (i == 0)
                {
                  for (int j = 0; j < npCount_v.size(); j++)
                  {
                    if (npCount_v[j] == 0)
                    {
                      if (dup2(npPipeID_in[j], STDIN_FILENO) < 0)
                      {
                        fprintf(stderr, "np out dup error.\n");
                        exit(EXIT_FAILURE);
                      }

                      // 重要!!!!!!
                      close(npPipeID_in[j]);
                      close(npPipeID_in[j] + 1);
                      break;
                    }
                  }
                }

                same_pipe_ptr = 0;
                // store number pipe
                if (i == fork_count - 1 && is_numberPipe)
                {
                  // search npCount_v value
                  for (int j = 0; j < npCount_v.size(); j++)
                  {
                    if (same_pipe_ptr != 0)
                    {
                      break;
                    }
                    if (npCount_v[j] == numberPipeCount)
                    {
                      same_pipe_ptr = 1;
                      if (dup2(npPipeID_in[j] + 1, STDOUT_FILENO) < 0)
                      {
                        fprintf(stderr, "out dup error.\n");
                        exit(EXIT_FAILURE);
                      }
                      if (is_exclamation)
                      {
                        if (dup2(npPipeID_in[j] + 1, STDERR_FILENO) < 0)
                        {
                          fprintf(stderr, "out dup error.\n");
                          exit(EXIT_FAILURE);
                        }
                      }
                    }
                  }
                  if (same_pipe_ptr == 0)
                  {
                    if (dup2(pipefd[i * 2 + 1], STDOUT_FILENO) < 0)
                    {
                      fprintf(stderr, "out dup error.\n");
                      exit(EXIT_FAILURE);
                    }
                    if (is_exclamation)
                    {
                      if (dup2(pipefd[i * 2 + 1], STDERR_FILENO) < 0)
                      {
                        fprintf(stderr, "out dup error.\n");
                        exit(EXIT_FAILURE);
                      }
                    }
                  }
                }

                //if (i == fork_count - 1 && !is_numberPipe) {
                //  dup2(new_socket, STDOUT_FILENO);
                //  close(new_socket);
                //}

                for (int j = 0; j < 2 * pipe_count; j++)
                  close(pipefd[j]);
                //if (j != (pipe_count*2 - 2))
                //  close(pipefd[j]);

                // file redirection
                if (i == fork_count - 1)
                {
                  int argcount = 0;
                  int fd = 0;
                  vector<string>::iterator iter = std::find(split_cmd.begin(), split_cmd.end(), ">");
                  if (iter != split_cmd.end())
                  {
                    // 找出argcount, 避免>之後的參數不只一個
                    for (int i = 0; i < split_cmd.size(); i++)
                    {
                      if (split_cmd[i] == ">")
                        break;
                      argcount++;
                    }
                    iter++; // 指向>符號的下個位置(為輸出檔案名稱)
                    string temp_output = *iter;
                    fd = open(temp_output.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                  }
                  else
                    argcount = split_cmd.size();
                }

                // exec
                char **args = (char **)malloc((v_cmd[i].size() + 1) * sizeof(char *));
                for (int j = 0; j < v_cmd[i].size(); j++)
                {
                  args[j] = (char *)v_cmd[i][j].c_str();
                }
                args[v_cmd[i].size()] = NULL;

                //dup2(new_socket, STDOUT_FILENO);
                //dup2(new_socket, STDERR_FILENO);
                if (execvp(args[0], args) == -1)
                {
                  fprintf(stderr, "Unknown command: [%s].\n", args[0]);
                  exit(EXIT_FAILURE);
                }
                exit(EXIT_FAILURE);
              }
            }

            /* SOCK SEND */
            //char send_buffer[1000000];
            //ssize_t size = read(pipefd[(fork_count-1)*2 - 1], send_buffer, 1000000);
            //ssize_t size = read(fd_s[0], send_buffer, 1000000);
            //if ( (size > 0) && (size < sizeof(send_buffer))) {
            //  send_buffer[size] = '\0';
            //  send(new_socket, send_buffer, strlen(send_buffer), 0);
            //}
            //close(fd_s[0]);
            //close(fd_s[1]);

            for (int i = 0; i < npCount_v.size(); i++)
            {
              if (npCount_v[i] == 0)
              {
                close(npPipeID_in[i]);
                close(npPipeID_in[i] + 1);
                break;
              }
            }

            // store number pipe FD
            if (is_numberPipe && same_pipe_ptr == 0)
            {
              npCount_v.push_back(numberPipeCount);
              npPipeID_in.push_back(pipefd[(fork_count - 1) * 2]); // store STDIN
            }

            // parent finally close all pipe
            for (int i = 0; i < 2 * pipe_count; i++)
            {
              if (is_numberPipe && i >= 2 * pipe_count - 2)
              {
                continue;
              }
              close(pipefd[i]);
            }

            // wait last child process, if status != 0 is normal exit
            // https://www.twblogs.net/a/5b83317c2b717766a1eb53be
            while (true)
            {
              waitpid(pid, &status, 0);
              if (WIFEXITED(status) != 0) // normal exit
                break;
            }
          }

          for (int i = 0; i < npCount_v.size(); i++)
          {
            if (npCount_v[i] == 0)
            {
              close(npPipeID_in[i]);
              close(npPipeID_in[i] + 1);
            }
            npCount_v[i]--;
          }
          //cout << "% ";
        }
        //close(new_socket);

      } // fork
      //else
      //{
      //  close(new_socket);

     //     while (true)
     //     {
         //waitpid(pid, &status, WNOHANG);
     //      if (WIFEXITED(status) != 0) // normal exit
     //          break;
     //       }

      //}
    }
  }

  return EXIT_SUCCESS;
}
