#include "Programs.h"
#include "SgfLoader.h"
#include "Move.h"
#include "policy_handler.h"

#include <iostream>
#include <stdio.h>      /* printf */
#include <math.h>       /* fabs, isnan*/
#include <time.h>
#include <sstream>

using namespace std;
class Fight{
public:
    vector<Move> m_game;
    vector<string> m_comments;
    Color m_winColor;
    bool m_use_MCTS;

    void initial(){
        m_game.clear();
        m_comments.clear();
    }

    void genmove(Color c, Program& p, float eps) {
        p.play(m_game, true);
        vector<float> policy;
        if(m_use_MCTS) {
            auto mresult = p.get_mcts_result();
            policy = mresult.getPolicy();
            m_comments.push_back(mresult.toString());
        }else{
            auto nresult = p.get_nn_result();
            policy = nresult.vPolicy;
            m_comments.push_back(nresult.toString());
        }

        PolicyHandler ph(policy);
        while(1){
            auto m = Move(c, ph.GetBestActionWithEps(eps));
            m_game.push_back(m);
            p.play(m_game, true);
            if(!p.isLegal()){
                ph.policy_[m.getPosition()] = 0;
                m_game.pop_back();
                m_comments.back() += m.toGtpString() + " is illegal\n";
                cout << m.getPosition() << ' '<< m.toGtpString() + " is illegal\n";

            }else{
                break;
            }
        }
    }

    Color check_game_end(Color c, Program& pB, Program& pW){
        pB.play(m_game, true);
        auto b_mresult = pB.get_mcts_result();
        pW.play(m_game, true);
        auto w_mresult = pW.get_mcts_result();
        cout << b_mresult.fValue << ' ' << w_mresult.fValue << endl;
        if(b_mresult.fValue < 0.05 && w_mresult.fValue < 0.05){
            m_comments.back() += "\n final B mcts result:\n" + b_mresult.toString();
            m_comments.back() += "\n final W mcts result:\n" + w_mresult.toString() + "\n";
            return AgainstColor(c);
        }else if(b_mresult.fValue > 0.95 && w_mresult.fValue > 0.95){
            m_comments.back() += "\n final B mcts result:\n" + b_mresult.toString();
            m_comments.back() += "\n final W mcts result:\n" + w_mresult.toString() + "\n";
            return c;
        }
        return COLOR_NONE;
    }
    
    void play_one_game(Program& pB, Program& pW) {
        initial();
        Color  c = COLOR_BLACK;
        float eps = 0.5;
        int check_feq = 200;
        for(int step=1; step <= GRID_NUM; step++){
            if(step == 40){
                eps = 0.1;
            }else if (step == 220){
                check_feq = 5;
            }
            
            if(c == COLOR_BLACK){
                genmove(c, pB, eps);
            }else{
                genmove(c, pW, eps);
            }
            
            c = AgainstColor(c);  
            if(step % check_feq == 0){
                m_winColor = check_game_end(c, pB, pW);
                if(m_winColor != COLOR_NONE){
                    break;
                }
            }
        }
    }
    string get_sgf_name(string name, int i, Program& pB, Program& pW){
        std::stringstream ss;
        ss << name + "_" + pB.m_short_name + "vs" + pW.m_short_name + "_" << i << "_";
        if(m_winColor == COLOR_BLACK){
            ss << "BW.sgf";
        }else{
            ss << "WW.sgf";
        }
        return ss.str();
    }
    float generate_games(Program& p1, Program& p2, int n, string name){
        string s;
        float bw, bt, ww, wt;
        bw = bt = ww = wt = 0;
        for(int i=0; i<n; i++){
            cout <<"generating "<< i << " game\n";
            string sgf_name;
            if (i % 2 == 0) {
                play_one_game(p1, p2);
                sgf_name = get_sgf_name(name + "_B", i/2, p1, p2);
                if(m_winColor == COLOR_BLACK){
                    bw+=1;
                }
                bt+=1;
            }else{
                play_one_game(p2, p1);
                sgf_name = get_sgf_name(name + "_W", i/2, p2, p1);
                if(m_winColor == COLOR_WHITE){
                    ww+=1;
                }
                wt+=1;
            }
            ofstream fs;
            fs.open(sgf_name);
            fs << SgfLoader::toSgfString(m_game, m_comments);
            fs.close();
            if(i>2){
                cout << "Black Winrate: " << bw/bt << ' ' << bt << endl;
                cout << "White Winrate: " << ww/wt << ' ' << wt << endl;
                cout << "Total Winrate: " << (bw+ww)/(bt+wt) << ' ' << bt+wt << endl;
            }
        }
       
        return (bw+ww)/(bt+wt);
    }
};