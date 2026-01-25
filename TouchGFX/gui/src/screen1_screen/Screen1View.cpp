#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/widgets/BoxWithBorder.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/TextArea.hpp>
#include <touchgfx/Unicode.hpp>
#include <touchgfx/Bitmap.hpp>
#include <BitmapDatabase.hpp>
#include "main.h"
#include "cmsis_os.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

static int delayTicks = 0;

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

	capBallSpeed(ballVx, ballVy);
}

void Screen1View::initBall2Speed() {
	int i = (HAL_GetTick() + 1) % (sizeof(launchTable)/sizeof(launchTable[0]));
	ball2Vx = launchTable[i][0];
	ball2Vy = launchTable[i][1];

	capBallSpeed(ball2Vx, ball2Vy);
}

Screen1View::Screen1View()
{

}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    begin = false;
    ball2Active = false;

    initBallSpeed();

    ballX = circle1.getX();
    ballY = circle1.getY();
    circle1.getRadius(ballRadius);

    paddleX = box1.getX();
    paddleY = box1.getY();
    paddleWidth = box1.getWidth();
    paddleHeight = box1.getHeight();

    paddleNormalWidth = paddleWidth;

    for (int i = 0; i < 24; i++) blocksAlive[i] = true;

    Unicode::snprintf(textArea1Buffer, 10, "%d", score);
    textArea1.invalidate();

    Unicode::snprintf(textArea2Buffer, 10, "%d", ++round);
    textArea2.invalidate();

    // Setup Ball 2
    circle2.setXY(circle1.getX(), circle1.getY());
    circle2.setRadius(ballRadius);
    circle2.setLineWidth(0);
    circle2Painter.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    circle2.setPainter(circle2Painter);
    circle2.setVisible(false);
    add(circle2);

    multiBallPowerUp.setBitmap(touchgfx::Bitmap(BITMAP_ARROW_POWERUP_ID)); // Reusing arrow icon
    multiBallPowerUp.setVisible(false);
    add(multiBallPowerUp);

    spawnPowerUp();

    uint8_t _;
    while (osMessageQueueGetCount(buttonQueue) > 0) osMessageQueueGet(buttonQueue, &_, NULL, osWaitForever);
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::tickEvent() {
	updatePaddle();
	updateBall();
	updateBall2();
	updatePowerUp();
	checkWallCollision();
	checkWallCollision2();
	checkPaddleCollision();
	checkPaddleCollision2();
	checkBlockCollisions();
	checkBlockCollisions2();
	checkPowerUpCollision();
	checkPaddleExtensionTimeout();
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
			if (paddleX + paddleWidth > HAL::DISPLAY_WIDTH) {
				paddleX = HAL::DISPLAY_WIDTH - paddleWidth;
			} else begin = true;
			break;
		default:
			break;
		}
	}
}

void Screen1View::updateBall2() {
	if (begin && ball2Active) {
		ball2X += ball2Vx;
		ball2Y += ball2Vy;
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
		if (ball2Active) {
			ballX = -100; // Move off-screen
			ballVx = 0;
			ballVy = 0;
			// Ball 1 is lost, but game continues with Ball 2
		} else {
			loseLife();
		}
	}
}

void Screen1View::checkWallCollision2() {
	if (!ball2Active) return;

	if (ball2X <= 0 || ball2X + 2*ballRadius >= HAL::DISPLAY_WIDTH) {
		ball2Vx = -ball2Vx;
	}

	if (ball2Y <= 0) {
		ball2Vy = -ball2Vy;
	}

	if (ball2Y + 2*ballRadius >= HAL::DISPLAY_HEIGHT) {
		ball2Active = false;
		circle2.setVisible(false);
		circle2.invalidate();
		// Continue with Ball 1 if it's still alive, or lose life if both gone
		if (ballVy == 0 && ballY < 0) { // Ball 1 was already lost
			loseLife();
		}
	}
}

bool Screen1View::intersectPaddle() {
	return ballX + 2*ballRadius >= paddleX && ballX <= paddleX + paddleWidth &&
			ballY + 2*ballRadius >= paddleY && ballY <= paddleY + paddleHeight;
}

