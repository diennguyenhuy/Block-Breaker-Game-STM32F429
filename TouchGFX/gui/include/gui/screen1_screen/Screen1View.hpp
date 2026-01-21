#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>
#include <touchgfx/widgets/BoxWithBorder.hpp>
#include <touchgfx/widgets/Image.hpp>
#include <touchgfx/Unicode.hpp>

// Power-up types enum
enum PowerUpType {
    POWERUP_NONE = 0,
    POWERUP_EXTRA_LIFE,      // Lấy lại 1 mạng (Regain 1 life)
    POWERUP_EXTEND_PADDLE,   // Làm dài bệ phóng (Lengthen paddle)
    POWERUP_DOUBLE_BALL      // Nhân đôi bóng (Double ball)
};

// Power-up structure
struct PowerUp {
    int x;                   // X position
    int y;                   // Y position
    int width;              // Width of power-up
    int height;             // Height of power-up
    int velocityY;          // Fall speed (positive = downward)
    PowerUpType type;       // Type of power-up
    bool active;            // Is this power-up currently active/falling?
    
    PowerUp() : x(0), y(0), width(20), height(20), velocityY(2), 
                type(POWERUP_NONE), active(false) {}
};

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
    int ballVx, ballVy;
    int paddleX, paddleY, paddleWidth, paddleHeight;
    int paddleV = 3;
    bool begin;

    static const int MAX_BALL_SPEED = 3;
    static const int MIN_BALL_SPEED = 2;
    static const int START_BALL_SPEED = 3;

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
    int lives = 3;
    int score = 0;

    int delayTicks = 0;

    // Power-up system variables
    static const int MAX_POWERUPS = 5;           // Maximum simultaneous power-ups
    static const int POWERUP_SPAWN_CHANCE = 30;  // % chance to spawn on block break
    static const int POWERUP_FALL_SPEED = 2;     // Pixels per tick
    static const int POWERUP_WIDTH = 20;         // Power-up width
    static const int POWERUP_HEIGHT = 20;        // Power-up height
    
    PowerUp powerUps[MAX_POWERUPS];              // Array of power-ups
    
    // Paddle extension power-up state
    int originalPaddleWidth = 0;                 // Store original paddle width
    int paddleExtensionTimer = 0;                // Timer for paddle extension duration
    static const int PADDLE_EXTENSION_DURATION = 300;  // Ticks (~5 seconds at 60fps)
    static const int PADDLE_EXTENSION_MULTIPLIER = 15; // Pixels to add to paddle


private:
    void updatePaddle();
    void updateBall();
    void resetBall();
    void checkWallCollision();
    void checkPaddleCollision();
    void checkBlockCollisions();
    void render();
    bool intersectPaddle();
    bool intersectBox(BoxWithBorder* b);
    void capBallSpeed();
    void initBallSpeed();
    void addScore(int points);
    void loseLife();
    void newRound();
    void switchGameOverScreen();
    
    // Power-up system functions
    void spawnPowerUp(int x, int y);             // Spawn a random power-up
    void updatePowerUps();                        // Update all active power-ups
    void checkPowerUpCollisions();                // Check paddle-powerup collisions
    void applyPowerUp(PowerUpType type);          // Apply power-up effect
    bool intersectPowerUp(PowerUp* p);            // Check if paddle intersects power-up
    void updatePaddleExtension();                 // Update paddle extension timer
    void renderPowerUps();                        // Render falling power-ups on screen

};

#endif // SCREEN1VIEW_HPP
