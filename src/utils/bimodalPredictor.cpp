#include "bimodalPredictor.hpp"

BimodalPredictor::BimodalPredictor() : prediction(2) {};

bool BimodalPredictor::GetPrediction() {
    return (this->prediction >> 1);
};

void BimodalPredictor::UpdatePrediction(bool branchTaken) {
    if ((branchTaken) && (this->prediction < 3)) {
        this->prediction++;

        return;
    }

    if ((!branchTaken) && (this->prediction > 0)) {
        this->prediction--;

        return;
    }
};

BimodalPredictor::~BimodalPredictor() {};