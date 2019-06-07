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

#include <flatbuffers/flatbuffers.h>
#include "MetisPartitioner.h"
#include "../../util/Conts.h"
#include "../../util/logger/Logger.h"

Logger partitioner_logger;
std::mutex partFileMutex;
std::mutex masterFileMutex;
std::mutex partAttrFileMutex;
std::mutex masterAttrFileMutex;


MetisPartitioner::MetisPartitioner(SQLiteDBInterface *sqlite) {
    this->sqlite = *sqlite;
}

void MetisPartitioner::loadDataSet(string inputFilePath, int graphID) {
    partitioner_logger.log("Processing dataset for partitioning", "info");
    const clock_t begin = clock();
    this->graphID = graphID;
    // Output directory is created under the users home directory '~/.jasminegraph/tmp/'
    this->outputFilePath = utils.getHomeDir() + "/.jasminegraph/tmp/" + std::to_string(this->graphID);

    // Have to call createDirectory twice since it does not support recursive directory creation. Could use boost::filesystem for path creation
    this->utils.createDirectory(utils.getHomeDir() + "/.jasminegraph/");
    this->utils.createDirectory(utils.getHomeDir() + "/.jasminegraph/tmp");
    this->utils.createDirectory(this->outputFilePath);

    std::ifstream dbFile;
    dbFile.open(inputFilePath, std::ios::binary | std::ios::in);

    int firstVertex = -1;
    int secondVertex = -1;
    string line;
    char splitter;

    std::getline(dbFile, line);

    if (!line.empty()) {
        if (line.find(" ") != std::string::npos) {
            splitter = ' ';
        } else if (line.find('\t') != std::string::npos) {
            splitter = '\t';
        } else if (line.find(",") != std::string::npos) {
            splitter = ',';
        }
    }

    while (!line.empty()) {
        string vertexOne;
        string vertexTwo;

        std::istringstream stream(line);
        std::getline(stream, vertexOne, splitter);
        stream >> vertexTwo;

        firstVertex = std::stoi(vertexOne);
        secondVertex = std::stoi(vertexTwo);

        if (!zeroflag) {
            if (firstVertex == 0 || secondVertex == 0) {
                zeroflag = true;
                partitioner_logger.log("Graph has zero vertex", "info");
            }
        }

        std::vector<int> firstEdgeSet = graphStorageMap[firstVertex];
        std::vector<int> vertexEdgeSet = graphEdgeMap[firstVertex];

        if (firstEdgeSet.empty()) {
            vertexCount++;
            edgeCount++;
            edgeCountForMetis++;
            firstEdgeSet.push_back(secondVertex);
            vertexEdgeSet.push_back(secondVertex);

        } else {
            if (std::find(firstEdgeSet.begin(), firstEdgeSet.end(), secondVertex) == firstEdgeSet.end()) {
                firstEdgeSet.push_back(secondVertex);
                edgeCountForMetis++;
            }
            vertexEdgeSet.push_back(secondVertex);
            edgeCount++;
        }

        graphStorageMap[firstVertex] = firstEdgeSet;
        graphEdgeMap[firstVertex] = vertexEdgeSet;

        std::vector<int> secondEdgeSet = graphStorageMap[secondVertex];

        if (secondEdgeSet.empty()) {
            vertexCount++;
            secondEdgeSet.push_back(firstVertex);

        } else {
            if (std::find(secondEdgeSet.begin(), secondEdgeSet.end(), firstVertex) == secondEdgeSet.end()) {
                secondEdgeSet.push_back(firstVertex);
            }
        }

        graphStorageMap[secondVertex] = secondEdgeSet;

        if (firstVertex > largestVertex) {
            largestVertex = firstVertex;
        }

        if (secondVertex > largestVertex) {
            largestVertex = secondVertex;
        }

        if (firstVertex < smallestVertex) {
            smallestVertex = firstVertex;
        }

        if (secondVertex < smallestVertex) {
            smallestVertex = secondVertex;
        }

        std::getline(dbFile, line);
        while (!line.empty() && line.find_first_not_of(splitter) == std::string::npos) {
            std::getline(dbFile, line);
        }
    }

    float time =  float( clock () - begin ) /  CLOCKS_PER_SEC;
    partitioner_logger.log("Processing dataset completed in " + to_string(time) + " seconds", "info");
    cout << "Total vertex count : " << vertexCount << endl;
    cout << "Total edge count : " << edgeCount << endl;
    cout << "Largest vertex : " << largestVertex << endl;
    cout << "Smallest vertex : " << smallestVertex << endl;

}

