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

#ifndef INPUTDEVICE_H
#define INPUTDEVICE_H

#include <QDialog>
#include <QMap>

#include <portaudiocpp/Device.hxx>
#include <portaudiocpp/HostApi.hxx>

namespace Ui {
class InputDevice;
}

class InputDevice : public QDialog
{
  Q_OBJECT

public:
  explicit InputDevice(QWidget *parent = 0);
  ~InputDevice();

  PaDeviceIndex getDeviceIndex() const;

public slots:

  void on_cmbAudioHost_currentIndexChanged(int index);

private:

  Ui::InputDevice *ui;

  QMap<int, PaHostApiTypeId> m_indexToHostApiTypeId;
  QMap<int, PaDeviceIndex> m_indexToDeviceIndex;
};

#endif // INPUTDEVICE_H
