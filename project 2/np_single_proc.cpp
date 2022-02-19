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

/* CPP */
#include <bits/stdc++.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* DEFINE */
#define MAX_INPUT_VALUE 30000
#define USER_MAXIMUM 200
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

int setEnv(vector<string> &cmd, vector<string> &cEnv, int cIndex)
{
  // printenv
  if (cmd[0] == "printenv")
  {
    if (cmd.size() == 2)
      if (char *p = getenv(cmd[1].c_str()))
        cout << p << endl;
        //cout << cEnv[cIndex] << endl;
    return EXIT_SUCCESS;
  }

  // setenv
  if (cmd[0] == "setenv")
  {
    if (cmd.size() == 3)
      setenv(cmd[1].c_str(), cmd[2].c_str(), 1);
    if (cmd[1] == "PATH")
      cEnv[cIndex] = cmd[2];
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

void updateEnv(vector<string> &cEnv, int cIndex) {
  string P = "PATH";
  setenv(P.c_str(), cEnv[cIndex].c_str(), 1);
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
int UserPipe_Count = 0;

void kill_child(int sig)
{
  kill(pid, SIGKILL);
}

int* store_std(){
    int* arr= new int[3];
    arr[0] = dup(STDIN_FILENO);
    arr[1] = dup(STDOUT_FILENO);
    arr[2] = dup(STDERR_FILENO);
    return arr;
}

void restore_std(int* arr){
    dup2(arr[0], STDIN_FILENO);
    dup2(arr[1], STDOUT_FILENO);
    dup2(arr[2], STDERR_FILENO);
    close(arr[0]);
    close(arr[1]);
    close(arr[2]);
    delete[] arr;
}

int main(int argc, char *argv[])
{

  /* np server GLOBAL VAR*/
  string input;
  vector<string> initial_env{"setenv", "PATH", "bin:."};
  //setEnv(initial_env);
  //setenv(initial_env[1].c_str(), initial_env[2].c_str(), 1);
  vector<vector<int>> npPipeID_in(USER_MAXIMUM+1);
  vector<vector<int>> npCount_v(USER_MAXIMUM+1);
  vector<string> user_env(USER_MAXIMUM+1, "");
  vector<vector<int>> UserPipeClientSock_v(USER_MAXIMUM+1, vector<int>(USER_MAXIMUM+1));
  vector<vector<int>> UserPipeContent_v(USER_MAXIMUM+1, vector<int>(USER_MAXIMUM+1));
  for (int i = 0; i < USER_MAXIMUM+1; i++)
    for (int j = 0; j < USER_MAXIMUM+1; j++) {
      UserPipeContent_v[i][j] = -1;
      UserPipeClientSock_v[i][j] = -1;
    }
  int GLOBAL_COUNT = 0;
  vector<int> UserPipePB;

  /* SINGLE PROCESS CONCURRENT */
	int PORT = atoi(argv[1]);
	int opt = true;
	int master_socket, addrlen, new_socket, client_socket[USER_MAXIMUM], max_clients = USER_MAXIMUM;
  int activity, valread, sd, max_sd;
	struct sockaddr_in address;

	char buffer[MAX_INPUT_VALUE];

	//set of socket descriptors
	fd_set readfds;

	//a message
  char *percent_sign = (char *) "% ";

	//initialise all client_socket[] to 0 so not checked
	for (int i = 0; i < max_clients; i++)
	{
		client_socket[i] = 0;
	}

	//create a master socket
	if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	//set master socket to allow multiple connections ,
	//this is just a good habit, it will work without this
	if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
								 sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	//type of socket created
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	//bind the socket to localhost port 8888
	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	//printf("Listener on port %d \n", PORT);

	//try to specify maximum of 3 pending connections for the master socket
	if (listen(master_socket, 3) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	//accept the incoming connection
	addrlen = sizeof(address);
	//puts("Waiting for connections ...");

  /* CUSTOMIZE */
  string UserName = "(no name)";
  vector<string> newSock_and_UN_v(USER_MAXIMUM, "");  // index = new_socket, value = UserName
  vector<string> IP_and_PORT_v(USER_MAXIMUM, "");
  vector<int> USER_ID_v(USER_MAXIMUM, 0);  // index+1 = id, value = new_socket fd

	while (true)
	{
		//clear the socket set
		FD_ZERO(&readfds);

		//add master socket to set
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;

		//add child sockets to set
		for (int i = 0; i < max_clients; i++)
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
			fprintf(stderr, "select error.\n");
		}

		//If something happened on the master socket,
		//then its an incoming connection
		if (FD_ISSET(master_socket, &readfds))
		{
			if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
			{
				perror("accept error.");
				exit(EXIT_FAILURE);
			}

      //cout << new_socket << endl;

      /* CUSTOMIZE */
      for (int i = 0; i < newSock_and_UN_v.size(); i++) {
        if (i == new_socket) {
          newSock_and_UN_v[i] = UserName;
        }
      }

      for (int i = 1; i < USER_ID_v.size(); i++) {
        if (USER_ID_v[i] == 0) {
          USER_ID_v[i] = new_socket;
          break;
        }
      }

      string welcome_info = "****************************************\n** Welcome to the information server. **\n****************************************\n";
      char ip_arr[INET_ADDRSTRLEN];
      string ip_str = inet_ntop(AF_INET, &address.sin_addr, ip_arr, INET_ADDRSTRLEN);
      string port_str = to_string(ntohs(address.sin_port));
      string ip_and_port = ip_str + ":" + port_str;
      string login_info = "*** User '" + newSock_and_UN_v[new_socket] + "' entered from " + ip_and_port + ". ***\n";

      for (int i = 0; i < newSock_and_UN_v.size(); i++) {
        if (i == new_socket) {
          IP_and_PORT_v[i] = ip_and_port;
        }
      }

      // set client env
      user_env[new_socket] = "bin:.";

      // send welcome message
      if (send(new_socket, welcome_info.c_str(), welcome_info.length(), 0) != welcome_info.length())
        fprintf(stderr, "send error.\n");

      // send new user info to all users
      for (int i = 0; i < newSock_and_UN_v.size(); i++) {
        if (newSock_and_UN_v[i] != "") {  // 沒有使用者設置為空值
          if (send(i, login_info.c_str(), login_info.length(), 0) != login_info.length())
            fprintf(stderr, "send error.\n");
        }
      }

      if (send(new_socket, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
        fprintf(stderr, "send %% error.\n");
      }

			//add new socket to array of sockets
			for (int i = 0; i < max_clients; i++)
			{
				//if position is empty
				if (client_socket[i] == 0)
				{
					client_socket[i] = new_socket;
					break;
				}
			}
		}

		//else its some IO operation on some other socket
		for (int mc = 0; mc < max_clients; mc++)
		{
			sd = client_socket[mc];

			if (FD_ISSET(sd, &readfds))
			{
        char recv_buffer[MAX_INPUT_VALUE] = {0};
				//Check if it was for closing , and also read the
				//incoming message
				if ((valread = read(sd, recv_buffer, MAX_INPUT_VALUE-1)) == 0)
				{
					//Somebody disconnected , get his details and print
					getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
					//printf("Host disconnected, ip %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

					//Close the socket and mark as 0 in list for reuse
					close(sd);
					client_socket[mc] = 0;
				}

				//Echo back the message that came in
				else
				{
          int *std = store_std();  // 保存client的std
          
          dup2(client_socket[mc], STDOUT_FILENO);
          dup2(client_socket[mc], STDERR_FILENO);
          
          string recv_str(recv_buffer);
          replace(recv_str, "\n", "");
          replace(recv_str, "\r", "");

          if (recv_str == "") {
            if (send(client_socket[mc], percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
              fprintf(stderr, "send %% error.\n");
            }
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
            //Somebody disconnected , get his details and print
            getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
            //printf("Host disconnected, ip %s, port %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            //Close the socket and mark as 0 in list for reuse
            string logout_info = "*** User '" + newSock_and_UN_v[client_socket[mc]] + "' left. ***\n";
            newSock_and_UN_v[client_socket[mc]] = "";
            IP_and_PORT_v[client_socket[mc]] = "";
            int temp_id = 0;
            for (int i = 1; i < USER_ID_v.size(); i++) {
              if (USER_ID_v[i] == client_socket[mc]) {
                USER_ID_v[i] = 0;
                temp_id = i;
                break;
              }
            }

            for (int i = 0; i < UserPipeClientSock_v[temp_id].size(); i++)
              UserPipeClientSock_v[temp_id][i] = -1;

            for (int i = 0; i < UserPipeContent_v[sd].size(); i++) {
              if (UserPipeContent_v[sd][i] != -1)
                close(UserPipePB[UserPipeContent_v[sd][i]]);
              UserPipeContent_v[sd][i] = -1;
            }

            close(client_socket[mc]);
            client_socket[mc] = 0;


            for (int i = 0; i < newSock_and_UN_v.size(); i++) {
              if (newSock_and_UN_v[i] != "") {  // 沒有使用者設置為空值
                if (send(i, logout_info.c_str(), logout_info.length(), 0) != logout_info.length())
                  fprintf(stderr, "send error.\n");
              }
            }
            close(sd);
            FD_CLR(sd, &readfds);
            restore_std(std);
            continue;
          }

          // update PATH
          updateEnv(user_env, client_socket[mc]);

          // if return EXIT_SUCCESS
          if (!setEnv(split_cmd, user_env, client_socket[mc]))
          {
            for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
            {
              npCount_v[client_socket[mc]][i]--;
            }
            if (send(client_socket[mc], percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
              fprintf(stderr, "send %% error.\n");
            }
            restore_std(std);
            continue;
          }

          /* WHO */
          if (split_cmd[0] == "who") {
            string who_head = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
            string tag_me = "<-me";
            string user_info = "";
            if (send(sd, who_head.c_str(), who_head.length(), 0) != who_head.length()) {
              fprintf(stderr, "send %% error.\n");
            }

            for (int i = 0; i < newSock_and_UN_v.size(); i++) {
              if (newSock_and_UN_v[i] != "") {
                int user_id = 0;
                for (int j = 1; j < USER_ID_v.size(); j++) {
                  if (USER_ID_v[j] == i) {
                    user_id = j;
                    break;
                  }
                }
                if (i == sd) {  // 自己
                  user_info = to_string(user_id) + "\t" + newSock_and_UN_v[i] + "\t" + IP_and_PORT_v[i] + "\t" + tag_me + "\n";
                }
                else {
                  user_info = to_string(user_id) + "\t" + newSock_and_UN_v[i] + "\t" + IP_and_PORT_v[i] + "\n";
                }
                if (send(sd, user_info.c_str(), user_info.length(), 0) != user_info.length()) {
                  fprintf(stderr, "send %% error.\n");
                }
              }
            }

            for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
            {
              npCount_v[client_socket[mc]][i]--;
            }
            if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
              fprintf(stderr, "send %% error.\n");
            }
            restore_std(std);
            continue;
          }

          /* NAME */
          if (split_cmd[0] == "name") {
            string new_name_str = split_cmd[1];
            string name_info = "";
            bool same_name_flag = false;
            for (int i = 0; i < newSock_and_UN_v.size(); i++) {
              if (newSock_and_UN_v[i] == new_name_str) {
                same_name_flag = true;
                break;
              }
            }
            if (!same_name_flag) {
              newSock_and_UN_v[sd] = new_name_str;
              name_info = "*** User from " + IP_and_PORT_v[sd] + " is named '" + newSock_and_UN_v[sd] + "'. ***\n";
              for (int i = 0; i < newSock_and_UN_v.size(); i++) {
                if (newSock_and_UN_v[i] != "") {
                  if (send(i, name_info.c_str(), name_info.length(), 0) != name_info.length()) {
                    fprintf(stderr, "send %% error.\n");
                  }
                }
              }
            }
            else {
              name_info = "*** User '" + new_name_str + "' already exists. ***\n";
              if (send(sd, name_info.c_str(), name_info.length(), 0) != name_info.length()) {
                fprintf(stderr, "send %% error.\n");
              }
            }

            for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
            {
              npCount_v[client_socket[mc]][i]--;
            }
            if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
              fprintf(stderr, "send %% error.\n");
            }
            restore_std(std);
            continue;
          }

          /* TELL */
          if (split_cmd[0] == "tell") {
            string be_told_info = "";
            string tellerror_info = "";
            int be_told_sock = 0;

            // check id, USER_ID_v[i] + 1 is id (because index is id minus 1)
            if (USER_ID_v[stoi(split_cmd[1])] != 0) {
              be_told_sock = USER_ID_v[stoi(split_cmd[1])];
            }
            if (be_told_sock != 0) {
              be_told_info = "*** " + newSock_and_UN_v[sd] + " told you ***: ";
              for (int i = 0; i < split_cmd.size(); i++) {
                if (i > 1) {
                  be_told_info += split_cmd[i];
                }
                if (i > 1 && i != split_cmd.size()-1)
                  be_told_info += " ";
                if (i > 1 && i == split_cmd.size()-1) {
                  be_told_info += "\n";
                }
              }
              if (send(be_told_sock, be_told_info.c_str(), be_told_info.length(), 0) != be_told_info.length()) {
                fprintf(stderr, "send %% error.\n");
              }
            }
            else {
              tellerror_info = "*** Error: user #" + split_cmd[1] + " does not exist yet. ***\n";
              if (send(sd, tellerror_info.c_str(), tellerror_info.length(), 0) != tellerror_info.length()) {
                fprintf(stderr, "send %% error.\n");
              }
            }

            for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
            {
              npCount_v[client_socket[mc]][i]--;
            }
            if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
              fprintf(stderr, "send %% error.\n");
            }
            restore_std(std);
            continue;
          }

          /* YELL */
          if (split_cmd[0] == "yell") {
            string broadcast_info = "*** " + newSock_and_UN_v[sd] + " yelled ***: ";
            for (int i = 1; i < split_cmd.size(); i++) {
              broadcast_info += split_cmd[i];
              if (i != split_cmd.size()-1)
                broadcast_info += " ";
              if (i == split_cmd.size()-1)
                broadcast_info += "\n";
            }
            for (int i = 1; i < USER_ID_v.size(); i++) {
              if (USER_ID_v[i] != 0) {
                if (send(USER_ID_v[i], broadcast_info.c_str(), broadcast_info.length(), 0) != broadcast_info.length()) {
                  fprintf(stderr, "send %% error.\n");
                }
              }
            }

            for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
            {
              npCount_v[client_socket[mc]][i]--;
            }
            if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
              fprintf(stderr, "send %% error.\n");
            }
            restore_std(std);
            continue;
          }


          // check send and recv User Pipe
          bool is_sendPipe = false;
          bool is_recvPipe = false;
          int sendto_id = 0;
          int recvfrom_id = 0;
          int pfd[2] = {0};
          string send2UserPipe_info = "";
          string recvfromUserPipe_info = "";
          int my_uid = 0;
          int sendto_sock = 0;
          int recvfrom_sock = 0;
          int user_pipe_flag = 0;
          int recvfrom_user_pipe_flag = 0;

          // check send User Pipe
          int send_index = 0;
          if (split_cmd.size() > 1) {
            for (int i = 0; i < split_cmd.size(); i++) {
              if (split_cmd[i].size() > 1) {  // 避免 file redirection
                if (split_cmd[i].find(">") != string::npos) {
                  is_sendPipe = true;
                  send_index = i;
                }
              }
            }
            if (is_sendPipe && send_index != 0) {
              replace(split_cmd[send_index], ">", "");
              sendto_id = stoi(split_cmd[send_index]);
              vector<string>::iterator iter = find(split_cmd.begin(), split_cmd.end(), split_cmd[send_index]);
              split_cmd.erase(iter);

              if (pipe(pfd) < 0) {
                fprintf(stderr, "User Pipe Error.\n");
                exit(EXIT_FAILURE);
              }
              for (int i = 1; i < USER_ID_v.size(); i++) {
                if (USER_ID_v[i] == sd)
                  my_uid = i;
                if (i == sendto_id)
                  sendto_sock = USER_ID_v[i];
              }
 
              // assign info
              if (sendto_sock == 0) {
                send2UserPipe_info = "*** Error: user #" + to_string(sendto_id) + " does not exist yet. ***\n";
                user_pipe_flag = 1;
              }
              else if (UserPipeClientSock_v[sendto_id][my_uid] != -1 && sendto_sock != 0) {
                send2UserPipe_info = "*** Error: the pipe #" + to_string(my_uid) + "->#" + to_string(sendto_id) + " already exists. ***\n";
                user_pipe_flag = 2;
              }
              else {
                send2UserPipe_info = "*** " + newSock_and_UN_v[sd] + " (#" + to_string(my_uid) + ") just piped '" + recv_str + "' to " + newSock_and_UN_v[USER_ID_v[sendto_id]] + " (#" + to_string(sendto_id) + ") ***\n";
                user_pipe_flag = 3;
                UserPipeClientSock_v[sendto_id][my_uid] = sd;
              }
              signal(SIGCHLD, SIG_IGN);
            }
          }



          // recv Pipe info
          int recv_index = 0;
          if (split_cmd.size() > 1) {
            for (int i = 0; i < split_cmd.size(); i++) {
              if (split_cmd[i].find("<") != string::npos) {
                is_recvPipe = true;
                recv_index = i;
              }
            }
            if (is_recvPipe && recv_index != 0) {
              replace(split_cmd[recv_index], "<", "");
              recvfrom_id = stoi(split_cmd[recv_index]);
              vector<string>::iterator iter = find(split_cmd.begin(), split_cmd.end(), split_cmd[recv_index]);
              split_cmd.erase(iter);
              for (int i = 1; i < USER_ID_v.size(); i++) {
                if (USER_ID_v[i] == sd)
                  my_uid = i;
                if (i == recvfrom_id)
                  recvfrom_sock = USER_ID_v[i];
              }

              // assign info
              if (recvfrom_sock == 0) {
                recvfromUserPipe_info = "*** Error: user #" + to_string(recvfrom_id) + " does not exist yet. ***\n";
                recvfrom_user_pipe_flag = 1;
              }
              else if (UserPipeClientSock_v[my_uid][recvfrom_id] == -1 && recvfrom_sock != 0) {
                recvfromUserPipe_info = "*** Error: the pipe #" + to_string(recvfrom_id) + "->#" + to_string(my_uid) + " does not exist yet. ***\n";
                recvfrom_user_pipe_flag = 2;
              }
              else {
                recvfromUserPipe_info = "*** " + newSock_and_UN_v[sd] + " (#" + to_string(my_uid) + ") just received from " + newSock_and_UN_v[recvfrom_sock] + " (#" + to_string(recvfrom_id) + ") by '" + recv_str + "' ***\n";
                recvfrom_user_pipe_flag = 3;
              }
              signal(SIGCHLD, SIG_IGN);
            }
          }


          // if no pipe
          if (recv_str.find("|") == string::npos && recv_str.find("!") == string::npos)
          {
            //signal(SIGCHLD, sig_handler);
            //signal(SIGALRM, (void (*)(int))kill_child);
            // fork cmd
            pid = fork();
            if (pid < 0)
            {
              fprintf(stderr, "fork error.\n");
              exit(EXIT_FAILURE);
            }
            else if (pid == 0)
            { // child
              
              // check recv User Pipe
              if (is_recvPipe && recvfrom_user_pipe_flag != 0) {
                if (recvfrom_user_pipe_flag == 1) {  // 沒有該user
                  if (send(sd, recvfromUserPipe_info.c_str(), recvfromUserPipe_info.length(), 0) != recvfromUserPipe_info.length()) {
                    fprintf(stderr, "recv UserPipe error.\n");
                  }
                  //if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
                  //  fprintf(stderr, "send %% error.\n");
                  //}
                  //continue;
                }
                else if (recvfrom_user_pipe_flag == 2) {  // 該pipe已經存在
                  if (send(sd, recvfromUserPipe_info.c_str(), recvfromUserPipe_info.length(), 0) != recvfromUserPipe_info.length()) {
                    fprintf(stderr, "recv UserPipe error.\n");
                  }
                  //if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
                  //  fprintf(stderr, "send %% error.\n");
                  //}
                  //continue;
                }
                else {  // broadcast
                  for (int i = 0; i < USER_ID_v.size(); i++) {
                    if (USER_ID_v[i] != 0) {
                      if (send(USER_ID_v[i], recvfromUserPipe_info.c_str(), recvfromUserPipe_info.length(), 0) != recvfromUserPipe_info.length()) {
                        fprintf(stderr, "recv UserPipe error.\n");
                      }
                    }
                  }
                  if (dup2(UserPipePB[UserPipeContent_v[my_uid][recvfrom_id]], STDIN_FILENO) < 0) {
                    fprintf(stderr, "User Pipe Content dup Error.\n");
                    exit(EXIT_FAILURE);
                  }
                }
              }

              if (is_sendPipe) {                
                // check send User Pipe
                if (user_pipe_flag != 0) {
                  if (user_pipe_flag == 1) {  // 沒有該user
                    if (send(sd, send2UserPipe_info.c_str(), send2UserPipe_info.length(), 0) != send2UserPipe_info.length()) {
                      fprintf(stderr, "send UserPipe error.\n");
                    }
                    //if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
                    //  fprintf(stderr, "send %% error.\n");
                    //}
                    //continue;
                  }
                  else if (user_pipe_flag == 2) {  // 該pipe已經存在
                    if (send(sd, send2UserPipe_info.c_str(), send2UserPipe_info.length(), 0) != send2UserPipe_info.length()) {
                      fprintf(stderr, "send UserPipe error.\n");
                    }
                    //if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
                    //  fprintf(stderr, "send %% error.\n");
                    //}
                    //continue;
                  }
                  else {  // broadcast
                    for (int i = 0; i < USER_ID_v.size(); i++) {
                      if (USER_ID_v[i] != 0) {
                        if (send(USER_ID_v[i], send2UserPipe_info.c_str(), send2UserPipe_info.length(), 0) != send2UserPipe_info.length()) {
                          fprintf(stderr, "send UserPipe error.\n");
                        }
                      }
                    }
                    if (dup2(pfd[1], STDOUT_FILENO) < 0) {
                      fprintf(stderr, "User Pipe dup Error.\n");
                      exit(EXIT_FAILURE);
                    }
                  }
                }  // User Pipe
              }

              if (is_sendPipe && is_recvPipe) {
                close(pfd[0]);
                close(pfd[1]);
              }

              if (is_recvPipe && recvfrom_user_pipe_flag == 3) {
                UserPipeContent_v[my_uid][recvfrom_id] = -1;
              }

              removeVectorSpaces(split_cmd, "");
              int devNull = 0;
              if (is_sendPipe && (user_pipe_flag == 1 || user_pipe_flag == 2)) {
                devNull = open("/dev/null", O_WRONLY);
                dup2(devNull, STDIN_FILENO);
                dup2(devNull, STDOUT_FILENO);
                if (split_cmd.size() <= 1 && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                  dup2(devNull, STDERR_FILENO);
                }
              }

              /*
              if (is_sendPipe && (user_pipe_flag == 1 || user_pipe_flag == 2)) {
                if (split_cmd.size() <= 1 && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                  devNull = open("/dev/null", O_WRONLY);
                  dup2(devNull, STDIN_FILENO);
                  dup2(devNull, STDOUT_FILENO);
                  dup2(devNull, STDERR_FILENO);
                }
                else if (split_cmd.size() > 1 && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                  devNull = open("/dev/null", O_WRONLY);
                  dup2(devNull, STDIN_FILENO);
                  dup2(devNull, STDOUT_FILENO);
                }
                else if () {
                }
              }
              */

              if (is_recvPipe && (recvfrom_user_pipe_flag == 1 || recvfrom_user_pipe_flag == 2)) {
                devNull = open("/dev/null", O_WRONLY);
                dup2(devNull, STDIN_FILENO);
                dup2(devNull, STDOUT_FILENO);
                if (split_cmd.size() <= 1 && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                  dup2(devNull, STDERR_FILENO);
                }
              }


              /*
              if (is_recvPipe && (recvfrom_user_pipe_flag == 1 || recvfrom_user_pipe_flag == 2)) {
                if (split_cmd.size() <= 1 && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                  devNull = open("/dev/null", O_WRONLY);
                  dup2(devNull, STDIN_FILENO);
                  dup2(devNull, STDOUT_FILENO);
                  dup2(devNull, STDERR_FILENO);
                }
                else if (split_cmd.size() > 1 && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                  devNull = open("/dev/null", O_WRONLY);
                  dup2(devNull, STDIN_FILENO);
                  dup2(devNull, STDOUT_FILENO);
                }
              }
              */



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
              for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
              {
                if (npCount_v[client_socket[mc]][i] == 0)
                {
                  if (dup2(npPipeID_in[client_socket[mc]][i], STDIN_FILENO) < 0)
                  {
                    fprintf(stderr, "np dup create error.\n");
                    exit(EXIT_FAILURE);
                  }

                  //  重要!!!!!!
                  close(npPipeID_in[client_socket[mc]][i]);
                  close(npPipeID_in[client_socket[mc]][i] + 1);
                  break;
                }
              }

              char **args = (char **)malloc((argcount + 1) * sizeof(char *));
              for (int i = 0; i < argcount; i++)
                args[i] = (char *)split_cmd[i].c_str();
              args[argcount] = NULL;

              if (execvp(args[0], args) == -1)
              {
                fprintf(stderr, "Unknown command: [%s].\n", args[0]);
                exit(EXIT_FAILURE);
              }
              exit(EXIT_FAILURE);
            }


            if (is_sendPipe && user_pipe_flag == 3) {
              UserPipePB.push_back(pfd[0]);
              UserPipeContent_v[sendto_id][my_uid] = GLOBAL_COUNT;
              GLOBAL_COUNT++;
            }

            if (is_recvPipe && recvfrom_user_pipe_flag == 3) {
              UserPipeClientSock_v[my_uid][recvfrom_id] = -1;
            }

            if (is_sendPipe) {
              close(pfd[1]);
            }



            for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
            {
              if (npCount_v[client_socket[mc]][i] == 0)
              {
                close(npPipeID_in[client_socket[mc]][i]);
                close(npPipeID_in[client_socket[mc]][i] + 1);
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
            int fork_count = 0;
            if (!is_numberPipe)
              fork_count = pipe_count + 1;
            else
              fork_count = pipe_count;


            for (int i = 0; i < pipe_count; i++) {
              if (pipe(pipefd + i * 2) < 0)
              { // 一次迴圈加兩個記憶體位置, (0, 1), (2, 3)......
                fprintf(stderr, "pipe create error.\n");
                exit(EXIT_FAILURE);
              }
            }


            for (int i = 0; i < fork_count; i++)
            {

              /*             
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
              */


              /*
              for (int i = 0; i < pipe_count; i++) {
                if (pipe(pipefd + i * 2) < 0)
                { // 一次迴圈加兩個記憶體位置, (0, 1), (2, 3)......
                  fprintf(stderr, "pipe create error.\n");
                  exit(EXIT_FAILURE);
                }
              }
              */


              //signal(SIGCHLD, sig_handler);
              //signal(SIGALRM, (void (*)(int))kill_child);

              pid = fork();
              if (pid < 0)
              { // fork failed
                fprintf(stderr, "fork create error.\n");
                exit(EXIT_FAILURE);
              }
              else if (pid == 0)
              { // child

                // 第一個fork先檢查有沒有number pipe需要stdin
                if (i == 0)
                {
                  for (int j = 0; j < npCount_v[client_socket[mc]].size(); j++)
                  {
                    if (npCount_v[client_socket[mc]][j] == 0)
                    {
                      if (dup2(npPipeID_in[client_socket[mc]][j], STDIN_FILENO) < 0)
                      {
                        fprintf(stderr, "np out dup error.\n");
                        exit(EXIT_FAILURE);
                      }

                      // 重要!!!!!!
                      close(npPipeID_in[client_socket[mc]][j]);
                      close(npPipeID_in[client_socket[mc]][j] + 1);
                      break;
                    }
                  }
                }





                if (i == 0) {                  
                  // check recv User Pipe
                  if (is_recvPipe && recvfrom_user_pipe_flag != 0) {
                    if (recvfrom_user_pipe_flag == 1) {  // 沒有該user
                      if (send(sd, recvfromUserPipe_info.c_str(), recvfromUserPipe_info.length(), 0) != recvfromUserPipe_info.length()) {
                        fprintf(stderr, "recv UserPipe error.\n");
                      }
                      //if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
                      //  fprintf(stderr, "send %% error.\n");
                      //}
                      //continue;
                    }
                    else if (recvfrom_user_pipe_flag == 2) {  // 該pipe已經存在
                      if (send(sd, recvfromUserPipe_info.c_str(), recvfromUserPipe_info.length(), 0) != recvfromUserPipe_info.length()) {
                        fprintf(stderr, "recv UserPipe error.\n");
                      }
                      //if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
                      //  fprintf(stderr, "send %% error.\n");
                      //}
                      //continue;
                    }
                    else {  // broadcast
                      for (int i = 0; i < USER_ID_v.size(); i++) {
                        if (USER_ID_v[i] != 0) {
                          if (send(USER_ID_v[i], recvfromUserPipe_info.c_str(), recvfromUserPipe_info.length(), 0) != recvfromUserPipe_info.length()) {
                            fprintf(stderr, "recv UserPipe error.\n");
                          }
                        }
                      }
                      if (dup2(UserPipePB[UserPipeContent_v[my_uid][recvfrom_id]], STDIN_FILENO) < 0) {
                        fprintf(stderr, "User Pipe Content dup Error.\n");
                        exit(EXIT_FAILURE);
                      }
                    }
                  }
                }

                if (i == fork_count - 1) {
                  if (is_sendPipe) {
                    // check send User Pipe
                    if (user_pipe_flag != 0) {
                      if (user_pipe_flag == 1) {  // 沒有該user
                        if (send(sd, send2UserPipe_info.c_str(), send2UserPipe_info.length(), 0) != send2UserPipe_info.length()) {
                          fprintf(stderr, "send UserPipe error.\n");
                        }
                        //if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
                        //  fprintf(stderr, "send %% error.\n");
                        //}
                        //continue;
                      }
                      else if (user_pipe_flag == 2) {  // 該pipe已經存在
                        if (send(sd, send2UserPipe_info.c_str(), send2UserPipe_info.length(), 0) != send2UserPipe_info.length()) {
                          fprintf(stderr, "send UserPipe error.\n");
                        }
                        //if (send(sd, percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
                        //  fprintf(stderr, "send %% error.\n");
                        //}
                        //continue;
                      }
                      else {  // broadcast
                        for (int i = 0; i < USER_ID_v.size(); i++) {
                          if (USER_ID_v[i] != 0) {
                            if (send(USER_ID_v[i], send2UserPipe_info.c_str(), send2UserPipe_info.length(), 0) != send2UserPipe_info.length()) {
                              fprintf(stderr, "send UserPipe error.\n");
                            }
                          }
                        }
                        if (dup2(pfd[1], STDOUT_FILENO) < 0) {
                          fprintf(stderr, "User Pipe dup Error.\n");
                          exit(EXIT_FAILURE);
                        }
                      }
                    }  // User Pipe
                  }
                }


                int devNull = 0;
                if (is_sendPipe && (user_pipe_flag == 1 || user_pipe_flag == 2)) {
                  devNull = open("/dev/null", O_WRONLY);
                  dup2(devNull, STDIN_FILENO);
                  dup2(devNull, STDOUT_FILENO);
                  if (split_cmd[1] == "|" && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                    dup2(devNull, STDERR_FILENO);
                  }
                }


                /*
                if (is_sendPipe && (user_pipe_flag == 1 || user_pipe_flag == 2)) {
                  if (split_cmd[1] == "|" && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                    devNull = open("/dev/null", O_WRONLY);
                    dup2(devNull, STDIN_FILENO);
                    dup2(devNull, STDOUT_FILENO);
                    dup2(devNull, STDERR_FILENO);
                  }
                  else if (split_cmd[1] != "|" && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                    devNull = open("/dev/null", O_WRONLY);
                    dup2(devNull, STDIN_FILENO);
                    dup2(devNull, STDOUT_FILENO);
                  }
                  else if (split_cmd[0] != "cat" && split_cmd[0] != "number" && split_cmd[0] != "removetag" && split_cmd[0] != "removetag0" || split_cmd[0] != "manyblessings") {
                    devNull = open("/dev/null", O_WRONLY);
                    dup2(devNull, STDIN_FILENO);
                    dup2(devNull, STDOUT_FILENO);
                  }
                }
                */


                if (is_recvPipe && (recvfrom_user_pipe_flag == 1 || recvfrom_user_pipe_flag == 2)) {
                  devNull = open("/dev/null", O_WRONLY);
                  dup2(devNull, STDIN_FILENO);
                  dup2(devNull, STDOUT_FILENO);
                  if (split_cmd[1] == "|" && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                    dup2(devNull, STDERR_FILENO);
                  }
                }

                /*
                if (is_recvPipe && (recvfrom_user_pipe_flag == 1 || recvfrom_user_pipe_flag == 2)) {
                  if (split_cmd[1] == "|" && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                    devNull = open("/dev/null", O_WRONLY);
                    dup2(devNull, STDIN_FILENO);
                    dup2(devNull, STDOUT_FILENO);
                    dup2(devNull, STDERR_FILENO);
                  }
                  else if (split_cmd[1] != "|" && (split_cmd[0] == "cat" || split_cmd[0] == "number" || split_cmd[0] == "removetag" || split_cmd[0] == "removetag0" || split_cmd[0] == "manyblessings")) {
                    devNull = open("/dev/null", O_WRONLY);
                    dup2(devNull, STDIN_FILENO);
                    dup2(devNull, STDOUT_FILENO);
                  }
                  else if (split_cmd[0] != "cat" && split_cmd[0] != "number" && split_cmd[0] != "removetag" && split_cmd[0] != "removetag0" || split_cmd[0] != "manyblessings") {
                    devNull = open("/dev/null", O_WRONLY);
                    dup2(devNull, STDIN_FILENO);
                    dup2(devNull, STDOUT_FILENO);
                  }
                }
                */


                if (is_sendPipe && is_recvPipe) {
                  close(pfd[0]);
                  close(pfd[1]);
                }

                if (is_recvPipe && recvfrom_user_pipe_flag == 3) {
                  UserPipeContent_v[my_uid][recvfrom_id] = -1;
                }





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




                same_pipe_ptr = 0;
                // store number pipe
                if (i == fork_count - 1 && is_numberPipe)
                {
                  // search npCount_v value
                  for (int j = 0; j < npCount_v[client_socket[mc]].size(); j++)
                  {
                    if (same_pipe_ptr != 0)
                    {
                      break;
                    }
                    if (npCount_v[client_socket[mc]][j] == numberPipeCount)
                    {
                      same_pipe_ptr = 1;
                      if (dup2(npPipeID_in[client_socket[mc]][j] + 1, STDOUT_FILENO) < 0)
                      {
                        fprintf(stderr, "out dup error.\n");
                        exit(EXIT_FAILURE);
                      }
                      if (is_exclamation)
                      {
                        if (dup2(npPipeID_in[client_socket[mc]][j] + 1, STDERR_FILENO) < 0)
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
                

                for (int j = 0; j < 2 * pipe_count; j++)
                  close(pipefd[j]);

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

                if (execvp(args[0], args) == -1)
                {
                  fprintf(stderr, "Unknown command: [%s].\n", args[0]);
                  exit(EXIT_FAILURE);
                }
                exit(EXIT_FAILURE);
              }
            }






            if (is_sendPipe && user_pipe_flag == 3) {
              UserPipePB.push_back(pfd[0]);
              UserPipeContent_v[sendto_id][my_uid] = GLOBAL_COUNT;
              GLOBAL_COUNT++;
            }

            if (is_recvPipe && recvfrom_user_pipe_flag == 3) {
              UserPipeClientSock_v[my_uid][recvfrom_id] = -1;
            }

            if (is_sendPipe) {
              close(pfd[1]);
            }










            for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
            {
              if (npCount_v[client_socket[mc]][i] == 0)
              {
                close(npPipeID_in[client_socket[mc]][i]);
                close(npPipeID_in[client_socket[mc]][i] + 1);
                break;
              }
            }

            // store number pipe FD
            if (is_numberPipe)
            {
              npCount_v[client_socket[mc]].push_back(numberPipeCount);
              npPipeID_in[client_socket[mc]].push_back(pipefd[(pipe_count - 1) * 2]); // store STDIN
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

          for (int i = 0; i < npCount_v[client_socket[mc]].size(); i++)
          {
            npCount_v[client_socket[mc]][i]--;
          }

          if (send(client_socket[mc], percent_sign, strlen(percent_sign), 0) != strlen(percent_sign)) {
            fprintf(stderr, "send %% error.\n");
          }
          restore_std(std);  // 回傳client
				}
			}
		}
	}

	return EXIT_SUCCESS;
}
