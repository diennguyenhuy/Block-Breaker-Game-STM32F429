#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void tickEvent();
protected:
    int ballX, ballY, ballRadius;
    int ballVx = 2, ballVy = -2;
    int paddleX, paddleY, paddleWidth, paddleHeight;
    int paddleV = 2;

    static const int MAX_BALL_SPEED = 2;

    bool blocksAlive[24];
    bool begin;

    touchgfx::BoxWithBorder* blocks[24] = {
            &boxWithBorder01,
        	&boxWithBorder02,
        	&boxWithBorder03,
        	&boxWithBorder04,
        	&boxWithBorder05,
        	&boxWithBorder06,
        	&boxWithBorder07,
        	&boxWithBorder08,
        	&boxWithBorder09,
        	&boxWithBorder10,
        	&boxWithBorder11,
        	&boxWithBorder12,
        	&boxWithBorder13,
        	&boxWithBorder14,
        	&boxWithBorder15,
        	&boxWithBorder16,
        	&boxWithBorder17,
        	&boxWithBorder18,
        	&boxWithBorder19,
        	&boxWithBorder20,
        	&boxWithBorder21,
        	&boxWithBorder22,
        	&boxWithBorder23,
        	&boxWithBorder24
        };

private:
    void updatePaddle();
    void updateBall();
    void resetBall();
    void checkWallCollision();
    void checkPaddleCollision();
    void checkBlockCollisions();
    void render();
    bool intersectPaddle();
    bool intersectBox(touchgfx::BoxWithBorder* b);
    void capBallSpeed();
};

#endif // SCREEN1VIEW_HPP
