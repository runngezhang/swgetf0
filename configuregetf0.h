/*
  Copyright 2014 Sarah Wong

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

#ifndef INCLUDED_CONFIGUREGETF0_H
#define INCLUDED_CONFIGUREGETF0_H

namespace GetF0 {
class GetF0;
}

namespace config {
class EspsConfig;
}

void ConfigureGetF0(GetF0::GetF0& f0, const config::EspsConfig& espsConfig);

#endif
