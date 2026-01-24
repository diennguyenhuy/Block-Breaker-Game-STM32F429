#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>

Model::Model() : modelListener(0)
{

}

void Model::tick()
{

}

void Model::setScore(int score) {
	this->score = score;
	if (this->highScore < score) this->highScore = score;
}

int Model::getScore() const {
	return this->score;
}

int Model::getHighScore() const {
	return this->highScore;
}