int MetisPartitioner::constructMetisFormat(string graph_type) {
    partitioner_logger.log("Constructing metis input format", "info");
    const clock_t begin = clock();
    graphType = graph_type;
    int adjacencyIndex = 0;
    std::ofstream outputFile;
    string outputFileName = this->outputFilePath + "/grf";
    outputFile.open(outputFileName);

    outputFile << (vertexCount) << ' ' << (edgeCountForMetis) << std::endl;

    xadj.push_back(adjacencyIndex);

    for (int vertexNum = 0; vertexNum <= largestVertex; vertexNum++) {
        std::vector<int> vertexSet = graphStorageMap[vertexNum];

        if (vertexNum > smallestVertex && vertexSet.empty()) {
            partitioner_logger.log("Vertex list is not sequential. Reformatting vertex list", "info");
            vertexCount = 0;
            edgeCount = 0;
            edgeCountForMetis = 0;
            graphEdgeMap.clear();
            graphStorageMap.clear();
            smallestVertex = std::numeric_limits<int>::max();
            largestVertex = 0;
            zeroflag = false;
            return 0;
        }

        std::sort(vertexSet.begin(), vertexSet.end());

        //To handle situations where a blank line gets printed because vertexSet of zero vertex is zero in graphs with no zero vertex
        if (!zeroflag && vertexNum == 0) {
            continue;
        }

        for (std::vector<int>::const_iterator i = vertexSet.begin(); i != vertexSet.end(); ++i) {
            //To handle zero vertex
            if (zeroflag) {
                //To handle self loops
                if (vertexNum == *i) {
                    outputFile << (*i + 1) << ' ' << (*i + 1) << ' ';
                } else {
                    outputFile << (*i + 1) << ' ';
                }
            } else {
                //To handle self loops
                if (vertexNum == *i) {
                    outputFile << (*i) << ' ' << (*i) << ' ';
                } else {
                    outputFile << (*i) << ' ';
                }
            }
        }

        outputFile << std::endl;
    }
    float time =  float( clock () - begin ) /  CLOCKS_PER_SEC;
    partitioner_logger.log("Constructing metis format completed in " + to_string(time) + " seconds", "info");
    return 1;
}

