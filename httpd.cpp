/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
//��Ϊ��strcat �Ⱥ��� ��Ҫʹ�� _CRT_SECURE_NO_WARNINGS Ԥ�����Ӵ��澯

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <thread>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

//windows socket headers 
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
 // Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
 // #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
using namespace std;
void accept_request(void *);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/


/************************************************************************/
/*                                                                      */
/************************************************************************/
#ifdef _MSC_VER 
//not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

void accept_request(int client)
{
    char buf[1024];
    size_t numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct _stat64 st;
    int cgi = 0;      /* becomes true if server decides this is a CGI
                       * program */
    char *query_string = NULL;

	//���յ�һ������ ��һ�� "GET / HTTP/1.1\n"
	//���� "GET /index.html HTTP/1.1\n"
    numchars = get_line(client, buf, sizeof(buf));
	
    i = 0; j = 0;
	//��ȡ�ǿո�ĵ�һ�� ��Ϊ��һ�ж��� get xxx.com post xxx.com ��������ô�õ�
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j=i;//��¼����
    method[i] = '\0';

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        return;
    }
	//post����cgi�ű���
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
	//ȥ���ո�
    while (ISspace(buf[j]) && (j < numchars))
        j++;
	//��ȡurl
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
		//?�����ʾ���ŵĲ��� \0���ǽ�����
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?')
        {
            cgi = 1;
			//emmm ������Ҫ�ˡ���
            *query_string = '\0';
            query_string++;
        }
    }
	//�����ļ�����
    sprintf(path, "htdocs%s", url);
	//����index �͸����Ӹ�index �����index����html Ҳ������
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");

	//������ļ�ϵͳ״̬�ĺ��� �����Դ���������ж��Ƿ����ĳ���ļ� �ļ��Ƿ����ִ�еȵ�  Ŀ������ν posix ��׼��? ��sysĿ¼�£��������п��ܰ�
    if (_stat64(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else
    {
		//�������ӵ���htdocs ���ټӸ�index
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");

		//���ʲô�� ��֮�ǿ�ִ�г��򣬾͵���cgi ������?
//         if ((st.st_mode & _S_IXUSR) ||
//                 (st.st_mode & S_IXGRP) ||
//                 (st.st_mode & S_IXOTH) )
//             cgi = 1;
		
        if (!cgi)
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string);
    }

    closesocket(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);
	getchar();
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
	const char *method, const char *query_string)
{
	char buf[1024];
	int cgi_output[2];
	int cgi_input[2];
	int pid;
	int status;
	int i;
	char c;
	int numchars = 1;
	int content_length = -1;
	//��һ�� ������ ������Ϊ�ж���׼
	buf[0] = 'A'; buf[1] = '\0';
	if (strcasecmp(method, "GET") == 0)
		while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
			numchars = get_line(client, buf, sizeof(buf));
	else if (strcasecmp(method, "POST") == 0) /*POST*/
	{
		//��Ϊget�Ļ� ֻ��header?
		//post�Ļ�header���������content-length ��Ҫ��ȡ����
		numchars = get_line(client, buf, sizeof(buf));
		while ((numchars > 0) && strcmp("\n", buf))
		{
			buf[15] = '\0';
			if (strcasecmp(buf, "Content-Length:") == 0)
				content_length = atoi(&(buf[16]));
			numchars = get_line(client, buf, sizeof(buf));
		}
		//post û�л�ȡ��content-len ����һ����������
		if (content_length == -1) {
			bad_request(client);
			return;
		}
	}
	else/*HEAD or other*/
	{
	}

	//���������ܵ������ڽ��̼�ͨ��
// 	if (pipe(cgi_output) < 0) {
// 		cannot_execute(client);
// 		return;
// 	}
// 	if (pipe(cgi_input) < 0) {
// 		cannot_execute(client);
// 		return;
// 	}
// 
// 	if ((pid = fork()) < 0) {
// 		cannot_execute(client);
// 		return;
// 	}
// 	sprintf(buf, "HTTP/1.0 200 OK\r\n");
// 	send(client, buf, strlen(buf), 0);
// 	if (pid == 0)  /* child: CGI script */
// 	{
// 		char meth_env[255];
// 		char query_env[255];
// 		char length_env[255];
//		//�ض���stdout �ض���output������  stdout��ԱԴ������������뵽output����ȥ������ӡ����Ļ�ˣ�
// 		dup2(cgi_output[1], STDOUT);
//		//��stdin �ض���input�������  ��˼�� ���̵�������Ч�� ��input�����������Ϊ����Դ��
// 		dup2(cgi_input[0], STDIN);
// 		close(cgi_output[0]);
// 		close(cgi_input[1]);
//		���û������� ��CGI�ű����� CGI�ű������ڽű����� ����ű����߿�����python Ҳ������perl ��Ҫ�����е��﷨��ʽ
// 		sprintf(meth_env, "REQUEST_METHOD=%s", method);
// 		putenv(meth_env);
//		get���û�������Ϊ query_string ����linux ��putenvֻ��Ե�ǰ���̣����Կ�����ô�ã�windows�Ŀɲ��ǡ��� Ҫ�ĵĻ����Ըĳ�д���ļ���
// 		if (strcasecmp(method, "GET") == 0) {
// 			sprintf(query_env, "QUERY_STRING=%s", query_string);
// 			putenv(query_env);
// 		}
// 		else {   /* POST */
// 			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
// 			putenv(length_env);
// 		}
//		//ִ�нű� ���н������˳��ֽ���  �����ݱ����͵�stdout ����output���� ����ͨ��output��[0]���ж�ȡ
// 		execl(path, NULL);
// 		exit(0);
// 	}
// 	else {    /* parent */
// 		close(cgi_output[1]);
// 		close(cgi_input[0]);
// 		if (strcasecmp(method, "POST") == 0)
// 			for (i = 0; i < content_length; i++) {
//				//�����post ��ȡ������ �����뵽 �������뵽std::in�� �ֽ��п����Դ���Ϊ��������
// 				recv(client, &c, 1, 0);
// 				write(cgi_input[1], &c, 1);
// 			}
//		//Ȼ����߾�һֱ��ȡoutput�ܵ������ݾ����ˣ����˾ͷ��ͣ�ֱ��û�����ݣ����û�г�ʱ���ǵȳ�ʱ�𣿻��߻�����Ĭ���г�ʱ��
// 		while (read(cgi_output[0], &c, 1) > 0)
// 			send(client, &c, 1, 0);
// 
// 		close(cgi_output[0]);
// 		close(cgi_input[1]);
//		//���ȴ��߳̽���
// 		waitpid(pid, &status, 0);
// 	}
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
				//����һֱpeek?
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{

	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}



    int httpd = 0;
    int on = 1;
    struct sockaddr_in name;

    httpd = socket(AF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on))) < 0)  
    {  
        error_die("setsockopt failed");
    }
	
    if (::bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    if (*port == 0)  /* if dynamically allocating a port */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{
	//ûʵ�� header
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
	//ͷ�����Ȼû�г�����
	//���ҵ�˼·�Ļ� �������ȵ���string �Ȼ��һ��
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;
    u_short port = 4000;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t  client_name_len = sizeof(client_name);
    std::thread* newthread;

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);

    while (1)
    {
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        /* accept_request(&client_sock); */
// 		if (pthread_create(&newthread, NULL, (void *)accept_request, (void *)(intptr_t)client_sock) != 0)
// 			perror("pthread_create");
		//Ϊʲô����һֱ����? google�е���ְ�
		newthread = new std::thread([&] {accept_request(client_sock); });
		
    }

    closesocket(server_sock);
	WSACleanup();
    return(0);
}
