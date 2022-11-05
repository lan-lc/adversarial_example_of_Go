#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <string>
#include <vector>
#include <iomanip>
#include "Move.h"
#include "NN_Result.h"
using namespace std;



class MCTSResult {
public:
    float fValue;
    class NodeInfo {
    public:
        Move move;
        int simulation;
        float fValue;
    };
    Move bestMove;
    std::vector<NodeInfo> vChildInfo;

public:
    NNResult toNNResult() {
        NNResult ret;
        ret.hasPassPolicy = false;
        ret.hasTerritory = false;
        ret.fValue = fValue;
        ret.vPolicy = getPolicy();
        ret.updateBestPos();
        return ret;
    }

    vector<float> getPolicy(){
        vector<float> policy;
        policy.resize(GRID_NUM,0.0);
        float sum = 0;
        for(size_t i = 0; i<vChildInfo.size(); i++){
            if(vChildInfo[i].move.getPosition() >= GRID_NUM) continue;
            sum += vChildInfo[i].simulation;
            policy[vChildInfo[i].move.getPosition()] = vChildInfo[i].simulation;
        }
        for(size_t i = 0; i<vChildInfo.size(); i++){
            if(vChildInfo[i].move.getPosition() >= GRID_NUM) continue;
            policy[vChildInfo[i].move.getPosition()] /= sum;
        }
        return policy;
    }
    void updateValue()
    {
        float total = 0;
        fValue = 0;
        for (size_t i = 0; i < vChildInfo.size(); i++) {
            fValue += (float)vChildInfo[i].simulation * vChildInfo[i].fValue;
            total += vChildInfo[i].simulation;
        }
        fValue /= total;
        return;
    }
    float getPosValue(int pos)
    {
        for (size_t i = 0; i < vChildInfo.size(); i++) {
            if (vChildInfo[i].move.getPosition() == pos) {
                return vChildInfo[i].fValue;
            }
        }
        return 0.0;
    }
    int getPosSim(int pos)
    {
        for (size_t i = 0; i < vChildInfo.size(); i++) {
            if (vChildInfo[i].move.getPosition() == pos) {
                return vChildInfo[i].simulation;
            }
        }
        return 0;
    }
    string toString() const
    {
        std::stringstream ss;
        ss << std::setprecision(3);
        ss << "v " << fValue << endl;
        ss << "bm " << bestMove.toGtpString(true) << endl;

        ss << "n " << vChildInfo.size() << endl;
        ss << "cm " << endl;
        for (size_t i = 0; i < vChildInfo.size(); i++) {
            ss << vChildInfo[i].move.toGtpString() << ' ';
        }
        ss << endl;
        ss << "cs " << endl;
        for (size_t i = 0; i < vChildInfo.size(); i++) {
            ss << vChildInfo[i].simulation << ' ';
        }
        ss << endl;
        ss << "cv " << endl;
        for (size_t i = 0; i < vChildInfo.size(); i++) {
            ss << vChildInfo[i].fValue << ' ';
        }
        ss << endl;
        ss << "end\n";
        return ss.str();
    }
    void load(std::istream& is)
    {
        string cmd;
        string c;
        Color cc = COLOR_WHITE;
        string pos;
        int n = 0;
        while (is >> cmd) {
            if (cmd == "end") {
                break;
            } else if (cmd == "v") {
                is >> fValue;
            } else if (cmd == "bm") {
                is >> c;
                if (c[0] == 'B' or c[0] == 'b') {
                    cc = COLOR_BLACK;
                }
                is >> pos;
                bestMove = Move(cc, pos);
            } else if (cmd == "n") {
                is >> n;
                vChildInfo.resize(n);
            } else if (cmd == "cm") {
                for (int i = 0; i < n; i++) {
                    is >> pos;
                    vChildInfo[i].move = Move(cc, pos);
                }
            } else if (cmd == "cs") {
                for (int i = 0; i < n; i++) {
                    is >> vChildInfo[i].simulation;
                }
            } else if (cmd == "cv") {
                for (int i = 0; i < n; i++) {
                    is >> vChildInfo[i].fValue;
                }
            }
        }
    }
};