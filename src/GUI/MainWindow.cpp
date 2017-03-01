/*
 * MainWindow.cpp
 *
 *  Created on: 04 feb 2017
 *      Author: lorenzo
 */

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "../Engine.h"
#include "../SoundUtils/SoundUtils.h"
#include "qcustomplot/qcustomplot.h"

#include <QAudioFormat>

namespace cb {

MainWindow::MainWindow(Engine *engine, QWidget *parent) :
				QMainWindow(parent),
				_pos_layer("play_position"),
				_engine(engine),
				_ui(new Ui::MainWindow) {
	_ui->setupUi(this);
	_init_plot();

	connect(_ui->action_open, &QAction::triggered, this, &MainWindow::_open);

	connect(_engine, &Engine::play_position_changed, this, &MainWindow::_play_position_changed);

	connect(_engine, &Engine::playing, this, &MainWindow::_engine_playing);
	connect(_engine, &Engine::paused, this, &MainWindow::_engine_paused);
	connect(_engine, &Engine::stopped, this, &MainWindow::_engine_stopped);

	connect(_ui->play_button, &QPushButton::toggled, this, &MainWindow::_toggle_play);
	connect(_ui->stop_button, &QPushButton::clicked, this, &MainWindow::_stop);

	// right-align all the elements in the leftmost column of the tempo/pitch change grid layout
	for(int i = 0; i < _ui->slider_layout->rowCount(); i++) {
		_ui->slider_layout->itemAtPosition(i, 0)->setAlignment(Qt::AlignRight);
	}
}

MainWindow::~MainWindow() {

}

void MainWindow::erase_statusbar(QMouseEvent *event) {
	_ui->statusbar->showMessage("");
}

void MainWindow::_open() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Open file"), "", tr("WAV files(*.wav)"));
	if(filename.size() > 0) {
		this->setEnabled(false);
		_ui->play_button->setEnabled(false);
		_ui->stop_button->setEnabled(false);
		_ui->pitch_slider->setEnabled(false);
		_ui->tempo_slider->setEnabled(false);
		_ui->plot_scrollbar->setEnabled(false);

		const QByteArray *buffer =_engine->load(filename);
		qint64 length = buffer->length();
		_plot->clearGraphs();
		int bytes = _engine->sample_size() / 8;
		long n_samples = length / bytes;
		qreal length_in_seconds = _engine->duration();
		long increment = _engine->channel_count();

		qDebug() << length << n_samples << increment << length_in_seconds;

		const short *samples = reinterpret_cast<const short *>(buffer->data());
		QVector<qreal> x_data(n_samples);
		QVector<qreal> y_data(n_samples);
		for(int i = 0; i < n_samples; i += increment) {
			int idx = i / increment;
			x_data[idx] = idx / (qreal) _engine->sample_rate();
			y_data[idx] = (qreal) samples[i];
		}

		QCPGraph *graph = _plot->addGraph();
		graph->setPen(QPen(QColor("black")));
		graph->setData(x_data, y_data);
		_ui->plot_scrollbar->setRange(0, length_in_seconds);
		_plot->xAxis->setRange(0, length_in_seconds);

		long max_val = 2 << (_engine->sample_size() - 2);
		long min_val = (_engine->sample_type() == QAudioFormat::UnSignedInt) ? 0 : -max_val;
		_plot->yAxis->setRange(min_val, max_val);

		_plot->replot();

		_ui->play_button->setEnabled(true);
		_ui->stop_button->setEnabled(true);
		_ui->pitch_slider->setEnabled(true);
		_ui->tempo_slider->setEnabled(true);
		_ui->plot_scrollbar->setEnabled(true);
		this->setEnabled(true);
	}
}

void MainWindow::_toggle_play(bool s) {
	if(s) {
		qreal tempo_change = (qreal) _ui->tempo_slider->value() - 100.;
		int pitch_change = _ui->pitch_slider->value();
		_engine->play(tempo_change, pitch_change);
	}
	else _engine->pause();
}

void MainWindow::_stop() {
	_engine->stop();
}

void MainWindow::_play_position_changed(qint64 position) {
	qreal pos_in_sec = position / (qreal) 1000000.;
	_position->point1->setCoords(pos_in_sec, -1);
	_position->point2->setCoords(pos_in_sec, 1);
	_plot->layer(_pos_layer)->replot();
}

void MainWindow::_plot_on_mouse_press(QMouseEvent *event) {
	qreal x = _plot->xAxis->pixelToCoord(event->pos().x());
	qint64 curr_pos_us = x * 1000000;
	_engine->seek(curr_pos_us);
	_press_pos = event->pos();

	_selection->setVisible(false);
	_selection->topLeft->setCoords(x, _plot->yAxis->range().upper);
	_plot->layer(_pos_layer)->replot();
}

