#include <random>
#include "Move.h"
#include <iostream>
#include <stdio.h>      /* printf */
#include <math.h>       /* fabs, isnan*/
#include <time.h>
#include <sstream>


namespace random_utility {

extern std::mt19937 random_generator;
float NextReal(float f);
int NextInt(int i);
void Reset();

};

namespace random_utility {

std::mt19937 random_generator;

float NextReal(float f) {
  std::uniform_real_distribution<float> dis(0, f);
  return dis(random_generator);
}

int NextInt(int i) {
  std::uniform_int_distribution<int> dis(0, i - 1);
  return dis(random_generator);
}

void Reset() {
  std::random_device rd;
  random_generator = std::mt19937(rd());
}

};  // namespace random_utility

class PolicyHandler{
public:
    std::vector<float>& policy_;
public:
    PolicyHandler(std::vector<float>& policy) : policy_(policy) {}
    // by deteministic policy
    int GetBestAction() {
        int best_positon = -1;
        float best_val = 0;
        for (size_t i = 0; i < policy_.size(); i++) {
            if (!isnan(policy_[i]) && best_val < policy_[i]) {
                best_positon = i;
                best_val = policy_[i];
            }
        }
        return best_positon;
    }

    // by sotistic policy
    int GetAction() {
        int selected_position = -1;
        float sum = 0.0f;
        for (size_t i = 0; i < policy_.size(); i++) {
            if(!isnan(policy_[i]) && policy_[i]>0.005){
                sum += policy_[i];
                if (random_utility::NextReal(sum) < policy_[i]) {
                    selected_position = i;
                }
            }
        }
        return selected_position;
    }

    // by sotistic policy
    int GetBestActionWithEps(float eps) {
        if(random_utility::NextReal(1.0) < eps){
            return GetAction();
        }else{
            return GetBestAction();
        }
    }
};