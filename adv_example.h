#include "SgfLoader.h"
#include <iostream>
#include <stdio.h>      /* printf */
#include <math.h>       /* fabs, isnan*/
#include <ctime>
#include <sstream>


using namespace std;

class AdvExample{
public:
    
    Program& m_examiner;
    Program& m_target;

    // current state s will be the state after playing m_game[0:m_step] actions
    vector<Move> m_game;
    int m_step;
    
    // results of current state
    NNResult m_ex_s_nresult; // examiner NN result on s
    MCTSResult m_ex_s_mresult; // examiner MCTS result on s
    MCTSResult m_ex_s_best_mresult; // examiner MCTS result of the state after playing the best action T(s, pi(s))
    NNResult m_tar_s_nresult; // target result on s (even if the target is MCTS agent, its result is still be stored in nn result)
    vector<bool> m_is_legal[COLOR_SIZE]; // if m_is_legal[c][p] is true, then action (c, p) is legal


    // if an action (c, p) is meaningful 
    vector<bool> m_meaningful[COLOR_SIZE];
    // store actions that are meaningless of current state
    vector<Move> m_meaningless_actions[COLOR_SIZE];
    // store all the meaningless actions of all the states in the game
    vector<Move> m_all_meaningless_actions[MAX_GAME_LENGTH][COLOR_SIZE];
    // best actions of the current state
    vector<Move> m_best_actions;

    // config:
    // the perturbed actions need to be in one of the territory
    bool m_use_territory;
    // find the policy adv example
    bool m_find_policy_example;
    // find a adv example that can atttack the rubust nn if true
    bool m_attack_robust_nn;
    float m_adv_thr; // \eta_adv
    float m_eq_thr; // \eta_eq
    float m_correct_thr; // \eta_correct

    // store the information of the adv example
    // the game id of the original state
    int m_adv_example_step;
    // store the perturbed actions
    vector<Move> m_example;
    // 1STEP or 2STEP
    int m_adv_step;
    // comment of the example, created by function save_adv_example
    string m_example_comment;
    // for saving sgf file
    vector<string> m_comments;
    // if search for adv example of robust nn store here
    NNResult m_robust_nresult;
    // total nn used
    int m_nn_cnt ;
    // total mcts used
    int m_mcts_cnt ;
    
    
    AdvExample(Program& examiner,Program& target)
        :m_examiner(examiner), m_target(target), m_find_policy_example(false)
    {
        m_adv_thr = 0.7;
        m_use_territory = true;
        m_eq_thr = 0.1;
        m_correct_thr = 0.2;
    }

    // switch to a new game
    void initial(vector<Move>& game){
        m_examiner.clear();
        m_target.clear();
        m_game = game;
        m_example.clear();
        m_nn_cnt = m_mcts_cnt = 0;

        // set all actions meaningless for the end state
        for(auto c : {COLOR_BLACK, COLOR_WHITE}){
            m_meaningful[c].resize(GRID_NUM);
            for(int i=0; i< GRID_NUM; i++){
                m_meaningful[c][i] = false;
            }
        }
        // set all_meaningless_actions to {pass}
        for(auto c: {COLOR_BLACK, COLOR_WHITE}){
            for(int i=0; i<MAX_GAME_LENGTH; i++){
                m_all_meaningless_actions[i][c].clear();
                m_all_meaningless_actions[i][c].push_back(Move(c, PASS_POSITION));
            }
        }

        // comments for output sgf
        m_comments.resize(game.size());
        for(size_t i=0; i<game.size(); i++) {
            m_comments[i] = "";
        }
        return;
    }

    string getExampleSgfString(){
        vector<string> vComments(m_example.size());
        vComments.back() = m_example_comment;
        return  SgfLoader::toSgfString(m_example, vComments);
    }
    
