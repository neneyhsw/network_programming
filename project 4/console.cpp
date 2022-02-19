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
#include <boost/algorithm/string.hpp>

using boost::asio::ip::tcp;
using namespace std;  // yhsw ++

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

// 特殊字符會出錯
bool replace(string &str, const string &from, const string &to)
{
  size_t start_pos = str.find(from);
  if (start_pos == string::npos)
    return false;
  str.replace(start_pos, from.length(), to);
  return true;
}

// Reference: https://www.delftstack.com/zh-tw/howto/cpp/how-to-read-a-file-line-by-line-cpp/
vector<string> read_file(string filename)
{
  vector<string> lines;
  string line;
  ifstream input_file(filename);

  while (getline(input_file, line)) {
    /* 重要!!!!!!
     * 一定要加\n, 如果沒加\n程式會掛掉
     */
    lines.push_back(line + "\n");
  }
  input_file.close();

  return lines;
}

bool is_proxy = false;
tcp::endpoint proxy_endpoint_;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket, tcp::endpoint endpoint, string filename, string idx)
    : socket_(std::move(socket))
  {
    endpoint_ = endpoint;
    test_case = read_file(filename);
    index = idx;
  }

  void start()
  {
    auto self(shared_from_this());
    if (!is_proxy) {
      socket_.async_connect(
        endpoint_,
        [this, self](const boost::system::error_code& ec) {
          if (!ec) {
            do_read();
          }
        }
      );
    }
    // proxy欄位有填入資料
    else {
      socket_.async_connect(
        proxy_endpoint_,
        [this, self](const boost::system::error_code& ec) {
          if (!ec) {
            send_conn();
          }
          else {
            //cerr << "proxy connection error." << endl;
            //cerr << ec.message() << endl;
          }
        }
      );
    }
  }

private:

  void send_conn() {
    auto self(shared_from_this());
    vector<string> nums;
    string dstip = endpoint_.address().to_string();
    boost::split(nums, dstip, boost::is_any_of("."), boost::token_compress_on);
    unsigned short dstport = endpoint_.port();
    cout << dstip << dstport << endl;
    unsigned char reply[9] = {
      4, 1, (unsigned char)(dstport / 256), (unsigned char)(dstport % 256), 
      (unsigned char)stoi(nums[0]), (unsigned char)stoi(nums[1]), 
      (unsigned char)stoi(nums[2]), (unsigned char)stoi(nums[3]), 0
    };
    memcpy(show_output, reply, 9);

    socket_.async_write_some(
      boost::asio::buffer(show_output, 9),
      [this, self](boost::system::error_code ec, size_t length) {
        if (!ec) {
          recv_conn();
        }
        else {
          cerr << "send_conn error" << endl;
          cerr << ec.message() << endl;
        }
      }
    );
  }

  void recv_conn() {
    auto self(shared_from_this());
    socket_.async_read_some(
      boost::asio::buffer(show_output, max_length),
      [this, self](boost::system::error_code ec, size_t length) {
        if (!ec) {
          memset(show_output, 0, max_length);
          memset(write_cmd, 0, max_length);
          do_read();
        }
        else {
          cerr << "recv_conn error." << endl;
          cerr << ec.message() << endl;
        }
      }
    );
  }

  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(show_output, max_length),
      [this, self](boost::system::error_code ec, std::size_t length)
      {
        if (!ec)
        {
          string data = string(show_output);  // 讀取螢幕顯示畫面
          
          // 讀寫資料後清空陣列
          // Reference: https://www.delftstack.com/zh-tw/howto/c/clear-array-in-c/
          memset(show_output, 0, max_length);
          memset(write_cmd, 0, max_length);

          // 寫入HTML需要將特殊字元替換掉, 如\r\n等
          escape_html(data);
          cout << "<script>document.getElementById(\'s" + index + 
          "\').innerHTML += \'" + data + "\';</script>" << endl;

          // 如果有cmd就輸入cmd (write)
          if (data.find("%") != std::string::npos) {
            do_write();
          }
          else {
            do_read();  // 如果沒有cmd則繼續讀取 (read)
          }
        }
      }
    );
  }

  void do_write()
  {
    auto self(shared_from_this());

    string cmd = test_case[0];  // 讀取test_case下一個指令
    test_case.erase(test_case.begin());  // 刪除已經執行的指令
    strcpy(write_cmd, cmd.c_str());  // 將指令寫入char

    // 轉換為HTML格式
    escape_html(cmd);
    cout << "<script>document.getElementById(\'s" + index + 
    "\').innerHTML += \'<b>" + cmd + "</b>\';</script>" << endl;

    boost::asio::async_write(socket_, boost::asio::buffer(write_cmd, strlen(write_cmd)),
      [this, self](boost::system::error_code ec, std::size_t)
      {
        if (!ec)
        {
          do_read();  // 已經寫了就繼續讀取 (read)
        }
      }
    );
  }

  // Reference: https://stackoverflow.com/questions/5665231/most-efficient-way-to-escape-xml-html-in-c-string
  void escape_html(string &str) 
  {
    boost::algorithm::replace_all(str, "&", "&amp;");
    boost::algorithm::replace_all(str, "\"", "&quot;");
    boost::algorithm::replace_all(str, "\'", "&apos;");
    boost::algorithm::replace_all(str, "<", "&lt;");
    boost::algorithm::replace_all(str, ">", "&gt;");
    boost::algorithm::replace_all(str, "\r\n", "\n");
    boost::algorithm::replace_all(str, "\n", "<br>");
  }

  tcp::socket socket_;
  enum { max_length = 10000000 };
  char data_[max_length];

  tcp::endpoint endpoint_;
  char show_output[max_length] = {0};
  char write_cmd[max_length] = {0};
  vector<string> test_case;
  string index;
};

