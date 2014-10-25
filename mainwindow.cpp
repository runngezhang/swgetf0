#include "mainwindow.h"

#include <QPainter>

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
  const QPen penRMS(QColor(0, 100, 100));
  const QBrush brushWhite(QColor(255, 255, 255));
  const QBrush brushRMS(QColor(0, 100, 100));


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

    {
      double ypos = m_height - (note.rms * (m_height / 3 / 24000.0) + (2.0 * m_height / 3));

      painter.setPen(penRMS);
      painter.setBrush(brushRMS);

      painter.drawRect(position, ypos - 1, noteWidth, noteWidth);
    }

    position += noteWidth;
  }

  const QPen penNormal(QColor(100, 0, 0));
  const QPen penSub(QColor(50, 0, 0)); // not much attention
  const QPen penTarget(QColor(255, 0, 0));

  // G3
  {
    double ypos = noteToPos(196);
    painter.setPen(penNormal);
    painter.drawLine(0, ypos, m_width, ypos);
  }

  // A3
  {
    double ypos = noteToPos(220);
    painter.setPen(penTarget);
    painter.drawLine(0, ypos, m_width, ypos);
  }

  // A#3
  {
    double ypos = noteToPos(233);
    painter.setPen(penSub);
    painter.drawLine(0, ypos, m_width, ypos);
  }

  // B3
  {
    double ypos = noteToPos(247);
    painter.setPen(penNormal);
    painter.drawLine(0, ypos, m_width, ypos);
  }

  // C4 - wow!
  {
    double ypos = noteToPos(262);
    painter.setPen(penNormal);
    painter.drawLine(0, ypos, m_width, ypos);
  }

  // D4
  {
    double ypos = noteToPos(294);
    painter.setPen(penNormal);
    painter.drawLine(0, ypos, m_width, ypos);
  }

  // E4
  {
    double ypos = noteToPos(330);
    painter.setPen(penNormal);
    painter.drawLine(0, ypos, m_width, ypos);
  }

  // C3
  {
    double ypos = noteToPos(131);
    painter.setPen(penNormal);
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

MainWindow::MainWindow(std::size_t bufferCapacity) : m_cb(bufferCapacity)
{
  m_ui.setupUi(this);

  ViewerWidget* vw = new ViewerWidget(this);

  setCentralWidget(vw);
}
