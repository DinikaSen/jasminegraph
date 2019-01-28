/**
Copyright 2019 JasmineGraph Team
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


#include <iostream>
#include <unistd.h>
#include "main.h"
#include "src/util/Conts.h"
#include "src/server/JasmineGraphInstance.h"
#include <log4cxx/logger.h>
#include "log4cxx/basicconfigurator.h"
#include "log4cxx/propertyconfigurator.h"
#include "log4cxx/helpers/exception.h"

using namespace log4cxx;
using namespace log4cxx::helpers;

LoggerPtr mainLogger(Logger::getLogger( "main"));

unsigned int microseconds = 10000000;
JasmineGraphServer *server;
JasmineGraphInstance *instance;

void fnExit3(void) {
    delete (server);
    LOG4CXX_INFO(mainLogger, "Shutting down the server...");
    puts("Shutting down the server.");
}


int main(int argc, char *argv[]) {
    atexit(fnExit3);
    PropertyConfigurator::configure(argv[2]);

    if (argc == 1) {
//        std::cout << "Need one argument. Use argument 1 to start JasmineGraph in Master mode. Use any other integer to "
//                  << "start as worker." << std::endl;
        LOG4CXX_INFO(mainLogger, "Need one argument. Use argument 1 to start JasmineGraph in Master mode. "
                                 "Use any other integer to start as worker");
        return -1;
    }


    int mode = atoi(argv[1]);

    if (mode == Conts::JASMINEGRAPH_RUNTIME_PROFILE_MASTER) {
        server = new JasmineGraphServer();
        server->run();

        while (server->isRunning()) {
            usleep(microseconds);
        }

        delete server;
    } else {
        instance = new JasmineGraphInstance();
        instance->start_running();

        while (instance->isRunning()) {
            usleep(microseconds);
        }

        delete instance;
    }

    return 0;
}