    Color getTerritory(int pos){
        // normally if the territory output of a position is > 0.8 or < -0.8, 
        // then it is very certain.
        float thr = 0.8;
        if(m_ex_s_nresult.vTerritory[pos] > thr){
            return COLOR_BLACK;
        }else if (m_ex_s_nresult.vTerritory[pos] < -thr){
            return COLOR_WHITE;
        }else{
            return COLOR_NONE;
        }
    }

    void get_legal_moves(){
        auto toPlay = getToPlayColor();
        auto nextPlay = AgainstColor(toPlay);
        m_is_legal[toPlay].resize(GRID_NUM);
        m_is_legal[nextPlay].resize(GRID_NUM);
        set_state(m_examiner);
        auto tmp_nresult = m_examiner.get_nn_result();
        for(int i = 0; i<GRID_NUM; i++){
            if(isnan(tmp_nresult.vPolicy[i]) or tmp_nresult.vPolicy[i] == 0){
                m_is_legal[toPlay][i] = false;
            }else{
                m_is_legal[toPlay][i] = true;
            }
        }
        set_state(m_examiner, {Move(toPlay, PASS_POSITION)});
        tmp_nresult = m_examiner.get_nn_result();
        for(int i = 0; i<GRID_NUM; i++){
            if(isnan(tmp_nresult.vPolicy[i]) or tmp_nresult.vPolicy[i] == 0){
                m_is_legal[nextPlay][i] = false;
            }else{
                m_is_legal[nextPlay][i] = true;
            }
        }
    }

    bool is_best_action(Move action){
        for(size_t i =  0; i<m_best_actions.size(); i++){
            if(action == m_best_actions[i]){
                return true;
            }
        }
        return false;
    }
    void change_state(int i) {
        m_best_actions.clear();
        m_step = i;
        set_state(m_examiner);
        m_ex_s_nresult = m_examiner.get_nn_result(); 
        set_state(m_examiner);
        m_ex_s_mresult = m_examiner.get_mcts_result();
        set_state(m_target);
        m_tar_s_nresult = m_target.get_nn_result();
        set_state(m_examiner, {m_ex_s_mresult.bestMove});
        m_ex_s_best_mresult = m_examiner.get_mcts_result();
        get_legal_moves();
        if(m_step%5 == ((int)(m_game.size()))%5){
            for(int i = 0; i < BOARD_SIZE; i++){
                for(int j=0; j< BOARD_SIZE; j++){
                    int p = i*BOARD_SIZE + j;
                    auto v = m_ex_s_nresult.vTerritory[p];
                    if(v>0.8){
                        cout<<"@";
                    }else if(v<-0.8){
                        cout<<"O";
                    }else{
                        cout<<".";
                    }
                }
                cout<<endl;
            }
        }
        switch_meaningless_actions();
    }
    
    void remove_action(Move a, vector<Move>& list){
        for(size_t i=0; i<list.size(); i++){
            if(list[i] == a){
                list[i] = list.back();
                list.pop_back();
                return;
            }
        }
        return;
    }

