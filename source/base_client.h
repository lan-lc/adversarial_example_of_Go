#include "Move.h"
#include <algorithm>
#include <boost/asio.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <string>
#include <vector>
#include <set>

using namespace std;

class BaseClient {
protected:
    bool m_bDebug;
    const std::string SERVER_IP;
    const std::string SERVER_PORT;

    boost::asio::io_service m_io_service;
    boost::asio::ip::tcp::socket m_socket;
    

public:
    BaseClient(std::string sIP, std::string sPort, bool bDebug = false)
        : m_bDebug(bDebug), SERVER_IP(sIP), SERVER_PORT(sPort), m_socket(m_io_service)
    {
        
    }

    virtual ~BaseClient() {}
    virtual bool run() = 0;

protected:
    bool connectToServer()
    {
        try {
            boost::asio::ip::tcp::resolver resolver(m_io_service);
            boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), SERVER_IP.c_str(), SERVER_PORT.c_str());
            boost::asio::connect(m_socket, resolver.resolve(query));
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
        return true;
    }

    bool writeToServer(std::string sResult)
    {
        if (m_bDebug) { std::cerr << "write to server: \"" << sResult << "\""; }
        return (sResult.length() == boost::asio::write(m_socket, boost::asio::buffer(sResult.c_str(), sResult.length())));
    }

    std::string readFromServerUntil(std::string sEnd)
    {
        boost::asio::streambuf streambuf;
        boost::asio::read_until(m_socket, streambuf, sEnd);

        boost::asio::streambuf::const_buffers_type bufs = streambuf.data();
        std::string sCommand(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + streambuf.size());
        if (m_bDebug) { std::cerr << "read from server: \"" << sCommand << "\"" << std::endl; }

        return sCommand;
    }
};

class Gnugo : public BaseClient {
public:
    int m_cnt;
    int m_true_cnt;
    std::map<std::string, bool> m_isLegal;

public:
    
    Gnugo(std::string sIP, std::string sPort)
        : BaseClient(sIP, sPort) {
        clear();
    }

    void clear(){
        m_cnt = 0;
        m_true_cnt = 0;
        m_isLegal.clear();
    }


    inline bool run() { return connectToServer(); }

    inline string showboard()
    {
        writeToServer("showboard\n");
        std::string sResult = readFromServerUntil("\n\n");
        sResult = sResult.substr(sResult.find("\n") + 1);
        return sResult;
    }

    inline void clear_board()
    {
        writeToServer("clear_board\n");
        readFromServerUntil("= \n\n");
    }

    inline bool play(const Move& m)
    {
        writeToServer("play " + m.toGtpString(true) + "\n");
        std::string sResult = readFromServerUntil("\n\n");
        return (sResult[0] == '=');
    }

    inline void undo()
    {
        writeToServer("undo\n");
        readFromServerUntil("\n\n");
    }

    inline bool isLegalMove(const Move& m)
    {
        bool bLegal = play(m);
        if (bLegal) { undo(); }
        return bLegal;
    }

    inline std::string get_play_str(const Move& m)
    {
        return "play " + m.toGtpString(true) + "\n";
    }
    
    vector<bool> get_legal_moves(const vector<Move>& moves, int size = -1){
        if(size == -1){
            size = moves.size();
        }
        std::vector<bool> ret;
        ret.resize(GRID_NUM, false);
        if(!check_legal(moves, size)) { return ret; }
        auto toPlay = AgainstColor(moves[size-1].getColor());
        for(int i=0; i<GRID_NUM; i++){
            auto m = Move(toPlay, i);
            ret[i] = isLegalMove(m);
        }
        return ret;
    }
    string getHashStr(const vector<Move>& moves, int size){
        string s;
        int i=0;
        if(size > 10){
            i = size - 10;
        }
        for(; i < size; i++){
            s += moves[i].toGtpString();
        }
        std::string reversed(s.rbegin(), s.rend());
        return std::to_string(size) + reversed;
    }

    bool check_legal(const vector<Move>& moves, int size = -1)
    {
        if(size == -1){
            size = moves.size();
        }
        m_cnt ++;
        auto hashStr = getHashStr(moves, size);
        auto it  = m_isLegal.find(hashStr);
        if (it != m_isLegal.end()) { return it->second; }
        m_true_cnt ++;
        std::string s = "clear_board \n";
        for (int i=0; i<size; i++) {
            s += get_play_str(moves[i]);
        }
        writeToServer(s);
        std::string sResult;
        bool ret = true;
        int cnt = 0;
        while (cnt < size + 1) {
            std::string sResult = readFromServerUntil("\n");
            for (char& c : sResult) {
                if (c == '=' || c == '?') {
                    if (c == '?') {
                        ret = false;
                    }
                    cnt++;
                }
            }
        }
        m_isLegal[hashStr] = ret;
        return ret;
    }

    void save(string filename){
        string ggfilename = filename + ".gnugo.txt";
        int cnt = 0;
        ofstream ggfs;
        ggfs.open(ggfilename, std::fstream::out);
        cout << "start saving " << ggfilename << endl;
        for (auto const& x : m_isLegal) {
            ggfs << x.first << ' '  << x.second << endl;
            cnt++;
        }
        cout << "saving gg bools " << cnt << endl;
        ggfs.close();
    }

    void load(string filename){
        string ggfilename = filename + ".gnugo.txt";
        ifstream ggfs;
        ggfs.open (ggfilename, std::ifstream::in);
        if(ggfs.good()){
            string hashStr;
            bool val;
            int cnt;
            cout << "start reading " << ggfilename << endl;
            while(ggfs >> hashStr){
                ggfs >> val;
                m_isLegal[hashStr] = val; 
                cnt++;
            }
            cout << ggfilename << " load " << cnt << " gg results\n";
        }else{
            cout << "no such nn file\n";
        }
            
    }
};