std::vector<std::map<int,std::string>> MetisPartitioner::partitioneWithGPMetis() {
    partitioner_logger.log("Partitioning with gpmetis", "info");
    const clock_t begin = clock();
    char buffer[128];
    std::string result = "";
    FILE *headerModify;
    string metisCommand = "gpmetis " + this->outputFilePath + "/grf " + to_string(this->nParts) + " 2>&1";
    FILE *input = popen(metisCommand.c_str(), "r");
    if (input) {
        // read the input
        while (!feof(input)) {
            if (fgets(buffer, 128, input) != NULL) {
                result.append(buffer);
            }
        }
        pclose(input);
        if (!result.empty() && result.find("Premature") != std::string::npos) {
            vertexCount -= 1;
            string newHeader = std::to_string(vertexCount) + ' ' + std::to_string(edgeCountForMetis);
            //string command = "sed -i \"1s/.*/" + newHeader +"/\" /tmp/grf";
            string command = "sed -i \"1s/.*/" + newHeader + "/\" " + this->outputFilePath + "/grf";
            char *newHeaderChar = new char[command.length() + 1];
            strcpy(newHeaderChar, command.c_str());
            headerModify = popen(newHeaderChar, "r");
            partitioneWithGPMetis();
        } else if (!result.empty() && result.find("out of bounds") != std::string::npos) {
            vertexCount += 1;
            string newHeader = std::to_string(vertexCount) + ' ' + std::to_string(edgeCountForMetis);
            //string command = "sed -i \"1s/.*/" + newHeader +"/\" /tmp/grf";
            string command = "sed -i \"1s/.*/" + newHeader + "/\" " + this->outputFilePath + "/grf";
            char *newHeaderChar = new char[command.length() + 1];
            strcpy(newHeaderChar, command.c_str());
            headerModify = popen(newHeaderChar, "r");
            partitioneWithGPMetis();
            //However, I only found
        } else if (!result.empty() && result.find("However, I only found") != std::string::npos) {
            string firstDelimiter = "I only found";
            string secondDelimite = "edges in the file";
            unsigned first = result.find(firstDelimiter);
            unsigned last = result.find(secondDelimite);
            string newEdgeSize = result.substr(first + firstDelimiter.length() + 1,
                                               last - (first + firstDelimiter.length()) - 2);
            string newHeader = std::to_string(vertexCount) + ' ' + newEdgeSize;
            string command = "sed -i \"1s/.*/" + newHeader + "/\" " + this->outputFilePath + "/grf";
            char *newHeaderChar = new char[command.length() + 1];
            strcpy(newHeaderChar, command.c_str());
            headerModify = popen(newHeaderChar, "r");
            partitioneWithGPMetis();
        } else if (!result.empty() && result.find("Timing Information") != std::string::npos) {
            std::string line;
            string fileName = this->outputFilePath + "/grf.part." + to_string(this->nParts);
            std::ifstream infile(fileName);
            int counter = 1;
            if (zeroflag){
                counter = 0;
            }
            std::map<int, int> partIndex;
            while (std::getline(infile, line)) {
                std::istringstream iss(line);
                int a;
                if (!(iss >> a)) {
                    break;
                } else {
                    partIndex[counter] = a;
                    counter++;
                }
            }
            float time =  float( clock () - begin ) /  CLOCKS_PER_SEC;
            partitioner_logger.log("Done partitioning with gpmetis in " + to_string(time) + " seconds", "info");
            createPartitionFiles(partIndex);

            string sqlStatement =
                    "UPDATE graph SET vertexcount = '" + std::to_string(this->vertexCount) +
                    "' ,centralpartitioncount = '" + std::to_string(this->nParts) + "' ,edgecount = '"
                    + std::to_string(this->edgeCount) + "' WHERE idgraph = '" + std::to_string(this->graphID) +
                    "'";
            this->sqlite.runUpdate(sqlStatement);
            this->fullFileList.push_back(this->partitionFileList);
            this->fullFileList.push_back(this->centralStoreFileList);
            this->fullFileList.push_back(this->partitionAttributeFileList);
            this->fullFileList.push_back(this->centralStoreAttributeFileList);
            return (this->fullFileList);
        }
        perror("popen");
    } else {
        perror("popen error");
        // handle error
    }
}

void MetisPartitioner::createPartitionFiles(std::map<int, int> partMap) {
    const clock_t begin_time = clock();
    partitioner_logger.log("Populating edge lists before writing to files", "info");
    edgeMap = GetConfig::getEdgeMap();
    articlesMap = GetConfig::getAttributesMap();

    std::thread *threadList = new std::thread[nParts];
    int count = 0;
    for (int part = 0; part < nParts; part++) {
        threadList[count] = std::thread(&MetisPartitioner::populatePartMaps, this, partMap, part);
        count++;
    }

    for (int threadCount = 0; threadCount < count; threadCount++) {
        threadList[threadCount].join();
    }

    // Populate the masterEdgeLists with the remaining edges after thread functions
    for (int part = 0; part < nParts; part++){
        std::map<int, std::map<int, std::vector<int>>> commonCentralStoreEdgePartMap = commonCentralStoreEdgeMap[part];
        std::map<int, std::vector<int>>::iterator edgeMapIterator;
        //std::map<int, vector<pair<int, int>>> tempPartMap = commonCentralStoreEdgeMap[part];
        for (int subPart = 0; subPart < nParts; subPart++){
            if (part == subPart){
                continue;
            }
            else {
                std::map<int, std::vector<int>> partMasterEdgesSet = masterGraphStorageMap[subPart];
                std::map<int, std::vector<int>> commonMasterEdgeSet = commonCentralStoreEdgePartMap[subPart];
                for (edgeMapIterator = commonMasterEdgeSet.begin(); edgeMapIterator!= commonMasterEdgeSet.end();++edgeMapIterator) {
                    std::vector<int> centralGraphVertexVector = partMasterEdgesSet[edgeMapIterator->first];
                    std::vector<int> secondVertexVector = edgeMapIterator->second;
                    std::vector<int>::iterator secondVertexIterator;
                    for (secondVertexIterator = secondVertexVector.begin();secondVertexIterator!=secondVertexVector.end(); ++secondVertexIterator) {
                        int endVertex = *secondVertexIterator;
                        centralGraphVertexVector.push_back(endVertex);
                    }
                    partMasterEdgesSet[edgeMapIterator->first] = centralGraphVertexVector;
                }
                masterGraphStorageMap[subPart] = partMasterEdgesSet;
            }
        }
    }

    float t1 =  float( clock () - begin_time ) /  CLOCKS_PER_SEC;
    partitioner_logger.log("Populating edge lists completed", "info");
    partitioner_logger.log("Writing edge lists to files", "info");
    const clock_t begin_time2 = clock();
    std::thread *threads = new std::thread[nParts];
    count = 0;
    for (int part = 0; part < nParts; part++) {
        threads[count] = std::thread(&MetisPartitioner::writePartitionFiles, this, part);
        count++;
    }

    for (int threadCount = 0; threadCount < count; threadCount++) {
        threads[threadCount].join();
    }

    partitioner_logger.log("writing to files completed", "info");
    float t2 = float( clock () - begin_time2 ) /  CLOCKS_PER_SEC ;

    string sqlStatement2 =
            "UPDATE graph SET time_to_populate = '" + to_string(t1) + "' ,time_to_write = '" +
            to_string(t2) + "' WHERE idgraph = '" + to_string(graphID) + "'";
    this->sqlite.runUpdate(sqlStatement2);
}

