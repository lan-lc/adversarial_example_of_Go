#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <cmath>
#include <string>
#include <vector>
#include <iomanip>
#include "Move.h"
using namespace std;


class NNResult {
public:
    bool hasPassPolicy;
    bool hasTerritory;

    float fValue;
    int bestPos;
    float fPassPolicy;
    std::vector<float> vPolicy;
    std::vector<float> vTerritory;

    static NNResult merge(vector<NNResult> nresults){
        NNResult ret;
        ret.hasPassPolicy = nresults[0].hasPassPolicy;
        ret.hasTerritory = nresults[0].hasTerritory;
        ret.initial();
        float n = nresults.size();
        for(size_t i = 0; i<nresults.size(); i++){
            ret.fValue += nresults[i].fValue / n;
            ret.fPassPolicy += nresults[i].fPassPolicy / n;
            for(size_t j = 0; j<ret.vPolicy.size(); j++){
                ret.vPolicy[j] += nresults[i].vPolicy[j] / n;
            }
            if(ret.hasTerritory){
                for(size_t j = 0; j<ret.vPolicy.size(); j++){
                    ret.vTerritory[j] += nresults[i].vTerritory[j] / n;
                }
            }
        }
        ret.updateBestPos();
        return ret;
    }
    
    void initial(){
        fValue = 0.0;
        fPassPolicy = 0.0;
        vPolicy.assign(GRID_NUM, 0);
        if(hasTerritory){
            vTerritory.assign(GRID_NUM, 0);
        } 
    }

    NNResult()
    {
        fValue = 0.0f;
        fPassPolicy = 0.0f;
        hasPassPolicy = false;
        hasTerritory = false;
    }

    void updateBestPos()
    {
        bestPos = -1;
        for (int i = 0; i < GRID_NUM; i++) {
            if (!std::isnan(vPolicy[i])) {
                if (bestPos == -1 || vPolicy[i] > vPolicy[bestPos]) {
                    bestPos = i;
                }
            }
        }
    }
    string toString() const
    {
        std::stringstream ss;
        ss << std::setprecision(3);
        ss << "v " << fValue << endl;
        ss << "bp " << bestPos << endl;
        ss << "p ";
        for (size_t i = 0; i < vPolicy.size(); i++) {
            ss << vPolicy[i] << ' ';
        }
        ss << endl;
        if (hasTerritory) {
            ss << "t ";
            for (size_t i = 0; i < vTerritory.size(); i++) {
                ss << vTerritory[i] << ' ';
            }
            ss << endl;
        }
        if (hasPassPolicy) {
            ss << "pp " << fPassPolicy << endl;
        }
        ss << "end\n";
        return ss.str();
    }
    void load(std::istream& is)
    {
        string cmd;
        hasPassPolicy = hasTerritory = false;
        string s;
        while (is >> cmd) {
            if (cmd == "end") {
                break;
            } else if (cmd == "v") {
                is >> fValue;
            } else if (cmd == "bp") {
                is >> bestPos;
            } else if (cmd == "p") {
                vPolicy.resize(GRID_NUM);
                for (int i = 0; i < GRID_NUM; i++) {
                    is >> s;
                    vPolicy[i] = stof(s);
                }
            } else if (cmd == "t") {
                hasTerritory = true;
                vTerritory.resize(GRID_NUM);
                for (int i = 0; i < GRID_NUM; i++) {
                    is >> vTerritory[i];
                }
            } else if (cmd == "pp") {
                hasPassPolicy = true;
                is >> fPassPolicy;
            }
        }
    }
    float get_bestPos_prob()
    {
        return vPolicy[bestPos];
    }
};