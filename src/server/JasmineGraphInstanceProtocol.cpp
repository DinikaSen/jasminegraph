/**
Copyright 2019 JasminGraph Team
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

#include "JasmineGraphInstanceProtocol.h"

const string JasmineGraphInstanceProtocol::HANDSHAKE = "hske";
const string JasmineGraphInstanceProtocol::HANDSHAKE_OK = "hske";
const string JasmineGraphInstanceProtocol::CLOSE = "close";
const string JasmineGraphInstanceProtocol::CLOSE_ACK = "close-ok" ;
const string JasmineGraphInstanceProtocol::SHUTDOWN = "shtdn";
const string JasmineGraphInstanceProtocol::SHUTDOWN_ACK = "shtdn-ok";
const string JasmineGraphInstanceProtocol::READY = "ready";
const string JasmineGraphInstanceProtocol::OK = "ok";