#include "Programs.h"
#include "SgfLoader.h"
#include "adv_example.h"
#include "fight.h"
#include <iostream>
#include <time.h>

using namespace std;

void display_nn_eval(NNResult nnResult)
{
    cout << "==================" << endl;
    cout << "NN evaluation" << endl;
    cout << "value: " << nnResult.fValue << endl;
    nnResult.updateBestPos();
    auto m = Move(COLOR_BLACK, nnResult.bestPos);
    cout << "best move: " <<  m.toGtpString() <<' '<< nnResult.get_bestPos_prob() <<endl;
    cout << "policy: ";
    for (auto& p : nnResult.vPolicy) { cout << p << " "; }
    cout << endl;
    if (nnResult.hasPassPolicy) { cout << "pass policy: " << nnResult.fPassPolicy << endl; }
    if (nnResult.hasTerritory) {
        cout << "territory: ";
        for (auto& t : nnResult.vTerritory) { cout << t << " "; }
        cout << endl;
    }
}

void display_mcts_eval(MCTSResult mctsResult)
{
    cout << "==================" << endl;
    cout << "MCTS evaluation" << endl;
    cout << "Val " << mctsResult.fValue <<endl;
    cout << "best move: " << mctsResult.bestMove.toGtpString() << endl;
    cout << "node info: " << endl;
    for (auto& node : mctsResult.vChildInfo) {
        cout << "\t" << node.move.toGtpString() << " " << node.simulation << " " << node.fValue << endl;
    }
}

void run_program_test(Program& program)
{
    SgfLoader sgfLoader;
    // if (!sgfLoader.parseFromFile("./Game_011_adv.sgf")) {
    if (!sgfLoader.parseFromFile("./fox.sgf")) {
        cerr << "error for parsing sgf file" << endl;
        return;
    }

    program.run();
    program.clear_board();
    for (unsigned int i = 0; i < sgfLoader.getMoves().size(); ++i) {
        const Move& m = sgfLoader.getMoves()[i];
        program.play(m);
    }
    if (!program.update()) {
        cerr << "error during play move" << endl;
        return;
    }

    

    // test mcts eval
    display_mcts_eval(program.get_mcts_result()); // results from program
    // display_mcts_eval(program.get_mcts_result()); // results from lookup table

    // test nn eval
    display_nn_eval(program.get_nn_result()); // results from program
    // display_nn_eval(program.get_nn_result()); // results from lookup table
}

void run_illegal_move_test(Gnugo& gnugo)
{
    SgfLoader sgfLoader;
    if (!sgfLoader.parseFromFile("../test.sgf")) {
        cerr << "error for parsing sgf file" << endl;
        return;
    }
    gnugo.run();
    gnugo.clear_board();
    Color turnColor = COLOR_BLACK;
    for (unsigned int i = 0; i < sgfLoader.getMoves().size(); ++i) {
        const Move& m = sgfLoader.getMoves()[i];
        gnugo.play(m);
        turnColor = AgainstColor(turnColor);

        // showboard & test illegal move
        cout << gnugo.showboard();
        cout << "Illegal move:";
        vector<Move> vIllegal;
        for (int pos = 0; pos < GRID_NUM; ++pos) {
            Move test_move(turnColor, pos);
            if (!gnugo.isLegalMove(test_move)) { cout << " " << test_move.toGtpString(); }
        }
        cout << endl;
    }
}


vector<Program*> programs;
Gnugo* gnugo;

