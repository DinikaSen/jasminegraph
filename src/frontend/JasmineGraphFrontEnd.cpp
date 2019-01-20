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

#include <sstream>
#include "JasmineGraphFrontEnd.h"
#include "../util/Conts.h"
#include "../util/Utils.h"
#include "JasmineGraphFrontEndProtocol.h"
#include "../metadb/SQLiteDBInterface.h"
#include <log4cxx/logger.h>
#include "log4cxx/helpers/exception.h"

using namespace log4cxx;
using namespace log4cxx::helpers;

LoggerPtr frontEndLogger(Logger::getLogger( "JasmineGraphFrontEnd"));

using namespace std;

Utils utils;
static int connFd;
static bool IS_DISTRIBUTED = Utils::parseBoolean(utils.getJasmineGraphProperty("org.jasminegraph.server.mode.isdistributed"));

void *task1(void *dummyPt)
{
    //cout << "Thread No: " << pthread_self() << endl;
    LOG4CXX_INFO(frontEndLogger, "Thread no : " << pthread_self());
    char data[300];
    bzero(data, 301);
    bool loop = false;
    while(!loop)
    {
        bzero(data, 301);
        read(connFd, data, 300);

        string line (data);
        //cout << line << endl;

        line = Utils::trim_copy(line, " \f\n\r\t\v");

        if(line.compare(EXIT) == 0)
        {
            LOG4CXX_INFO(frontEndLogger, "Command received : " << EXIT);
            break;
        }
        else if (line.compare(LIST) == 0)
        {
            LOG4CXX_INFO(frontEndLogger, "Command received : " << LIST);
            SQLiteDBInterface* sqlite = (SQLiteDBInterface*) dummyPt;
            std::vector<vector<pair<string,string>>> v = sqlite->runSelect("SELECT idgraph, name, upload_path, upload_start_time, upload_end_time, "
                                                                           "graph_status_idgraph_status, vertexcount, centralpartitioncount, edgecount FROM graph;");
            for (std::vector<vector<pair<string,string>>>::iterator i = v.begin(); i != v.end(); ++i) {
                for (std::vector<pair<string,string>>::iterator j = (i->begin()); j != i->end(); ++j) {
                    string dataString = " " + j->first + " = " + j->second + "\n";
                    write(connFd, dataString.c_str(),dataString.size());
                    //std::cout << "  " << j->first << " = " << j->second << std::endl;
                }
                string newLine = "\n";
                write(connFd,newLine.c_str() , 2);
                //std::cout << "\n" << endl;
            }
        }
        else if (line.compare(SHTDN) == 0)
        {
            LOG4CXX_INFO(frontEndLogger, "Command received : " << SHTDN);
            close(connFd);
            exit(0);
        }
        // Add graph from outside
        else if (line.compare(ADGR) == 0)
        {
            LOG4CXX_INFO(frontEndLogger, "Command received : " << ADGR);
            write(connFd, (SEND + '\n').c_str(), SEND.size()+1);
            // We get the name and the path to graph as a pair separated by |.
            char graph_data[300];
            bzero(graph_data, 301);
            string name = "";
            string path = "";

            read(connFd, graph_data, 300);
            string gData (graph_data);
            LOG4CXX_INFO(frontEndLogger, "Data received : " << gData);
            gData = Utils::trim_copy(gData, " \f\n\r\t\v");

            std::vector<std::string> strArr = Utils::split(gData, '|');

            if(strArr.size() != 2){
                string message = ERROR + ":Message format not recognized\n";
                write(connFd, message.c_str(), message.size());
                LOG4CXX_ERROR(frontEndLogger, "Message format not recognized");
            }

            name = strArr[0];
            path = strArr[1];

            if(JasmineGraphFrontEnd::graphExists(path, dummyPt)){
                string message = ERROR + ":Graph already exists\n";
                write(connFd, message.c_str(), message.size());
                LOG4CXX_ERROR(frontEndLogger, "Graph already exists");
            }

            if(JasmineGraphFrontEnd::fileExists(path)){
                std::cout << "Path exists" << endl;
                // TODO : Add this as a debug log
                if(IS_DISTRIBUTED){
                    // TODO :: Upload distributed graph
                }
                else{
                    // TODO :: Upload graph locally
                }
            }else{
                string message = ERROR + ":Graph data file does not exist on the specified path\n";
                write(connFd, message.c_str(), message.size());
                LOG4CXX_ERROR(frontEndLogger, "Graph data file does not exist on the specified path");
            }
        }
        // Remove graph from JasmineGraph
        else if (line.compare(RMGR) == 0)
        {
            LOG4CXX_INFO(frontEndLogger, "Command received : " << RMGR);
            write(connFd, (SEND+'\n').c_str(), SEND.size()+1);
            // Get the graph id as a user input.
            char graphID[20];
            bzero(graphID, 21);

            read(connFd, graphID, 21);
            string graph_id (graphID);
            graph_id = Utils::trim_copy(graph_id, " \f\n\r\t\v");

            if(!JasmineGraphFrontEnd::graphExistsByID(graph_id, dummyPt)){
                string message = ERROR + ":The specified graph id does not exist\n";
                write(connFd, message.c_str(), message.size());
                LOG4CXX_ERROR(frontEndLogger, "The specified graph id does not exist");
            }
            else{
                // TODO :: Remove graph
            };
        }
        else
        {
            string message = ERROR + ":Message format not recognized\n";
            write(connFd, message.c_str(), message.size());
            LOG4CXX_ERROR(frontEndLogger, "Message format not recognized");
        }
    }
    //cout << "\nClosing thread " << pthread_self() << " and connection" << endl;
    LOG4CXX_INFO(frontEndLogger, "Closing thread" << pthread_self() << " and connection");
    close(connFd);
}

