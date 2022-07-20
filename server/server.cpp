#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>


#define SERVER_ADRESS "127.0.0.1"
#define SERVER_PORT 5002
#define MESSAGE_MEMORY 1024

int Daemon(int, char **);//Основная функция демона-сервера
void *thread_func(void *arg); //функция-поток, в которой считывается информация с сокета
void toLog(std::string); //функция для логирования
void handler(int); //обработчик сигналов (для sigterm и sighup)
void wait4children(int); //дополнительный обработчик для уничножения зомби-процессов

pthread_t thread[10];
int listener;

int main(int argc, char *argv[]) {

  signal(SIGCHLD, wait4children);

  int i;
  pid_t pid;

  signal(SIGHUP, handler);
  signal(SIGTERM, handler);

  //Начало демонизации
  pid = fork();
  if (pid < 0) {
    toLog("ERROR: fork error!");
    perror("fork");
    exit(1);
  } else if (pid > 0) {
    toLog("try to make a daemond");
    exit(0);
  }
  setsid();

  char szPath[MESSAGE_MEMORY];
  if (getcwd(szPath, sizeof(szPath)) == NULL) {
    toLog("ERROR: dir error!");
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

int Daemon(int argc, char *argv[]) {

  int i = 0;
  int sock;
  struct sockaddr_in addr;

  toLog("Daemon server started.");
  listener = socket(AF_INET, SOCK_STREAM, 0);
  if (listener < 0) {
    toLog("ERROR: socket error!");
    perror("socket");
    exit(1);
  }
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVER_PORT);
  addr.sin_addr.s_addr = inet_addr(SERVER_ADRESS);
  if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {

    toLog("ERROR: bind error!");
    perror("bind");
    exit(2);
  }
  listen(listener, 1);
  bool flag = 0;

  //Блок обработки входящих сообщений. Распределяет нагрузку между 10 потоками
  while (1) {
    sock = accept(listener, NULL, NULL);

    if (sock < 0) {

      toLog("ERROR: accept socket error!");
      perror("accept");
      exit(3);
    }
    pthread_create(&thread[i], NULL, thread_func, &sock);
    i += 1;
    i %= 10;
  }

  return 0;
}

void *thread_func(void *arg) {
  //Поток, в котором обрабатываются сообщения определенного сокета
  int bytes_read;
  char buf[MESSAGE_MEMORY];
  FILE *fp;
  int sockid = *(int *)arg;

  bytes_read = recv(sockid, buf, MESSAGE_MEMORY, 0); //получаем название файла

  if (bytes_read <= 0) {
    close(sockid);
    return arg;
  }
  std::string str;
  str += "./files/";
  str += buf;
  fp = fopen(str.c_str(), "w");

  toLog(std::to_string(sockid) + " socket is active. accepting " + buf +
        " file");
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
  toLog("file accepted. socket " + std::to_string(sockid) + "is free.");
  close(sockid);
  return arg;
}

void toLog(std::string message) {

  FILE *lg;
  lg = fopen("server.log", "a");
  time_t seconds = time(NULL);
  tm *timeinfo = localtime(&seconds);
  fprintf(lg, "%s", asctime(timeinfo));
  fprintf(lg, "%s", message.c_str());
  fprintf(lg, "%s", "\n");
  fclose(lg);
}

void handler(int i) {
  switch (i) {
  case SIGTERM: {

    toLog("SIGTERM accepted");
    close(listener);
    for (int i = 0; i < 10; i++)
      pthread_join(thread[i], NULL);
    exit(EXIT_SUCCESS);
  }
  case SIGHUP: {

    toLog("SIGHUP accepted");
    for (int i = 0; i < 10; i++)
      pthread_join(thread[i], NULL);
    exit(EXIT_SUCCESS);
  }
  default: {
    toLog("signal " + std::to_string(i) + "accepted");
    exit(i);
  }
  }
}

void wait4children(int signo) {
  int status;
  while (waitpid(-1, &status, WNOHANG) > 0)
    ;
}