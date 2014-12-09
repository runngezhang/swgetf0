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

#include "config.h"

namespace {
const float MINNOTE = 100;
const float MAXNOTE = 400;
}

ViewerWidget::ViewerWidget(QWidget* parent)
    : QGLWidget(parent),
      m_timer(new QTimer(this)),
      m_parent(dynamic_cast<MainWindow*>(parent)),
      m_update_pending(false)
{
  QObject::connect(m_timer, SIGNAL(timeout()), this, SLOT(renderLater()));
  m_timer->setTimerType(Qt::PreciseTimer);
  m_timer->start(1000 / 120);
}

void ViewerWidget::renderLater() {
    if (!m_update_pending) {
        m_update_pending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

void ViewerWidget::renderNow() {
  QPainter painter(this);

  painter.fillRect(rect(), Qt::black);

  const QPen penWhite(QColor(255, 255, 255));
  const QBrush brushWhite(QColor(255, 255, 255));


  int m_width = 1024;
  int m_height = 800;

  std::lock_guard<std::mutex> lockGuard(m_parent->mutex());

  auto noteToPos = [this, m_height](double note) {
    return m_height - (note - MINNOTE) * (m_height / (MAXNOTE - MINNOTE));
  };

  const double noteWidth = 2;


  double position = 0;
  for (auto note : m_parent->cb()) {
    if (note.f0 != 0) {  // TODO float compare

      double ypos = noteToPos(note.f0);

      painter.setPen(penWhite);
      painter.setBrush(brushWhite);

      painter.drawRect(position, ypos - 1, noteWidth, noteWidth);
    }

    position += noteWidth;
  }

  for (const auto& line : m_parent->config().uiMarkerLines.lines) {
    double ypos = noteToPos(line.frequency);
    painter.setPen(line.pen);
    painter.drawLine(0, ypos, m_width, ypos);
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

void ViewerWidget::paintEvent(QPaintEvent* event) {
  renderNow();
}

MainWindow::MainWindow(std::size_t bufferCapacity, const config::Config& config)
    : m_cb(bufferCapacity), m_config(config)
{
  m_ui.setupUi(this);

  ViewerWidget* vw = new ViewerWidget(this);

  setCentralWidget(vw);
}

void MainWindow::on_action_About_triggered()
{
  About* about = new About(this);
  about->show();
}
