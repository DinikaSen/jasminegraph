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
#include <map>
#include "JasmineGraphServer.h"
#include "../metadb/SQLiteDBInterface.h"
#include "JasmineGraphInstance.h"
#include "../frontend/JasmineGraphFrontEnd.h"
#include "../util/Utils.h"
#include "../partitioner/local/MetisPartitioner.h"

struct workerPorts{
    std::vector<int> workerPortsVector;
    std::vector<int> workerDataPortsVector;
    std::string host;
};

static std::vector<workerPorts> workerDataList;

void *runfrontend(void *dummyPt) {
    JasmineGraphServer *refToServer = (JasmineGraphServer *) dummyPt;
    refToServer->frontend = new JasmineGraphFrontEnd(refToServer->sqlite);
    refToServer->frontend->run();
}

void *runbackend(void *dummyPt) {
    JasmineGraphServer *refToServer = (JasmineGraphServer *) dummyPt;
    refToServer->backend = new JasmineGraphBackend(refToServer->sqlite);
    refToServer->backend->run();
}


JasmineGraphServer::JasmineGraphServer() {

}

JasmineGraphServer::~JasmineGraphServer() {
    puts("Freeing up server resources.");
    sqlite.finalize();
}

int JasmineGraphServer::run() {
    std::cout << "Running the server..." << std::endl;

    this->sqlite = *new SQLiteDBInterface();
    this->sqlite.init();
    init();
    start_workers();
    return 0;
}

bool JasmineGraphServer::isRunning() {
    return true;
}

void JasmineGraphServer::init() {
    Utils utils;
    std::map<string, string> result = utils.getBatchUploadFileList(
            utils.getJasmineGraphProperty("org.jasminegraph.batchupload.file.path"));

    if (result.size() != 0) {
        std::map<std::string, std::string>::iterator iterator1 = result.begin();
        while (iterator1 != result.end()) {
            std::string fileName = iterator1->first;
            std::string filePath = iterator1->second;
            //Next, we need to implement the batch upload logic here.
            iterator1++;
        }
    }

    pthread_t frontendthread;
    pthread_t backendthread;
    pthread_create(&frontendthread, NULL, runfrontend, this);
    pthread_create(&backendthread, NULL, runbackend, this);
}

void JasmineGraphServer::start_workers() {
    Utils utils;
    int hostListModeNWorkers = 0;
    int numberOfWorkersPerHost;
    std::vector<std::string> hostsList = utils.getHostList();
    std::string nWorkers = utils.getJasmineGraphProperty("org.jasminegraph.server.nworkers");
    int workerPort = Conts::JASMINEGRAPH_INSTANCE_PORT;
    int workerDataPort = Conts::JASMINEGRAPH_INSTANCE_DATA_PORT;
    if (utils.is_number(nWorkers)) {
        numberOfWorkers = atoi(nWorkers.c_str());
    } else {
        std::cout<<"Number of Workers Have not Specified."<< std::endl;
        numberOfWorkers = 0;
    }

    if (numberOfWorkers > 0 && hostsList.size() > 0) {
        numberOfWorkersPerHost = hostsList.size()/numberOfWorkers;
        hostListModeNWorkers = hostsList.size() % numberOfWorkers;
    }

    std::vector<std::string>::iterator it;
    it = hostsList.begin();

    for (it = hostsList.begin(); it < hostsList.end(); it++) {
        std::string item = *it;
        int portCount = 0;
        std::vector<int> portVector = workerPortsMap[item];
        std::vector<int> dataPortVector = workerDataPortsMap[item];

        while (portCount < numberOfWorkersPerHost) {
            portVector.push_back(workerPort);
            dataPortVector.push_back(workerDataPort);
            workerPort = workerPort + 2;
            workerDataPort = workerDataPort + 2;
            portCount ++;
        }

        if (hostListModeNWorkers > 0) {
            portVector.push_back(workerPort);
            dataPortVector.push_back(workerDataPort);
            workerPort = workerPort + 2;
            workerDataPort = workerDataPort + 2;
            hostListModeNWorkers--;
        }

        workerPortsMap[item] = portVector;
        workerDataPortsMap[item] = dataPortVector;

    }

    std::vector<std::string>::iterator hostListIterator;
    hostListIterator = hostsList.begin();

    pthread_t threadArray[hostsList.size()];
    int count =0;

    for (hostListIterator = hostsList.begin(); hostListIterator < hostsList.end(); hostListIterator++) {
        std::string host = *hostListIterator;
        std::string result = "";
        char buffer[128];
        workerPorts workerPortsData;
        workerPortsData.workerPortsVector = workerPortsMap[host];
        workerPortsData.workerDataPortsVector = workerDataPortsMap[host];
        workerPortsData.host = host;
        std::cout<<workerPortsData.host<< std::endl;
        workerDataList.push_back(workerPortsData);
        pthread_create(&threadArray[count],NULL,&JasmineGraphServer::startRemoteWorkers,(void *)&workerPortsData);
    }
}

void* JasmineGraphServer::startRemoteWorkers(void *threadData) {
    Utils utils;
    struct workerPorts *workerPortsDataStruct;
    workerPortsDataStruct = (struct workerPorts *) threadData;
    std::vector<int> workerPortsVector = workerPortsDataStruct->workerPortsVector;
    std::vector<int> workerDataPortsVector = workerPortsDataStruct->workerDataPortsVector;
    std::string serverPath = utils.getJasmineGraphProperty("org.jasminegraph.worker.startup.path");
    string host = workerPortsDataStruct->host;
    std::string remoteServerStartScript;

    for (int i =0 ; i < workerPortsVector.size() ; i++) {
        if (host.find("localhost") != std::string::npos) {
            remoteServerStartScript = "sh "+ serverPath + " 2"+" "+ std::to_string(workerPortsVector.at(i)) + " " + std::to_string(workerDataPortsVector.at(i));
        } else {
//            remoteServerStartScript = "ssh -p 22 " + host+ " sh "+ serverPath + " 2"+" "+ std::to_string(workerPortsVector.at(i)) + " " + std::to_string(workerDataPortsVector.at(i));
            remoteServerStartScript = "ssh -p 22 localhost sh " + serverPath ;
        }
        std::cout<< "HOST : " << workerPortsDataStruct->host<< std::endl;
        std::cout<< "COMMAND : " << remoteServerStartScript<< std::endl;
        system(remoteServerStartScript.c_str());
    }
}

void JasmineGraphServer::uploadGraphLocally(std::string graphID) {
    Utils utils;
    std::string nWorkers = utils.getJasmineGraphProperty("org.jasminegraph.server.nworkers");
    std::vector<string> hostsList = utils.getHostList();
    std::vector<string> partitionFileList = MetisPartitioner::getPartitionFiles();
    std::cout << "List size is: " << partitionFileList[0] << std::endl;
    std::cout << "List size is: " << partitionFileList[1] << std::endl;
    std::cout << "List size is: " << partitionFileList[2] << std::endl;
    std::cout << "List size is: " << partitionFileList[3] << std::endl;
}
