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
extern RNG_HandleTypeDef hrng;

static uint32_t gen_rand() {
	uint32_t random;
	while (HAL_RNG_GenerateRandomNumber(&hrng, &random) != HAL_OK);
	return random;
}

void Ball::initSpeed() {
	int i = gen_rand() % (sizeof(launchTable)/sizeof(launchTable[0]));
	vx = launchTable[i][0];
	vy = launchTable[i][1];

	capSpeed();
}

void Ball::capSpeed() {
    int speedSq = vx * vx + vy * vy;

    if (speedSq == 0) return;

    int speed = isqrt(speedSq);

    // Clamp max speed
    if (speed > MAX_BALL_SPEED)
    {
        vx = vx * MAX_BALL_SPEED / speed;
        vy = vy * MAX_BALL_SPEED / speed;
    }

    // Enforce minimum vertical speed
    if (abs(vy) < MIN_BALL_SPEED)
        vy = (vy < 0) ? -MIN_BALL_SPEED : MIN_BALL_SPEED;

    // Enforce minimum horizontal speed
    if (abs(vx) < 1)
        vx = (HAL_GetTick() & 1) ? 1 : -1;
}

void Ball::normalizeSpeed()
{
    int speedSq = vx * vx + vy * vy;
    if (speedSq == 0) {
    	vx = START_BALL_SPEED;
    	vy = START_BALL_SPEED;
    }

    int speed = isqrt(speedSq);

    vx = vx * MAX_BALL_SPEED / speed;
    vy = vy * MAX_BALL_SPEED / speed;
}


Screen1View::Screen1View() : ball(circle1), paddle(box1, 3)
{

}

Ball::Ball(Circle& c) : ballObject(c) {
	this->x = c.getX();
	this->y = c.getY();
	c.getRadius(this->r);
	initSpeed();
}

Paddle::Paddle(Box& b, int v) : v(v), paddleObject(b) {
	this->x = b.getX();
	this->y = b.getY();
	this->w = b.getWidth();
	this->h = b.getHeight();
	this->normalw = w;
	this->isExtended = false;
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    begin = false;

    for (int i = 0; i < 24; i++) blocksAlive[i] = true;

    Unicode::snprintf(textArea1Buffer, TEXTAREA1_SIZE, "%d", score);
    textArea1.invalidate();

    Unicode::snprintf(textArea2Buffer, TEXTAREA2_SIZE, "%d", ++round);
    textArea2.invalidate();

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
	updatePowerUp();
	checkWallCollision();
	checkPaddleCollision();
	checkBlockCollisions();
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
			paddle.x -= paddle.v;
			if (paddle.x <= 0) {
				paddle.x = 0;
			} else begin = true;
			break;
		case 'R':
			paddle.x += paddle.v;
			if (paddle.x + paddle.w > HAL::DISPLAY_WIDTH) {
				paddle.x = HAL::DISPLAY_WIDTH - paddle.w;
			} else begin = true;
			break;
		default:
			break;
		}
	}
}

void Screen1View::updateBall() {
	if (begin) {
		ball.x += ball.vx;
		ball.y += ball.vy;
	}
}

void Screen1View::checkWallCollision() {
	if (ball.x <= 0) {
		ball.x = 0;
		ball.vx = abs(ball.vx);
	} else if (ball.x + 2*ball.r >= HAL::DISPLAY_WIDTH) {
		ball.x = HAL::DISPLAY_WIDTH - 2*ball.r;
		ball.vx = -abs(ball.vx);
	}

	if (ball.y <= 0) {
		ball.y = 0;
		ball.vy = abs(ball.vy);
	} else if (ball.y + 2*ball.r >= HAL::DISPLAY_HEIGHT) {
		//resetBall();
		loseLife();
	}
}

bool Screen1View::intersectPaddle() {
	return ball.x + 2*ball.r >= paddle.x && ball.x <= paddle.x + paddle.w &&
			ball.y + 2*ball.r >= paddle.y && ball.y <= paddle.y + paddle.h;
}

void Screen1View::checkPaddleCollision() {
	if (intersectPaddle() && ball.vy > 0) {
        //ball.y = paddle.y - 2 * ball.r - 1;

        int hitPos = (ball.x + ball.r) - paddle.x;
        int center = paddle.w / 2;
        int dx = hitPos - center;

        // Clamp hit distance
        if (dx > center) dx = center;
        if (dx < -center) dx = -center;

        // Change direction only
        ball.vx = dx * Ball::MAX_BALL_SPEED / center;
        ball.vy = -abs(ball.vy);

        ball.vx += (HAL_GetTick() % 3) - 1;

		ball.normalizeSpeed();
	}
    // Prevent flat vertical shots
    if (ball.vx == 0) ball.vx = (HAL_GetTick() & 1) ? Ball::MIN_BALL_SPEED : -Ball::MIN_BALL_SPEED;
	//Prevent straight horizontal shots
	if (ball.vy == 0) ball.vy = -Ball::MIN_BALL_SPEED;
}

