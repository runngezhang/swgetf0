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

#include "mainwindow.h"
#include "about.h"

#include <QPainter>
#include <QResizeEvent>

#include <cmath>

#include "config.h"

ViewerWidget::ViewerWidget(QWidget* parent)
    : QGLWidget(parent),
      m_parent(dynamic_cast<MainWindow*>(parent)),
      m_update_pending(false)
{
}

void ViewerWidget::renderLater() {
    if (!m_update_pending) {
        m_update_pending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

namespace {

double frequencyToKey(double frequency)
{
  // http://en.wikipedia.org/wiki/Piano_key_frequencies
  return 12 * log2(frequency / 440) + 49;
}

} // namespace anonymous

void ViewerWidget::renderNow() {
  QPainter painter(this);

  painter.fillRect(rect(), Qt::black);

  const QPen penNote(QColor(255, 255, 255));
  const QBrush brushNote(QColor(255, 255, 255));

  const QPen penWave(Qt::darkGray);
  const QBrush brushWave(Qt::darkGray);

  const auto& uiConfig = m_parent->config().uiConfig;

  int width = size().width();
  int height = size().height();

  double minNote = frequencyToKey(uiConfig.min_note);
  double maxNote = frequencyToKey(uiConfig.max_note);

  auto noteToPos = [height, minNote, maxNote](double note) {
    note = frequencyToKey(note);
    return height -
           (note - minNote) *
               (static_cast<double>(height) / (maxNote - minNote));
  };

  const double noteWidth = uiConfig.note_width;
  const double waveformScale = uiConfig.waveform_scale;

  {
    std::lock_guard<std::mutex> lockGuard(m_parent->f0thread().mutex());

    double position = 0;
    for (auto f0point : m_parent->f0thread().cb()) {

      // Draw the waveform (before the note in order to not cover it)
      if (waveformScale != 0) {
          painter.setPen(penWave);
          painter.setBrush(brushWave);
          float scaledMaxSampleVal = f0point.maxsampleval / waveformScale;
          painter.drawRect(position, 0, noteWidth, scaledMaxSampleVal);
      }

      // Draw the note
      double ypos = noteToPos(f0point.f0);
      painter.setPen(penNote);
      painter.setBrush(brushNote);
      painter.drawRect(position, ypos - 1, noteWidth, noteWidth);

      // Advance
      position += noteWidth;
    }
  }

  for (const auto& line : m_parent->config().uiMarkerLines.lines) {
    double ypos = noteToPos(line.frequency);
    painter.setPen(line.pen);
    painter.drawLine(0, ypos, width, ypos);
  }
}

bool ViewerWidget::event(QEvent* event)
{
  switch (event->type()) {
  case QEvent::UpdateRequest:
    m_update_pending = false;
    renderNow();
    return true;
  default:
    return QGLWidget::event(event);
  }
}

void ViewerWidget::paintEvent(QPaintEvent* event)
{
  renderNow();
}

void ViewerWidget::resizeEvent(QResizeEvent* event)
{
  emit widthChanged(event->size().width());
}

MainWindow::MainWindow(const config::Config& config, F0Thread& f0thread)
    : m_config(config), m_f0thread(f0thread)
{
  ViewerWidget* vw = new ViewerWidget(this);
  vw->setObjectName("viewer");

  m_ui.setupUi(this);
  resize(m_config.uiConfig.width, m_config.uiConfig.height);
  if (m_config.uiConfig.maximized) {
      showMaximized();
  }

  setCentralWidget(vw);

  QObject::connect(&f0thread, SIGNAL(updated()), vw, SLOT(renderLater()));
}

void MainWindow::on_action_About_triggered()
{
  About* about = new About(this);
  about->show();
}

void MainWindow::on_viewer_widthChanged(int width)
{
  std::lock_guard<std::mutex> lockGuard(m_f0thread.mutex());
  m_f0thread.cb().resize(width / m_config.uiConfig.note_width);
}
