#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/widgets/BoxWithBorder.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/TextArea.hpp>
#include <touchgfx/Unicode.hpp>
#include "main.h"
#include "cmsis_os.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

static const int launchTable[][2] = {
    { -2, -4 },
    { -1, -4 },
    {  1, -4 },
    {  2, -4 },
    { -3, -3 },
	{  3, -3 }
};

static int isqrt(int n) {
    if (n <= 0) return 0;

    int result = 0;
    int bit = 1 << 30; // The second-to-top bit (1<<30 for 32-bit int)

    // Find the highest power of four <= n
    while (bit > n) bit >>= 2;

    while (bit != 0) {
        if (n >= result + bit) {
            n -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return result;
}

extern osMessageQueueId_t buttonQueue;

void Screen1View::initBallSpeed() {
	int i = HAL_GetTick() % (sizeof(launchTable)/sizeof(launchTable[0]));
	ballVx = launchTable[i][0];
	ballVy = launchTable[i][1];

	capBallSpeed();
}

Screen1View::Screen1View()
{

}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    begin = false;

    initBallSpeed();

    ballX = circle1.getX();
    ballY = circle1.getY();
    circle1.getRadius(ballRadius);

    paddleX = box1.getX();
    paddleY = box1.getY();
    paddleWidth = box1.getWidth();
    paddleHeight = box1.getHeight();

    for (int i = 0; i < 24; i++) blocksAlive[i] = true;

    Unicode::snprintf(textArea1Buffer, 10, "%d", score);
    textArea1.invalidate();


    powerUpHeart = &plus1Life;
    powerUpActive = false;
    powerUpFalling = false;
    powerUpX = -100;  // Initialize off-screen
    powerUpY = -100;  // Initialize off-screen
    powerUpVy = POWERUP_FALL_SPEED;
    plus1Life.setVisible(false);  // FIXED: Correct method name
    plus1Life.invalidate();
    spawnPowerUp();


    uint8_t _;
    while (osMessageQueueGetCount(buttonQueue) > 0) osMessageQueueGet(buttonQueue, &_, NULL, osWaitForever);
}
void Screen1View::spawnPowerUp() {
    // Randomly select a block for the power-up
    powerUpBlockIndex = HAL_GetTick() % 24;  // Pick random number 0-23
    powerUpActive = true;      // Mark that a power-up exists in this round
    powerUpFalling = false;    // It's not falling yet (still hidden in block)

    // Position the power-up off-screen initially
    // It will be revealed when the block is hit
    powerUpHeart->setVisible(false);  // Keep it invisible until block breaks
}
void Screen1View::updatePowerUp() {
    if (powerUpFalling) {
        powerUpY += powerUpVy;

        // Check if power-up fell off screen
        if (powerUpY > HAL::DISPLAY_HEIGHT) {
            powerUpActive = false;
            powerUpFalling = false;
            powerUpHeart->setVisible(false);
        } else {
            powerUpHeart->moveTo(powerUpX, powerUpY);
            powerUpHeart->invalidate();
        }
    }
}
bool Screen1View::intersectPowerUp() {
    if (!powerUpFalling) return false;  // Not falling? Then can't collide

    int heartWidth = powerUpHeart->getWidth();
    int heartHeight = powerUpHeart->getHeight();

    // Rectangle collision detection
    return powerUpX + heartWidth >= paddleX &&      // Heart's right edge past paddle's left
           powerUpX <= paddleX + paddleWidth &&     // Heart's left edge before paddle's right
           powerUpY + heartHeight >= paddleY &&     // Heart's bottom past paddle's top
           powerUpY <= paddleY + paddleHeight;      // Heart's top before paddle's bottom
}

void Screen1View::checkPowerUpCollision() {
    if (intersectPowerUp()) {
        gainLife();
        powerUpActive = false;
        powerUpFalling = false;
        powerUpHeart->setVisible(false);
        powerUpHeart->invalidate();
    }
}
void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::tickEvent() {
	updatePaddle();
	updateBall();
	updatePowerUp();
	checkWallCollision();
	checkPaddleCollision();
	checkBlockCollisions();
	checkPowerUpCollision();
	render();
	newRound();
}

void Screen1View::updatePaddle() {
	uint8_t res;
	if (osMessageQueueGetCount(buttonQueue) > 0) {
		osMessageQueueGet(buttonQueue, &res, NULL, 10);
		switch (res) {
		case 'L':
			paddleX -= paddleV;
			if (paddleX <= 0) {
				paddleX = 0;
			} else begin = true;
			break;
		case 'R':
			paddleX += paddleV;
			if (paddleX + paddleWidth >= HAL::DISPLAY_WIDTH) {
				paddleX = HAL::DISPLAY_WIDTH - paddleWidth;
			} else begin = true;
			break;
		default:
			break;
		}
	}
}

void Screen1View::updateBall() {
	if (begin) {
		ballX += ballVx;
		ballY += ballVy;
	}
}

void Screen1View::checkWallCollision() {
	if (ballX <= 0 || ballX + 2*ballRadius >= HAL::DISPLAY_WIDTH) {
		ballVx = -ballVx;
	}

	if (ballY <= 0) {
		ballVy = -ballVy;
	}

	if (ballY + 2*ballRadius >= HAL::DISPLAY_HEIGHT) {
		//resetBall();
		loseLife();
	}
}

void Screen1View::resetBall() {
	ballX = paddleX + paddleWidth/2 - ballRadius;
	ballY = paddleY - 2*ballRadius;
	circle1.moveTo(ballX, ballY);
	begin = false;
	initBallSpeed();
}

bool Screen1View::intersectPaddle() {
	return ballX + 2*ballRadius >= paddleX && ballX <= paddleX + paddleWidth &&
			ballY + 2*ballRadius >= paddleY && ballY <= paddleY + paddleHeight;
}

void Screen1View::checkPaddleCollision() {
	if (intersectPaddle() && ballVy > 0) {
		ballVy = -ballVy;

		int hitPos = (ballX + ballRadius) - paddleX;
		int center = paddleWidth/2;

		ballVx = (hitPos - center)/6;

		capBallSpeed();
	}
}

bool Screen1View::intersectBox(BoxWithBorder* b) {
	int boxX = b->getX();
	int boxY = b->getY();
	int boxWidth = b->getWidth();
	int boxHeight = b->getHeight();

	return ballX + 2*ballRadius >= boxX && ballX <= boxX + boxWidth &&
			ballY + 2*ballRadius >= boxY && ballY <= boxY + boxHeight;
}

void Screen1View::checkBlockCollisions() {
	for (int i = 0; i < 24; i++) {
		if (!blocksAlive[i]) continue;
		BoxWithBorder* b = blocks[i];

		if (intersectBox(b)) {
			int boxX = b->getX();
			int boxY = b->getY();
			int boxWidth = b->getWidth();
			int boxHeight = b->getHeight();

			int overlapLeft = (ballX + 2*ballRadius) - boxX;
			int overlapRight = (boxX + boxWidth) - ballX;
			int overlapTop = (ballY + 2*ballRadius) - boxY;
			int overlapBottom = (boxY + boxHeight) - ballY;

			int minOverlapX = overlapLeft < overlapRight ? overlapLeft : overlapRight;
			int minOverlapY = overlapTop < overlapBottom ? overlapTop : overlapBottom;

			if (minOverlapX < minOverlapY) ballVx = -ballVx;
			else ballVy = -ballVy;

			blocksAlive[i] = false;
			b->setVisible(blocksAlive[i]);
			b->invalidate();
			countBlocksAlive--;
			addScore(20);

			if (powerUpActive && i == powerUpBlockIndex && !powerUpFalling) {
			                powerUpX = boxX + boxWidth/2 - powerUpHeart->getWidth()/2;
			                powerUpY = boxY;
			                powerUpFalling = true;
			                powerUpHeart->setVisible(true);
			                powerUpHeart->moveTo(powerUpX, powerUpY);
			                powerUpHeart->invalidate();
			            }

			break;
		}
	}
}

void Screen1View::addScore(int points) {
	score += points;
	Unicode::snprintf(textArea1Buffer, 10, "%d", score);
	textArea1.invalidate();
}

void Screen1View::capBallSpeed() {
	int speedSq = ballVx * ballVx + ballVy * ballVy;
	const int maxSpeedSq = MAX_BALL_SPEED * MAX_BALL_SPEED;

	if (speedSq > maxSpeedSq) {
		int speed = isqrt(speedSq);

	    // Fixed-point scaling (Q8.8)
		int scale = (MAX_BALL_SPEED << 8) / speed;

	    ballVx = (ballVx * scale) >> 8;
	    ballVy = (ballVy * scale) >> 8;
	}

    if (abs(ballVy) < MIN_BALL_SPEED) ballVy = -MIN_BALL_SPEED;
    if (abs(ballVx) < MIN_BALL_SPEED) {
    	if (ballVx < 0) ballVx = -MIN_BALL_SPEED;
    	else ballVx = MIN_BALL_SPEED;
    }
}

void Screen1View::render() {
	circle1.moveTo(ballX, ballY);
	circle1.invalidate();
	box1.moveTo(paddleX, paddleY);
	box1.invalidate();
}

void Screen1View::switchGameOverScreen() {
	presenter->gotoGameOver();
}

void Screen1View::loseLife() {
	if (++delayTicks < 60) return;
	delayTicks = 0;
	lives--;
	hearts[lives]->setVisible(false);
	hearts[lives]->invalidate();
	if (lives > 0) {
		resetBall();
	} else {
		switchGameOverScreen();
	}

}

void Screen1View::gainLife() {
	if(lives<MAX_LIVES)
	{
		hearts[lives]->setVisible(true);
	    hearts[lives]->invalidate();
	    lives++;
	}
	else
	{
		addScore(500);
	}
}

void Screen1View::newRound() {
	if (countBlocksAlive == 0) {
		begin = false;
	    if (++delayTicks < 60) return;
	    delayTicks = 0;

		resetBall();
		for (int i = 0; i < 24; i++) {
			blocksAlive[i] = true;
			blocks[i]->setVisible(blocksAlive[i]);
			blocks[i]->invalidate();
		}
		countBlocksAlive = 24;

		spawnPowerUp();
	}
}