bool Screen1View::intersectBox(BoxWithBorder* b) {
	int boxX = b->getX();
	int boxY = b->getY();
	int boxWidth = b->getWidth();
	int boxHeight = b->getHeight();

	return ball.x + 2*ball.r >= boxX && ball.x <= boxX + boxWidth &&
			ball.y + 2*ball.r >= boxY && ball.y <= boxY + boxHeight;
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

			int overlapLeft = (ball.x + 2*ball.r) - boxX;
			int overlapRight = (boxX + boxWidth) - ball.x;
			int overlapTop = (ball.y + 2*ball.r) - boxY;
			int overlapBottom = (boxY + boxHeight) - ball.y;

			int minOverlapX = overlapLeft < overlapRight ? overlapLeft : overlapRight;
			int minOverlapY = overlapTop < overlapBottom ? overlapTop : overlapBottom;

			if (minOverlapX < minOverlapY) ball.vx = -ball.vx;
			else ball.vy = -ball.vy;

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
	Unicode::snprintf(textArea1Buffer, TEXTAREA1_SIZE, "%d", score);
	textArea1.invalidate();
}

void Screen1View::render() {
	ball.ballObject.moveTo(ball.x, ball.y);
	ball.ballObject.invalidate();
	paddle.paddleObject.moveTo(paddle.x, paddle.y);
	paddle.paddleObject.invalidate();

	this->heartPowerUp.invalidate();
	this->extendPowerUp.invalidate();

}

int Screen1View::getScore() const {
	return this->score;
}

void Screen1View::switchGameOverScreen() {
	application().gotoScreen3ScreenBlockTransition();
}

void Screen1View::resetBall() {
	ball.x = paddle.x + paddle.w/2 - ball.r;
	ball.y = paddle.y - 2*ball.r;
	ball.ballObject.moveTo(ball.x, ball.y);
	begin = false;
	ball.initSpeed();
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
	this->blockIxWithHeartPowerUp = gen_rand() % 24;
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
		this->blockIxWithArrowPowerUp = gen_rand() % 24;
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
}

bool Screen1View::intersectHeartPowerUp() {
	if (blocksAlive[blockIxWithHeartPowerUp]) return false;

	int heartPW_X = this->heartPowerUp.getX();
	int heartPW_Y = this->heartPowerUp.getY();
	int heartPW_Width = this->heartPowerUp.getWidth();
	int heartPW_Height = this->heartPowerUp.getHeight();

	return heartPW_X + heartPW_Width >= paddle.x && heartPW_X <= paddle.x + paddle.w &&
				heartPW_Y + heartPW_Height >= paddle.y && heartPW_Y <= paddle.y + paddle.h;

}

bool Screen1View::intersectArrowPowerUp() {
	if (blocksAlive[blockIxWithArrowPowerUp]) return false;

	int arrowX = this->extendPowerUp.getX();
	int arrowY = this->extendPowerUp.getY();
	int arrowWidth = this->extendPowerUp.getWidth();
	int arrowHeight = this->extendPowerUp.getHeight();

	return arrowX + arrowWidth >= paddle.x && arrowX <= paddle.x + paddle.w &&
			arrowY + arrowHeight >= paddle.y && arrowY <= paddle.y + paddle.h;

}

void Screen1View::checkPowerUpCollision() {
	//check Heart collision
	if (intersectHeartPowerUp()) {
		gainLife();
		this->heartPowerUp.setVisible(false);
		this->heartPowerUp.moveRelative(0, 30); //make the heart fall out of screen
	}

	//check Arrow collision
	if (intersectArrowPowerUp() && !paddle.isExtended) {
		paddle.w += Paddle::PADDLE_EXTENSION;
		paddle.paddleObject.setWidth(paddle.w);
		paddle.x -= Paddle::PADDLE_EXTENSION/2;
		paddle.paddleObject.setX(paddle.x);

		if (paddle.x + paddle.w > HAL::DISPLAY_WIDTH) {
			paddle.x = HAL::DISPLAY_WIDTH - paddle.w;
		}
		paddle.paddleObject.invalidate();
		paddle.isExtended = true;
		paddle.extendStartTick = HAL_GetTick();

		this->extendPowerUp.setVisible(false);
		this->extendPowerUp.moveRelative(0, 30); //make the arrow fall out of screen
	}

}

void Screen1View::checkPaddleExtensionTimeout() {
	if (!paddle.isExtended) return;

	uint32_t now = HAL_GetTick();
	if (now - paddle.extendStartTick >= Paddle::PADDLE_EXTENSION_DURATION) {
		paddle.isExtended = false;
		paddle.paddleObject.setVisible(false);
		paddle.paddleObject.invalidate();

		paddle.w = paddle.normalw;
		paddle.x += Paddle::PADDLE_EXTENSION/2;
		paddle.paddleObject.setX(paddle.x);
		paddle.paddleObject.setWidth(paddle.w);
		paddle.paddleObject.setVisible(true);
		paddle.paddleObject.invalidate();
	}
}
