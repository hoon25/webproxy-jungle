/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL){
    // /* 기존*/
    // p = strchr(buf, '&'); // 문자의 첫번째 표시를 찾는다.
    // *p = '\0'; // 포인터를 초기화 위해서  //'\0', 문자열 0과 구별하기 위해 
    // strcpy(arg1, buf);
    // strcpy(arg2, p+1); // 이후 문자열복사
    /* 연습문제 11.10*/
    p = strchr(buf, '='); // 문자의 첫번째 표시를 찾는다.
    strcpy(arg1, p+1);
    p = strchr(p+1, '='); // 문자의 두번째 표시를 찾는다.
    strcpy(arg2, p+1); // 이후 문자열복사
    n1 = atoi(arg1); // atoi 문자열 정수로 정수+문자열이면 문자열이 나오기 전까지의 정수
    n2 = atoi(arg2); 
  }

  /* Make the response body */
  sprintf(content, "QEURY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sThe Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is : %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response*/
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);
  exit(0);
}
/* $end adder */
