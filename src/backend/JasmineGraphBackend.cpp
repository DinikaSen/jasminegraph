/**
Copyright 2018 JasminGraph Team
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
 */

#include "JasmineGraphBackend.h"
#include "../util/Utils.h"
#include "../util/Conts.h"
#include "JasmineGraphBackendProtocol.h"
#include "spdlog/spdlog.h"

using namespace std;

Utils utils;
thread_local int connFd;
//static int connFd;

void *backendservicesesion(void *dummyPt) {
    backendservicesessionargs *sessionargs = (backendservicesessionargs *) dummyPt;
    cout << "Thread No: " << pthread_self() << endl;
    char data[300];
    bzero(data, 301);
    bool loop = false;
    while (!loop) {
        bzero(data, 301);
        read(sessionargs->connFd, data, 300);

        string line(data);
        cout << line << endl;

        line = utils.trim_copy(line, " \f\n\r\t\v");

        if (line.compare(EXIT_BACKEND) == 0) {
            write(sessionargs->connFd, EXIT_ACK.c_str(), EXIT_ACK.size());
            write(sessionargs->connFd, "\r\n", 2);
            break;
        } else if (line.compare(HANDSHAKE) == 0) {
            write(sessionargs->connFd, HANDSHAKE_OK.c_str(), HANDSHAKE_OK.size());
            write(sessionargs->connFd, "\r\n", 2);

            char host[300];
            bzero(host, 301);
            read(sessionargs->connFd, host, 300);
            string hostname(host);
            Utils utils;
            hostname = utils.trim_copy(hostname, " \f\n\r\t\v");
            std::cout << "Hostname of the worker : " << hostname << endl;

        }
        else {
            std::cout << ERROR << ":Message format not recognized" << endl;
        }
    }
    cout << "\nClosing thread " << pthread_self() << " and connection" << endl;
    close(sessionargs->connFd);
}

JasmineGraphBackend::JasmineGraphBackend(SQLiteDBInterface db) {
    this->sqlite = db;
}

int JasmineGraphBackend::run() {
    int pId;
    int portNo = Conts::JASMINEGRAPH_BACKEND_PORT;
    int listenFd;
    socklen_t len;
    bool loop = false;
    struct sockaddr_in svrAdd;
    struct sockaddr_in clntAdd;

    //TODO: This seems there is only 3 back end instances can be kept running once. Need to double check this.
    pthread_t threadA[3];

    //create socket
    listenFd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenFd < 0) {
        cerr << "Cannot open socket" << endl;
        return 0;
    }

    bzero((char *) &svrAdd, sizeof(svrAdd));

    svrAdd.sin_family = AF_INET;
    svrAdd.sin_addr.s_addr = INADDR_ANY;
    svrAdd.sin_port = htons(portNo);

    int yes = 1;

    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
        perror("setsockopt");
        exit(1);
    }


    //bind socket
    if (bind(listenFd, (struct sockaddr *) &svrAdd, sizeof(svrAdd)) < 0) {
        cerr << "Cannot bind" << endl;
        return 0;
    }

    listen(listenFd, 5);

    len = sizeof(clntAdd);

    int noThread = 0;

    while (noThread < 3) {
        spdlog::info("Listening...");
        //cout << "Listening" << endl;

        //this is where client connects. svr will hang in this mode until client conn
        connFd = accept(listenFd, (struct sockaddr *) &clntAdd, &len);

        if (connFd < 0) {
            cerr << "Cannot accept connection" << endl;
            return 0;
        } else {
            cout << "Connection successful" << endl;
        }

        struct backendservicesessionargs backendservicesessionargs1;
        backendservicesessionargs1.sqlite = this->sqlite;
        backendservicesessionargs1.connFd = connFd;


        pthread_create(&threadA[noThread], NULL, backendservicesesion,
                       &backendservicesessionargs1);

        noThread++;
    }

    for (int i = 0; i < 3; i++) {
        pthread_join(threadA[i], NULL);
    }


}