void run_adv_attack(bool isPolicy, float adv_thr, string name, bool use_territory, bool attack_robust_nn)
{
    cout << "starting " << name << endl;
    bool is_same = (programs[0]->m_name == programs[1]->m_name);
    for (int i = 0; i < 2; i++) {
        if (programs[i]->m_run == false) {
            programs[i]->run();
        }
        if (programs[i]->m_run == false) {
            cout << "program " << programs[i]->m_name << " run fail\n";
            return;
        }
    }
    if (gnugo != nullptr) {
        if (gnugo->run()) {
            cout << "gnugo connected\n";
        } else {
            cout << "gnugo run fail\n";
            return;
        }
    }
    cout << "programs start successfully\n";

    if (programs[0]->m_rule == false) {
        cout << "examiner must know the rules of GO\n";
        return;
    }

    AdvExample adv_example(*programs[0], *programs[1]);
    adv_example.m_find_policy_example = isPolicy;
    adv_example.m_adv_thr = adv_thr;
    adv_example.m_use_territory = use_territory;
    adv_example.m_attack_robust_nn = attack_robust_nn;

    SgfLoader sgfLoader;
    std::string filename;
    float sum_adv_pos_ratio = 0;
    int fail_mcts_cnt, fail_nn_cnt, suc_mcts_cnt, suc_nn_cnt, fail_cnt, suc_cnt, suc_1step_cnt, total_cnt, gnugo_cnt;
    fail_mcts_cnt = fail_nn_cnt = suc_mcts_cnt = suc_nn_cnt = total_cnt = fail_cnt = suc_cnt = suc_1step_cnt = gnugo_cnt = 0;
    while (cin >> filename) { // game name
        cout << "try " << filename << endl;
        if (!sgfLoader.parseFromFile(filename)) {
            cerr << "error for parsing sgf file" << endl;
            continue;
        }
        total_cnt++;
        
        for (int i = 0; i < 2; i++) {
            if (i == 1 && is_same) break;
            string lookupTableName = filename + "." + programs[i]->m_name;
            programs[i]->m_lookupTable.clear();
            programs[i]->m_lookupTable.load(lookupTableName); // load cache if exist
        }
        gnugo->clear();
        gnugo->load(filename); // load cache if exist
        
        auto step_id = adv_example.get_adv_example(sgfLoader.getMoves());
        gnugo_cnt += gnugo->m_cnt;

        if (step_id == -1) { // fail
            fail_cnt++;
            fail_nn_cnt += adv_example.m_nn_cnt;
            fail_mcts_cnt += adv_example.m_mcts_cnt;
            cout << filename << " fail!! \n\n";
        } else { // success
            suc_cnt++;
            suc_nn_cnt += adv_example.m_nn_cnt;
            suc_mcts_cnt += adv_example.m_mcts_cnt;
            if (adv_example.m_adv_step == 1) {
                suc_1step_cnt++;
                cout << "on step " << step_id << " " << filename << " 1step success!! \n\n";
            } else if (adv_example.m_adv_step == 2) {
                cout << "on step " << step_id << " " << filename << " 2step success!! \n\n";
            }
            sum_adv_pos_ratio += (float)step_id / (float)sgfLoader.getMoves().size();
            // save adv example as sgf
            ofstream adv_fs; 
            string advFileName = filename + "." + name + ".sgf";
            adv_fs.open(advFileName, std::fstream::out);
            adv_fs << adv_example.getExampleSgfString() << endl;
            adv_fs.close();
        }
        // save cache
        for (int i = 0; i < 2; i++) {
            if (i == 1 && is_same) break;
            string lookupTableName = filename + "." + programs[i]->m_name;
            programs[i]->m_lookupTable.save(lookupTableName);
        }
        gnugo->save(filename);


        cout << "gnugo check legal cnt: " << gnugo->m_cnt << "(" << gnugo->m_true_cnt << ") " << gnugo_cnt / (float) total_cnt <<endl;
        cout << "averge adv pos ratio: " << sum_adv_pos_ratio / (float) total_cnt << endl;
        cout << "suc1 suc total: " << suc_1step_cnt << ' ' << suc_cnt << ' ' << total_cnt << endl;
        cout << "suc fail total (nn, mcts) ";
        if (suc_cnt > 0) {
            cout << "success avg nn and mcts cnt: "<< "(" << suc_nn_cnt / suc_cnt << ", " << suc_mcts_cnt / suc_cnt << ") ";
        }
        if (fail_cnt > 0) {
            cout << "fail avg nn and mcts cnt: "<< "(" << fail_nn_cnt / fail_cnt << ", " << fail_mcts_cnt / fail_cnt << ") ";
        }
        cout << "(" << (suc_nn_cnt + fail_nn_cnt) / total_cnt << ", ";
        cout << (suc_mcts_cnt + fail_mcts_cnt) / total_cnt << ")\n";
    }
}


