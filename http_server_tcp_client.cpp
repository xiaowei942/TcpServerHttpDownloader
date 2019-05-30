#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <thread>
#include <sstream>

#define MYPORT  8888
#define BUFFER_SIZE 1024
char* SERVER_IP = "127.0.0.1";

int main()
{
	///定义sockfd
	int sock_cli = socket(AF_INET,SOCK_STREAM, 0);

	///定义sockaddr_in
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(MYPORT);  ///服务器端口
	servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);  ///服务器ip

	printf("连接%s:%d\n",SERVER_IP,MYPORT);
	///连接服务器，成功返回0，错误返回-1
	if (connect(sock_cli, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("connect");
		exit(1);
	}
	printf("服务器连接成功\n");
	char sendbuf[BUFFER_SIZE];
	char recvbuf[BUFFER_SIZE];

	memset(sendbuf, 0, sizeof(sendbuf));
	memset(recvbuf, 0, sizeof(recvbuf));

	recv(sock_cli, recvbuf, sizeof(recvbuf),0); ///接收
	printf("从服务器接收数据：\n%s\n",recvbuf);

	FILE *fp;
	//fp = fopen("history.txt", "r");
	fp = fopen("HttpDownloader.cpp", "r");

	fseek(fp, 0L, SEEK_END);

	int size = ftell(fp);
	printf("文件大小: %d bytes\n", size);

	char *buf_file = (char *)malloc(size);
	memset(buf_file, 0, size);

	fseek(fp, 0L, SEEK_SET);


	int len = fread(buf_file, 1, size, fp);
	if(len != size) {
		printf("读取文件失败\n");
		fclose(fp);
	}

	fclose(fp);

	std::stringstream ss;
	ss << size;

	std::string response;
	response.append("HTTP/1.1 200 OK\r\n");
	response.append("Content-Length: ");
	response.append(ss.str() + "\r\n");
	response.append("Content-Type: application/octet-stream\r\n");
	response.append("Last-Modified: Wed, 10 Oct 2005 00:56:34 GMT\r\n");
	response.append("Accept-Ranges: bytes\r\n");
	response.append("Server: Microsoft-IIS/6.0\r\n");
	response.append("X-Powered-By: ASP.NET\r\n");
	response.append("Date: Wed, 16 Nov 2005 01:57:54 GMT\r\n");
	response.append("Connection: close\r\n");
	response.append("\r\n");

	printf("发送响应 ~\n");
	len = send(sock_cli, response.c_str(), response.size(), 0); ///发送
	if(len != response.size()) {
		printf("发送响应失败\n");
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	int sent = 0;
	do {
		int last = size-sent;
		if(last>1024) {
			len = send(sock_cli, buf_file+sent, 1024, 0);
		}else{
			len = send(sock_cli, buf_file+sent, last, 0);
		}

		if(len > 0) {
			sent += len;
		}

		printf("Sent %d bytes\n", sent);
		if(sent == size) {
			printf("Sent %d bytes total\n", sent);
			break;
		}
	}while(sent<size);

	close(sock_cli);
	return 0;
}
