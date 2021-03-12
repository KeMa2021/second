//===-- SMTLIBLoggingSolver.cpp -------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


#include "QueryLoggingSolver.h"

#include "STPSolver.h"
#include "klee/Expr/ExprSMTLIBPrinter.h"

using namespace klee;

/// This QueryLoggingSolver will log queries to a file in the SMTLIBv2 format
/// and pass the query down to the underlying solver.

namespace {
  llvm::cl::opt<bool> UseSocket(
      "use-socket",
      llvm::cl::desc("Enable out method")
  );
}

int getMsg(){

  ///define sockfd
  int server_sockfd = socket(AF_INET,SOCK_STREAM, 0);

  ///define sockaddr_in
  struct sockaddr_in server_sockaddr;
  server_sockaddr.sin_family = AF_INET;
  server_sockaddr.sin_port = htons(8889);
  server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  ///bind，success return 0，fail return -1
  if(bind(server_sockfd,(struct sockaddr *)&server_sockaddr,sizeof(server_sockaddr))==-1)
  {
    perror("bind");
    llvm::errs() << "bind fail";
    exit(1);
  }

  ///listen，return 0，fail return -1
  if(listen(server_sockfd,1) == -1)
  {
    perror("listen");
    exit(1);
  }

  ///client socket
  char buffer[1024];
  struct sockaddr_in client_addr;
  socklen_t length = sizeof(client_addr);

  ///成功返回非负描述字，出错返回-1
  int conn = accept(server_sockfd, (struct sockaddr*)&client_addr, &length);
  if(conn<0)
  {
    perror("connect");
    exit(1);
  }

//  while(1)
  {
    memset(buffer,0,sizeof(buffer));
    recv(conn, buffer, sizeof(buffer),0);
//    if(strcmp(buffer,"exit\n")==0)
//      break;
//    fputs(buffer, stdout);
//    send(conn, buffer, len, 0);
    llvm::errs() << buffer;
  }
  close(conn);
  close(server_sockfd);

  return buffer[0] - '0';
}
char sendbuf[1024 * 1000];
int exprCnt = 1000;
bool flag = 0;
class SMTLIBLoggingSolver : public QueryLoggingSolver
{
        private:
    
                ExprSMTLIBPrinter printer;

                virtual void printQuery(const Query& query,
                                        const Query* falseQuery = 0,
                                        const std::vector<const Array*>* objects = 0) 
                {
                        if (0 == falseQuery) 
                        {
                                printer.setQuery(query);
                        }
                        else
                        {
                                printer.setQuery(*falseQuery);
                        }

                        if (0 != objects)
                        {
                                printer.setArrayValuesToGet(*objects);
                        }
                        std::string out;
                        llvm::raw_string_ostream rso(out);
                        if (UseSocket)
                          printer.setOutput(rso);
                        printer.generateOutput();
//                        llvm::errs() << rso.str();

                        bool use_socket = UseSocket;
                        if (UseSocket){
                          if (rso.str().length() > 200000){
                            use_socket = false;
                            socket_STP::use_Tseitin = false;
                          }else if (rso.str().length() < 2000){
                            use_socket = false;
                            socket_STP::use_Tseitin = true;
                          }
                        }

                        if (exprCnt > 100){
                          use_socket = false;
                        }


                        if(use_socket){
                          int sock_cli = socket(AF_INET, SOCK_STREAM, 0);
                          struct sockaddr_in servaddr;
                          memset(&servaddr, 0, sizeof(servaddr));
                          servaddr.sin_family = AF_INET;
                          servaddr.sin_port = htons(8887);  ///server port
                          servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");  ///server ip

                          if (connect(sock_cli, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
                          {
                            perror("connect");
                            exit(1);
                          }

                          memset(sendbuf, 0, sizeof(sendbuf));
                          char recvbuf[1024];
                          strcpy(sendbuf, rso.str().c_str());
//                          sendbuf[]
//                          llvm::errs() << strlen(rso.str().c_str()) << "\n";
                          send(sock_cli, sendbuf, strlen(rso.str().c_str()) + 1,0); ///send
//                          send(sock_cli, "\0", strlen(rso.str().c_str()),0); ///send
                          recv(sock_cli, recvbuf, sizeof(recvbuf),0); ///receive
//                        int msg = getMsg();
                          int msg = recvbuf[0] - '0';
                          if(msg == 0){
                            socket_STP::use_Tseitin = false;
                          }else{
                            socket_STP::use_Tseitin = true;
                          }

//                        llvm::errs() << recvbuf;

                          memset(recvbuf, 0, sizeof(recvbuf));

                          close(sock_cli);
                        }
//                        else {
//                          std::string out;
//                          llvm::raw_string_ostream rso(out);
//                          printer.setOutput(rso);
//                          printer.generateOutput();
////                        llvm::errs() << rso.str();
//                        }
                        if (exprCnt == 0){
                          flag = socket_STP::use_Tseitin;
                          exprCnt++;
                        }else if (exprCnt < 100){
                          if (flag == socket_STP::use_Tseitin){
                            exprCnt++;
                          }else {
                            exprCnt = 1;
                            flag = socket_STP::use_Tseitin;
                          }
                        }

                }    
        
	public:
		SMTLIBLoggingSolver(Solver *_solver,
                        std::string path,
                        time::Span queryTimeToLog,
                        bool logTimedOut)
		: QueryLoggingSolver(_solver, path, ";", queryTimeToLog, logTimedOut)
		{
		  //Setup the printer
		  printer.setOutput(logBuffer);
		}
};


Solver* klee::createSMTLIBLoggingSolver(Solver *_solver, std::string path,
                                        time::Span minQueryTimeToLog, bool logTimedOut)
{
  return new Solver(new SMTLIBLoggingSolver(_solver, path, minQueryTimeToLog, logTimedOut));
}