void MainWindow::_plot_on_mouse_release(QMouseEvent *event) {

}

void MainWindow::_plot_on_mouse_move(QMouseEvent *event) {
	QString msg;
	qreal x = _plot->xAxis->pixelToCoord(event->pos().x());
	if(_engine->is_ready()) {
		// transform the mouse position to x,y coordinates and show them in the status bar
		msg = QString("%1 s").arg(x, 0, 'f', 2);
	}
	_ui->statusbar->showMessage(msg);

	int x_diff = event->pos().x() - _press_pos.x();
	bool left_pressed = event->buttons() & Qt::LeftButton;
	if(left_pressed && x_diff > 10) {
		_selection->bottomRight->setCoords(x, _plot->yAxis->range().lower);
		_selection->setVisible(true);
		_plot->layer(_pos_layer)->replot();
	}
}

void MainWindow::_engine_playing() {
	_ui->play_button->setChecked(true);
	_ui->play_button->setText("Pause");
}

void MainWindow::_engine_paused() {
	_ui->play_button->setChecked(false);
	_ui->play_button->setText("Play");
}

void MainWindow::_engine_stopped() {
	_ui->play_button->setChecked(false);
	_ui->play_button->setText("Play");
}

void MainWindow::_x_axis_changed(const QCPRange &range) {
	// make sure that we do not zoom out too much
	if(range.lower < 0. || range.upper > _engine->duration()) {
		QCPRange new_range = range.bounded(0, _engine->duration());
		_plot->xAxis->setRange(new_range);
	}
	else {
		// adjust the position of the scroll bar slider
		_ui->plot_scrollbar->setRange(0, qRound(_engine->duration() - range.size()));
		_ui->plot_scrollbar->setValue(qRound(range.center()));
		// adjust the size of the scroll bar slider
		_ui->plot_scrollbar->setPageStep(qRound(range.size()));
	}
}

void MainWindow::_plot_scrollbar_changed(int value) {
	// we don't want to replot twice if the user is dragging plot
	if(qAbs(_plot->xAxis->range().center() - value / 100.0) > 0.01) {
		_plot->xAxis->setRange(value, _plot->xAxis->range().size(), Qt::AlignCenter);
		_plot->replot();
	}
}

void MainWindow::_init_plot() {
	_plot = _ui->plot;
	_plot->setMaximumHeight(300);
	_plot->setInteractions(QCP::iRangeZoom);
	_plot->axisRect()->setRangeDrag(Qt::Horizontal);
	_plot->axisRect()->setRangeZoom(Qt::Horizontal);

	_plot->xAxis->setTickLength(0, 0);
	_plot->xAxis->setSubTicks(false);
	_plot->xAxis->setTickLabels(true);
	_plot->xAxis->setBasePen(Qt::NoPen);

	_plot->yAxis->setTickLabels(false);
	_plot->yAxis->setTicks(false);
	_plot->yAxis->grid()->setVisible(false);
	_plot->yAxis->setBasePen(Qt::NoPen);

	_plot->addLayer(_pos_layer, 0, QCustomPlot::limAbove);
	_plot->layer(_pos_layer)->setMode(QCPLayer::lmBuffered);

	_selection = new QCPItemRect(_plot);
	_selection->setBrush(QBrush(QColor(100, 100, 100, 100)));
	_selection->setPen(Qt::NoPen);
	_selection->setLayer(_pos_layer);
	_selection->setVisible(false);

	_position = new QCPItemStraightLine(_plot);
	_position->point1->setCoords(0, -1);
	_position->point2->setCoords(0, 1);
	QPen line_pen(QColor("red"));
	line_pen.setWidth(3);
	_position->setPen(line_pen);
	_position->setLayer(_pos_layer);

	// setup the scrollbar
	connect(_ui->plot_scrollbar, &QScrollBar::valueChanged, this, &MainWindow::_plot_scrollbar_changed);
	connect(_plot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(_x_axis_changed(QCPRange)));

	connect(_plot, &QCustomPlot::mouseMove, this, &MainWindow::_plot_on_mouse_move);
	connect(_plot, &QCustomPlot::mousePress, this, &MainWindow::_plot_on_mouse_press);
	connect(_plot, &QCustomPlot::mouseRelease, this, &MainWindow::_plot_on_mouse_release);
//	connect(_plot, &QCustomPlot::leaveEvent, this, &MainWindow::erase_statusbar);
}

} /* namespace cb */