    bool check_meaningless(Move a){
        auto c = a.getColor();
        auto p = a.getPosition();
        if(p == PASS_POSITION){
            return true;
        }
        cout << "meaningful value of " << a.toGtpString(true) << ' ' << m_meaningful[c][p] << endl;
        if(m_meaningful[c][p]){
            return false;
        }
        vector<Move> moves;
        auto toPlay = getToPlayColor();
        auto nextPlay = AgainstColor(toPlay);
        if(c == toPlay){
            moves.push_back(a);
            moves.push_back(Move(nextPlay, PASS_POSITION));
        }else{
            moves.push_back(Move(toPlay, PASS_POSITION));
            moves.push_back(a);
        }
        set_state(m_examiner, moves);
        auto mcts_result = m_examiner.get_mcts_result(); m_mcts_cnt++;
        if(isDiff(mcts_result.fValue, m_ex_s_mresult.fValue)){
            m_meaningful[c][p] = true;
            remove_action(a, m_all_meaningless_actions[m_step][c]);
            cout << "action " << a.toGtpString(true) << "is meaningful!!\n";
        }
        return !m_meaningful[c][p];
    }
    // get the meaningless actions of the current step
    // and store it in m_meaningless_actions and m_all_meaningless_actions
    void get_meaningless_actions(){
        auto toPlay = getToPlayColor();
        auto nextPlay = AgainstColor(toPlay);
        m_meaningless_actions[toPlay].clear();
        m_meaningless_actions[nextPlay].clear();
        for(int p=0; p<GRID_NUM; p++){
            if(!territory_check(p)) continue;
            for(auto c : {COLOR_BLACK, COLOR_WHITE}){
                vector<Move> moves;
                if(!m_is_legal[c][p])continue;
                if(c == toPlay){
                    moves.push_back(Move(toPlay, p));
                    moves.push_back(Move(nextPlay, PASS_POSITION));
                }else{
                    moves.push_back(Move(toPlay, PASS_POSITION));
                    moves.push_back(Move(nextPlay, p));
                }
                moves.push_back(m_ex_s_mresult.bestMove);
                set_state(m_examiner, moves);
                if(!m_examiner.isLegal()) {
                    m_meaningful[c][p] = true;
                    continue;
                }
                auto tmp_ex_nresult = m_examiner.get_nn_result();
                bool potential_meaningful = isDiff(tmp_ex_nresult.fValue, m_ex_s_best_mresult.fValue);
                if(potential_meaningful != m_meaningful[c][p]){
                    set_state(m_examiner, moves);
                    auto mcts_result = m_examiner.get_mcts_result(); 
                    if(isDiff(mcts_result.fValue, m_ex_s_best_mresult.fValue)){
                        m_meaningful[c][p] = true;
                    }else{
                        m_meaningful[c][p] = false;
                    }
                }
                if(!m_meaningful[c][p]){
                    m_meaningless_actions[c].push_back(Move(c, p));
                }
            }
        }
        m_all_meaningless_actions[m_step][COLOR_BLACK] = m_meaningless_actions[COLOR_BLACK];
        m_all_meaningless_actions[m_step][COLOR_WHITE] = m_meaningless_actions[COLOR_WHITE];
    }
    // switch the meaningless_actions to the current state.
    // since 1STEP and 2STEP share the same meaningless actions
    void switch_meaningless_actions()
    {
        if(m_all_meaningless_actions[m_step][COLOR_BLACK][0].getPosition() == PASS_POSITION){
            get_meaningless_actions();
        }else{
            m_meaningless_actions[COLOR_BLACK] = m_all_meaningless_actions[m_step][COLOR_BLACK];
            m_meaningless_actions[COLOR_WHITE] = m_all_meaningless_actions[m_step][COLOR_WHITE];
        }
        auto toPlay = getToPlayColor();
        auto nextPlay = AgainstColor(toPlay);
        cout <<"candidates Nums(" << colorToChar(toPlay) <<", " <<  colorToChar(nextPlay) << "): " << m_meaningless_actions[toPlay].size() << ' ' << m_meaningless_actions[nextPlay].size() << endl;
    }

    bool isDiff(float v1, float v2, float thr = -1){
        if(thr < 0){
            thr = m_eq_thr;
        }
        if(v1 > 0.8 && v2 > 0.8){
            return false;
        }
        if(v1 < 0.2 && v2 < 0.2){
            return false;
        }
        auto diff = fabs(v1 - v2);
        if(diff < thr){
            return false;
        }
        return true;
    }

    
    void set_state_with_exchange(Program& agent, vector<Move> moves){
        agent.clear_board();
        for(int i=0; i<m_step-2; i++){
            agent.play(m_game[i]);
        }
        for(size_t j=0;j<moves.size();j++){
            agent.play(moves[j]);
        }
        for(int i=m_step-2; i<m_step; i++){
            agent.play(m_game[i]);
        }
    }


