#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "HttpDownloader.h"

#define BUFFER_SIZE 1024

unsigned long long fileTotalSize = 0;
unsigned long long receivedDataSize = 0;

int HttpDownloader::_acceptClientFunc(int fd, std::function<int (int)> func)
{
	std::cout << "Now accepting ~" << std::endl;
	while(!stopConnection) {
		struct sockaddr_in addr;
		socklen_t addrLen = sizeof(addr);

		int clientFd = accept(fd, (struct sockaddr*)&addr, &addrLen);
		if(clientFd == -1) {
			std::cout << "Error code: " << errno << "  [" << strerror(errno) << "]" << std::endl;
			switch(errno) {
				case EAGAIN:
					continue;
				default:
					break;
			}
		}else{

			std::cout << "Client connected ~" << std::endl;

			int on = 1;
			setsockopt(clientFd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));

			if(fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
				close(clientFd);
				func(-1);
			}

			func(clientFd);
			break;
		}
	}

	return 0;
}

int HttpDownloader::_acceptClient(int fd, std::function<int (int)> func)
{
	acceptThread = std::make_shared<std::thread>(&HttpDownloader::_acceptClientFunc, this, fd, func);
	if(acceptThread == nullptr) {
		return -1;
	}

	acceptThread->detach();

	return 0;
}

int HttpDownloader::_prepareConnection(std::string server, int port, std::function<int (int)> func)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd == -1) {
		return -1;
	}

	struct sockaddr_in addr;
	bzero((char *)&addr, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	//addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//addr.sin_addr.s_addr = htonl(address);
	socklen_t socklen = sizeof(addr);

	int on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		close(fd);
		return -1;
	}

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		close(fd);
		return -1;
	}

	if (bind(fd, (struct sockaddr*)&addr, socklen) != 0) {
		close(fd);
		return -1;
	}

	if (listen(fd, 512) != 0) {
		close(fd);
		return -1;
	}

	return _acceptClient(fd, func);
}

int HttpDownloader::_parseUri(std::string &requestUri, std::string &server, std::string &port, std::string &path)
{
	const std::string& http___ = "http://";
	const std::string& https___ = "https://";
	std::string temp_data = requestUri;

	// 截断http协议头
	if (temp_data.find(http___) == 0) {
		temp_data = temp_data.substr(http___.length());
	}
	else if (temp_data.find(https___) == 0) {
		temp_data = temp_data.substr(https___.length());
	}
	else {
		return -3;
	}

	// 解析域名
	std::size_t idx = temp_data.find('/');
	// 解析 域名后的page
	if (std::string::npos == idx) {
		path = "/";
		idx = temp_data.size();
	}
	else {
		path = temp_data.substr(idx);
	}

	// 解析域名
	server = temp_data.substr(0, idx);

	// 解析端口
	idx = server.find(':');
	if (std::string::npos == idx) {
		port = "80";
	}
	else {
		port = server.substr(idx + 1);
		server = server.substr(0, idx);
	}

	return 0;
}

int HttpDownloader::_makeDownloadRequest(std::string server, std::string path, std::string& request)
{
	request.append("GET ");
	request.append(path);
	request.append(" ");
	request.append("HTTP/1.1\r\n");

	request.append("Host: ");
	request.append(server);
	request.append("\r\n");

	request.append("User-Agent: PowerSDK\r\n");
	request.append("Connection: close\r\n");

	request.append("\r\n");

	return 0;
}

int HttpDownloader::_sendDownloadRequest(int fd, std::string server, std::string path)
{
	std::string request;
	int ret = _makeDownloadRequest(server, path, request);
	if(ret != 0) {
		std::cout << "_makeDownloadRequest return failed" << std::endl;
		return -1;
	}

	ret = send(fd, request.c_str(), request.size(), 0);
	if(ret != request.size()) {
		std::cout << "send return failed" << std::endl;
		return -1;
	}

	return 0;
}

