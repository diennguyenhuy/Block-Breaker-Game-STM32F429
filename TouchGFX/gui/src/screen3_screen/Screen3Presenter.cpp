#include <gui/screen3_screen/Screen3View.hpp>
#include <gui/screen3_screen/Screen3Presenter.hpp>

Screen3Presenter::Screen3Presenter(Screen3View& v)
    : view(v)
{

}

void Screen3Presenter::activate()
{

}

void Screen3Presenter::deactivate()
{

}

int Screen3Presenter::getScore() {
	return this->model->getScore();
}

int Screen3Presenter::getHighScore() {
	return this->model->getHighScore();
}