    Color getToPlayColor(){
        if(m_step % 2 == 0){
            return COLOR_BLACK;
        }
        return COLOR_WHITE;
    }

    bool territory_check(int pos){
        if(!m_use_territory){
            return true;
        }
        return getTerritory(pos) != COLOR_NONE;
    }
    
    // set the agent's state to the current step plus the moves
    void set_state(Program& agent, vector<Move> moves = {}){
        if(agent.m_game_i < m_step){
            for(int i=agent.m_game_i; i< m_step; i++){
                agent.m_vMoves[i] = m_game[i];
            }
        }
        agent.m_game_i = m_step;
        for(size_t j=0; j<moves.size(); j++){
            agent.m_vMoves[m_step+j] = moves[j];
        }
        agent.m_true_size = m_step + moves.size();
        agent.m_updated = false;
        agent.m_turnColor = AgainstColor(agent.m_vMoves[agent.m_true_size-1].getColor());
        return;
    }
    
    int get_meaningful_moves_cnt(Color c){
        int ret =0;
        for(int i = 0; i<GRID_NUM; i++){
            if(m_meaningful[c][i] == true) ret++;
        }
        return ret;
    }
    bool canHaveAdv(float v){
        if(!m_find_policy_example && v < 0.5){
            v = 1-v;
        }
        if(v < m_adv_thr+0.01){
            cout << "current state can't have adv " << v << endl;
            return false;
        }
        return true;
    }

