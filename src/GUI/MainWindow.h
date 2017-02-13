/*
 * MainWindow.h
 *
 *  Created on: 04 feb 2017
 *      Author: lorenzo
 */

#ifndef SRC_GUI_MAINWINDOW_H_
#define SRC_GUI_MAINWINDOW_H_

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QAudioFormat;
class QCustomPlot;
class QCPItemStraightLine;

namespace cb {

class Engine;

class MainWindow: public QMainWindow {
Q_OBJECT

public:
	MainWindow(Engine *engine, QWidget *parent = 0);
	virtual ~MainWindow();

public slots:
	void format_changed(const QAudioFormat *new_format);
	void buffer_changed(qint64 position, qint64 length, const QByteArray &buffer);
	void play_position_changed(qint64 position);
	void on_mouse_move(QMouseEvent *event);

private:
	Engine *_engine;
	Ui::MainWindow *_ui;

	QCustomPlot *_plot;
	QCPItemStraightLine *_position;
	const QAudioFormat *_audio_format;
	void _init_plot();
};

} /* namespace cb */

#endif /* SRC_GUI_MAINWINDOW_H_ */