string MetisPartitioner::reformatDataSet(string inputFilePath, int graphID) {
    this->graphID = graphID;

    std::ifstream inFile;
    inFile.open(inputFilePath, std::ios::binary | std::ios::in);

    string outputFile = utils.getHomeDir() + "/.jasminegraph/tmp/" + std::to_string(this->graphID) + "/" +
                        std::to_string(this->graphID);
    std::ofstream outFile;
    outFile.open(outputFile);

    int firstVertex = -1;
    int secondVertex = -1;
    string line;
    char splitter;

    std::getline(inFile, line);

    if (!line.empty()) {
        if (line.find(" ") != std::string::npos) {
            splitter = ' ';
        } else if (line.find('\t') != std::string::npos) {
            splitter = '\t';
        } else if (line.find(",") != std::string::npos) {
            splitter = ',';
        }
    }

    int idCounter = 1;

    while (!line.empty()) {
        string vertexOne;
        string vertexTwo;

        std::istringstream stream(line);
        std::getline(stream, vertexOne, splitter);
        stream >> vertexTwo;

        firstVertex = std::stoi(vertexOne);
        secondVertex = std::stoi(vertexTwo);

        if (vertexToIDMap.find(firstVertex) == vertexToIDMap.end()) {
            vertexToIDMap.insert(make_pair(firstVertex, idCounter));
            idToVertexMap.insert(make_pair(idCounter, firstVertex));
            idCounter++;
        }
        if (vertexToIDMap.find(secondVertex) == vertexToIDMap.end()) {
            vertexToIDMap.insert(make_pair(secondVertex, idCounter));
            idToVertexMap.insert(make_pair(idCounter, secondVertex));
            idCounter++;
        }

        int firstVertexID = vertexToIDMap.find(firstVertex)->second;
        int secondVertexID = vertexToIDMap.find(secondVertex)->second;

        outFile << (firstVertexID) << ' ' << (secondVertexID) << std::endl;

        std::getline(inFile, line);
        while (!line.empty() && line.find_first_not_of(splitter) == std::string::npos) {
            std::getline(inFile, line);
        }
    }

    partitioner_logger.log("Reformatting completed", "info");
    return outputFile;
}


void MetisPartitioner::loadContentData(string inputAttributeFilePath, string graphtype) {
    graphTypeInt = graphtype;

    std::ifstream dbFile;
    dbFile.open(inputAttributeFilePath, std::ios::binary | std::ios::in);
    std::cout << "Content file is loading..." << std::endl;


    char splitter = '\t';
    string line;

    while (std::getline(dbFile, line)) {
        long vertex;
        string class_label;
        std::vector<string> attributes;

        string vertex_str;
        string attribute;

        std::istringstream stream(line);
        std::getline(stream, vertex_str, splitter);
        vertex = stol(vertex_str);


        while (stream) {
            std::string attribute;
            stream >> attribute;

            if (attribute.length()) {

                attributes.push_back(attribute);
            }
        }

        attributeDataMap.insert({vertex, attributes});


    }

}