    bool isEasyForTarget() {
        if(!m_find_policy_example){
            if(isDiff(m_tar_s_nresult.fValue, m_ex_s_mresult.fValue, m_correct_thr)){
                cout << "target net is bad before attack at step " << m_step << endl;
			    return false;
            }
        }else{

            if(isDiff(m_ex_s_mresult.getPosValue(m_tar_s_nresult.bestPos), m_ex_s_mresult.fValue, m_correct_thr)){
                cout << "target net policy is bad before attack at step " << m_step <<endl;
                return false;
            }
        }
        return true;
    }
    // at most call examiner's NN once => quick
    bool quick_check(NNResult& nresult){
        auto thr = m_adv_thr - m_eq_thr; 
        auto nbest = Move(getToPlayColor(), nresult.bestPos);
        set_state(m_examiner, {nbest});
        if(!m_examiner.isLegal()){ // check if the best action legal
            return false;
        }
        if(!m_find_policy_example){ // find value example
            auto diff = fabs(nresult.fValue - m_ex_s_mresult.fValue);
            return diff > thr;
        }else{
            if(is_best_action(nbest)){ // nbest is already one of the best actions
                return false;
            }
            auto Q_s_nbest = m_ex_s_mresult.getPosValue(nresult.bestPos); // get Q(s, nbest)
            auto N_s_nbest = m_ex_s_mresult.getPosSim(nresult.bestPos);  // get N(s, nbest)
            if(N_s_nbest < 5){ // never be considered during MCTS
                // set_state(m_examiner, {nbest});
                // auto tmp_nresult = m_examiner.get_nn_result(); m_nn_cnt ++;
                // Q_s_nbest = 1.0 - tmp_nresult.fValue;
                // auto diff = fabs(Q_s_nbest - m_ex_s_mresult.fValue);
                // if(diff>thr) {
                //     cout << "cal q(s, "<< nbest.toGtpString(true) <<") for missing val: " << m_ex_s_mresult.fValue << ' ' <<  Q_s_nbest <<' ';
                //     cout << m_target.m_mcts_true_cnt << ' ' << m_examiner.m_mcts_true_cnt << ' ';
                //     cout <<  m_target.m_nn_true_cnt << ' ' << m_examiner.m_nn_true_cnt << endl << flush;
                // }else if(diff < m_eq_thr){ // might be a best action
                //     cout << "cal q(s, "<< nbest.toGtpString(true) <<") for missing val: " << m_ex_s_mresult.fValue << ' ' <<  Q_s_nbest <<' ';
                //     cout << m_target.m_mcts_true_cnt << ' ' << m_examiner.m_mcts_true_cnt << ' ';
                //     cout <<  m_target.m_nn_true_cnt << ' ' << m_examiner.m_nn_true_cnt << " and add to good actions"<< endl << flush;
                //     m_best_actions.push_back(nbest);
                // }
                return true;
            }
            auto diff = fabs(Q_s_nbest - m_ex_s_mresult.fValue);
            return diff > thr;
        }
    }
    // nresult is the s' result of target agent
    // best_q is Q(s', Pi(s))
    bool check_adv_with_examiner(vector<Move>& moves, NNResult nresult, float best_q){
        if(!m_find_policy_example){ // value attack
            set_state(m_examiner, moves);
            auto mresult_adv = m_examiner.get_mcts_result(); m_mcts_cnt++; // ensure that no any other better moves due to adding "moves"
            auto V = best_q;
            if(V < mresult_adv.fValue){
                V = mresult_adv.fValue;
            }
            auto diff = fabs(nresult.fValue - V);
            cout << "final checking "<< m_step<< " value (nv, bq, ) on "; 
            for(size_t i=0; i<moves.size(); i++){
                cout << moves[i].toGtpString(true) << ' ';
            }
            cout << ": " << nresult.fValue <<' ';
            cout << best_q <<' '<< mresult_adv.fValue << endl;
            return diff > m_adv_thr;
        }else{ // policy atttack
            auto moves_nbest = moves;
            auto nbest = Move(getToPlayColor(), nresult.bestPos); 
            moves_nbest.push_back(nbest);
            set_state(m_examiner, moves_nbest);
            if(!m_examiner.isLegal()) return true; // play an illegal move 
            auto mresult_nbest = m_examiner.get_mcts_result(); m_mcts_cnt++; // ensure nbest is bad
            auto Q = 1.0 - mresult_nbest.fValue;
            auto diff = best_q - Q;
            cout << "final checking "<< m_step <<" policy (adv_nv, adv_obq, adv_nbq): " << nresult.fValue <<' ';
            cout << best_q << ' ' << Q << ' ' << nbest.toGtpString(true) << ' ' << m_ex_s_mresult.getPosValue(nresult.bestPos) << endl;
            if(diff < (m_adv_thr-m_correct_thr)){
                cout << "add " << nbest.toGtpString(true) << " to good action\n";
                m_best_actions.push_back(nbest);
            }
            return diff > m_adv_thr;
        }
    }
    string get_mcts_nn_cnt_str(){
        stringstream ss;
        ss << "examiner nn mcts cnt: " << m_examiner.m_nn_cnt <<"("<<  m_examiner.m_nn_true_cnt << ") " ;
        ss << m_examiner.m_mcts_cnt <<"("<<  m_examiner.m_mcts_true_cnt << ")\n" ;
        ss << "target nn mcts cnt: " << m_target.m_nn_cnt <<"("<<  m_target.m_nn_true_cnt << ") " ;
        ss << m_target.m_mcts_cnt <<"("<<  m_target.m_mcts_true_cnt << ")\n" ;
        return ss.str();
    }
    // nresult: the s' result of target agent
    // mresult_best: the result of examiner on T(s', Pi(s))
    void save_adv_example(vector<Move>& moves, NNResult& nresult, MCTSResult& mresult_best){
        m_adv_example_step = m_step;
        m_example.clear();
        for(int i =0 ; i< m_adv_example_step;i++){
            auto m = m_game[i];
            m_example.push_back(m);
            cout << colorToChar(m.getColor())<< ' ' << m.toGtpString() << ' ';
        }
        cout << endl;
        stringstream ss;
        if(m_find_policy_example){
            ss << "policy adv example of "<< m_adv_thr <<" at step: "<< m_adv_example_step  << endl;
        }else{
            ss << "value adv example of "<< m_adv_thr <<" at step: "<< m_adv_example_step  << endl;
        }
        ss << "adv actions: ";
        for(auto m : moves){
            m_example.push_back(m);
            ss << m.toGtpString(true) << ' ';
        }
        ss << endl;
        
        ss << "MCTS V BM BMQ: " <<  m_ex_s_mresult.fValue << ' ' << m_ex_s_mresult.bestMove.toGtpString(true);
        ss << ' ' << 1.-m_ex_s_best_mresult.fValue << endl; 
        
        set_state(m_examiner, moves);
        auto adv_mresult = m_examiner.get_mcts_result();

        ss << "ADV MCTS V BM OBMQ: " <<  adv_mresult.fValue << ' ' << adv_mresult.bestMove.toGtpString(true);
        ss << ' ' << 1. - mresult_best.fValue << endl;

        auto nnBestMove = Move(getToPlayColor(), nresult.bestPos);
        ss << "ADV Target V BM: " << nresult.fValue <<' ' << nnBestMove.toGtpString(true) << endl;

        ss << "example actions territory and diff: \n";
        for(auto m : moves){
            ss << colorToChar(m.getColor())<< ' ' << m.toGtpString() << ' ' << m_ex_s_nresult.vTerritory[m.getPosition()] << " ";
        }
        ss << endl;

        // for policy attack
        if(m_find_policy_example){
            float net_bestPos_value = adv_mresult.getPosValue(nresult.bestPos);
            ss << "nn best move P Q N: " << nnBestMove.toGtpString() << " ";
            ss << nresult.get_bestPos_prob() << ' ' << net_bestPos_value;
            ss << ' ' << adv_mresult.getPosSim(nresult.bestPos) << endl;
            {
                vector<Move> new_moves(moves);
                new_moves.push_back(Move(getToPlayColor(), nresult.bestPos));
                set_state(m_examiner, new_moves);
                auto tmp_nresult = m_examiner.get_nn_result();
                set_state(m_examiner, new_moves);
                auto tmp_mresult = m_examiner.get_mcts_result();
                ss << "after playing nn best move, value give by examiner net: " << 1.0 - tmp_nresult.fValue << endl;
                ss << "after playing nn best move, value give by examiner mcts: " << 1.0 - tmp_mresult.fValue << endl;
            }

            auto pos = adv_mresult.bestMove.getPosition();
            ss << "mcts best move P Q N: " << adv_mresult.bestMove.toGtpString();
            ss << ' ' << nresult.vPolicy[pos];
            ss << ' ' << adv_mresult.getPosValue(pos);
            ss << ' ' << adv_mresult.getPosSim(pos)<< endl;

            pos = m_ex_s_mresult.bestMove.getPosition();
            ss << "original mcts best move P Q N: " << m_ex_s_mresult.bestMove.toGtpString();
            ss << ' ' << nresult.vPolicy[pos];
            ss << ' ' << adv_mresult.getPosValue(pos);
            ss << ' ' << adv_mresult.getPosSim(pos) << endl;
        }

        ss << get_mcts_nn_cnt_str();
        ss << "]\nLB[" << toSgfString(m_ex_s_mresult.bestMove.getPosition()) <<":A][";
        ss << toSgfString(nnBestMove.getPosition())  << ":B][";
        ss << toSgfString(m_game[m_step-1].getPosition()) << ":0][";
        if(moves[0].getPosition() == PASS_POSITION){
            ss << toSgfString(moves[1].getPosition()) << ":2";
        }else if(moves[1].getPosition() == PASS_POSITION){
            ss << toSgfString(moves[0].getPosition()) << ":1";
        }else{
            ss << toSgfString(moves[0].getPosition()) << ":1][";
            ss << toSgfString(moves[1].getPosition()) << ":2";
        }
        m_example_comment = ss.str();
        cout << m_example_comment <<endl;
    }

