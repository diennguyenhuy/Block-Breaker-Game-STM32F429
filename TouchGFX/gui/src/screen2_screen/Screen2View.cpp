#include <gui/screen2_screen/Screen2View.hpp>
#include <touchgfx/Unicode.hpp>

Screen2View::Screen2View()
{

}

void Screen2View::setupScreen()
{
    Screen2ViewBase::setupScreen();
    if (this->presenter->getHighScore() != 0) {
    	Unicode::snprintf(this->textArea1Buffer, TEXTAREA1_SIZE, "%d", this->presenter->getHighScore());
    	textArea1.setVisible(true);
    } else {
    	textArea1.setVisible(false);
    }
    textArea1.invalidate();
}

void Screen2View::tearDownScreen()
{
    Screen2ViewBase::tearDownScreen();
}
