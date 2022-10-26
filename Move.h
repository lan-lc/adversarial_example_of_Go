#pragma once

#include <sstream>
#include <string>

enum Color {
    COLOR_NONE = 0,
    COLOR_BLACK = 1,
    COLOR_WHITE = 2,
    COLOR_SIZE = 3
};


inline Color charToColor(char c)
{
    switch (c) {
    case 'N': return COLOR_NONE; break;
    case 'B':
    case 'b': return COLOR_BLACK; break;
    case 'W':
    case 'w': return COLOR_WHITE; break;
    default: return COLOR_SIZE; break;
    }
}

inline char colorToChar(Color c)
{
    switch (c) {
    case COLOR_NONE: return 'N'; break;
    case COLOR_BLACK: return 'B'; break;
    case COLOR_WHITE: return 'W'; break;
    default: return '?'; break;
    }
}

inline Color AgainstColor(Color c)
{
    return (Color)(c ^ COLOR_SIZE);
}

const int BOARD_SIZE = 19;
const int GRID_NUM = BOARD_SIZE*BOARD_SIZE;
const int MAX_POSITION = GRID_NUM + 1;
const int PASS_POSITION = GRID_NUM;
const int MAX_GAME_LENGTH = 500;

inline std::string toSgfString(const int pos){
    std::ostringstream oss;
    int x = pos % BOARD_SIZE;
    int y = (BOARD_SIZE - 1) - (pos / BOARD_SIZE);
    oss <<(char)(x + 'a') << (char)(((BOARD_SIZE - 1) - y) + 'a');
    return oss.str();
}


class Move {
public:
    Move() {}
    Move(Color c, int position) : m_color(c), m_position(position) {}
    Move(Color c, std::string s)
    {
        m_color = c;
        if (isdigit(s[1])) {
            // GTP string
            int y = BOARD_SIZE - atoi(s.substr(1).c_str()), x = toupper(s[0]) - 'A';
            if (toupper(s[0]) > 'I') { --x; }
            m_position = y * BOARD_SIZE + x;
        } else {
            // SGF string
            int x = toupper(s[0]) - 'A';
            int y = toupper(s[1]) - 'A';
            m_position = y * BOARD_SIZE + x;
        }
    }
    ~Move() {}

    inline void reset()
    {
        m_color = COLOR_NONE;
        m_position = -1;
    }
    inline Color getColor() const { return m_color; }
    inline int getPosition() const { return m_position; }
    inline std::string toGtpString(bool withColor=false) const
    {
        std::ostringstream oss;
        if(withColor){
            oss << std::string(1, colorToChar(getColor())) + " ";
        }
        if(m_position == PASS_POSITION){
            oss << "PASS";
        }else{
            int x = m_position % BOARD_SIZE;
            int y = (BOARD_SIZE - 1) - m_position / BOARD_SIZE;
            oss << (char)(x + 'A' + (x >= 8)) << y + 1;
        }
        return oss.str();
    }
    inline std::string toSgfString() const
    {
        std::ostringstream oss;
        oss << colorToChar(m_color) << "[";
        if(m_position == PASS_POSITION){
            oss << "tt" ;
        }else{
            int x = m_position % BOARD_SIZE;
            int y = (BOARD_SIZE - 1) - m_position / BOARD_SIZE;
            oss <<(char)(x + 'a') << (char)(((BOARD_SIZE - 1) - y) + 'a');
        }
        oss << "]";
        return oss.str();
    }
    bool operator==(const Move& rhs){
        if(m_color == rhs.getColor() && m_position == rhs.getPosition()){
            return true;
        }
        return false;
    }
protected:
    Color m_color;
    int m_position;
};