    // check if moves, which is the perturbation, can make the target agent make a mistake
    bool check_moves(vector<Move>& moves){
        set_state(m_examiner, moves);
        if(!m_examiner.isLegal()) { // check legal
            return false;
        }
        set_state(m_target, moves);
        auto nresult = m_target.get_nn_result(); m_nn_cnt++; // get target
        if(!quick_check(nresult)){ // check is possible by only using the mcts of original state
            return false;
        }

        // check if the perturbation is meaningless V(T(s', Pi(s))) == V(T(s, Pi(s)))
        auto moves_best = moves;
        moves_best.push_back(m_ex_s_mresult.bestMove);
        set_state(m_examiner, moves_best);
        auto ex_ps_best_mresult = m_examiner.get_mcts_result(); m_mcts_cnt++;
        auto ps_best_V = ex_ps_best_mresult.fValue; // V(T(s', Pi(s)))
        if(isDiff(ps_best_V, m_ex_s_best_mresult.fValue)) { 
            cout << "moves are not meaningless " << ps_best_V << ' ';
            cout << m_ex_s_best_mresult.fValue << endl;
            if(m_adv_step == 1){ // move is not meaningless need to remove from candidates
                Move a;
                if(moves[0].getPosition() == PASS_POSITION){
                    a = moves[1];
                }else{
                    a = moves[0];
                }
                auto c = a.getColor();
                auto p = a.getPosition();
                m_meaningful[c][p] = true;
                remove_action(a, m_all_meaningless_actions[m_step][c]);
                cout << "1steps finds action " << a.toGtpString(true) << "is meaningful!!\n";
            }
            return false;
        }

        if(!canHaveAdv(1.0-ps_best_V)){
            return false;
        }
        if(!check_adv_with_examiner(moves, nresult ,1.0-ps_best_V)){
            return false;        
        }
        if(m_adv_step == 2) { // check meaningless
            cout << "start checking if meamingless " << moves.size() << endl; 
            if(!check_meaningless(moves[0]) || !check_meaningless(moves[1])){
                return false;
            }
        }
        if(!m_attack_robust_nn) {
            save_adv_example(moves, nresult, ex_ps_best_mresult);
            return true;
        }
        cout<< "start checking robust" << endl;
        set_state(m_target, moves);
        m_robust_nresult = m_target.get_robust_nn_result(); m_nn_cnt++;
        if(!check_adv_with_examiner(moves, m_robust_nresult ,1.0-ps_best_V)) {
            return false;
        }
        save_adv_example(moves, m_robust_nresult, ex_ps_best_mresult);
        return true;
    }
    // main function
    int get_adv_example(vector<Move>& game){
        if(m_find_policy_example){
            cout << "finding policy example\n";
        }else{
            cout << "finding value example\n";
        }
        cout << "thr used: "<< m_adv_thr << endl;
        initial(game);
        cout << "size: " << game.size() << endl;
        auto bt =  time(NULL);
        m_adv_step = 1;
        
        int old_nn_cnt, old_mcts_cnt;

        // 1STEP
        for(int i = m_game.size(); i>=10 && i > (int)(m_game.size()/10); i--){
            old_mcts_cnt = m_mcts_cnt;
            old_nn_cnt = m_nn_cnt;
            auto st =  time(NULL);
            change_state(i);
            cout <<"1step " << i << " tn en em eq_best: ";
            cout << m_tar_s_nresult.fValue << ' ' << m_ex_s_nresult.fValue << ' ' << m_ex_s_mresult.fValue << ' ' << 1.0 - m_ex_s_best_mresult.fValue <<endl;
            if(!canHaveAdv(m_ex_s_mresult.fValue) or !isEasyForTarget()){
                auto et =  time(NULL);
                cout << "step " << i << " use " << et-st << "sec\n\n" << flush;
                continue;
            }
            auto toPlay = getToPlayColor();
            auto nextPlay = AgainstColor(toPlay);
            auto oldGnugoCnt = m_examiner.m_gnugo->m_cnt;
            for(auto c : {toPlay, nextPlay}){
                for(size_t j=0; j<m_meaningless_actions[c].size(); j++){
                    vector<Move> moves;
                    if(c == toPlay){
                        moves.push_back(m_meaningless_actions[c][j]);
                        moves.push_back(Move(nextPlay, PASS_POSITION));
                    }else{
                        moves.push_back(Move(toPlay, PASS_POSITION));
                        moves.push_back(m_meaningless_actions[c][j]);
                    }
                    auto is_example = check_moves(moves);
                    if(is_example){
                        cout << "total nn mcts gnogo: " << m_nn_cnt - old_nn_cnt << ' ' << m_mcts_cnt - old_mcts_cnt << ' ' << m_examiner.m_gnugo->m_cnt - oldGnugoCnt << endl;
                        auto et =  time(NULL);
                        cout << "step " << i << " use " << double(et-st)  << "sec\n\n";
                        cout << "total time: " << double(et-bt)  << "sec\n\n";
                        return i;
                    }
                }
            }
            cout << "total nn mcts gnogo: " << m_nn_cnt - old_nn_cnt << ' ' << m_mcts_cnt - old_mcts_cnt << ' ' << m_examiner.m_gnugo->m_cnt - oldGnugoCnt << endl;
            auto et =  time(NULL);
            cout << "step " << i << " use " << et-st  << "sec\n\n" << flush;
        }
        cout << get_mcts_nn_cnt_str();
        auto ett =  time(NULL);
        cout << "1step total time : " << double(ett-bt)  << "sec\n\n";
        // return -1;
        // 2 steps
        m_adv_step = 2;
        for(int i = m_game.size(); i>=10 && i > (int)(m_game.size()/10); i--){
            old_mcts_cnt = m_mcts_cnt;
            old_nn_cnt = m_nn_cnt;
            auto st =  time(NULL);
            change_state(i);
            cout <<"2step " << i << " tn en em eq_best: ";
            cout << m_tar_s_nresult.fValue << ' ' << m_ex_s_nresult.fValue << ' ' << m_ex_s_mresult.fValue << ' ' << 1.0 - m_ex_s_best_mresult.fValue <<endl;
            if(!canHaveAdv(m_ex_s_mresult.fValue) or !isEasyForTarget()){
                auto et =  time(NULL);
                cout << "step " << i << " use " << double(et-st)  << "sec\n\n";
                continue;
            }
            auto toPlay = getToPlayColor();
            auto nextPlay = AgainstColor(toPlay);
            auto oldGnugoCnt = m_examiner.m_gnugo->m_cnt;
            for (auto mT : m_meaningless_actions[toPlay]){
                for (auto mN : m_meaningless_actions[nextPlay]){
                    if(mT.getPosition() == mN.getPosition()) continue;
                    vector<Move> moves({mT, mN});
                    auto is_example = check_moves(moves);
                    if(is_example){
                        cout << "total nn mcts gnogo: " << m_nn_cnt - old_nn_cnt << ' ' << m_mcts_cnt - old_mcts_cnt << ' ' << m_examiner.m_gnugo->m_cnt - oldGnugoCnt << endl;
                        auto et =  time(NULL);
                        cout << "total time : " << et-bt  << "sec\n\n";
                        return i;
                    }
                }
            }
            cout << "total nn mcts gnogo: " << m_nn_cnt - old_nn_cnt << ' ' << m_mcts_cnt - old_mcts_cnt << ' ' << m_examiner.m_gnugo->m_cnt - oldGnugoCnt << endl;
            auto et =  time(NULL);
            cout << "step " << i << " use " << et-st  << "sec\n\n" << flush;
        }
        cout << get_mcts_nn_cnt_str();
        ett =  time(NULL);
        cout << "total time : " << double(ett-bt)  << "sec\n\n";
        return -1;
    }

};