bool Screen1View::intersectPaddle2() {
	return ball2X + 2*ballRadius >= paddleX && ball2X <= paddleX + paddleWidth &&
			ball2Y + 2*ballRadius >= paddleY && ball2Y <= paddleY + paddleHeight;
}

void Screen1View::checkPaddleCollision() {
	if (intersectPaddle() && ballVy > 0) {
		ballVy = -ballVy;

		int hitPos = (ballX + ballRadius) - paddleX;
		int center = paddleWidth/2;

		ballVx = (hitPos - center)/6;

		capBallSpeed(ballVx, ballVy);
	}
}

void Screen1View::checkPaddleCollision2() {
	if (ball2Active && intersectPaddle2() && ball2Vy > 0) {
		ball2Vy = -ball2Vy;

		int hitPos = (ball2X + ball2Radius) - paddleX;
		int center = paddleWidth/2;

		ball2Vx = (hitPos - center)/6;

		capBallSpeed(ball2Vx, ball2Vy);
	}
}

bool Screen1View::intersectBox(BoxWithBorder* b, int bX, int bY) {
	int boxX = b->getX();
	int boxY = b->getY();
	int boxWidth = b->getWidth();
	int boxHeight = b->getHeight();

	return bX + 2*ballRadius >= boxX && bX <= boxX + boxWidth &&
			bY + 2*ballRadius >= boxY && bY <= boxY + boxHeight;
}

void Screen1View::checkBlockCollisions() {
	for (int i = 0; i < 24; i++) {
		if (!blocksAlive[i]) continue;
		BoxWithBorder* b = blocks[i];

		if (intersectBox(b, ballX, ballY)) {
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

			break;
		}
	}
}

void Screen1View::checkBlockCollisions2() {
	if (!ball2Active) return;

	for (int i = 0; i < 24; i++) {
		if (!blocksAlive[i]) continue;
		BoxWithBorder* b = blocks[i];

		if (intersectBox(b, ball2X, ball2Y)) {
			int boxX = b->getX();
			int boxY = b->getY();
			int boxWidth = b->getWidth();
			int boxHeight = b->getHeight();

			int overlapLeft = (ball2X + 2*ballRadius) - boxX;
			int overlapRight = (boxX + boxWidth) - ball2X;
			int overlapTop = (ball2Y + 2*ballRadius) - boxY;
			int overlapBottom = (boxY + boxHeight) - ball2Y;

			int minOverlapX = overlapLeft < overlapRight ? overlapLeft : overlapRight;
			int minOverlapY = overlapTop < overlapBottom ? overlapTop : overlapBottom;

			if (minOverlapX < minOverlapY) ball2Vx = -ball2Vx;
			else ball2Vy = -ball2Vy;

			blocksAlive[i] = false;
			b->setVisible(blocksAlive[i]);
			b->invalidate();
			countBlocksAlive--;
			addScore(20);

			break;
		}
	}
}

void Screen1View::addScore(int points) {
	score += points;
	Unicode::snprintf(textArea1Buffer, 10, "%d", score);
	textArea1.invalidate();
}

void Screen1View::capBallSpeed(int& bVx, int& bVy) {
	int speedSq = bVx * bVx + bVy * bVy;
	const int maxSpeedSq = MAX_BALL_SPEED * MAX_BALL_SPEED;

	if (speedSq > maxSpeedSq) {
		int speed = isqrt(speedSq);

	    // Fixed-point scaling (Q8.8)
		int scale = (MAX_BALL_SPEED << 8) / speed;

	    bVx = (bVx * scale) >> 8;
	    bVy = (bVy * scale) >> 8;
	}

    if (abs(bVy) < MIN_BALL_SPEED) bVy = -MIN_BALL_SPEED;
    if (abs(bVx) < MIN_BALL_SPEED) {
    	if (bVx < 0) bVx = -MIN_BALL_SPEED;
    	else bVx = MIN_BALL_SPEED;
    }
}

void Screen1View::render() {
	circle1.moveTo(ballX, ballY);
	circle1.invalidate();

	if (ball2Active) {
		circle2.moveTo(ball2X, ball2Y);
		circle2.invalidate();
	}

	box1.moveTo(paddleX, paddleY);
	box1.invalidate();

	this->heartPowerUp.invalidate();

}

int Screen1View::getScore() const {
	return this->score;
}

void Screen1View::switchGameOverScreen() {
	application().gotoScreen3ScreenBlockTransition();
}

