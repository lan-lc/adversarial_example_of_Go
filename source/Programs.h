#pragma once

#include "LookupTable.h"
#include "Move.h"
#include "base_client.h"
#include <algorithm>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <string>
#include <vector>
using namespace std;

class Program : public BaseClient {
public:
    Color m_turnColor;
    std::string m_name;
    std::string m_short_name;
    LookupTable m_lookupTable;
    bool m_rule;
    bool m_run;
    int m_nn_cnt;
    int m_mcts_cnt;
    int m_nn_true_cnt;
    int m_mcts_true_cnt;
    bool m_updated;
    bool m_has_gnugo;
    int m_robust_shift_num;
    bool m_robust_nn;
    bool m_replace_nn_w_mcts;
    int m_game_i;
    Gnugo* m_gnugo;
    std::vector<Move> m_vMoves;
    int m_true_size;

public:
    Program(std::string sIP, std::string sPort, bool bDebug = false)
        : BaseClient(sIP, sPort, bDebug), m_rule(false), m_run(false)
    {
        clear();   
        m_has_gnugo = false;
        m_gnugo = NULL;
        m_robust_shift_num = 2;
        m_robust_nn = false;
        m_replace_nn_w_mcts = false;
        m_vMoves.resize(GRID_NUM+10);
    }

    void clear() // call when initial a game
    {
        m_nn_cnt = 0;
        m_mcts_cnt = 0;
        m_nn_true_cnt = 0;
        m_mcts_true_cnt = 0;
        m_game_i = 0;
        m_true_size = 0;
    }

    void set_replace_nn_w_mcts(string new_name)
    {
        m_replace_nn_w_mcts = true;
        m_name = new_name;
    }

    void setGnugo(Gnugo* gnugo)
    {
        m_has_gnugo = true;
        m_rule = true;
        m_gnugo = gnugo;
    }
    
    bool run()
    {
        m_run = connectToServer();
        return m_run;
    }

    void clearLoookupTable()
    {
        m_lookupTable.nnTable.clear();
        m_lookupTable.mctsTable.clear();
    }

    inline void clear_board()
    {
        m_turnColor = COLOR_BLACK;
        m_updated = false;
        m_game_i = 0;
        m_true_size = 0;
    }

    inline std::string get_play_str(const Move& m)
    {
        return "play " + m.toGtpString(true) + "\n";
    }

    bool update()
    {
        if (m_updated) {
            return true;
        }
        std::string s = "clear_board\n";
        for(int i=0; i<m_true_size; i++){
            s += get_play_str(m_vMoves[i]);
        }
        writeToServer(s);
        std::string sResult;
        bool ret = true;
        int cnt = 0;
        while (cnt < m_true_size + 1) {
            std::string sResult = readFromServerUntil("\n\n");
            for (char& c : sResult) {
                if (c == '=' || c == '?') {
                    if (c == '?') {
                        ret = false;
                    }
                    cnt++;
                }
            }
        }
        m_updated = true;
        if (!ret) {
            m_updated = false;
        }
        return ret;
    }

    bool isLegal()
    {
        if (m_updated) return true;
        auto hashStr = getHashStr();
        auto it = m_lookupTable.nnTable.find(hashStr);
        if (it != m_lookupTable.nnTable.end()) return true;
        if (m_has_gnugo) {
            return m_gnugo->check_legal(m_vMoves, m_true_size);
        }
        return update();
    }

    void play(Move m)
    {
        m_turnColor = AgainstColor(m.getColor());
        m_vMoves[m_true_size] = m;
        m_true_size++;
        m_updated = false;
    }

    void play(const std::vector<Move> moves, bool from_start = false, int limit = -1)
    {
        if (limit == -1) {
            limit = moves.size();
        }
        if (from_start) {
            clear_board();
        }
        m_game_i = 0;
        for (int i = 0; i < limit; i++) {
            play(moves[i]);
        }
    }

    std::string getHashStr()
    {
        string s;
        int i=0;
        if(m_true_size > 10){
            i = m_true_size - 10;
        }
        for(; i<m_true_size; i++){
            s += m_vMoves[i].toGtpString();
        }
        std::string reversed(s.rbegin(), s.rend());
        s = std::to_string(m_true_size) + reversed;
        return s.substr(0, key_length);
    }
    
    NNResult get_nn_result(){
        if(m_robust_nn){
            return get_robust_nn_result();
        }else{
            return get_pure_nn_result();
        }
    }

