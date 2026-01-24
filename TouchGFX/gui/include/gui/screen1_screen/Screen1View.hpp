#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>
#include <touchgfx/widgets/BoxWithBorder.hpp>
#include <touchgfx/widgets/Image.hpp>
#include <touchgfx/Unicode.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void tickEvent();
    int getScore() const;
protected:
    int ballX, ballY, ballRadius;
    int ballVx, ballVy;
    int paddleX, paddleY, paddleWidth, paddleHeight;
    int paddleV = 3;
    bool begin;

    static const int MAX_BALL_SPEED = 2;
    static const int MIN_BALL_SPEED = 2;
    static const int START_BALL_SPEED = 2;

    bool blocksAlive[24];
    int countBlocksAlive = 24;

    BoxWithBorder* blocks[24] = {
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

    Image* hearts[3] = {
    		&heart1,
			&heart2,
			&heart3
    };
    static const int MAX_LIVES = 3;
    int lives = 3;
    int score = 0;
    int round = 0;

    int blockIxWithHeartPowerUp;
    int blockIxWithArrowPowerUp;
    static const int POWERUP_FALL_SPEED = 1;

    uint32_t paddleExtendStartTick;
    bool isPaddleExtended = false;
    //int paddleExtendedWidth;
    int paddleNormalWidth;
    static const int PADDLE_EXTENSION = 40;
    static constexpr uint32_t PADDLE_EXTENSION_DURATION = 6000;

private:
    //Paddle and Wall logic
    void updatePaddle();
    void updateBall();
    void resetBall();
    void checkWallCollision();
    void checkPaddleCollision();
    void checkBlockCollisions();
    bool intersectPaddle();
    bool intersectBox(BoxWithBorder* b);
    void render();
    //Ball speed functionalities
    void capBallSpeed();
    void initBallSpeed();
    //Score, life logic
    void addScore(int points);
    void gainLife();
    void loseLife();
    void newRound();
    void switchGameOverScreen();
    //PowerUp logic
    void spawnPowerUp();
    void updatePowerUp();
    void checkPowerUpCollision();
    bool intersectHeartPowerUp();
    bool intersectArrowPowerUp();
    void checkPaddleExtensionTimeout();
};

#endif // SCREEN1VIEW_HPP