void Screen1View::resetBall() {
	ballX = paddleX + paddleWidth/2 - ballRadius;
	ballY = paddleY - 2*ballRadius;
	circle1.moveTo(ballX, ballY);
	circle1.setVisible(true);
	circle1.invalidate();
	begin = false;
	initBallSpeed();
}

void Screen1View::resetBall2() {
	ball2Active = false;
	circle2.setVisible(false);
	circle2.invalidate();
}

void Screen1View::gainLife() {
	if (lives >= MAX_LIVES) return;
	hearts[lives]->setVisible(true);
	hearts[lives]->invalidate();
	lives++;
}

void Screen1View::loseLife() {
	if (++delayTicks < 60) return;
	delayTicks = 0;
	lives--;
	hearts[lives]->setVisible(false);
	hearts[lives]->invalidate();

	resetBall2();

	if (lives > 0) {
		resetBall();
	} else {
		switchGameOverScreen();
	}

}

void Screen1View::newRound() {
	if (countBlocksAlive == 0) {
		begin = false;
	    if (++delayTicks < 60) return;
	    delayTicks = 0;

		resetBall();
		resetBall2();

		for (int i = 0; i < 24; i++) {
			blocksAlive[i] = true;
			blocks[i]->setVisible(blocksAlive[i]);
			blocks[i]->invalidate();
		}
		countBlocksAlive = 24;

	    Unicode::snprintf(textArea2Buffer, 10, "%d", ++round);
	    textArea2.invalidate();

	    spawnPowerUp();
	}
}

void Screen1View::spawnPowerUp() {
	//Heart Power Up
	this->blockIxWithHeartPowerUp = HAL_GetTick() % 24;
	int boxX = blocks[blockIxWithHeartPowerUp]->getX();
	int boxY = blocks[blockIxWithHeartPowerUp]->getY();
	int boxWidth = blocks[blockIxWithHeartPowerUp]->getWidth();
	int boxHeight = blocks[blockIxWithHeartPowerUp]->getHeight();

	int heartX = heartPowerUp.getX();
	int heartY = heartPowerUp.getY();
	int heartWidth = heartPowerUp.getWidth();
	int heartHeight = heartPowerUp.getHeight();

	heartX = boxX + boxWidth/2 - heartWidth/2;
	heartY = boxY + boxHeight/2 - heartHeight/2;

	this->heartPowerUp.moveTo(heartX, heartY);
	this->heartPowerUp.setVisible(true);
	this->heartPowerUp.invalidate();

	//Extend paddle (Arrow) Power Up
	do {
		this->blockIxWithArrowPowerUp = HAL_GetTick() % 24;
	} while (this->blockIxWithArrowPowerUp == this->blockIxWithHeartPowerUp);
	boxX = blocks[blockIxWithArrowPowerUp]->getX();
	boxY = blocks[blockIxWithArrowPowerUp]->getY();
	boxWidth = blocks[blockIxWithArrowPowerUp]->getWidth();
	boxHeight = blocks[blockIxWithArrowPowerUp]->getHeight();

	int arrowX = extendPowerUp.getX();
	int arrowY = extendPowerUp.getY();
	int arrowWidth = extendPowerUp.getWidth();
	int arrowHeight = extendPowerUp.getHeight();

	arrowX = boxX + boxWidth/2 - arrowWidth/2;
	arrowY = boxY + boxHeight/2 - arrowHeight/2;

	this->extendPowerUp.moveTo(arrowX, arrowY);
	this->extendPowerUp.setVisible(true);
	this->extendPowerUp.invalidate();

	//Multi Ball Power Up
	do {
		this->blockIxWithMultiBallPowerUp = HAL_GetTick() % 24;
	} while (this->blockIxWithMultiBallPowerUp == this->blockIxWithHeartPowerUp || this->blockIxWithMultiBallPowerUp == this->blockIxWithArrowPowerUp);
	boxX = blocks[blockIxWithMultiBallPowerUp]->getX();
	boxY = blocks[blockIxWithMultiBallPowerUp]->getY();

	int multiX = multiBallPowerUp.getX();
	int multiY = multiBallPowerUp.getY();

	multiX = boxX + boxWidth/2 - multiBallPowerUp.getWidth()/2;
	multiY = boxY + boxHeight/2 - multiBallPowerUp.getHeight()/2;

	this->multiBallPowerUp.moveTo(multiX, multiY);
	this->multiBallPowerUp.setVisible(true);
	this->multiBallPowerUp.invalidate();
}

