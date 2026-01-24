#ifndef MODEL_HPP
#define MODEL_HPP

class ModelListener;

class Model
{
public:
    Model();

    void bind(ModelListener* listener)
    {
        modelListener = listener;
    }

    void tick();

    void setScore(int score);
    int getScore() const;

    int getHighScore() const;
protected:
    ModelListener* modelListener;
    int score;
    int highScore = 0;
};

#endif // MODEL_HPP