int main(int argc, char* argv[])
{
    srand (time(NULL));
    gnugo = nullptr;
    if (string(argv[1]) == "test_program") { // ./GO_attack test_program katago
        if (argc != 3){
            cout << "need argc=3, get " << argc << endl;
            return 0;
        }
        if (string(argv[2]) == "katago") {
            Program_KataGo katago("localhost", "9999" /*, true*/);
            run_program_test(katago);
        } else if (string(argv[2]) == "elfopengo") {
            Program_ELFOpenGo elfopengo("localhost", "9997" /*, true*/);
            run_program_test(elfopengo);
        } else if (string(argv[2]) == "leelazero") {
            Program_LeelaZero leelazero("localhost", "9998" /*, true*/);
            run_program_test(leelazero);
        } else if (string(argv[2]) == "gnugo") {
            Gnugo gnugo("localhost", "9990");
            run_illegal_move_test(gnugo);
        } else if (string(argv[2]) == "cgigo") {
            Program_CGIGo cgigo("localhost", "9996", true);
            run_program_test(cgigo);
        }  
    } else if (string(argv[1]) == "test_tosgf") { // ./GO_attack test_tosgf
        if (argc != 2){
            cout << "need argc=2, get " << argc << endl;
            return 0;
        }
        SgfLoader sgfLoader;
        cout << "read sgf file ..." << endl;
        if (!sgfLoader.parseFromFile("../test.sgf")) {
            cerr << "error for parsing sgf file" << endl;
            exit(-1);
        }
        cout << "sgf from sgf file: " << sgfLoader.getSgfString() << endl;

        // test function for toSgfString
        const vector<Move>& vMove = sgfLoader.getMoves();
        vector<string> vComments(vMove.size());
        vComments[0] = "first comment!";
        vComments.back() = "last comment!";
        cout << "sgf from toSgfString function: " << SgfLoader::toSgfString(vMove, vComments) << endl;
    } else if (string(argv[1]) == "adv") { // ./GO_attack adv k 9990 e 9991 psa 0.7 test_name 9992
        if (argc != 10){
            cout << "need argc=10, get " << argc << endl;
            return 0;
        }
        programs.resize(2);
        auto examiner_name = string(argv[2]);
        auto examiner_port = string(argv[3]);
        auto target_name = string(argv[4]);
        auto target_port = string(argv[5]);
        cout << examiner_name << ' ' << target_name << endl;
        if (examiner_name == "katago" || examiner_name == "k") {
            programs[0] = new Program_KataGo("localhost", examiner_port);
        } else if (examiner_name == "elfopengo" || examiner_name == "e") {
            programs[0] = new Program_ELFOpenGo("localhost", examiner_port);
        } else if (examiner_name == "leelazero" || examiner_name == "l") {
            programs[0] = new Program_LeelaZero("localhost", examiner_port);
        } else if (examiner_name == "cgigo" || examiner_name == "c") {
            programs[0] = new Program_CGIGo("localhost", examiner_port);
        }
        cout << "examiner is " << programs[0]->m_name << endl;
        if (examiner_name == target_name && examiner_port == target_port) {
            programs[1] = programs[0];
        } else {
            if (target_name == "katago" || target_name == "k") {
                programs[1] = new Program_KataGo("localhost", target_port);
            } else if (target_name == "elfopengo" || target_name == "e") {
                programs[1] = new Program_ELFOpenGo("localhost", target_port);
            } else if (target_name == "leelazero" || target_name == "l") {
                programs[1] = new Program_LeelaZero("localhost", target_port);
            } else if (target_name == "cgigo" || target_name == "c") {
                programs[1] = new Program_CGIGo("localhost", target_port);
            } else if (target_name == "katago_mcts" || target_name.substr(0,2) == "km") {
                programs[1] = new Program_KataGo("localhost", target_port);
                programs[1]->set_replace_nn_w_mcts("katago_mcts");
            } else if (target_name == "elfopengo_mcts" || target_name.substr(0,2) == "em") {
                programs[1] = new Program_ELFOpenGo("localhost", target_port);
                programs[1]->set_replace_nn_w_mcts("elfopengo_mcts");
            } else if (target_name == "leelazero_mcts" || target_name.substr(0,2) == "lm") {
                programs[1] = new Program_LeelaZero("localhost", target_port);
                programs[1]->set_replace_nn_w_mcts("leelazero_mcts");
            } else if (target_name == "cgigo_mcts" || target_name.substr(0,2) == "cm") {
                programs[1] = new Program_CGIGo("localhost", target_port);
                programs[1]->set_replace_nn_w_mcts("cgigo_mcts");
            }
        }

        cout << "target is " << programs[1]->m_name << endl;
        auto attack_type = string(argv[6]);
        bool policy_attack = false; // attack value
        cout << "attack_type " << attack_type << endl;
        if (attack_type[0] == 'P' || attack_type[0] == 'p') { // attack policy
            policy_attack = true;
        }
        bool use_territory=true; // set false for other games
        bool attack_robust_nn=false;
        if (attack_type.size() > 1) { // e.g., par pr pa pra
            if(attack_type[1] == 'a') { // use "all" meaningless actions
                use_territory = false;
            }else if (attack_type[1] == 'r') { // attack robust nn 
                attack_robust_nn = true;
            }
            if (attack_type.size() > 2) { // pas psa
                if(attack_type[2] == 'a') {
                    use_territory = false;
                }else if (attack_type[2] == 'r') {
                    attack_robust_nn = true;
                }
            }
        }
        auto adv_thr = stof(argv[7]);
        if(policy_attack){
            cout << "attack policy ";
        }else{
            cout << "attack value ";
        }
        cout << " with adv thr: " << adv_thr;
        cout << ", use_territory: " << use_territory;
        cout << ", attack_robust_nn: " << attack_robust_nn << endl;
        auto name = string(argv[8]);
        cout << "name of experiment: " << name << endl;
        auto gPort = string(argv[9]);
        gnugo = new Gnugo("localhost", gPort);
        cout << "use gnugo as rule with port " << gPort << endl;
        programs[0]->setGnugo(gnugo);
        programs[1]->setGnugo(gnugo);
        run_adv_attack(policy_attack, adv_thr, name, use_territory, attack_robust_nn);
    }
    else 
    {
        cout << "need to have argv like ./GO_attack adv k 9990 k 9990 1 0.7 test\n";
    }

    return 0;
}