    NNResult get_pure_nn_result() // pure compared to robust
    {
        m_nn_cnt++;
        auto hashStr = getHashStr();
        auto it = m_lookupTable.nnTable.find(hashStr);
        if (it != m_lookupTable.nnTable.end()) { return it->second; }
        m_nn_true_cnt++;
        NNResult nresult;
        if(m_replace_nn_w_mcts){
            auto mresult = get_mcts_result();
            nresult = mresult.toNNResult();
        }else{
            if (!update()) {
                cerr << "illegal state for nn result!!\n";
                cin.get();
            }
            nresult = run_nn_eval();
            nresult.updateBestPos();
        }
        m_lookupTable.nnTable[hashStr] = nresult;
        return nresult;
    }

    std::vector<Move> get_changed_game(){
        std::vector<Move> tmp_game;
        for(int i=0; i<m_true_size; i++){
            tmp_game.push_back(m_vMoves[i]);
        }
        auto last_move = tmp_game[tmp_game.size()-1];
        auto last_2_move = tmp_game[tmp_game.size()-2];
        tmp_game[tmp_game.size()-1] = tmp_game[tmp_game.size()-1 - m_robust_shift_num];
        tmp_game[tmp_game.size()-2] = tmp_game[tmp_game.size()-2 - m_robust_shift_num];
        tmp_game[tmp_game.size()-1 - m_robust_shift_num] = last_move;
        tmp_game[tmp_game.size()-2 - m_robust_shift_num] = last_2_move;
        return tmp_game;
    }

    void printLegalMoves(vector<bool> v){
        for(int i=0; i<BOARD_SIZE;i++){
            for(int j=0; j<BOARD_SIZE; j++){
                if(v[i*BOARD_SIZE+j]){
                    cout << 1;
                }else{
                    cout << 0;
                }
            }
            cout <<endl;
        }
    }

    bool legal_moves_are_same(vector<bool> lm1, vector<bool> lm2){
        if(lm1.size() != lm2.size()){
            return false;
        }
        for(size_t i=0; i<lm1.size(); i++){
            if(lm1[i] != lm2[i]){
                return false;
            }
        }
        return true;
    }

    NNResult get_robust_nn_result()
    {
        auto old_nresult = get_pure_nn_result();
        if(m_true_size <= 10 || m_true_size <= m_robust_shift_num){
            return old_nresult;
        }
        auto old_legal_moves = m_gnugo->get_legal_moves(m_vMoves, m_true_size);
        std::vector<Move> old_game(m_vMoves);
        auto tmp_game = get_changed_game();
        auto legal_moves = m_gnugo->get_legal_moves(tmp_game, m_true_size);
        if(legal_moves_are_same(old_legal_moves, legal_moves)){
            int old_size = m_true_size;
            int old_game_i = m_game_i;
            play(tmp_game, true);
            auto nresult = get_pure_nn_result();
            nresult = NNResult::merge(vector<NNResult>({old_nresult, nresult}));        
            play(old_game, true);
            m_game_i = old_game_i;
            m_true_size = old_size;
            return nresult;
        }
        return old_nresult;
    }

    MCTSResult get_mcts_result()
    {
        m_mcts_cnt++;
        auto hashStr = getHashStr();
        auto it = m_lookupTable.mctsTable.find(hashStr);
        if (it != m_lookupTable.mctsTable.end()) { return it->second; }
        m_mcts_true_cnt++;
        if (!update()) {
            cerr << "illegal state for mcts result!!\n";
            cin.get();
        }
        auto mresult = run_mcts_eval();
        mresult.updateValue();
        m_lookupTable.mctsTable[hashStr] = mresult;
        return mresult;
    }

    // program-dependent function
    virtual NNResult run_nn_eval() = 0;
    virtual MCTSResult run_mcts_eval() = 0;
};

class Program_CGIGo : public Program {
public:
    Program_CGIGo(std::string sIP, std::string sPort, bool bDebug = false)
        : Program(sIP, sPort, bDebug)
    {
        m_name = "CGIGo";
        m_short_name = "C";
    }

    NNResult run_nn_eval()
    {
        writeToServer("dcnn_policy_value\n");
        std::string sResult = readFromServerUntil("\n\n");
        
        std::string sTmp;
        std::istringstream in(sResult);
        NNResult result;

        // policy
        in >> sTmp; // for "= "
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
            in >> sTmp;
            result.vPolicy.push_back(stof(sTmp));
        }

