#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/widgets/BoxWithBorder.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/TextArea.hpp>
#include "main.h"
#include "cmsis_os.h"
#include "math.h"

Screen1View::Screen1View()
{

}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    begin = false;

    ballX = circle1.getX();
    ballY = circle1.getY();
    circle1.getRadius(ballRadius);

    paddleX = box1.getX();
    paddleY = box1.getY();
    paddleWidth = box1.getWidth();
    paddleHeight = box1.getHeight();

    for (int i = 0; i < 24; i++) blocksAlive[i] = true;
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::tickEvent() {
	updatePaddle();
	updateBall();
	checkWallCollision();
	checkPaddleCollision();
	checkBlockCollisions();
	render();
}

extern osMessageQueueId_t buttonQueue;

void Screen1View::updatePaddle() {
	uint8_t res;
	if (osMessageQueueGetCount(buttonQueue) > 0) {
		osMessageQueueGet(buttonQueue, &res, NULL, 10);
		switch (res) {
		case 'L':
			paddleX -= paddleV;
			if (paddleX <= 0) {
				paddleX = 0;
			}
			begin = true;
			break;
		case 'R':
			paddleX += paddleV;
			if (paddleX + paddleWidth >= HAL::DISPLAY_WIDTH) {
				paddleX = HAL::DISPLAY_WIDTH - paddleWidth;
			}
			begin = true;
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
		resetBall();
	}
}

void Screen1View::resetBall() {
	ballX = paddleX + paddleWidth/2 - ballRadius;
	ballY = paddleY - 2*ballRadius;
	circle1.moveTo(ballX, ballY);
	begin = false;
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

bool Screen1View::intersectBox(touchgfx::BoxWithBorder* b) {
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
		touchgfx::BoxWithBorder* b = blocks[i];

		if (intersectBox(b)) {
			int boxX = b->getX();
			int boxY = b->getY();
			int boxWidth = b->getWidth();
			int boxHeight = b->getHeight();

			//check whether the ball hit the box from the top
			if (ballY <= boxY) {
				ballVy = -ballVy;
			}
			//check whether the ball hit the box from the bottom
			if (ballY + 2*ballRadius >= boxY + boxHeight) {
				ballVy = -ballVy;
			}
			//check whether the ball hit the box from the left
			if (ballX <= boxX) {
				ballVx = -ballVx;
			}
			//check whether the ball hit the box from the right
			if (ballX + 2*ballRadius >= boxX + boxWidth) {
				ballVx = -ballVx;
			}

			blocksAlive[i] = false;
			b->setVisible(false);
			b->invalidate();
			break;
		}
	}
}

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


void Screen1View::capBallSpeed() {
	int speedSq = ballVx * ballVx + ballVy * ballVy;
	    const int maxSpeedSq = MAX_BALL_SPEED * MAX_BALL_SPEED;

	    if (speedSq > maxSpeedSq)
	    {
	        int speed = isqrt(speedSq);

	        // Fixed-point scaling (Q8.8)
	        int scale = (MAX_BALL_SPEED << 8) / speed;

	        ballVx = (ballVx * scale) >> 8;
	        ballVy = (ballVy * scale) >> 8;
	    }
}

void Screen1View::render() {
	circle1.moveTo(ballX, ballY);
	circle1.invalidate();
	box1.moveTo(paddleX, paddleY);
	box1.invalidate();
}
