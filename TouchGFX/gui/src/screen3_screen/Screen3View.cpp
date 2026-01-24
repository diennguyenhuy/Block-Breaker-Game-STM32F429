#include <gui/screen3_screen/Screen3View.hpp>
#include <touchgfx/Unicode.hpp>

Screen3View::Screen3View()
{

}

void Screen3View::setupScreen()
{
    Screen3ViewBase::setupScreen();
    Unicode::snprintf(this->textArea1Buffer, TEXTAREA1_SIZE, "%d", this->presenter->getScore());
    this->textArea1.invalidate();
    Unicode::snprintf(this->textArea2Buffer, TEXTAREA2_SIZE, "%d", this->presenter->getHighScore());
    this->textArea2.invalidate();
}

void Screen3View::tearDownScreen()
{
    Screen3ViewBase::tearDownScreen();
}