        // pass move
        in >> sTmp; // for "Pass:"
        in >> sTmp;
        result.fPassPolicy = stof(sTmp);
        result.hasPassPolicy = true;

        // value
        in >> sTmp; // for "Value:"
        in >> sTmp;
        result.fValue = stof(sTmp);

        // territory
        writeToServer("dcnn_bv_vn\n");
        sResult = readFromServerUntil("\n\n");

        in.str(sResult);
        in >> sTmp; // for "= "
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
            in >> sTmp;
            result.vTerritory.push_back(stof(sTmp));
        }
        result.hasTerritory = true;

        return result;
    }

    MCTSResult run_mcts_eval()
    {
        writeToServer("peek " + to_string(BOARD_SIZE * BOARD_SIZE + 1) + "\n");
        std::string sResult = readFromServerUntil("\n\n");

        std::string sTmp;
        std::istringstream in(sResult);
        MCTSResult result;
        int max_count = 0;
        in >> sTmp; // for "="
        while (in >> sTmp) {
            MCTSResult::NodeInfo node;
            node.move = Move(m_turnColor, sTmp);

            in >> sTmp;
            node.simulation = stoi(sTmp);
            if (node.simulation > max_count) {
                max_count = node.simulation;
                result.bestMove = node.move;
            }

            in >> sTmp;
            node.fValue = (stof(sTmp) + 1) / 2;
        }
        m_updated = false;
        return result;
    }
};

class Program_KataGo : public Program {
public:
    Program_KataGo(std::string sIP, std::string sPort, bool bDebug = false)
        : Program(sIP, sPort, bDebug)
    {
        m_name = "KataGo";
        m_short_name = "K";
        m_rule = true;
    }

    NNResult run_nn_eval()
    {
        int r = rand() % 8;
        string sCmd = "kata-raw-nn " + to_string(r) + "\n";
        cout << sCmd << endl;
        writeToServer(sCmd);
        std::string sResult = readFromServerUntil("\n\n");
        std::string sTmp;
        std::istringstream in(sResult);
        NNResult result;
        while (in >> sTmp) {
            if (sTmp == "whiteWin") {
                in >> sTmp;
                result.fValue = (m_turnColor == COLOR_WHITE) ? stof(sTmp) : 1 - stof(sTmp);
            } else if (sTmp == "policy") {
                for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
                    in >> sTmp;
                    result.vPolicy.push_back(stof(sTmp));
                }
            } else if (sTmp == "whiteOwnership") {
                for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
                    in >> sTmp;
                    // result.vTerritory.push_back((m_turnColor == COLOR_WHITE) ? stof(sTmp) : -stof(sTmp));
                    result.vTerritory.push_back(-stof(sTmp));
                }
                result.hasTerritory = true;
            }
        }
        return result;
    }

    MCTSResult run_mcts_eval()
    {
        writeToServer("genmove_analyze\n");
        std::string sResult = readFromServerUntil("\n\n");
        std::string sTmp;
        std::istringstream in(sResult);
        bool bFirst = true;
        MCTSResult result;
        MCTSResult::NodeInfo node;
        while (in >> sTmp) {
            if (sTmp == "move") {
                if (bFirst) {
                    bFirst = false;
                } else {
                    result.vChildInfo.push_back(node);
                }
                in >> sTmp;
                node.move = Move(m_turnColor, sTmp);
            } else if (sTmp == "visits") {
                in >> sTmp;
                node.simulation = stoi(sTmp);
            } else if (sTmp == "winrate") {
                in >> sTmp;
                // node.fValue = (m_turnColor == COLOR_BLACK) ? stof(sTmp) : 1 - stof(sTmp);
                node.fValue = stof(sTmp);
            } else if (sTmp == "play") {
                in >> sTmp;
                result.bestMove = Move(m_turnColor, sTmp);
                if (!bFirst) { result.vChildInfo.push_back(node); }
            }
        }
        m_updated = false;
        return result;
    }
    

private:
    void clear_cache()
    {
        writeToServer("clear_cache\n");
        readFromServerUntil("= \n\n");
    }
};


class Program_ELFOpenGo : public Program {
public:
    Program_ELFOpenGo(std::string sIP, std::string sPort, bool bDebug = false)
        : Program(sIP, sPort, bDebug)
    {
        std::cerr << "[Note] ELF OpenGo doesn't support detecting illegal move in play function." << std::endl;
        std::cerr << "[Note] ELF OpenGo doesn't support consecutive moves with the same color." << std::endl;
        m_name = "ELFGo";
        m_short_name = "E";
    }

