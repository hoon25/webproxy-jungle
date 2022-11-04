/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);


int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 듣기 소켓을 오픈한
  while (1) { // 무한 서버 루프 실행
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  // line:netp:tiny:accept  // 반복적으로 연결 요청을 접수
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0); 
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit //트랜잭션 수행
    Close(connfd);  // line:netp:tiny:close
  }
}

// 한 개의 HTTP 트랜잭션을 처리
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers*/
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);

  printf("Request headers :\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")) {// strcasecmp 대소문자 비교하지 않고 문자열 비교
    clienterror(fd, method, "501", "Not implemented", 
      "Tiny does not implement this method");
      return;
  }

  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs); 
  if (stat(filename, &sbuf) < 0) { // 파일 경로를 넘겨주면, 파일의 정보를 얻는다. // 경로가 맞지 않을 경우 
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
    return;
  }

  if (is_static) { /* Serve static content 정적 파일*/
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //파일의 종류 및 접근 권한 // 파일 권한 체크
      clienterror(fd, filename, "403", "Forbidden",
        "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }


  else { /* Serve dynamic content 동적 파일*/
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //파일의 종류 및 접근 권한 // 파일 권한 체크
      clienterror(fd, filename, "403", "Forbidden",
        "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

// clienterror 에러메시지를 클라이언트에게 보냄
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The tiny Web server</em>\r\n", body);

  /* Print the HTTP response*/
  sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// 요청헤더를 읽고 무시한다
// 헤더 내의 어떤 정보도 사용하지 않는다. 
void read_requesthdrs (rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {   // strcmp 문자열 비교 함수
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

// HTTP URI 분석
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if(!strstr(uri, "cgi-bin")){ // Static content // strstr 일치하는 문자열 확인
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/'){
      strcat(filename, "home.html");
    }
    return 1;
  }

  else { /*Dynamic content */
    ptr = index(uri, '?'); 
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else{
      strcpy(cgiargs, "");
    }

    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

// 정적 컨텐츠를 클라이언트에게 서비스
void serve_static (int fd, char *filename, int filesize) 
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client*/
  get_filetype(filename, filetype); // 파일 타입
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers: \n");
  printf("%s", buf);

  /* Send response body to client*/
  srcfd = Open(filename, O_RDONLY, 0); // 읽기 전용
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //가상메모리영억으로 매핑 
  Close(srcfd); // 파일 닫기
  Rio_writen(fd, srcp, filesize);  // 실제로 파일을 클라이언트에게 전송
  Munmap(srcp, filesize); // 가상메모리 반환
}

/*
* get_filetype - Derive file type from filename
*/
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html")){
    strcpy(filetype, "text/html");
  }
  else if (strstr(filename, ".gif")){
    strcpy(filetype, "image/gif");
  }
  else if (strstr(filename, ".png")){
    strcpy(filetype, "image/png");
  }
  else if (strstr(filename, ".jpg")){
    strcpy(filetype, "image/jpeg");
  }
  else{
    strcpy(filetype, "text/plain");
  }
}


// 동적콘텐츠를 클라이언트에게 제공
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response*/
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  if (Fork() == 0){ /*Child*/ // 자식 프로세스 생성(부모 메모리 그대로 복사해서 가져옴)후 자식프로세스에서 CGI실행
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); // cgi관련 환경변수 초기화
    Dup2(fd, STDOUT_FILENO); //자식의 표준출력을 연결 파일 식별자로 재지정
    Execve(filename, emptylist, environ); // cgi 로드 후 실행
  }
  Wait(NULL); // 자식이 종료되어 정리되는 것을 기다리기 위해 wait함수에서 블록 

}


