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

#include "Conts.h"

std::string Conts::JASMINEGRAPH_EXECUTABLE = "run.sh";
std::string Conts::JASMINEGRAPH_HOME  = "JASMINEGRAPH_HOME";

int Conts::JASMINEGRAPH_FRONTEND_PORT = 7777;
int Conts::JASMINEGRAPH_BACKEND_PORT = 7778;
int Conts::JASMINEGRAPH_VERTEXCOUNTER_PORT = 7779;
int Conts::JASMINEGRAPH_INSTANCE_PORT = 7780;//Worker port
int Conts::JASMINEGRAPH_INSTANCE_DATA_PORT = 7781;//Data Port


int Conts::JASMINEGRAPH_RUNTIME_PROFILE_MASTER = 1;
int Conts::JASMINEGRAPH_RUNTIME_PROFILE_WORKER = 2;

const int Conts::GRAPH_STATUS::LOADING = 1;
const int Conts::GRAPH_STATUS::OPERATIONAL = 2;
const int Conts::GRAPH_STATUS::DELETING = 3;
const int Conts::GRAPH_STATUS::NONOPERATIONAL = 4;