int HttpDownloader::_receiveResponse(int fd)
{
	char responseHeader[BUFFER_SIZE];
	memset(responseHeader, 0, BUFFER_SIZE);

	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	//接收响应头
	int receivedBytes = 0;    //已接收字节数
	while (1)
	{
		if (recv(fd, responseHeader+receivedBytes, 1, 0) <= 0)
			continue;
		receivedBytes++;
		if (receivedBytes >= BUFFER_SIZE)
			return false;
		if (receivedBytes >= 4 &&
				responseHeader[receivedBytes-4]=='\r' && responseHeader[receivedBytes-3]=='\n' &&
				responseHeader[receivedBytes-2]=='\r' && responseHeader[receivedBytes-1]=='\n')
			break;
	}
	responseHeader[receivedBytes] = '\0';

	if (strncmp(responseHeader, "HTTP/", 5))
		return -1;
	int status = 0;
	float version = 0.0;
	unsigned long long startPos = 0, endPos, totalLength;
	startPos = endPos = totalLength = 0;
	if (sscanf(responseHeader, "HTTP/%f %d ", &version, &status) != 2)
		return -1;
	char* findStr = strstr(responseHeader, "Content-Length: ");
	if (findStr == NULL)
		return -1;
	if (sscanf(findStr, "Content-Length: %lld", &totalLength) != 1)
		return -1;
	endPos = totalLength;
	if ((status != 200 && status != 206) || totalLength == 0)
		return -1;

	fileTotalSize = totalLength;

	return 0;
}

int HttpDownloader::_receiveFile(int fd, std::string path)
{
	char buf[BUFFER_SIZE] = {0};

	FILE *fp;
	fp = fopen(path.c_str(), "wb");    //创建文件
	if (fp == NULL) {
		return -1;
	}

	int ret;
	while(1)
	{
		int length = recv(fd, buf, BUFFER_SIZE, 0);
		if(length < 0) {
			continue;
		}

		if (length == 0)    //socket被优雅关闭
			ret = 1;
		size_t written = fwrite(buf, sizeof(char), length, fp);
		if(written < length)
			ret = -1;
		receivedDataSize += length;
		if (receivedDataSize == fileTotalSize)    //文件接收完毕
		{
			fclose(fp);
			ret = 0;
			break;
		}
	}

	return ret;
}

int HttpDownloader::_requestDownloadFileFunc(int fd, std::string server, std::string path, std::string destFilePath, std::function<int (HttpDownloadResult)> func)
{
	std::cout << "Now in _requestDownloadFileFunc " << std::endl;

	int ret = _sendDownloadRequest(fd, server, path);
	if(ret != 0) {
		std::cout << "_sendDownloadRequest return failed" << std::endl;
		return ret;
	}

	ret = _receiveResponse(fd);
	if(ret != 0) {
		std::cout << "_receiveResponse return failed" << std::endl;
		return ret;
	}

	ret = _receiveFile(fd, destFilePath);
	if(ret != 0) {
		std::cout << "_receiveFile return failed" << std::endl;
		return ret;
	}

	HttpDownloadResult result = HttpDownloadResult::HTTP_DOWNLOAD_RESULT_SUCCESS;
	func(result);

	return 0;
}

std::size_t HttpDownloader::downloadFileAsync(std::string requestUri, std::string destFilePath, std::function<int (HttpDownloadResult)> func)
{
	std::string server;
	std::string port;
	std::string path;

	int ret = _parseUri(requestUri, server, port, path);
	if(ret != 0) {
		return -1;
	}

	int portNumber = std::atoi(port.c_str());

	assert(portNumber > 0);
	if(portNumber  <= 0) {
		return -1;
	}

	ret = _prepareConnection("127.0.0.1", 8888, [=](int fd) -> int {
			if(fd > 0) {
			std::cout << "Client fd: " << fd << std::endl;

			downloadFileThread = std::make_shared<std::thread>(&HttpDownloader::_requestDownloadFileFunc, this, fd, server, path, destFilePath, func);
		}

		downloadFileThread->detach();
		return 0;
	});
	if(ret != 0) {
		return ret;
	}

	return 0;
}