    NNResult run_nn_eval()
    {
        writeToServer("actor\n");
        std::string sResult = readFromServerUntil("\n\n");
        std::string sTmp;
        std::istringstream in(sResult);
        NNResult result;
        while (in >> sTmp) {
            if (sTmp == "policy") {
                in >> sTmp; // for [custom_output]
                for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
                    in >> sTmp;
                    result.vPolicy.push_back(stof(sTmp));
                }
                // pass move
                in >> sTmp;
                result.fPassPolicy = stof(sTmp);
                result.hasPassPolicy = true;
            } else if (sTmp == "value") {
                in >> sTmp; // for [custom_output]
                in >> sTmp;
                result.fValue = (stof(sTmp) + 1) / 2;
                result.fValue = (m_turnColor == COLOR_BLACK) ? result.fValue : 1 - result.fValue;
            }
        }
        return result;
    }

    MCTSResult run_mcts_eval()
    {
        writeToServer("genmove " + std::string(1, colorToChar(m_turnColor)) + "\n");
        std::string sResult = readFromServerUntil("\n\n");

        std::string sTmp;
        std::istringstream in(sResult);
        MCTSResult result;
        int max_count = 0;
        while (in >> sTmp) {
            if (sTmp == "[custom_output]") {
                MCTSResult::NodeInfo node;
                in >> sTmp;
                sTmp = sTmp.substr(sTmp.find("[") + 1, sTmp.find("]") - sTmp.find("[") - 1);
                node.move = Move(m_turnColor, sTmp);

                in >> sTmp;
                node.simulation = stoi(sTmp);
                if (node.simulation > max_count) {
                    max_count = node.simulation;
                    result.bestMove = node.move;
                }

                in >> sTmp;
                node.fValue = (stof(sTmp) + 1) / 2;
                node.fValue = (m_turnColor == COLOR_BLACK) ? node.fValue : 1 - node.fValue;

                result.vChildInfo.push_back(node);
            }
        }
        m_updated = false;
        return result;
    }
};

class Program_LeelaZero : public Program {
public:
    Program_LeelaZero(std::string sIP, std::string sPort, bool bDebug = false)
        : Program(sIP, sPort, bDebug)
    {
        m_name = "LeelaZero";
        m_short_name = "L";
    }

    NNResult run_nn_eval()
    {
        writeToServer("heatmap\n");
        std::string sResult, sTmp;
        bool finished_read = false;
        while (!finished_read) {
            auto sTmp = readFromServerUntil("\n");
            sResult = sResult + sTmp;
            for (char c : sTmp) {
                if (c == '=' || c == '?') {
                    finished_read = true;
                }
            }
        }
        std::istringstream in(sResult);
        NNResult result;

        // policy
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; ++i) {
            in >> sTmp;
            result.vPolicy.push_back(stof(sTmp));
        }

        // pass move
        in >> sTmp; // for "pass:"
        in >> sTmp;
        result.fPassPolicy = stof(sTmp);
        result.hasPassPolicy = true;

        // value
        in >> sTmp; // for "winrate:"
        in >> sTmp;
        result.fValue = stof(sTmp);
        return result;
    }

    MCTSResult run_mcts_eval()
    {
        writeToServer("lz-genmove_analyze\n");
        std::string sResult, sTmp, sPre;
        bool finished_read = false;
        while (!finished_read) {
            auto sTmp = readFromServerUntil("\n\n");
            sResult = sResult + sTmp;
            for (char c : sTmp) {
                if (c == '=' || c == '?') {
                    finished_read = true;
                }
            }
        }

        std::istringstream in(sResult);
        MCTSResult result;
        int max_count = 0;
        while (in >> sTmp) {
            if (sTmp == "->") {
                MCTSResult::NodeInfo node;
                node.move = Move(m_turnColor, sPre);

                // simulation
                in >> sTmp;
                node.simulation = stoi(sTmp);
                if (node.simulation > max_count) {
                    max_count = node.simulation;
                    result.bestMove = node.move;
                }

                // win rate
                in >> sTmp;
                in >> sTmp;
                sTmp = sTmp.substr(0, sTmp.length() - 2);
                node.fValue = stof(sTmp) / 100;

                result.vChildInfo.push_back(node);
            }

            sPre = sTmp;
        }
        m_updated = false;
        return result;
    }
};