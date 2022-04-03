# network_programming
The project of network programming course in NCTU.

## project 1
In project 1, you are asked to design a shell named npshell. The npshell should support the following features:
* Execution of commands
* Ordinary Pipe
* Numbered Pipe
* File Redirection
More details are defined in NP_Project1_Spec.pdf.

## project 2
In project 2, you are asked to design 3 kinds of servers:
* Design a Concurrent connection-oriented server. This server allows one client connect to it.
* Design a server of the chat-like systems, called remote working systems (rwg). In this system, users can communicate with other users. You need to use the single-process concurrent paradigm to design this server.
* Design the rwg server using the concurrent connection-oriented paradigm with shared memory and FIFO.
**These three servers must support all functions in project 1.**
More details are defined in NP_Project2_Spec.pdf.

## project 3
In project 3, you are asked to implement a simple HTTP server called http server and a CGI program console.cgi. We will use Boost.Asio library to accomplish this project.
More details are defined in NP_Project3_Spec.pdf.

## project 4
In project 4, you are going to implement the SOCKS 4/4A protocol in the application layer of the OSI model.
SOCKS is similar to a proxy (i.e., intermediary-program) that acts as both server and client for the purpose of making requests on behalf of other clients. Because the SOCKS protocol is independent of application protocols, it can be used for many different services: telnet, ftp, WWW, etc.
There are two types of the SOCKS operations, namely CONNECT and BIND. You have to implement both of them in this project. We will use Boost.Asio library to accomplish this project.
More details are defined in NP_Project4_Spec.pdf.