int main(int argc, char* argv[])
{
  try
  {
    string hc1 = 
      "Content-type: text/html\r\n\r\n"
      "<!DOCTYPE html>\n"
      "<html lang=\"en\">\n"
      "  <head>\n"
      "    <meta charset=\"UTF-8\" />\n"
      "    <title>NP Project 3 Sample Console</title>\n"
      "    <link\n"
      "      rel=\"stylesheet\"\n"
      "      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n"
      "      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n"
      "      crossorigin=\"anonymous\"\n"
      "    />\n"
      "    <link\n"
      "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
      "      rel=\"stylesheet\"\n"
      "    />\n"
      "    <link\n"
      "      rel=\"icon\"\n"
      "      type=\"image/png\"\n"
      "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
      "    />\n"
      "    <style>\n"
      "      * {\n"
      "        font-family: 'Source Code Pro', monospace;\n"
      "        font-size: 1rem !important;\n"
      "      }\n"
      "      body {\n"
      "        background-color: #212529;\n"
      "      }\n"
      "      pre {\n"
      "        color: #cccccc;\n"
      "      }\n"
      "      b {\n"
      "        color: #01b468;\n"
      "      }\n"
      "    </style>\n"
      "  </head>\n"
      "  <body>\n"
      "    <table class=\"table table-dark table-bordered\">\n"
      "      <thead>\n"
      "        <tr>\n";
      //"          <th scope=\"col\">nplinux1.cs.nctu.edu.tw:1234</th>\n"
      //"          <th scope=\"col\">nplinux2.cs.nctu.edu.tw:5678</th>\n"

    string col_1 = "<th scope=\"col\">";
    string col_2 = "</th>\n";

    string hc2 =
      "        </tr>\n"
      "      </thead>\n"
      "      <tbody>\n"
      "        <tr>\n";
      //"          <td><pre id=\"s0\" class=\"mb-0\"></pre></td>\n"
      //"          <td><pre id=\"s1\" class=\"mb-0\"></pre></td>\n"

    string cols_1 = "<td><pre id=\"s";
    string cols_2 = "\" class=\"mb-0\"></pre></td>\n";

    string hc3 =
      "        </tr>\n"
      "      </tbody>\n"
      "    </table>\n"
      "  </body>\n"
      "</html>\n";


    /* QUERY_STRING EXAMPLE
     * h0=nplinux7.cs.nctu.edu.tw&p0=57786&f0=t2.txt&h1=&p1=&f1=&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=
     * */
    string req_uri = string(getenv("QUERY_STRING"));
    //cerr << "req_uri: " << req_uri << endl;
    vector<string> param_value, host_name, host_port, test_case;
    vector<string> req_uri_v = splitString(req_uri, "&");
    //for (int i = 0; i < req_uri_v.size(); i++)
    //  cerr << req_uri_v[i] << endl;
    int server_count = 0;

    // for proxy
    string sh = "";  // index = req_uri_v.size() - 2
    string sp = "";  // index = req_uri_v.size() - 1

    if (req_uri.find("sp=") != std::string::npos) {
      param_value = splitString(req_uri, "&sp=");
      sp = param_value[1];
      if (param_value[0].find("sh=") != std::string::npos) {
        param_value = splitString(param_value[0], "sh=");
        sh = param_value[1];
      }
    }

    // 抓出參數和server數量
    for (int i = 0; i < req_uri_v.size(); i++) {

      /*
      if (req_uri_v[i].find("sh") != std::string::npos) {
        param_value = splitString(req_uri_v[i], "=");
        sh = param_value[1];
        break;
      }
      if (req_uri_v[i].find("sp") != std::string::npos) {
        param_value = splitString(req_uri_v[i], "=");
        sp = param_value[1];
        break;
      }
      */


      param_value = splitString(req_uri_v[i], "=");

      // 如果有參數
      if (param_value.size() > 1) {

        // proxy part
        if (i == (req_uri_v.size() - 2)) {
          sh = param_value[1];
        }
        if (i == (req_uri_v.size() - 1)) {
          sp = param_value[1];
        }


        if (param_value[1] == "") {
          break;
        }
        server_count = ceil((i+1) / 3);  // 每個server有3個參數, 所以(i+1) / 3後無條件進位

        // host name
        if ((i%3) == 0) {
          host_name.push_back(param_value[1]);
        }
        // host port
        else if ((i%3) == 1) {
          host_port.push_back(param_value[1]);
        }
        // host file
        else if ((i%3) == 2) {
          test_case.push_back("test_case/" + param_value[1]);
        }
      }
      else {
        break;
      }
    }

    cerr << "server count: " << server_count << endl;
    for (int i = 0; i < server_count; i++) {
      cerr << "\n" << (i+1) << ":" << endl;
      cerr << "host name: " << host_name[i] << endl;
      cerr << "host port: " << host_port[i] << endl;
      cerr << "test case: " << test_case[i] << endl;
    }
    cerr << "proxy host: " << sh << endl;
    cerr << "proxy port: " << sp << endl;


    // 處理host name, port, test_case
    string html_middle = "";
    string html_middle_2 = "";
    for (int i = 0; i < server_count; i++) {
      html_middle += (col_1 + host_name[i] + ":" + host_port[i] + col_2);
      html_middle_2 += (cols_1 + to_string(i) + cols_2);
    }

    string html_all = hc1 + html_middle + hc2 + html_middle_2 + hc3;
    cout << html_all;  // 輸出內容於網頁中


    // 處理resolver
    boost::asio::io_context io_context;

    // proxy part
    if (sh != "" && sp != "") {
      tcp::resolver resolver(io_context);
      tcp::resolver::query query(sh, sp);
      tcp::resolver::iterator iter = resolver.resolve(query);
      proxy_endpoint_ = iter->endpoint();
      is_proxy = true;
    }

    // np single golden
    for (int i = 0; i < server_count; i++) {
      tcp::resolver resolver(io_context);
      tcp::resolver::query query(host_name[i], host_port[i]);
      tcp::resolver::iterator iter = resolver.resolve(query);
      tcp::endpoint endpoint = iter -> endpoint();
      tcp::socket socket(io_context);
      make_shared<session>(move(socket), endpoint, test_case[i], to_string(i)) -> start();
    }

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

