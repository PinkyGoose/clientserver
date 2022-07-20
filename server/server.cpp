#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#define SERVER_ADRESS "127.0.0.1"
#define SERVER_PORT 5002
#define MESSAGE_MEMORY 1024

int Daemon(int, char **);

pthread_t thread[10];
int listener;

void handler(int i) {
  FILE *lg;
  switch (i) {
  case SIGTERM: {
    lg = fopen("server.log", "a");
    fprintf(lg, "%s", "SIGTERM accepted\n");
    fclose(lg);
    close(listener);
    for (int i = 0; i < 10; i++)
      pthread_join(thread[i], NULL);
    exit(EXIT_SUCCESS);
  }
  case SIGHUP: {
    lg = fopen("server.log", "a");
    fprintf(lg, "%s", "SIGHUP accepted\n");
    fclose(lg);
    exit(EXIT_SUCCESS);
  }
  default: {
    lg = fopen("server.log", "a");
    fprintf(lg, "%s", "unknown signal");
    fprintf(lg, "%d", i);
    fprintf(lg, "%s", "\n");

    fclose(lg);
    exit(i);
  }
  }
}
void wait4children(int signo) {
  int status;
  while (waitpid(-1, &status, WNOHANG) > 0)
    ;
}

int main(int argc, char *argv[]) {

  FILE *lg;
  signal(SIGCHLD, wait4children);

  int i;
  pid_t pid;

  signal(SIGHUP, handler);
  signal(SIGTERM, handler);


  //Начало демонизации
  pid = fork();
  if (pid < 0) {
    lg = fopen("server.log", "a");
    fprintf(lg, "%s", "fork error!\n");
    fclose(lg);
    perror("fork error!");
    exit(1);
  } else if (pid > 0) {
    lg = fopen("server.log", "a");
    fprintf(lg, "%s", "try to make a daemond\n");
    fclose(lg);
    exit(0);
  }
  setsid();

  char szPath[MESSAGE_MEMORY];
  if (getcwd(szPath, sizeof(szPath)) == NULL) {
    lg = fopen("server.log", "a");
    fprintf(lg, "%s", "dir error!\n");
    fclose(lg);
    exit(1);
  } else {
    chdir(szPath);
  }

  umask(0);
  for (i = 0; i < 3; ++i) {
    close(i);
  }
//конец демонизации
  signal(SIGCHLD, SIG_IGN);
  return Daemon(argc, argv);
  return 0;
}

void *thread_func(void *arg) {
//Поток, в котором обрабатываются сообщения определенного сокета
  int bytes_read;
  char buf[MESSAGE_MEMORY];
  FILE *fp, *lg;
  int sockid = *(int *)arg;

  bytes_read = recv(sockid, buf, MESSAGE_MEMORY, 0);//получаем название файла



  if (bytes_read <= 0) {
    close(sockid);
    return arg;
  }
  std::string str;
  str += "./files/";
  str += buf;
  fp = fopen(str.c_str(), "w");

  lg = fopen("server.log", "a");
  str = std::to_string(sockid) + " socket is active. accepting ";
  str += buf;
  str += " file\n";
  fprintf(lg, "%s", str.c_str());
  fclose(lg);
  send(sockid, buf, bytes_read, 0);




  while (1) {
    bytes_read = recv(sockid, buf, MESSAGE_MEMORY, 0);

    if (bytes_read <= 0)
      break;
    {
      fprintf(fp, "%s", buf);
      send(sockid, buf, bytes_read, 0);
    }
  }
  fclose(fp);



  lg = fopen("server.log", "a");
  fprintf(lg, "%s", "file accepted. socket is free.\n");
  fclose(lg);

  close(sockid);
  return arg;
}

int Daemon(int argc, char *argv[]) {

  int i = 0;
  int sock;
  struct sockaddr_in addr;
  FILE *lg;

  

  lg = fopen("server.log", "a");
  fprintf(lg, "%s", "Daemon server started.\n");
  fclose(lg);
  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    lg = fopen("server.log", "a");
    fprintf(lg, "%s", "socket error!\n");
    fclose(lg);
    perror("socket");
    exit(1);
  }
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVER_PORT);
  addr.sin_addr.s_addr = inet_addr(SERVER_ADRESS);
  if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    lg = fopen("server.log", "a");
    fprintf(lg, "%s", "bind error\n");
    fclose(lg);
    perror("bind");
    exit(2);
  }
  listen(listener, 1);
  bool flag = 0;



//Блок обработки входящих сообщений. Распределяет нагрузку между 10 потоками
  while (1) {
    sock = accept(listener, NULL, NULL);

    if (sock < 0) {
      lg = fopen("server.log", "a");
      fprintf(lg, "%s", "accept socket error!\n");
      fclose(lg);
      perror("accept");
      exit(3);
    }
    pthread_create(&thread[i], NULL, thread_func, &sock);
    i += 1;
    i %= 10;
  }

  return 0;
}
