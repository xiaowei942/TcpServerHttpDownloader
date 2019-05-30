#include <memory>
#include <iostream>
#include "HttpDownloader.h"

bool isExit = false;

int main()
{
	int port = 2345;
	int clientFd = -1;
	std::string srcFilePath = "http://111.222.333.444:8080/zzz.txt";
	std::string destFilePath = "/build/http2/zzz.txt";

	static std::unique_ptr<HttpDownloader> downloader(new HttpDownloader());

	downloader->downloadFileAsync(srcFilePath, destFilePath, [](HttpDownloader::HttpDownloadResult result) -> int {
		if(result == HttpDownloader::HttpDownloadResult::HTTP_DOWNLOAD_RESULT_SUCCESS) {
			isExit = true;
			std::cout << "Download file success !" << std::endl;
		}

		return 0;
	});

	while(!isExit) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::cout << "Now exit" << std::endl;
	return 0;
}