void Screen1View::updatePowerUp() {
	//Update Heart
	if (!blocksAlive[blockIxWithHeartPowerUp]) {
		this->heartPowerUp.moveRelative(0, POWERUP_FALL_SPEED);
	}

	//Update Arrow
	if (!blocksAlive[blockIxWithArrowPowerUp]) {
		this->extendPowerUp.moveRelative(0, POWERUP_FALL_SPEED);
	}

	//Update Multi Ball
	if (!blocksAlive[blockIxWithMultiBallPowerUp]) {
		this->multiBallPowerUp.moveRelative(0, POWERUP_FALL_SPEED);
	}
}

bool Screen1View::intersectHeartPowerUp() {
	if (blocksAlive[blockIxWithHeartPowerUp]) return false;

	int heartPW_X = this->heartPowerUp.getX();
	int heartPW_Y = this->heartPowerUp.getY();
	int heartPW_Width = this->heartPowerUp.getWidth();
	int heartPW_Height = this->heartPowerUp.getHeight();

	return heartPW_X + heartPW_Width >= paddleX && heartPW_X <= paddleX + paddleWidth &&
				heartPW_Y + heartPW_Height >= paddleY && heartPW_Y <= paddleY + paddleHeight;

}

bool Screen1View::intersectArrowPowerUp() {
	if (blocksAlive[blockIxWithArrowPowerUp]) return false;

	int arrowX = this->extendPowerUp.getX();
	int arrowY = this->extendPowerUp.getY();
	int arrowWidth = this->extendPowerUp.getWidth();
	int arrowHeight = this->extendPowerUp.getHeight();

	return arrowX + arrowWidth >= paddleX && arrowX <= paddleX + paddleWidth &&
			arrowY + arrowHeight >= paddleY && arrowY <= paddleY + paddleHeight;

}

bool Screen1View::intersectMultiBallPowerUp() {
	if (blocksAlive[blockIxWithMultiBallPowerUp]) return false;

	int multiX = this->multiBallPowerUp.getX();
	int multiY = this->multiBallPowerUp.getY();
	int multiWidth = this->multiBallPowerUp.getWidth();
	int multiHeight = this->multiBallPowerUp.getHeight();

	return multiX + multiWidth >= paddleX && multiX <= paddleX + paddleWidth &&
			multiY + multiHeight >= paddleY && multiY <= paddleY + paddleHeight;
}

void Screen1View::checkPowerUpCollision() {
	//check Heart collision
	if (intersectHeartPowerUp()) {
		gainLife();
		this->heartPowerUp.setVisible(false);
		heartPowerUp.moveRelative(0, 30); //make the heart fall out of screen
	}

	//check Arrow collision
	if (intersectArrowPowerUp() && !isPaddleExtended) {
		paddleWidth += PADDLE_EXTENSION;
		this->box1.setWidth(paddleWidth);
		if (paddleX + paddleWidth > HAL::DISPLAY_WIDTH) {
			paddleX = HAL::DISPLAY_WIDTH - paddleWidth;
		}
		this->box1.invalidate();
		isPaddleExtended = true;
		paddleExtendStartTick = HAL_GetTick();
	}

	//check Multi Ball collision
	if (intersectMultiBallPowerUp() && !ball2Active) {
		ball2Active = true;
		ball2X = paddleX + paddleWidth/2 - ballRadius;
		ball2Y = paddleY - 2*ballRadius;
		initBall2Speed();
		circle2.setVisible(true);
		circle2.invalidate();
		this->multiBallPowerUp.setVisible(false);
		multiBallPowerUp.moveRelative(0, 30);
	}

}

void Screen1View::checkPaddleExtensionTimeout() {
	if (!isPaddleExtended) return;

	uint32_t now = HAL_GetTick();
	if (now - paddleExtendStartTick >= PADDLE_EXTENSION_DURATION) {
		isPaddleExtended = false;

		paddleWidth = paddleNormalWidth;
		this->box1.setWidth(paddleWidth);
		this->box1.invalidate();
	}
}
