#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <string>
#include <vector>
#include "Move.h"
#include "MCTS_Result.h"
using namespace std;

const int key_length = 20;
class LookupTable {
public:
    std::map<std::string, NNResult> nnTable;
    std::map<std::string, MCTSResult> mctsTable;
    
public:
    void clear()
    {
        nnTable.clear();
        mctsTable.clear();
    }
    void save(string filename)
    {
        string nnfilename = filename + ".nn.txt";
        string mctsfilename = filename + ".mcts.txt";
        int cnt = 0;
        ofstream nnfs;
        nnfs.open(nnfilename, std::fstream::out);
        
        cout << "start saving " << nnfilename << endl;
        for (auto const& x : nnTable) {
            nnfs << x.first << endl
                 << endl;
            nnfs << x.second.toString() << endl;
            cnt++;
        }
        cout << "saving nresults " << cnt << endl;
        nnfs.close();

        cnt = 0;
        ofstream mctsfs;
        mctsfs.open(mctsfilename, std::fstream::out);
        cout << "start saving " << mctsfilename << endl;
        for (auto const& x : mctsTable) {
            mctsfs << x.first << endl
                   << endl;
            mctsfs << x.second.toString() << endl;
            cnt++;
        }
        cout << "saving mresults " << cnt << endl;
        mctsfs.close();
    }
    void load(string filename)
    {
        string nnfilename = filename + ".nn.txt";
        string mctsfilename = filename + ".mcts.txt";
        string key;

        NNResult nresult;
        ifstream nnFs(nnfilename.c_str());
        if (nnFs.good()) {
            int cnt = 0;
            cout << "start reading " << nnfilename << endl;
            while (nnFs >> key) {
                nresult.load(nnFs);
                key = key.substr(0, key_length);
                nnTable[key] = nresult;
                cnt++;
            }
            nnFs.close();
            cout << nnfilename << " load " << cnt << " nn results\n";
        } else {
            cout << "no such nn file\n";
        }

        MCTSResult mresult;
        ifstream mctsFs(mctsfilename.c_str());
        if (mctsFs.good()) {
            int cnt = 0;
            cout << "start reading " << mctsfilename << endl;
            while (mctsFs >> key) {
                mresult.load(mctsFs);
                key = key.substr(0, key_length);
                mctsTable[key] = mresult;
                cnt++;
            }
            cout << mctsfilename << " load " << cnt << " mcts results\n";
            mctsFs.close();
        } else {
            cout << "no such mcts file\n";
        }
    }
};