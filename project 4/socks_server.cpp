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
#include <fstream>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp> // include Boost, a C++ library, FOR replace all

using boost::asio::ip::tcp;
using namespace std; // yhsw ++

boost::asio::io_context io_context;

class server //: public std::enable_shared_from_this<server>
{
public:
  //boost::asio::io_context& io_context;

  server(boost::asio::io_context &io_context, short port)
      : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
        bind_acceptor_(io_context, tcp::endpoint(tcp::v4(), 0))
  {
    do_accept();
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

  vector<string> read_file(string filename)
  {
    vector<string> commands_temp;
    ifstream ifs(filename, std::ios::in);
    if (!ifs.is_open())
    {
      cerr << "cannot open file.\n";
    }
    else
    {
      string temp;
      while (getline(ifs, temp))
      {
        commands_temp.push_back(temp + "\n");
      }
      ifs.close();
    }
    return commands_temp;
  }

  void do_parse()
  {
    //auto self(shared_from_this());
    client_sock_.async_read_some(
        boost::asio::buffer(client_buffer, max_length),
        [this](boost::system::error_code ec, size_t length)
        {
          if (!ec)
          {

            string VN = to_string(client_buffer[0]);
            string CD = to_string(client_buffer[1]);

            auto dst_port = to_string(((int)client_buffer[2] << 8) + (int)client_buffer[3]);
            dst_port = to_string(((int)client_buffer[2] << 9) + (int)client_buffer[3]);
            dst_port = to_string((int)client_buffer[2] * 256 + (int)client_buffer[3]);

            if (to_string(client_buffer[2]) == "0" && to_string(client_buffer[3]) == "80")
            {
              dst_port = "80";
            }
            else if (to_string(client_buffer[2]) == "1" && to_string(client_buffer[3]) == "-69")
            {
              dst_port = "443";
            }

            auto dst_ip = to_string((int)client_buffer[4] < 0 ? ((int)client_buffer[4] + 256) : (int)client_buffer[4]) + "." +
                          to_string((int)client_buffer[5] < 0 ? ((int)client_buffer[5] + 256) : (int)client_buffer[5]) + "." +
                          to_string((int)client_buffer[6] < 0 ? ((int)client_buffer[6] + 256) : (int)client_buffer[6]) + "." +
                          to_string((int)client_buffer[7] < 0 ? ((int)client_buffer[7] + 256) : (int)client_buffer[7]);

            auto src_ip = client_sock_.remote_endpoint().address().to_string();
            auto src_port = client_sock_.remote_endpoint().port();
            auto domain = get_domain(length);

            string Command = "";
            string Reply = "";
            if (CD == "1")
            {
              Command = "CONNECT";

              // 要避免ip為0.0.0.0，所以先檢查domain
              string HOST = "";
              if (domain != "") {
                HOST = domain;
              }
              else {
                HOST = dst_ip;
              }

              tcp::resolver resolver(io_context);
              tcp::resolver::query query(HOST, dst_port);
              tcp::resolver::iterator iter = resolver.resolve(query);
              endpoint_ = iter->endpoint();
              tcp::socket new_sock(io_context);
              server_sock_ = move(new_sock);

              if (is_pass(dst_ip, CD)) {
                Reply = "Accept";

                // 同時有不同client連線時，同一個cout顯示時才不會被分割開
                cout << "<S_IP>: " << src_ip << endl
                     << "<S_PORT>: " << src_port << endl
                     << "<D_IP>: " << dst_ip << endl
                     << "<D_PORT>: " << dst_port << endl
                     << "<Command>: " << Command << endl
                     << "<Reply>: " << Reply << endl
                     << endl;

                // connect
                memset(client_buffer, 0x00, max_length);
                memset(server_buffer, 0x00, max_length);
                server_sock_.async_connect(
                    endpoint_,
                    [this](const boost::system::error_code &ec)
                    {
                      if (!ec)
                      {
                        // REPLY CONNECT
                        unsigned char reply[8] = {0, 90, 0, 0, 0, 0, 0, 0};
                        memcpy(client_buffer, reply, 8);
                        boost::asio::async_write(
                            client_sock_, boost::asio::buffer(client_buffer, 8),
                            [this](boost::system::error_code ec, size_t length)
                            {
                              if (!ec)
                              {
                                recvfrom_client();
                                recvfrom_server();
                              }
                            });
                      }
                      else
                      {
                        cerr << "Connect error" << endl;
                        cerr << ec.message() << endl;
                      }
                    });
              }
              else {
                Reply = "Reject";
                cout << "<S_IP>: " << src_ip << endl
                     << "<S_PORT>: " << src_port << endl
                     << "<D_IP>: " << dst_ip << endl
                     << "<D_PORT>: " << dst_port << endl
                     << "<Command>: " << Command << endl
                     << "<Reply>: " << Reply << endl
                     << endl;

                // REPLY REJECT
                unsigned char reply[8] = {0, 91, 0, 0, 0, 0, 0, 0};
                memcpy(client_buffer, reply, 8);
                boost::asio::async_write(
                    client_sock_, boost::asio::buffer(client_buffer, 8),
                    [this](boost::system::error_code ec, size_t /*length*/)
                    {
                      if (!ec)
                      {
                        client_sock_.close();
                      }
                    });
              }
            }
            else if (CD == "2")
            {
              Command = "BIND";
              if (is_pass(dst_ip, CD)) {
                Reply = "Accept";
                cout << "<S_IP>: " << src_ip << endl
                     << "<S_PORT>: " << src_port << endl
                     << "<D_IP>: " << dst_ip << endl
                     << "<D_PORT>: " << dst_port << endl
                     << "<Command>: " << Command << endl
                     << "<Reply>: " << Reply << endl
                     << endl;

                memset(client_buffer, 0x00, max_length);
                memset(server_buffer, 0x00, max_length);

                bind_port = bind_acceptor_.local_endpoint().port();
                unsigned char reply[8] = {0, 90, (unsigned char)(bind_port / 256), (unsigned char)(bind_port % 256), 0, 0, 0, 0};
                memcpy(client_buffer, reply, sizeof(reply));
                boost::asio::async_write(
                    client_sock_, boost::asio::buffer(client_buffer, sizeof(reply)),
                    [this](boost::system::error_code ec, size_t /*length*/)
                    {
                      if (!ec)
                      {
                        bind_accept();
                      }
                      else
                      {
                        cerr << "Bind error." << endl;
                      }
                    });

              }
              else {
                Reply = "Reject";
                cout << "<S_IP>: " << src_ip << endl
                     << "<S_PORT>: " << src_port << endl
                     << "<D_IP>: " << dst_ip << endl
                     << "<D_PORT>: " << dst_port << endl
                     << "<Command>: " << Command << endl
                     << "<Reply>: " << Reply << endl
                     << endl;

                // REPLY REJECT
                unsigned char reply[8] = {0, 91, 0, 0, 0, 0, 0, 0};
                memcpy(client_buffer, reply, 8);
                boost::asio::async_write(
                    client_sock_, boost::asio::buffer(client_buffer, 8),
                    [this](boost::system::error_code ec, size_t /*length*/)
                    {
                      if (!ec)
                      {
                        client_sock_.close();
                      }
                    });
              }
            }
          }
          //else
          //{
          //  client_sock_.close();
          //}
        });
  }

  string get_domain(size_t length)
  {
    //auto self(shared_from_this());

    vector<int> null_posi;
    string domain = "";

    for (int i = 8; i < length; i++)
    {
      if (client_buffer[i] == 0x00)
      {
        null_posi.push_back(i);
      }
    }
    if (null_posi.size() != 1)
    {
      for (int i = null_posi[0] + 1; i < null_posi[1]; i++)
      {
        domain += client_buffer[i];
      }
    }
    return domain;
  }

  // 允許通過的server ip (dst ip)
  bool is_pass(string dst_ip, string CD) {
    vector<string> rules = read_file("socks.conf");
    bool flag = false;
    for (int i = 0; i < rules.size(); i++) {
      vector<string> rule_split;
      rule_split = splitString(rules[i], " ");
      if((rule_split[1] == "c" && CD == "1") || (rule_split[1] == "b" && CD == "2")) {
        vector<string> server_ip_split, dst_ip_split;
        server_ip_split = splitString(rule_split[2], ".");
        dst_ip_split = splitString(dst_ip, ".");
        boost::replace_all(server_ip_split[3], "\n", "");  // 去除換行符號

        int count = 0;
        for (int j = 0; j < server_ip_split.size() && j < dst_ip_split.size(); j++) {
          if ((server_ip_split[j] != "*" && server_ip_split[j] == dst_ip_split[j]) || server_ip_split[j] == "*") {
            count++;
          }
        }
        if (count == 4) {
          return true;
        }
      }
    }
    return false;
  }

  void recvfrom_client()
  {
    //auto self(shared_from_this());

    client_sock_.async_read_some(
        boost::asio::buffer(client_buffer, max_length),
        [this](boost::system::error_code ec, size_t length)
        {
          if (!ec)
          {
            sendto_server(length);
          }
          else
          {
            client_sock_.close();
          }
        });
  }

  void recvfrom_server()
  {
    //auto self(shared_from_this());

    server_sock_.async_read_some(
        boost::asio::buffer(server_buffer, max_length),
        [this](boost::system::error_code ec, size_t length)
        {
          if (!ec)
          {
            sendto_client(length);
          }
          else
          {
            server_sock_.close();
          }
        });
  }

  void sendto_server(size_t length)
  {
    //auto self(shared_from_this());

    boost::asio::async_write(
        server_sock_, boost::asio::buffer(client_buffer, length),
        [this](boost::system::error_code ec, size_t length)
        {
          if (!ec)
          {
            recvfrom_client();
          }
          else
          {
            client_sock_.close();
          }
        });
  }

  void sendto_client(size_t length)
  {
    //auto self(shared_from_this());

    boost::asio::async_write(
        client_sock_, boost::asio::buffer(server_buffer, length),
        [this](boost::system::error_code ec, size_t length)
        {
          if (!ec)
          {
            recvfrom_server();
          }
          else
          {
            server_sock_.close();
          }
        });
  }

  void bind_accept()
  {
    //auto self(shared_from_this());

    bind_acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            server_sock_ = move(socket);
            bind_reply();
            bind_acceptor_.close();
          }
          else
          {
            cout << "bind accept error." << endl;
          }
        });
  }

  void bind_reply()
  {
    //auto self(shared_from_this());

    unsigned char reply[8] = {0, 90, (unsigned char)(bind_port / 256), (unsigned char)(bind_port % 256), 0, 0, 0, 0};
    memcpy(client_buffer, reply, 8);
    boost::asio::async_write(
        client_sock_, boost::asio::buffer(client_buffer, 8),
        [this](boost::system::error_code ec, size_t /*length*/)
        {
          if (!ec)
          {
            recvfrom_client();
            recvfrom_server();
          }
        });
  }

  void do_accept()
  {
    //auto self(shared_from_this());

    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            io_context.notify_fork(boost::asio::io_context::fork_prepare);
            pid_t pid = fork();
            
            if (pid == 0) {
              io_context.notify_fork(boost::asio::io_context::fork_child);
              acceptor_.close();
              //std::make_shared<session>(std::move(socket))->start();
              client_sock_ = move(socket);
              do_parse();
            }
            else if (pid < 0) {
              cerr << "fork error." << endl;
            }
            else {
              io_context.notify_fork(boost::asio::io_context::fork_parent);
              socket.close();
              do_accept();
            } 
          }
        });
  }

  enum
  {
    max_length = 10000000
  };
  tcp::acceptor acceptor_;
  tcp::acceptor bind_acceptor_;

  tcp::socket client_sock_{io_context};
  tcp::socket server_sock_{io_context};

  tcp::endpoint endpoint_;

  unsigned char client_buffer[max_length];
  unsigned char server_buffer[max_length];
  unsigned short bind_port;
};

int main(int argc, char *argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    //boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception &e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
