#pragma once

#include "Move.h"
#include <fstream>
#include <vector>

class SgfLoader {
    std::string m_sFileName;
    std::string m_sSgfString;
    std::vector<Move> m_vMoves;

public:
    SgfLoader() { clear(); }

    bool parseFromFile(const std::string& sgffile)
    {
        this->clear();

        std::ifstream fin(sgffile.c_str());
        if (!fin) { return false; }

        std::string sgf;
        std::string line;
        while (getline(fin, line)) { sgf += line; }

        bool bIsSuccess = parseFromString(sgf);
        m_sFileName = sgffile;

        return bIsSuccess;
    }

    bool parseFromString(const std::string& sgfString)
    {
        this->clear();

        m_sSgfString = sgfString;
        int index = sgfString.find(";") + 1;
        while (index < static_cast<int>(sgfString.length())) {
            size_t endIndex = sgfString.find("];", index);
            if (endIndex == std::string::npos) { endIndex = sgfString.find("])", index); }
            if (endIndex == std::string::npos) { return false; }
            if (!addMove(sgfString, index, endIndex)) { return false; }
            index = endIndex + 2;
        }

        return true;
    }

    inline std::string getFileName() const { return m_sFileName; }
    inline std::string getSgfString() const { return m_sSgfString; }
    inline std::vector<Move>& getMoves() { return m_vMoves; }
    inline const std::vector<Move>& getMoves() const { return m_vMoves; }

    static std::string toSgfString(const vector<Move>& vMoves, const vector<string>& vComments) {
		ostringstream oss;
		oss << "(;FF[4]CA[UTF-8]SZ[" << BOARD_SIZE << "]";

		for (size_t i = 0; i < vMoves.size(); ++i) {
			oss << ";" << vMoves[i].toSgfString();
			if (i < vComments.size() && vComments[i] != "") { oss << "C[" << vComments[i] << "]"; }
		}
        
		oss << ")";
		return oss.str();
	}

private:
    bool addMove(const std::string& sgfString, int start_index, int end_index)
    {
        bool bracket = false;
        std::string sKey = "", sValue = "", sBuffer = "";
        for (int i = start_index; i <= end_index; ++i) {
            if (sgfString[i] == '[') {
                if (!bracket) {
                    if (!sBuffer.empty()) { sKey = sBuffer; }
                    bracket = true;
                    sBuffer.clear();
                } else {
                    return false;
                }
            } else if (sgfString[i] == ']') {
                sValue = sBuffer;
                bracket = false;
                sBuffer.clear();

                if (sKey == "B" || sKey == "W") { m_vMoves.push_back(Move(charToColor(sKey[0]), sValue)); }
            } else {
                if (bracket) {
                    sBuffer += sgfString[i];
                } else if (isupper(sgfString[i])) {
                    sBuffer += sgfString[i];
                }
            }
        }

        return true;
    }

    void clear()
    {
        m_sFileName = "";
        m_sSgfString = "";
        m_vMoves.clear();
    }
};