JasmineGraphFrontEnd::JasmineGraphFrontEnd(SQLiteDBInterface db)
{
    this->sqlite = db;
}

int JasmineGraphFrontEnd::run() {
    int pId;
    int portNo = Conts::JASMINEGRAPH_FRONTEND_PORT;;
    int listenFd;
    socklen_t len;
    bool loop = false;
    struct sockaddr_in svrAdd;
    struct sockaddr_in clntAdd;

    pthread_t threadA[3];

    //create socket
    listenFd = socket(AF_INET, SOCK_STREAM, 0);

    if(listenFd < 0)
    {
        cerr << "Cannot open socket" << endl;
        LOG4CXX_ERROR(frontEndLogger, "Cannot open socket");
        return 0;
    }

    bzero((char*) &svrAdd, sizeof(svrAdd));

    svrAdd.sin_family = AF_INET;
    svrAdd.sin_addr.s_addr = INADDR_ANY;
    svrAdd.sin_port = htons(portNo);

    int yes=1;

    if (setsockopt(listenFd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
        perror("setsockopt");
        exit(1);
    }


    //bind socket
    if(bind(listenFd, (struct sockaddr *) &svrAdd, sizeof(svrAdd)) < 0)
    {
        cerr << "Cannot bind" << endl;
        LOG4CXX_ERROR(frontEndLogger, "Cannot bind");
        return 0;
    }

    listen(listenFd, 5);

    len = sizeof(clntAdd);

    int noThread = 0;

    while (noThread < 3)
    {
        //cout << "Listening" << endl;
        LOG4CXX_INFO(frontEndLogger, "Listening..." );
        //this is where client connects. svr will hang in this mode until client conn
        connFd = accept(listenFd, (struct sockaddr *)&clntAdd, &len);

        if (connFd < 0)
        {
            cerr << "Cannot accept connection" << endl;
            LOG4CXX_ERROR(frontEndLogger, "Cannot accept connection");
            return 0;
        }
        else
        {
            //cout << "Connection successful" << endl;
            LOG4CXX_INFO(frontEndLogger, "Connection successful" );
        }

        struct frontendservicesessionargs frontendservicesessionargs1;
        frontendservicesessionargs1.sqlite = this->sqlite;
        frontendservicesessionargs1.connFd = connFd;


        pthread_create(&threadA[noThread], NULL, task1, &frontendservicesessionargs1);

        noThread++;
    }

    for(int i = 0; i < 3; i++)
    {
        pthread_join(threadA[i], NULL);
    }


}

/**
 * This method checks if a graph exists in JasmineGraph.
 * This method uses the unique path of the graph.
 * @param basic_string
 * @param dummyPt
 * @return
 */
  bool JasmineGraphFrontEnd::graphExists(string path, void *dummyPt) {
     bool result = true;
     string stmt = "SELECT COUNT( * ) FROM graph WHERE upload_path LIKE '" + path + "';";
     SQLiteDBInterface *sqlite = (SQLiteDBInterface *) dummyPt;
     std::vector<vector<pair<string, string>>> v = sqlite->runSelect(stmt);
     int count = std::stoi(v[0][0].second);
     //std::cout << "No of columns  : " << count << endl;
     if (count == 0) {
         result = false;
     }
     return result;
 }

 /**
  * This method checks if a graph exists in JasmineGraph with the same unique ID.
  * @param id
  * @param dummyPt
  * @return
  */
bool JasmineGraphFrontEnd::graphExistsByID(string id, void *dummyPt) {
    bool result = true;
    string stmt = "SELECT COUNT( * ) FROM graph WHERE idgraph LIKE '" + id + "';";
    SQLiteDBInterface *sqlite = (SQLiteDBInterface *) dummyPt;
    std::vector<vector<pair<string, string>>> v = sqlite->runSelect(stmt);
    int count = std::stoi(v[0][0].second);
    //std::cout << "No of columns  : " << count << endl;
    if (count == 0) {
        result = false;
    }
    return result;
}

/**
 * This method checks if a file with the given path exists.
 * @param fileName
 * @return
 */
bool JasmineGraphFrontEnd::fileExists(const string fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

