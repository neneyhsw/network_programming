//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Reference from boost_asio_example/echo_server.cpp

#include <iostream>
#include <boost/asio.hpp>
#include <map>  // yhsw ++

using boost::asio::ip::tcp;
using namespace std;  // yhsw ++

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
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
      //res.push_back(delimiter); // yhsw <++
    }

    res.push_back(s.substr(pos_start));
    return res;
  }

  void removeVectorSpaces(vector<string> &v, string delimiter) {
    vector<string>::iterator iter;
    for (iter = v.begin(); iter != v.end();) {
      if (*iter == delimiter)
        v.erase(iter);
      else
        iter++;
    }
  }

  void start()
  {
    do_read();
  }

  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(
      boost::asio::buffer(data_, max_length),
      [this, self](boost::system::error_code ec, size_t length) {
        if (!ec) {
          string raw_data(data_);
          map<string, string> Env_Var;
          vector<string> parser, basic_info;

          //cout << raw_data << endl;
          /* RAW DATA 
 
          GET /panel.cgi HTTP/1.1
          Host: nplinux7.cs.nctu.edu.tw:58663
          Connection: keep-alive
          Cache-Control: max-age=0
          Upgrade-Insecure-Requests: 1
          User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/96.0.4664.55 Safari/537.36 Edg/96.0.1054.43
          Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,;q=0.8,application/signed-exchange;v=b3;q=0.9
          Accept-Encoding: gzip, deflate
          Accept-Language: zh-TW,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6
          Cookie: _ga=GA1.3.2047756430.1632646256

          */


          // 字串處理
          parser = splitString(raw_data, "\r\n");  // 將每行分開
          removeVectorSpaces(parser, "");  // 去除最後兩個空行vector
          basic_info = splitString(parser[0], " ");  // 將 "GET /panel.cgi HTTP/1.1" 切割
          
          cout << "parser:" << endl;
          for (int i = 0; i < parser.size(); i++)
            cout << parser[i] << endl;
          cout << "\nbasic info:" << endl;
          for (int i = 0; i < basic_info.size(); i++)
            cout << basic_info[i] << endl;



          Env_Var["REQUEST_METHOD"] = basic_info[0];  // GET
          Env_Var["REQUEST_URI"] = basic_info[1];  // /panel.cgi
          Env_Var["QUERY_STRING"] = "";
          
          string req_uri = basic_info[1].substr(1, basic_info[1].size());  // 去除第一個斜線/
          /* 如果cgi有用?接其他參數 */
          if (req_uri.find("?") != string::npos) {
            vector<string> transmit = splitString(req_uri, "?");

            /* 儲存?後面的字串 */
            Env_Var["QUERY_STRING"] = transmit[1];
            req_uri = transmit[0];
          }

          Env_Var["SERVER_PROTOCOL"] = basic_info[2];  // HTTP/1.1

          vector<string> host_info = splitString(parser[1], " ");
          Env_Var["HTTP_HOST"] = host_info[1];

          Env_Var["SERVER_ADDR"] = socket_.local_endpoint().address().to_string();
          Env_Var["SERVER_PORT"] = to_string(socket_.local_endpoint().port());
          Env_Var["REMOTE_ADDR"] = socket_.remote_endpoint().address().to_string();
          Env_Var["REMOTE_PORT"] = to_string(socket_.remote_endpoint().port());

          do_write(Env_Var, req_uri, strlen(data_));
        }
      }
    );
  }

  void do_write(map<string, string> Env_Var, string req_uri, std::size_t length) {
    auto self(shared_from_this());
    strcpy(data_, "HTTP/1.1 200 OK\r\n");

    // CHANGE async_write TO async_write_some
    socket_.async_write_some(boost::asio::buffer(data_, strlen(data_)),  // 使用length會失敗
      [this, self, Env_Var, req_uri](boost::system::error_code ec, size_t /*length*/) {
        if (!ec) {
          pid_t pid = fork();
          if (pid == 0) {
            /* 設定環境變數 */
            clearenv();  /* 重要!!!!! 如果不加這行無法執行 */
            for (auto &element : Env_Var) {
              setenv(element.first.c_str(), element.second.c_str(), 1);
            }

            // 輸出螢幕
            if (dup2(socket_.native_handle(), STDIN_FILENO) < 0) {
              fprintf(stderr, "dup in error.\n");
              exit(EXIT_FAILURE);
            }
            if (dup2(socket_.native_handle(), STDOUT_FILENO) < 0) {
              fprintf(stderr, "dup out error.\n");
              exit(EXIT_FAILURE);
            }
            if (execvp(req_uri.c_str(), NULL) == -1) {
              fprintf(stderr, "exec cgi error\n");
            }
          }
          else {
            socket_.close();  // 接好後close
          }
        }
      }
    );
  }

/*
private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            do_write(length);
          }
        });
  }

  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
        [this, self](boost::system::error_code ec, std::size_t)
        {
          if (!ec)
          {
            do_read();
          }
        });
  }
*/


  tcp::socket socket_;
  enum { max_length = 10000000 };
  char data_[max_length];
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (!ec)
        {
          std::make_shared<session>(std::move(socket))->start();
        }

        do_accept();
      }
    );
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
