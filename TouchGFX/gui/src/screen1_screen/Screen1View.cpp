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
static int soundTicks = 0;

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

static void startSound() {
	HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);
	soundTicks = 0;
	soundTicks++;
}

static const int MAX_SOUND_TICKS = 3;

static void endSound() {
	if (++soundTicks > MAX_SOUND_TICKS) {
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);
	}
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

    // Clamp max speed first
    if (speed > MAX_BALL_SPEED)
    {
        vx = vx * MAX_BALL_SPEED / speed;
        vy = vy * MAX_BALL_SPEED / speed;
        speed = MAX_BALL_SPEED;  // Update current speed after clamping
    }

    // If speed is below minimum, ramp it up to minimum
    else if (speed < MIN_BALL_SPEED)
    {
        vx = vx * MIN_BALL_SPEED / speed;
        vy = vy * MIN_BALL_SPEED / speed;
        speed = MIN_BALL_SPEED;  // Update current speed after ramping up
    }

    // Enforce minimum vertical speed component
    if (abs(vy) < MIN_BALL_SPEED) {
        vy = (vy < 0) ? -MIN_BALL_SPEED : MIN_BALL_SPEED;
    }

    // Enforce minimum horizontal speed component
    if (abs(vx) < 1) vx = (HAL_GetTick() & 1) ? 1 : -1;
}

void Ball::normalizeSpeed()
{
    int speedSq = vx * vx + vy * vy;
    if (speedSq == 0) {
        vx = START_BALL_SPEED;
        vy = START_BALL_SPEED;
        return;
    }

    int speed = isqrt(speedSq);

    // Normalize to MAX_BALL_SPEED to maintain consistent energy
    vx = vx * MAX_BALL_SPEED / speed;
    vy = vy * MAX_BALL_SPEED / speed;
}


Screen1View::Screen1View() : mainBall(circle1, circle1Painter), powerUpBall(circle2, circle2Painter), paddle(box1, 3)
{
	balls[0] = &mainBall;
	balls[1] = &powerUpBall;
}

Ball::Ball(Circle& c, PainterRGB565& p) : ballObject(c), ballObjectPainter(p) {
	this->x = c.getX();
	this->y = c.getY();
	c.getRadius(this->r);
	initSpeed();
	this->begin = false;
	this->alive = true;
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
	updateBalls();
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
			} else mainBall.begin = true;
			break;
		case 'R':
			paddle.x += paddle.v;
			if (paddle.x + paddle.w > HAL::DISPLAY_WIDTH) {
				paddle.x = HAL::DISPLAY_WIDTH - paddle.w;
			} else mainBall.begin = true;
			break;
		default:
			break;
		}
	}
}

void Screen1View::updateBalls() {
	for (int i = 0; i < MAX_NUM_BALLS; i++) {
		if (balls[i]->begin) {
			balls[i]->x += balls[i]->vx;
			balls[i]->y += balls[i]->vy;
		}
	}
}

void Screen1View::checkWallCollision() {
	int ballsAlive = 0;
	for (int i = 0; i < MAX_NUM_BALLS; i++) {

		if (balls[i]->x <= 0) {
			balls[i]->x = 0;
			balls[i]->vx = abs(balls[i]->vx);
			if (balls[i]->alive) startSound();
		} else if (balls[i]->x + 2*balls[i]->r >= HAL::DISPLAY_WIDTH) {
			balls[i]->x = HAL::DISPLAY_WIDTH - 2*balls[i]->r;
			balls[i]->vx = -abs(balls[i]->vx);
			if (balls[i]->alive) startSound();
		}

		if (balls[i]->y <= 0) {
			balls[i]->y = 0;
			balls[i]->vy = abs(balls[i]->vy);
			startSound();
		} else if (balls[i]->y + 2*balls[i]->r >= HAL::DISPLAY_HEIGHT) {
			balls[i]->alive = false;
		}
		if (balls[i]->alive) ballsAlive++;
	}

	if (ballsAlive == 0) loseLife();
}

bool Screen1View::intersectPaddle(const Ball* ball) {
	return ball->x + 2*ball->r >= paddle.x && ball->x <= paddle.x + paddle.w &&
			ball->y + 2*ball->r >= paddle.y && ball->y <= paddle.y + paddle.h;
}


void Screen1View::checkPaddleCollision() {
    for (int i = 0; i < MAX_NUM_BALLS; i++) {
        if (intersectPaddle(balls[i]) && balls[i]->vy > 0) {
            startSound();
            int hitPos = (balls[i]->x + balls[i]->r) - paddle.x;
            int center = paddle.w / 2;
            int dx = hitPos - center;

            // Clamp hit distance
            if (dx > center) dx = center;
            if (dx < -center) dx = -center;

            // Calculate new direction based on hit position
            balls[i]->vx = dx * Ball::MAX_BALL_SPEED / center;
            balls[i]->vy = -abs(balls[i]->vy);  // Ensure upward movement

            // Add small random variation BEFORE normalization
            int randomOffset = (HAL_GetTick() % 3) - 1;
            balls[i]->vx += randomOffset;

            // Normalize to max speed to maintain energy after paddle hit
            balls[i]->normalizeSpeed();

            // Final safety checks to prevent zero velocities
            if (balls[i]->vx == 0) {
                balls[i]->vx = (HAL_GetTick() & 1) ? Ball::MIN_BALL_SPEED : -Ball::MIN_BALL_SPEED;
            }
            if (balls[i]->vy == 0) {
                balls[i]->vy = -Ball::MIN_BALL_SPEED;
            }
        }

        // Apply capSpeed to ensure velocity bounds are maintained after any modifications
        balls[i]->capSpeed();
    }
}

