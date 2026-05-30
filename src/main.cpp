#include "web_server.h"

int main() {
    WebServer webServer;

    auto videoList = webServer.getVideoList();
    webServer.setVideoList(videoList);

    webServer.run(8080);
    return 0;
}