void MetisPartitioner::populatePartMaps(std::map<int, int> partMap, int part) {
    int partitionVertexCount = 0;
    int partitionEdgeCount = 0;
    int masterPartEdgeCount = 0;

    std::map<int, std::vector<int>>::iterator edgeMapIterator;
    std::map<int, std::vector<int>> partEdgesSet;
    std::map<int, std::vector<int>> partMasterEdgesSet;
    std::map<int, std::map<int, std::vector<int>>> commonMasterEdgeSet;

    for (edgeMapIterator = graphEdgeMap.begin(); edgeMapIterator!= graphEdgeMap.end();++edgeMapIterator) {

        int startVertex = edgeMapIterator->first;
        int startVertexPart = partMap[startVertex];
        if (startVertexPart == part) {
            partitionVertexCount++;
            std::vector<int> secondVertexVector = edgeMapIterator->second;
            std::vector<int> localGraphVertexVector;
            std::vector<int> centralGraphVertexVector;

            std::vector<int>::iterator secondVertexIterator;
            for (secondVertexIterator = secondVertexVector.begin();secondVertexIterator!=secondVertexVector.end(); ++secondVertexIterator) {
                int endVertex = *secondVertexIterator;
                int endVertexPart = partMap[endVertex];

                if (endVertexPart ==  part) {
                    partitionEdgeCount++;
                    localGraphVertexVector.push_back(endVertex);
                } else {
                    masterPartEdgeCount++;
                    /*This edge's two vertices belong to two different parts.
                    * Therefore the edge is added to both partMasterEdgeSets
                    * This adds the edge to the masterGraphStorageMap with key being the part of vertex 1
                    */
                    centralGraphVertexVector.push_back(endVertex);

                    /* We need to insert these central store edges to the masterGraphStorageMap where the key is the
                    * second vertex's part. But it cannot be done inside the thread as it generates a race condition
                    * due to multiple threads trying to write to masterGraphStorageMap's maps apart from the one
                    * assigned to the thread. Therefore, we take all such edges to a separate data structure and
                    * add them to the masterGraphStorageMap later by a single thread (main thread)
                    */
                    commonMasterEdgeSet[endVertexPart][startVertex].push_back(endVertex);
                }
            }

            partEdgesSet[startVertex] = localGraphVertexVector;
            partMasterEdgesSet[startVertex] = centralGraphVertexVector;
        }

    }

    partitionedLocalGraphStorageMap[part] = partEdgesSet;
    masterGraphStorageMap[part] = partMasterEdgesSet;
    commonCentralStoreEdgeMap[part] = commonMasterEdgeSet;

    string sqlStatement =
            "INSERT INTO partition (idpartition,graph_idgraph,vertexcount,edgecount,master_edgecount) VALUES(\"" +
            std::to_string(part) + "\", \"" + std::to_string(this->graphID) +
            "\", \"" + std::to_string(partitionVertexCount) + "\",\"" + std::to_string(partitionEdgeCount) + "\",\"" + std::to_string(masterPartEdgeCount) + "\")";
    this->sqlite.runUpdate(sqlStatement);

}

