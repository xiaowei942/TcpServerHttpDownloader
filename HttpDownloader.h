#ifndef __HTTPDOWNLOADER_H__
#define __HTTPDOWNLOADER_H__

#include <string>
#include <thread>
#include <functional>

class HttpDownloader
{
public:
	typedef enum class HttpDownloadResult {
		HTTP_DOWNLOAD_RESULT_SUCCESS,
		HTTP_DOWNLOAD_RESULT_FAILED,
		HTTP_DOWNLOAD_RESULT_TIMEOUT
	} HttpDownloadResult;

	HttpDownloader() { };
	~HttpDownloader() { };

	std::size_t downloadFileAsync(std::string requestUri, std::string destFilePath, std::function<int (HttpDownloadResult)> func);

private:
	bool stopConnection = false;
	std::shared_ptr<std::thread> acceptThread;
	std::shared_ptr<std::thread> downloadFileThread;

	std::string ip = "127.0.0.1";
	int port = 2345;

	std::string filePath = "";

	int _parseUri(std::string &requestUri, std::string &server, std::string &port, std::string &path);
	int _prepareConnection(std::string server, int port, std::function<int (int)> func);
	int _acceptClient(int fd, std::function<int (int)> func);
	int _acceptClientFunc(int fd, std::function<int (int)> func);
	int _makeDownloadRequest(std::string server, std::string path, std::string& request);
	int _sendDownloadRequest(int fd, std::string server, std::string path);
	int _receiveResponse(int fd);
	int _receiveFile(int fd, std::string path);

	int _requestDownloadFileFunc(int fd, std::string server, std::string path, std::string destFilePath, std::function<int (HttpDownloadResult)>);
};
#endif