bool Screen1View::intersectBox(const BoxWithBorder* block, const Ball* ball) {
	int boxX = block->getX();
	int boxY = block->getY();
	int boxWidth = block->getWidth();
	int boxHeight = block->getHeight();

	return ball->x + 2*ball->r >= boxX && ball->x <= boxX + boxWidth &&
			ball->y + 2*ball->r >= boxY && ball->y <= boxY + boxHeight;
}

void Screen1View::checkBlockCollisions() {
	for (int i = 0; i < MAX_NUM_BALLS; i++) {
		Ball* ball = balls[i];
		if (ball->begin == false) continue;

		for (int j = 0; j < 24; j++) {
			if (!blocksAlive[j]) continue;
			BoxWithBorder* b = blocks[j];

			if (intersectBox(b, ball)) {
				startSound();

				int boxX = b->getX();
				int boxY = b->getY();
				int boxWidth = b->getWidth();
				int boxHeight = b->getHeight();

				int overlapLeft = (ball->x + 2*ball->r) - boxX;
				int overlapRight = (boxX + boxWidth) - ball->x;
				int overlapTop = (ball->y + 2*ball->r) - boxY;
				int overlapBottom = (boxY + boxHeight) - ball->y;

				int minOverlapX = overlapLeft < overlapRight ? overlapLeft : overlapRight;
				int minOverlapY = overlapTop < overlapBottom ? overlapTop : overlapBottom;

				if (minOverlapX < minOverlapY) ball->vx = -ball->vx;
				else ball->vy = -ball->vy;

				blocksAlive[j] = false;
				b->setVisible(blocksAlive[j]);
				b->invalidate();
				countBlocksAlive--;
				addScore(this->scores[j]);

				if (j == blockIxWithDoubleBallPowerUp) {
					powerUpBall.begin = true;
					powerUpBall.alive = true;
					powerUpBall.x = paddle.x + paddle.w/2 - powerUpBall.r;
					powerUpBall.y = paddle.y - 2*powerUpBall.r;
					powerUpBall.ballObject.moveTo(powerUpBall.x, powerUpBall.y);
					powerUpBall.ballObjectPainter.setColor(blocks[blockIxWithDoubleBallPowerUp]->getColor());
					powerUpBall.ballObject.invalidate();
				}

				break;
			}
		}
	}
}

void Screen1View::addScore(int points) {
	score += points;
	Unicode::snprintf(textArea1Buffer, TEXTAREA1_SIZE, "%d", score);
	textArea1.invalidate();
}

void Screen1View::render() {
	for (int i = 0; i < MAX_NUM_BALLS; i++) {
		if (balls[i]->begin == false) continue;
		balls[i]->ballObject.moveTo(balls[i]->x, balls[i]->y);
		balls[i]->ballObject.invalidate();
	}

	paddle.paddleObject.moveTo(paddle.x, paddle.y);
	paddle.paddleObject.invalidate();

	this->heartPowerUp.invalidate();
	this->extendPowerUp.invalidate();

	endSound();

}

void Screen1View::switchGameOverScreen() {
	application().gotoScreen3ScreenBlockTransition();
}

void Screen1View::resetBall() {
	mainBall.x = paddle.x + paddle.w/2 - mainBall.r;
	mainBall.y = paddle.y - 2*mainBall.r;
	mainBall.ballObject.moveTo(mainBall.x, mainBall.y);
	mainBall.begin = false;
	mainBall.alive = true;
	mainBall.initSpeed();
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
		balls[0]->begin = false;
		balls[1]->begin = false;
	    if (++delayTicks < 60) return;
	    delayTicks = 0;

		resetBall();
		for (int i = 0; i < 24; i++) {
			blocksAlive[i] = true;
			blocks[i]->setVisible(blocksAlive[i]);
			blocks[i]->invalidate();
		}
		countBlocksAlive = 24;

	    Unicode::snprintf(textArea2Buffer, TEXTAREA2_SIZE, "%d", ++round);
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

	//Double ball Power Up
	do {
		this->blockIxWithDoubleBallPowerUp = gen_rand() % 24;
	} while (blockIxWithDoubleBallPowerUp == blockIxWithArrowPowerUp || blockIxWithDoubleBallPowerUp == blockIxWithHeartPowerUp);

	boxX = blocks[blockIxWithDoubleBallPowerUp]->getX();
	boxY = blocks[blockIxWithDoubleBallPowerUp]->getY();
	boxWidth = blocks[blockIxWithDoubleBallPowerUp]->getWidth();
	boxHeight = blocks[blockIxWithDoubleBallPowerUp]->getHeight();

	powerUpBall.x = boxX + boxWidth/2 - powerUpBall.r;
	powerUpBall.y = boxY + boxHeight/2 - powerUpBall.r;
	powerUpBall.alive = false;
	powerUpBall.begin = false;

	powerUpBall.ballObject.moveTo(powerUpBall.x, powerUpBall.y);
	powerUpBall.ballObjectPainter.setColor(colortype(0xFFFFFFFF));
	powerUpBall.ballObject.invalidate();
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

		if (paddle.x < 0) {
			paddle.x = 0;
		}

		if (paddle.x + paddle.w > HAL::DISPLAY_WIDTH) {
			paddle.x = HAL::DISPLAY_WIDTH - paddle.w;
		}

		paddle.paddleObject.setX(paddle.x);
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