void MetisPartitioner::writePartitionFiles(int part) {

    string outputFilePart = outputFilePath + "/" + std::to_string(this->graphID) + "_" + std::to_string(part);
    string outputFilePartMaster =
            outputFilePath + "/" + std::to_string(this->graphID) + "_centralstore_" + std::to_string(part);

    std::map<int, std::vector<int>> partEdgeMap = partitionedLocalGraphStorageMap[part];
    std::map<int, std::vector<int>> partMasterEdgeMap = masterGraphStorageMap[part];


    if (graphTypeInt == "1") {
        std::map<long, std::vector<string>> partitionAttributes;
        std::map<long, std::vector<string>> centralStoreAttributes;
        string attributeFilePart =
                outputFilePath + "/" + std::to_string(this->graphID) + "_attributes_" + std::to_string(part);
        string attributeFilePartMaster =
                outputFilePath + "/" + std::to_string(this->graphID) + "_centralstore_attributes_" +
                std::to_string(part);


        ofstream partfile;
        partfile.open(attributeFilePart);

        vector<int> partVertices;
        vector<int>::iterator finder;

        for (auto it = partEdgeMap.begin(); it != partEdgeMap.end(); ++it) {
            int vertex1 = it->first;
            if (graphType == Conts::GRAPH_TYPE_NORMAL_REFORMATTED) {
                vertex1 = idToVertexMap.find(it->first)->second;
            }
            finder = find(partVertices.begin(), partVertices.end(), vertex1);
            if (finder == partVertices.end()) {
                partVertices.push_back(vertex1);
                auto vertex1_ele = attributeDataMap.find(vertex1);
                std::vector<string> vertex1Attributes = vertex1_ele->second;
                partfile << vertex1_ele->first << "\t";
                for (auto itr = vertex1Attributes.begin(); itr != vertex1Attributes.end(); ++itr) {
                    partfile << *itr << "\t";
                }
                partfile << endl;
            }

            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                int vertex2 = *it2;
                if (graphType == Conts::GRAPH_TYPE_NORMAL_REFORMATTED) {
                    vertex2 = idToVertexMap.find(*it2)->second;
                }
                finder = find(partVertices.begin(), partVertices.end(), vertex2);

                if (finder == partVertices.end()) {
                    partVertices.push_back(vertex2);
                    auto vertex2_ele = attributeDataMap.find(vertex2);
                    std::vector<string> vertex2Attributes = vertex2_ele->second;
                    partfile << vertex2_ele->first << "\t";
                    for (auto itr2 = vertex2Attributes.begin(); itr2 != vertex2Attributes.end(); ++itr2) {
                        partfile << *itr2 << "\t";
                    }
                    partfile << endl;
                }
            }
        }

        partfile.close();

        partfile.open(attributeFilePartMaster);

        vector<int> masterPartVertices;

        for (auto it = partMasterEdgeMap.begin(); it != partMasterEdgeMap.end(); ++it) {

            int vertex1 = idToVertexMap.find(it->first)->second;
            finder = find(masterPartVertices.begin(), masterPartVertices.end(), vertex1);
            if (finder == masterPartVertices.end()) {
                masterPartVertices.push_back(vertex1);
                auto vertex1_ele = attributeDataMap.find(vertex1);
                std::vector<string> vertex1Attributes = vertex1_ele->second;
                partfile << vertex1_ele->first << "\t";
                for (auto itr = vertex1Attributes.begin(); itr != vertex1Attributes.end(); ++itr) {
                    partfile << *itr << "\t";
                }
                partfile << endl;
            }

            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {

                int vertex2 = idToVertexMap.find(*it2)->second;

                finder = find(masterPartVertices.begin(), masterPartVertices.end(), vertex2);

                if (finder == masterPartVertices.end()) {
                    masterPartVertices.push_back(vertex2);
                    auto vertex2_ele = attributeDataMap.find(vertex2);
                    std::vector<string> vertex2Attributes = vertex2_ele->second;
                    partfile << vertex2_ele->first << "\t";
                    for (auto itr2 = vertex2Attributes.begin(); itr2 != vertex2Attributes.end(); ++itr2) {
                        partfile << *itr2 << "\t";
                    }
                    partfile << endl;
                }
            }
        }

        partfile.close();

        this->utils.compressFile(attributeFilePart);
        partAttrFileMutex.lock();
        partitionAttributeFileList.insert(make_pair(part,attributeFilePart + ".gz"));
        partAttrFileMutex.unlock();
        this->utils.compressFile(attributeFilePartMaster);
        masterAttrFileMutex.lock();
        centralStoreAttributeFileList.insert(make_pair(part,attributeFilePartMaster + ".gz"));
        masterAttrFileMutex.unlock();
    }

    if (graphType == Conts::GRAPH_TYPE_RDF) {
        std::map<long, std::vector<string>> partitionedEdgeAttributes;
        std::map<long, std::vector<string>> centralStoreEdgeAttributes;
        string attributeFilePart =
                outputFilePath + "/" + std::to_string(this->graphID) + "_attributes_" + std::to_string(part);
        string attributeFilePartMaster =
                outputFilePath + "/" + std::to_string(this->graphID) + "_centralstore_attributes_" +
                std::to_string(part);

        //edge attribute separation for partition files
        for (auto it = partEdgeMap.begin(); it != partEdgeMap.end(); ++it) {
            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                auto entry = edgeMap.find(make_pair(it->first, *it2));
                long article_id = entry->second;
                std::vector<string> attributes;
                auto array = (articlesMap.find(article_id))->second;

                for (int itt = 0; itt < 7; itt++) {
                    string element = (array)[itt];
                    attributes.push_back(element);
                }

                partitionedEdgeAttributes.insert({article_id, attributes});
            }
        }

        //edge attribute separation for central store files
        for (auto it = partMasterEdgeMap.begin(); it != partMasterEdgeMap.end(); ++it) {
            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                auto entry = edgeMap.find(make_pair(it->first, *it2));
                long article_id = entry->second;
                std::vector<string> attributes;
                auto array = (articlesMap.find(article_id))->second;

                for (int itt = 0; itt < 7; itt++) {
                    string element = (array)[itt];
                    attributes.push_back(element);
                }

                centralStoreEdgeAttributes.insert({article_id, attributes});
            }
        }

        JasmineGraphHashMapLocalStore *hashMapLocalStore = new JasmineGraphHashMapLocalStore();
        hashMapLocalStore->storeAttributes(partitionedEdgeAttributes, attributeFilePart);
        hashMapLocalStore->storeAttributes(centralStoreEdgeAttributes, attributeFilePartMaster);

        this->utils.compressFile(attributeFilePart);
        partAttrFileMutex.lock();
        partitionAttributeFileList.insert(make_pair(part,attributeFilePart + ".gz"));
        partAttrFileMutex.unlock();
        this->utils.compressFile(attributeFilePartMaster);
        masterAttrFileMutex.lock();
        centralStoreAttributeFileList.insert(make_pair(part,attributeFilePartMaster + ".gz"));
        masterAttrFileMutex.unlock();
    }

    if (!partEdgeMap.empty()) {
        std::ofstream localFile(outputFilePart);

        if (localFile.is_open()) {
            for (auto it = partEdgeMap.begin(); it != partEdgeMap.end(); ++it) {
                int vertex = it->first;
                std::vector<int> destinationSet = it->second;

                if (!destinationSet.empty()) {
                    for (std::vector<int>::iterator itr = destinationSet.begin(); itr != destinationSet.end(); ++itr) {
                        string edge;

                        if (graphType == Conts::GRAPH_TYPE_RDF) {
                            auto entry = edgeMap.find(make_pair(vertex, (*itr)));
                            long article_id = entry->second;

                            edge = std::to_string(vertex) + " " + std::to_string((*itr)) + " " +
                                   std::to_string(article_id);
                        } else if (graphType == Conts::GRAPH_TYPE_NORMAL_REFORMATTED) {
                            int vertex2 = idToVertexMap.find(*itr)->second;
                            edge = std::to_string(idToVertexMap.find(vertex)->second) + " " + std::to_string(vertex2);
                        } else {
                            edge = std::to_string(vertex) + " " + std::to_string((*itr));
                        }

                        localFile << edge;
                        localFile << "\n";
                    }
                }
            }
        }

        localFile.flush();
        localFile.close();

        std::ofstream masterFile(outputFilePartMaster);

        if (masterFile.is_open()) {
            for (auto it = partMasterEdgeMap.begin(); it != partMasterEdgeMap.end(); ++it) {
                int vertex = it->first;
                std::vector<int> destinationSet = it->second;

                if (!destinationSet.empty()) {
                    for (std::vector<int>::iterator itr = destinationSet.begin();
                         itr != destinationSet.end(); ++itr) {
                        string edge;

                        if (graphType == Conts::GRAPH_TYPE_RDF) {
                            auto entry = edgeMap.find(make_pair(vertex, (*itr)));
                            long article_id = entry->second;

                            edge = std::to_string(vertex) + " " + std::to_string((*itr)) + " " +
                                   std::to_string(article_id);

                        } else if (graphType == Conts::GRAPH_TYPE_NORMAL_REFORMATTED) {
                            int vertex2 = idToVertexMap.find(*itr)->second;
                            edge = std::to_string(idToVertexMap.find(vertex)->second) + " " +
                                   std::to_string(vertex2);
                        } else {
                            edge = std::to_string(vertex) + " " + std::to_string((*itr));

                        }

                        masterFile << edge;
                        masterFile << "\n";
                    }
                }
            }
        }

        masterFile.flush();
        masterFile.close();

    }

    //Compress part files
    this->utils.compressFile(outputFilePart);
    partFileMutex.lock();
    partitionFileList.insert(make_pair(part,outputFilePart + ".gz"));
    partFileMutex.unlock();
    this->utils.compressFile(outputFilePartMaster);
    masterFileMutex.lock();
    centralStoreFileList.insert(make_pair(part,outputFilePartMaster + ".gz"));
    masterFileMutex.unlock();

}
