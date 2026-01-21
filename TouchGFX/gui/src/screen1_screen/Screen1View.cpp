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

    // Initialize power-ups
    for (int i = 0; i < MAX_POWERUPS; i++) {
        powerUps[i].active = false;
    }
    originalPaddleWidth = paddleWidth;  // Store original paddle width
    heartPowerUp.setVisible(false);     // Hide heart power-up initially

    Unicode::snprintf(textArea1Buffer, 10, "%d", score);
    textArea1.invalidate();

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
	checkWallCollision();
	checkPaddleCollision();
	checkBlockCollisions();
	
	// Power-up system updates
	updatePowerUps();           // Make power-ups fall
	checkPowerUpCollisions();   // Check if paddle caught them
	updatePaddleExtension();    // Update paddle timer
	renderPowerUps();           // Display power-ups on screen
	
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
			
			// Spawn power-up at block position
			int blockCenterX = boxX + boxWidth / 2;
			int blockCenterY = boxY + boxHeight / 2;
			spawnPowerUp(blockCenterX, blockCenterY);

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
	}
}

//=============================================================================
// POWER-UP SYSTEM FUNCTIONS
//=============================================================================

/**
 * @brief Spawn a random power-up at the given position
 * @param x X position (usually block center)
 * @param y Y position (usually block position)
 */
void Screen1View::spawnPowerUp(int x, int y) {
	// Check spawn chance (e.g., 30% probability)
	int randomChance = HAL_GetTick() % 100;
	if (randomChance >= POWERUP_SPAWN_CHANCE) {
		return; // Don't spawn
	}
	
	// Find an inactive power-up slot
	PowerUp* powerUp = nullptr;
	for (int i = 0; i < MAX_POWERUPS; i++) {
		if (!powerUps[i].active) {
			powerUp = &powerUps[i];
			break;
		}
	}
	
	// No available slot
	if (powerUp == nullptr) return;
	
	// Select random power-up type (1, 2, or 3)
	int randomType = (HAL_GetTick() % 3) + 1;
	
	// Initialize the power-up
	powerUp->type = (PowerUpType)randomType;
	powerUp->x = x - POWERUP_WIDTH / 2;  // Center on block
	powerUp->y = y;
	powerUp->width = POWERUP_WIDTH;
	powerUp->height = POWERUP_HEIGHT;
	powerUp->velocityY = POWERUP_FALL_SPEED;
	powerUp->active = true;
}

/**
 * @brief Update all active power-ups (make them fall)
 * Called every tick from tickEvent()
 */
void Screen1View::updatePowerUps() {
	for (int i = 0; i < MAX_POWERUPS; i++) {
		if (!powerUps[i].active) continue;
		
		// Update position (fall down)
		powerUps[i].y += powerUps[i].velocityY;
		
		// Check if power-up fell off screen
		if (powerUps[i].y > HAL::DISPLAY_HEIGHT) {
			powerUps[i].active = false;  // Deactivate it
		}
	}
}

/**
 * @brief Check if paddle intersects with a power-up
 * @param p Pointer to the power-up
 * @return true if intersecting, false otherwise
 */
bool Screen1View::intersectPowerUp(PowerUp* p) {
	return paddleX + paddleWidth >= p->x && 
	       paddleX <= p->x + p->width &&
	       paddleY + paddleHeight >= p->y && 
	       paddleY <= p->y + p->height;
}

/**
 * @brief Check collisions between paddle and all power-ups
 * Called every tick from tickEvent()
 */
void Screen1View::checkPowerUpCollisions() {
	for (int i = 0; i < MAX_POWERUPS; i++) {
		if (!powerUps[i].active) continue;
		
		if (intersectPowerUp(&powerUps[i])) {
			// Paddle caught the power-up!
			applyPowerUp(powerUps[i].type);
			powerUps[i].active = false;  // Deactivate it
		}
	}
}

/**
 * @brief Apply the power-up effect based on type
 * @param type The type of power-up to apply
 */
void Screen1View::applyPowerUp(PowerUpType type) {
	switch (type) {
		case POWERUP_EXTRA_LIFE:
			// Add 1 life (max 3)
			if (lives < 3) {
				lives++;
				hearts[lives - 1]->setVisible(true);
				hearts[lives - 1]->invalidate();
			}
			addScore(50);  // Bonus points
			break;
			
		case POWERUP_EXTEND_PADDLE:
			// Extend paddle width
			if (originalPaddleWidth == 0) {
				// First time extending
				originalPaddleWidth = paddleWidth;
			}
			paddleWidth = originalPaddleWidth + PADDLE_EXTENSION_MULTIPLIER;
			paddleExtensionTimer = PADDLE_EXTENSION_DURATION;
			box1.setWidth(paddleWidth);
			box1.invalidate();
			addScore(30);  // Bonus points
			break;
			
		case POWERUP_DOUBLE_BALL:
			// TODO: Implement double ball in next step
			addScore(100);  // Bonus points for now
			break;
			
		default:
			break;
	}
}

/**
 * @brief Update paddle extension timer
 * Called every tick from tickEvent()
 */
void Screen1View::updatePaddleExtension() {
	if (paddleExtensionTimer > 0) {
		paddleExtensionTimer--;
		
		// Timer expired, reset paddle to original size
		if (paddleExtensionTimer == 0 && originalPaddleWidth > 0) {
			paddleWidth = originalPaddleWidth;
			box1.setWidth(paddleWidth);
			box1.invalidate();
		}
	}
}

/**
 * @brief Render power-ups on screen (make them visible!)
 * Called every tick from tickEvent()
 */
void Screen1View::renderPowerUps() {
	// Find the first active EXTRA_LIFE power-up to display
	bool heartPowerUpFound = false;
	
	for (int i = 0; i < MAX_POWERUPS; i++) {
		if (!powerUps[i].active) continue;
		
		// Only render heart power-ups for now
		if (powerUps[i].type == POWERUP_EXTRA_LIFE && !heartPowerUpFound) {
			// Move the heartPowerUp image to the power-up's position
			heartPowerUp.moveTo(powerUps[i].x, powerUps[i].y);
			heartPowerUp.setVisible(true);
			heartPowerUp.invalidate();
			heartPowerUpFound = true;
			break;  // Only show one heart at a time (since we have 1 image)
		}
	}
	
	// Hide the heart if no active heart power-up exists
	if (!heartPowerUpFound) {
		heartPowerUp.setVisible(false);
	}
}

